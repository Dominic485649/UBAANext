from __future__ import annotations

import logging
import math
import random
import time
from datetime import datetime, timedelta
from typing import Callable, Iterable, Optional, TYPE_CHECKING

from .constants import BEIJING_TZ
from .limits import has_reached_td_limit, td_limit_message
from .models import BatchResult, User, UserResult
from .storage import AppStorage
from .telemetry import enqueue_daily_midnight_snapshot, enqueue_snapshot_event, flush_telemetry_queue

if TYPE_CHECKING:
    from .client import TDClient

WaitSeconds = Callable[[User], int]


def in_any_window(now: datetime, windows: Iterable[str]) -> bool:
    local_now = _localize(now)
    minutes = local_now.hour * 60 + local_now.minute
    for window in windows:
        start, end = parse_window(window)
        if start <= minutes <= end:
            return True
    return False


def parse_window(window: str) -> tuple[int, int]:
    start_text, end_text = window.split("-", 1)
    return _hhmm_to_minutes(start_text), _hhmm_to_minutes(end_text)


def run_scheduler_once(
    storage: AppStorage,
    run_once: Optional[Callable[[], object]] = None,
    now: Optional[datetime] = None,
    logger: Optional[logging.Logger] = None,
    client: Optional[TDClient] = None,
    wait_seconds: Optional[WaitSeconds] = None,
) -> BatchResult:
    del run_once
    now = _localize(now or datetime.now(BEIJING_TZ))
    logger = logger or logging.getLogger("auto_td")
    if client is None:
        from .client import TDClient

        client = TDClient(storage.load_config())
    wait_seconds = wait_seconds or _random_wait_seconds
    settings = storage.load_settings()
    schedule = settings.get("schedule", {})
    windows = schedule.get("windows", [])
    users = storage.list_users()
    state, day_key, user_states = _load_today_user_states(storage, users, now)
    _remove_deleted_user_states(user_states, users, logger)
    if not users:
        storage.save_state(state)
        logger.info("轮询心跳：当前没有用户，跳过")
        return BatchResult([])

    in_window = in_any_window(now, windows)
    results: list[UserResult] = []
    if not in_window:
        _discard_due_actions_outside_window(user_states, users, now, logger)
        storage.save_state(state)
        logger.info("轮询心跳：当前时间不在打卡窗口，没到时间")
        return _status_batch(users, user_states)

    for index, user in enumerate(users, start=1):
        user_state = user_states[user.student_id]
        status = str(user_state.get("status", "pending"))
        if status == "completed":
            logger.info("用户 %s 今日已完成 %s/%s，跳过", user.student_id, user.rounds, user.rounds)
            results.append(UserResult(index, user.student_id, True, "已完成，跳过"))
            continue
        if status == "error":
            message = str(user_state.get("last_error") or user_state.get("last_message") or "未知错误")
            logger.warning("用户 %s 今日状态为 error，等待重启 autotd run 后重试：%s", user.student_id, message)
            results.append(UserResult(index, user.student_id, False, message))
            continue
        if has_reached_td_limit(storage, user):
            message = td_limit_message(user)
            logger.info(message)
            user_state.update(_completed_user_state(user, now, message))
            enqueue_snapshot_event(storage, "td_limit_reached", now=now)
            results.append(UserResult(index, user.student_id, True, message))
            continue

        due_at = _parse_due_at(user_state.get("due_at"))
        if due_at is not None and due_at > now:
            remaining = math.ceil((due_at - now).total_seconds())
            logger.info(
                "用户 %s 等待中，%s 秒后进行第 %s/%s 轮%s打卡",
                user.student_id,
                remaining,
                int(user_state.get("completed_rounds", 0)) + 1,
                user.rounds,
                _action_label(str(user_state.get("next_action", "entrance"))),
            )
            results.append(UserResult(index, user.student_id, True, f"等待 {remaining} 秒"))
            continue

        try:
            ok, message = _perform_due_action(storage, client, user, user_state, now, wait_seconds, logger)
            results.append(UserResult(index, user.student_id, ok, message))
        except Exception as exc:
            _mark_error(user_state, exc, now)
            logger.exception("用户 %s 请求失败，已标记 error：%s", user.student_id, exc)
            results.append(UserResult(index, user.student_id, False, str(exc)))

    state.setdefault("daily_runs", {}).setdefault(day_key, {})["users"] = user_states
    _merge_counts_from_disk(storage, state)
    storage.save_state(state)
    return BatchResult(results)


def run_forever(
    storage: AppStorage,
    stop_requested: Optional[Callable[[], bool]] = None,
    logger: Optional[logging.Logger] = None,
    now_provider: Optional[Callable[[], datetime]] = None,
) -> None:
    stop_requested = stop_requested or (lambda: False)
    now_provider = now_provider or (lambda: datetime.now(BEIJING_TZ))
    last_seen_date = _localize(now_provider()).date()
    while not stop_requested():
        now = _localize(now_provider())
        if now.date() != last_seen_date:
            enqueue_daily_midnight_snapshot(storage, now=now)
            last_seen_date = now.date()
        run_scheduler_once(storage, now=now, logger=logger)
        flush_telemetry_queue(storage)
        poll_seconds = int(storage.load_settings().get("schedule", {}).get("poll_seconds", 60))
        _sleep_with_stop(max(1, poll_seconds), stop_requested)


def clear_today_errors(storage: AppStorage, now: Optional[datetime] = None) -> None:
    now = _localize(now or datetime.now(BEIJING_TZ))
    day_key = now.date().isoformat()
    state = storage.load_state()
    users = state.get("daily_runs", {}).get(day_key, {}).get("users", {})
    changed = False
    for user_state in users.values():
        if user_state.get("status") == "error":
            user_state["status"] = "pending"
            user_state["next_action"] = "entrance"
            user_state["due_at"] = None
            user_state["last_error"] = None
            user_state["last_message"] = "重启后清除错误，等待重新入口打卡"
            user_state["updated_at"] = _iso(now)
            changed = True
    if changed:
        storage.save_state(state)


def format_today_status(storage: AppStorage, now: Optional[datetime] = None) -> list[str]:
    now = _localize(now or datetime.now(BEIJING_TZ))
    day_key = now.date().isoformat()
    users = storage.load_state().get("daily_runs", {}).get(day_key, {}).get("users", {})
    if not users:
        return ["今日暂无用户打卡状态"]

    lines = []
    for student_id in sorted(users):
        user_state = users[student_id]
        status = str(user_state.get("status", "pending"))
        completed = int(user_state.get("completed_rounds", 0))
        next_action = user_state.get("next_action") or "-"
        due_at = _parse_due_at(user_state.get("due_at"))
        detail = str(user_state.get("last_error") or user_state.get("last_message") or "")
        if due_at is not None and due_at > now:
            detail = f"剩余 {math.ceil((due_at - now).total_seconds())} 秒"
        lines.append(f"{student_id}: {status} completed={completed} next={next_action} {detail}".rstrip())
    return lines


def _hhmm_to_minutes(value: str) -> int:
    hour_text, minute_text = value.strip().split(":", 1)
    hour = int(hour_text)
    minute = int(minute_text)
    if not 0 <= hour <= 23 or not 0 <= minute <= 59:
        raise ValueError(f"非法时间: {value}")
    return hour * 60 + minute


def _sleep_with_stop(seconds: int, stop_requested: Callable[[], bool]) -> None:
    for _ in range(seconds):
        if stop_requested():
            return
        time.sleep(1)


def _load_today_user_states(
    storage: AppStorage,
    users: list[User],
    now: datetime,
) -> tuple[dict, str, dict]:
    day_key = now.date().isoformat()
    state = storage.load_state()
    daily_runs = state.setdefault("daily_runs", {})
    if day_key not in daily_runs:
        daily_runs[day_key] = {"users": {}}
        if state.get("schedule", {}).get("last_attempt_date") == day_key:
            daily_runs[day_key]["users"] = {
                user.student_id: _completed_user_state(user, now, "由旧版今日已尝试状态迁移为已完成，避免重复打卡")
                for user in users
            }

    day_state = daily_runs.setdefault(day_key, {"users": {}})
    user_states = day_state.setdefault("users", {})
    for user in users:
        user_states.setdefault(user.student_id, _new_user_state(now))
    return state, day_key, user_states


def _remove_deleted_user_states(user_states: dict, users: list[User], logger: logging.Logger) -> None:
    active_ids = {user.student_id for user in users}
    for student_id in list(user_states):
        if student_id not in active_ids:
            logger.info("用户 %s 已从配置删除，取消今日待执行状态", student_id)
            user_states.pop(student_id, None)


def _discard_due_actions_outside_window(
    user_states: dict,
    users: list[User],
    now: datetime,
    logger: logging.Logger,
) -> None:
    for user in users:
        user_state = user_states[user.student_id]
        if user_state.get("status") not in ("pending", "waiting"):
            continue
        due_at = _parse_due_at(user_state.get("due_at"))
        if due_at is None or due_at > now:
            continue
        action = str(user_state.get("next_action", "entrance"))
        round_no = int(user_state.get("completed_rounds", 0)) + 1
        if action == "exit":
            message = f"第 {round_no}/{user.rounds} 轮出口到期时已过合法时间，本轮作废，下个窗口重新入口"
        else:
            message = f"第 {round_no}/{user.rounds} 轮入口到期时不在合法时间，等待下个窗口"
        logger.info("用户 %s %s", user.student_id, message)
        user_state.update(
            {
                "status": "pending",
                "next_action": "entrance",
                "due_at": None,
                "last_message": message,
                "updated_at": _iso(now),
            }
        )


def _perform_due_action(
    storage: AppStorage,
    client: TDClient,
    user: User,
    user_state: dict,
    now: datetime,
    wait_seconds: WaitSeconds,
    logger: logging.Logger,
) -> tuple[bool, str]:
    action = str(user_state.get("next_action") or "entrance")
    completed_rounds = int(user_state.get("completed_rounds", 0))
    round_no = completed_rounds + 1
    if completed_rounds >= user.rounds:
        user_state.update(_completed_user_state(user, now, f"今日已完成 {user.rounds}/{user.rounds}"))
        return True, "已完成，跳过"

    if action == "exit":
        return _perform_exit(storage, client, user, user_state, now, wait_seconds, logger, round_no)
    return _perform_entrance(storage, client, user, user_state, now, wait_seconds, logger, round_no)


def _perform_entrance(
    storage: AppStorage,
    client: TDClient,
    user: User,
    user_state: dict,
    now: datetime,
    wait_seconds: WaitSeconds,
    logger: logging.Logger,
    round_no: int,
) -> tuple[bool, str]:
    logger.info("用户 %s 第 %s/%s 轮入口打卡", user.student_id, round_no, user.rounds)
    response = client.check(user, user.entrance_machine_id, timestamp=now.timestamp())
    _record_count(storage, user, response.count, count_source="td_entrance" if response.success else "observation")
    logger.info("服务器消息：%s", response.server_message)
    if not response.success:
        if "非法时间" in response.server_message:
            _discard_current_round(user_state, user, now, "入口")
            logger.info("用户 %s 第 %s/%s 轮入口非法时间，本轮作废", user.student_id, round_no, user.rounds)
            return True, "非法时间，本轮作废"
        raise RuntimeError(response.server_message or "入口打卡失败")
    photo = storage.image_path(user.entrance_image).read_bytes()
    client.upload_photo(user.entrance_machine_id, photo, timestamp=now.timestamp())
    if has_reached_td_limit(storage, user):
        message = td_limit_message(user)
        logger.info(message)
        user_state.update(_completed_user_state(user, now, message))
        enqueue_snapshot_event(storage, "td_limit_reached", now=now)
        return True, message

    wait_s = int(wait_seconds(user))
    due_at = now + timedelta(seconds=max(0, wait_s))
    message = f"入口完成，等待 {wait_s} 秒后出口"
    logger.info(message)
    user_state.update(
        {
            "status": "waiting",
            "next_action": "exit",
            "due_at": _iso(due_at),
            "last_message": message,
            "last_error": None,
            "updated_at": _iso(now),
        }
    )
    return True, message


def _perform_exit(
    storage: AppStorage,
    client: TDClient,
    user: User,
    user_state: dict,
    now: datetime,
    wait_seconds: WaitSeconds,
    logger: logging.Logger,
    round_no: int,
) -> tuple[bool, str]:
    logger.info("用户 %s 第 %s/%s 轮出口打卡", user.student_id, round_no, user.rounds)
    response = client.check(user, user.exit_machine_id, timestamp=now.timestamp())
    _record_count(storage, user, response.count, count_source="td_exit" if response.success else "observation")
    logger.info("服务器消息：%s", response.server_message)
    if not response.success:
        if "非法时间" in response.server_message:
            _discard_current_round(user_state, user, now, "出口")
            logger.info("用户 %s 第 %s/%s 轮出口非法时间，本轮作废", user.student_id, round_no, user.rounds)
            return True, "非法时间，本轮作废"
        raise RuntimeError(response.server_message or "出口打卡失败")
    photo = storage.image_path(user.exit_image).read_bytes()
    client.upload_photo(user.exit_machine_id, photo, timestamp=now.timestamp())

    completed_rounds = int(user_state.get("completed_rounds", 0)) + 1
    if completed_rounds >= user.rounds:
        message = f"今日已完成 {completed_rounds}/{user.rounds}"
        logger.info("用户 %s 打卡流程完成：%s", user.student_id, message)
        user_state.update(_completed_user_state(user, now, message, completed_rounds=completed_rounds))
        return True, message

    wait_s = int(wait_seconds(user))
    due_at = now + timedelta(seconds=max(0, wait_s))
    message = f"出口完成，等待 {wait_s} 秒后下一轮入口"
    logger.info(message)
    user_state.update(
        {
            "status": "waiting",
            "completed_rounds": completed_rounds,
            "next_action": "entrance",
            "due_at": _iso(due_at),
            "last_message": message,
            "last_error": None,
            "updated_at": _iso(now),
        }
    )
    return True, message


def _discard_current_round(user_state: dict, user: User, now: datetime, action_label: str) -> None:
    round_no = int(user_state.get("completed_rounds", 0)) + 1
    user_state.update(
        {
            "status": "pending",
            "next_action": "entrance",
            "due_at": None,
            "last_message": f"第 {round_no}/{user.rounds} 轮{action_label}非法时间，本轮作废",
            "last_error": None,
            "updated_at": _iso(now),
        }
    )


def _mark_error(user_state: dict, exc: Exception, now: datetime) -> None:
    user_state.update(
        {
            "status": "error",
            "last_error": str(exc),
            "last_message": "请求失败，等待重启 autotd run 后重试",
            "updated_at": _iso(now),
        }
    )


def _status_batch(users: list[User], user_states: dict) -> BatchResult:
    results = []
    for index, user in enumerate(users, start=1):
        status = user_states[user.student_id].get("status", "pending")
        results.append(UserResult(index, user.student_id, status != "error", str(status)))
    return BatchResult(results)


def _new_user_state(now: datetime) -> dict:
    return {
        "status": "pending",
        "completed_rounds": 0,
        "next_action": "entrance",
        "due_at": None,
        "last_message": "等待入口打卡",
        "last_error": None,
        "updated_at": _iso(now),
    }


def _completed_user_state(
    user: User,
    now: datetime,
    message: str,
    completed_rounds: Optional[int] = None,
) -> dict:
    return {
        "status": "completed",
        "completed_rounds": int(completed_rounds if completed_rounds is not None else user.rounds),
        "next_action": None,
        "due_at": None,
        "last_message": message,
        "last_error": None,
        "updated_at": _iso(now),
    }


def _record_count(storage: AppStorage, user: User, count: Optional[int], count_source: str = "observation") -> None:
    if count is not None:
        storage.set_last_count(user.student_id, count, count_source=count_source)


def _merge_counts_from_disk(storage: AppStorage, state: dict) -> None:
    current_counts = storage.load_state().get("counts")
    if current_counts is not None:
        state["counts"] = current_counts


def _random_wait_seconds(user: User) -> int:
    return random.randint(user.wait_time_min, user.wait_time_max)


def _action_label(action: str) -> str:
    return "出口" if action == "exit" else "入口"


def _localize(value: datetime) -> datetime:
    if value.tzinfo is None:
        return value.replace(tzinfo=BEIJING_TZ)
    return value.astimezone(BEIJING_TZ)


def _iso(value: datetime) -> str:
    return _localize(value).isoformat(timespec="seconds")


def _parse_due_at(value: object) -> Optional[datetime]:
    if not value:
        return None
    parsed = datetime.fromisoformat(str(value))
    return _localize(parsed)
