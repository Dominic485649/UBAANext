import argparse
import json
import os
import signal
from pathlib import Path
from typing import List, Optional

from .background import clear_pid_file, get_scheduler_status, start_scheduler_process, stop_scheduler_process
from .client import TDClient
from .logging_utils import setup_logging
from .models import User
from .quick import build_quick_user
from .runner import run_all_users
from .scheduler import clear_today_errors, format_today_status, run_forever
from .storage import AppStorage
from .telemetry import (
    disable_telemetry,
    enable_telemetry,
    enqueue_daemon_started,
    enqueue_daemon_stopped,
    enqueue_install_initialized,
    enqueue_run_requested,
    enqueue_snapshot_event,
    enqueue_stop_requested,
    enqueue_user_changed,
    flush_telemetry_queue,
    get_telemetry_status,
)
from .ui import serve_ui


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.stop:
            return cmd_stop(args)
        if args.daemon_worker:
            return cmd_daemon_worker(args)
        if not hasattr(args, "func"):
            parser.print_help()
            return 2
        return int(args.func(args) or 0)
    except Exception as exc:
        print(f"error: {exc}")
        return 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="autotd")
    parser.add_argument("--stop", action="store_true", help="停止后台定时检测")
    parser.add_argument("--daemon-worker", action="store_true", help=argparse.SUPPRESS)
    subparsers = parser.add_subparsers(dest="command")

    init_parser = subparsers.add_parser("init", help="初始化 ~/.autoTD")
    init_parser.add_argument("--from", dest="source", help="从旧 autoTD 项目导入配置")
    init_parser.set_defaults(func=cmd_init)

    user_parser = subparsers.add_parser("user", help="用户管理")
    user_sub = user_parser.add_subparsers(dest="user_command", required=True)
    user_add = user_sub.add_parser("add", help="新增用户")
    _add_user_identity_args(user_add)
    user_add.set_defaults(func=cmd_user_add)

    user_sub.add_parser("list", help="列出用户").set_defaults(func=cmd_user_list)
    user_show = user_sub.add_parser("show", help="查看用户")
    user_show.add_argument("student_id")
    user_show.set_defaults(func=cmd_user_show)

    user_update = user_sub.add_parser("update", help="修改用户")
    user_update.add_argument("student_id")
    user_update.add_argument("--card-id")
    user_update.add_argument("--entrance", type=int)
    user_update.add_argument("--exit", type=int)
    user_update.add_argument("--entrance-image")
    user_update.add_argument("--exit-image")
    user_update.add_argument("--rounds", type=int)
    user_update.add_argument("--wait-time-min", type=int)
    user_update.add_argument("--wait-time-max", type=int)
    user_update.set_defaults(func=cmd_user_update)

    user_delete = user_sub.add_parser("delete", help="删除用户")
    user_delete.add_argument("student_id")
    user_delete.set_defaults(func=cmd_user_delete)

    user_count = user_sub.add_parser(
        "count",
        help="发送真实 checkdata 请求并解析服务器返回的本学期锻炼次数；可能产生服务器侧刷卡记录",
    )
    user_count.add_argument("student_id")
    user_count.add_argument("--refresh", action="store_true", help="发送真实请求刷新并写入本地缓存")
    user_count.set_defaults(func=cmd_user_count)

    image_parser = subparsers.add_parser("image", help="图片管理")
    image_sub = image_parser.add_subparsers(dest="image_command", required=True)
    image_add = image_sub.add_parser("add", help="新增图片")
    image_add.add_argument("path")
    image_add.add_argument("--name")
    image_add.add_argument("--overwrite", action="store_true")
    image_add.set_defaults(func=cmd_image_add)
    image_sub.add_parser("list", help="列出图片").set_defaults(func=cmd_image_list)

    run_parser = subparsers.add_parser("run", help="运行打卡")
    run_parser.add_argument("--once", action="store_true", help="强制立即运行一次")
    run_parser.set_defaults(func=cmd_run)

    subparsers.add_parser("status", help="查看后台定时检测状态").set_defaults(func=cmd_status)

    ui_parser = subparsers.add_parser("ui", help="打开 Web 管理端")
    ui_parser.add_argument("--host", default="127.0.0.1", help="监听地址，默认 127.0.0.1")
    ui_parser.add_argument("--port", type=int, default=8765, help="监听端口，默认 8765")
    ui_parser.add_argument("--no-open", action="store_true", help="只启动服务，不自动打开浏览器")
    ui_parser.set_defaults(func=cmd_ui)

    schedule_parser = subparsers.add_parser("schedule", help="定时配置")
    schedule_sub = schedule_parser.add_subparsers(dest="schedule_command", required=True)
    schedule_sub.add_parser("show", help="查看定时配置").set_defaults(func=cmd_schedule_show)
    schedule_set = schedule_sub.add_parser("set", help="设置定时参数")
    schedule_set.add_argument("--poll-seconds", type=int)
    schedule_set.add_argument("--windows")
    schedule_set.set_defaults(func=cmd_schedule_set)

    telemetry_parser = subparsers.add_parser("telemetry", help="遥测上报设置")
    telemetry_sub = telemetry_parser.add_subparsers(dest="telemetry_command", required=True)
    telemetry_sub.add_parser("status", help="查看遥测状态").set_defaults(func=cmd_telemetry_status)
    telemetry_enable = telemetry_sub.add_parser("enable", help="开启遥测上报")
    telemetry_enable.add_argument("--endpoint", help="Cloudflare Worker endpoint")
    telemetry_enable.set_defaults(func=cmd_telemetry_enable)
    telemetry_sub.add_parser("disable", help="关闭遥测上报并清空队列").set_defaults(func=cmd_telemetry_disable)
    telemetry_sub.add_parser("sync", help="立即发送本地遥测队列").set_defaults(func=cmd_telemetry_sync)
    return parser


def cmd_init(args) -> int:
    storage = AppStorage()
    storage.initialize(Path(args.source) if args.source else None)
    enqueue_install_initialized(storage)
    flush_telemetry_queue(storage)
    print(f"initialized {storage.home}")
    return 0


def cmd_user_add(args) -> int:
    storage = _storage()
    if args.quick:
        user = build_quick_user(storage, args.student_id, args.quick, card_id=args.card_id or "")
    else:
        _require_manual_user_args(args)
        user = User.from_dict(_user_args_to_dict(args))
    storage.save_user(user)
    enqueue_user_changed(storage, "add", user.student_id)
    flush_telemetry_queue(storage)
    print(f"added {user.student_id}")
    return 0


def cmd_user_list(_args) -> int:
    storage = _storage()
    users = storage.list_users()
    if not users:
        print("no users")
        return 0
    for user in users:
        print(f"{user.student_id}\tcard={user.card_id}\tentrance={user.entrance_machine_id}\texit={user.exit_machine_id}")
    return 0


def cmd_user_show(args) -> int:
    storage = _storage()
    user = storage.get_user(args.student_id)
    if user is None:
        raise KeyError(f"用户不存在: {args.student_id}")
    print(json.dumps(user, ensure_ascii=False, indent=2))
    return 0


def cmd_user_update(args) -> int:
    storage = _storage()
    current = storage.get_user(args.student_id)
    if current is None:
        raise KeyError(f"用户不存在: {args.student_id}")
    updates = {
        "card_id": args.card_id,
        "entrance_machine_id": args.entrance,
        "exit_machine_id": args.exit,
        "entrance_image": args.entrance_image,
        "exit_image": args.exit_image,
        "rounds": args.rounds,
        "wait_time_min": args.wait_time_min,
        "wait_time_max": args.wait_time_max,
    }
    for key, value in updates.items():
        if value is not None:
            current[key] = value
    user = User.from_dict(current)
    storage.save_user(user)
    enqueue_user_changed(storage, "update", user.student_id)
    flush_telemetry_queue(storage)
    print(f"updated {user.student_id}")
    return 0


def cmd_user_delete(args) -> int:
    storage = _storage()
    if not storage.delete_user(args.student_id):
        raise KeyError(f"用户不存在: {args.student_id}")
    enqueue_user_changed(storage, "delete", args.student_id)
    flush_telemetry_queue(storage)
    print(f"deleted {args.student_id}")
    return 0


def cmd_user_count(args) -> int:
    storage = _storage()
    student_id = str(args.student_id)
    if not args.refresh:
        count = storage.get_last_count(student_id)
        if count is None:
            raise ValueError(f"本地没有 {student_id} 的锻炼次数缓存，请在刷卡时间内运行 autotd user count {student_id} --refresh")
        print(f"{student_id}: 本学期锻炼次数 {count}（缓存）")
        return 0

    user = storage.get_user_model(student_id)
    client = TDClient(storage.load_config())
    count = client.query_count(user, machine_id=user.entrance_machine_id)
    storage.set_last_count(user.student_id, count)
    flush_telemetry_queue(storage)
    print(f"{user.student_id}: 本学期锻炼次数 {count}（服务器刷新）")
    return 0


def cmd_image_add(args) -> int:
    storage = _storage()
    name = storage.add_image(Path(args.path), name=args.name, overwrite=args.overwrite)
    print(f"added image {name}")
    return 0


def cmd_image_list(_args) -> int:
    storage = _storage()
    for name in storage.list_images():
        print(name)
    return 0


def cmd_run(args) -> int:
    storage = _storage()
    enqueue_run_requested(storage, once=bool(args.once))

    if args.once:
        logger = setup_logging(storage)
        run_once_foreground(storage, logger)
        flush_telemetry_queue(storage)
    else:
        clear_today_errors(storage)
        flush_telemetry_queue(storage)
        result = start_scheduler_process(storage)
        if result.already_running:
            print(f"后台定时检测已在运行 PID={result.pid}")
        else:
            print(f"后台定时检测已启动 PID={result.pid}")
            print(f"停止命令：autotd --stop")
    return 0


def run_once_foreground(storage: AppStorage, logger) -> object:
    result = run_all_users(storage, logger=logger)
    print_summary(result)
    return result


def cmd_daemon_worker(_args) -> int:
    storage = _storage()
    logger = setup_logging(storage, stream=False)
    stop_state = {"requested": False}

    def request_stop(_signum, _frame):
        stop_state["requested"] = True

    signal.signal(signal.SIGTERM, request_stop)
    signal.signal(signal.SIGINT, request_stop)

    logger.info("后台定时检测启动 PID=%s", os.getpid())
    enqueue_daemon_started(storage, os.getpid())
    flush_telemetry_queue(storage)
    try:
        run_forever(storage, stop_requested=lambda: stop_state["requested"], logger=logger)
    finally:
        enqueue_daemon_stopped(storage, os.getpid())
        flush_telemetry_queue(storage)
        clear_pid_file(storage.pid_path, expected_pid=os.getpid())
        logger.info("后台定时检测退出 PID=%s", os.getpid())
    return 0


def cmd_stop(_args) -> int:
    storage = AppStorage()
    if storage.home.exists():
        enqueue_stop_requested(storage)
        flush_telemetry_queue(storage)
    result = stop_scheduler_process(storage)
    print(result.message)
    return 0


def cmd_status(_args) -> int:
    storage = AppStorage()
    result = get_scheduler_status(storage)
    print(result.message)
    for line in format_today_status(storage):
        print(line)
    return 0


def cmd_ui(args) -> int:
    return serve_ui(host=args.host, port=args.port, open_browser=not args.no_open)


def cmd_schedule_show(_args) -> int:
    storage = _storage()
    print(json.dumps(storage.load_settings().get("schedule", {}), ensure_ascii=False, indent=2))
    return 0


def cmd_schedule_set(args) -> int:
    storage = _storage()
    patch = {"schedule": {}}
    if args.poll_seconds is not None:
        if args.poll_seconds <= 0:
            raise ValueError("poll-seconds 必须大于 0")
        patch["schedule"]["poll_seconds"] = args.poll_seconds
    if args.windows:
        patch["schedule"]["windows"] = [item.strip() for item in args.windows.split(",") if item.strip()]
    storage.update_settings(patch)
    print("schedule updated")
    return 0


def cmd_telemetry_status(_args) -> int:
    storage = _storage()
    status = get_telemetry_status(storage)
    print(json.dumps(status, ensure_ascii=False, indent=2))
    return 0


def cmd_telemetry_enable(args) -> int:
    storage = _storage()
    enable_telemetry(storage, endpoint=args.endpoint)
    enqueue_snapshot_event(storage, "telemetry_enabled")
    sent = flush_telemetry_queue(storage)
    print(f"telemetry enabled, sent={sent}")
    return 0


def cmd_telemetry_disable(_args) -> int:
    storage = _storage()
    disable_telemetry(storage)
    print("telemetry disabled")
    return 0


def cmd_telemetry_sync(_args) -> int:
    storage = _storage()
    sent = flush_telemetry_queue(storage)
    print(f"telemetry synced, sent={sent}")
    return 0


def print_summary(result) -> None:
    print(f"total={result.total} success={result.success_count} failure={result.failure_count}")
    for item in result.results:
        status = "success" if item.success else "failure"
        print(f"{item.index}. {item.student_id}: {status} {item.message}")


def _storage() -> AppStorage:
    storage = AppStorage()
    if not storage.home.exists():
        raise FileNotFoundError(f"配置目录不存在，请先运行 autotd init: {storage.home}")
    return storage


def _add_user_identity_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("student_id")
    parser.add_argument("--card-id")
    parser.add_argument("--quick", help="快速配置校区：沙河 或 学院路")
    parser.add_argument("--entrance", type=int)
    parser.add_argument("--exit", type=int)
    parser.add_argument("--entrance-image")
    parser.add_argument("--exit-image")
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--wait-time-min", type=int, default=180)
    parser.add_argument("--wait-time-max", type=int, default=240)


def _user_args_to_dict(args) -> dict:
    return {
        "student_id": args.student_id,
        "card_id": args.card_id or "",
        "entrance_machine_id": args.entrance,
        "exit_machine_id": args.exit,
        "entrance_image": args.entrance_image,
        "exit_image": args.exit_image,
        "rounds": args.rounds,
        "wait_time_min": args.wait_time_min,
        "wait_time_max": args.wait_time_max,
    }


def _require_manual_user_args(args) -> None:
    missing = []
    for attr, flag in (
        ("entrance", "--entrance"),
        ("exit", "--exit"),
        ("entrance_image", "--entrance-image"),
        ("exit_image", "--exit-image"),
    ):
        if getattr(args, attr) is None:
            missing.append(flag)
    if missing:
        raise ValueError("非 quick 添加用户时必须提供: " + ", ".join(missing))


if __name__ == "__main__":
    raise SystemExit(main())
