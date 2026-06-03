import logging
import random
import time
from typing import Callable, Iterable, Optional

from .client import TDClient
from .limits import has_reached_td_limit, td_limit_message
from .models import BatchResult, User, UserResult
from .storage import AppStorage
from .telemetry import enqueue_snapshot_event


LOGGER = logging.getLogger("auto_td")
LOGGER.addHandler(logging.NullHandler())
LOGGER.propagate = False

Sleeper = Callable[[int], None]


def run_user_pipeline(
    storage: AppStorage,
    client: TDClient,
    user: User,
    sleeper: Sleeper = None,
    logger: Optional[logging.Logger] = None,
) -> bool:
    sleeper = sleeper or sleep_with_countdown
    logger = logger or logging.getLogger("auto_td")
    entrance_photo = storage.image_path(user.entrance_image).read_bytes()
    exit_photo = storage.image_path(user.exit_image).read_bytes()

    logger.info("用户 %s 开始，入口=%s 出口=%s 轮数=%s", user.student_id, user.entrance_machine_id, user.exit_machine_id, user.rounds)
    for round_idx in range(user.rounds):
        if has_reached_td_limit(storage, user):
            message = td_limit_message(user)
            logger.info(message)
            enqueue_snapshot_event(storage, "td_limit_reached")
            return True

        logger.info("用户 %s 第 %s/%s 轮入口打卡", user.student_id, round_idx + 1, user.rounds)
        entrance = client.check(user, user.entrance_machine_id)
        _record_count(storage, user, entrance.count, count_source="td_entrance" if entrance.success else "observation")
        logger.info("服务器消息：%s", entrance.server_message)
        if not entrance.success:
            logger.warning("用户 %s 入口打卡失败", user.student_id)
            return False
        client.upload_photo(user.entrance_machine_id, entrance_photo)
        if has_reached_td_limit(storage, user):
            message = td_limit_message(user)
            logger.info(message)
            enqueue_snapshot_event(storage, "td_limit_reached")
            return True

        wait_s = random.randint(user.wait_time_min, user.wait_time_max)
        logger.info("入口完成，等待 %s 秒后进行出口打卡", wait_s)
        sleeper(wait_s)

        logger.info("用户 %s 第 %s/%s 轮出口打卡", user.student_id, round_idx + 1, user.rounds)
        exit_response = client.check(user, user.exit_machine_id)
        _record_count(storage, user, exit_response.count, count_source="td_exit" if exit_response.success else "observation")
        logger.info("服务器消息：%s", exit_response.server_message)
        if not exit_response.success:
            logger.warning("用户 %s 出口打卡失败", user.student_id)
            return False
        client.upload_photo(user.exit_machine_id, exit_photo)

        if round_idx < user.rounds - 1:
            wait_s = random.randint(user.wait_time_min, user.wait_time_max)
            logger.info("出口完成，等待 %s 秒后进行下一轮", wait_s)
            sleeper(wait_s)

    logger.info("用户 %s 打卡流程完成", user.student_id)
    return True


def run_all_users(
    storage: AppStorage,
    client: Optional[TDClient] = None,
    users: Optional[Iterable[User]] = None,
    sleeper: Sleeper = None,
    logger: Optional[logging.Logger] = None,
) -> BatchResult:
    logger = logger or logging.getLogger("auto_td")
    client = client or TDClient(storage.load_config())
    user_list = list(users) if users is not None else storage.list_users()
    results = []

    for index, user in enumerate(user_list, start=1):
        try:
            limit_reached_before = has_reached_td_limit(storage, user)
            ok = run_user_pipeline(storage, client, user, sleeper=sleeper, logger=logger)
            if ok and (limit_reached_before or has_reached_td_limit(storage, user)):
                message = td_limit_message(user)
            else:
                message = "成功" if ok else "pipeline returned failure"
            results.append(UserResult(index, user.student_id, ok, message))
        except Exception as exc:
            logger.exception("用户 %s 失败，继续下一个用户: %s", user.student_id, exc)
            results.append(UserResult(index, user.student_id, False, str(exc)))

    batch = BatchResult(results)
    logger.info("批量执行完成：total=%s success=%s failure=%s", batch.total, batch.success_count, batch.failure_count)
    return batch


def sleep_with_countdown(seconds: int) -> None:
    if seconds <= 0:
        return
    for _ in range(seconds):
        time.sleep(1)


def _record_count(storage: AppStorage, user: User, count: Optional[int], count_source: str = "observation") -> None:
    if count is not None:
        storage.set_last_count(user.student_id, count, count_source=count_source)
