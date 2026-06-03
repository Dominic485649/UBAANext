import json
import os
import signal
import subprocess
import sys
import time
from csv import reader as csv_reader
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from .storage import AppStorage


@dataclass(frozen=True)
class BackgroundStartResult:
    pid: int
    pid_path: Path
    already_running: bool


@dataclass(frozen=True)
class BackgroundStopResult:
    pid: Optional[int]
    stopped: bool
    message: str


@dataclass(frozen=True)
class BackgroundStatusResult:
    running: bool
    pid: Optional[int]
    stale: bool
    message: str


def start_scheduler_process(storage: AppStorage) -> BackgroundStartResult:
    pid = read_pid_file(storage.pid_path)
    if pid is not None and is_process_running(pid):
        return BackgroundStartResult(pid=pid, pid_path=storage.pid_path, already_running=True)

    clear_pid_file(storage.pid_path)
    storage.home.mkdir(parents=True, exist_ok=True)
    process = subprocess.Popen(
        [sys.executable, "-m", "auto_td.cli", "--daemon-worker"],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
        close_fds=True,
    )
    write_pid_file(storage.pid_path, process.pid)
    return BackgroundStartResult(pid=process.pid, pid_path=storage.pid_path, already_running=False)


def stop_scheduler_process(storage: AppStorage, timeout: float = 5.0) -> BackgroundStopResult:
    pid = read_pid_file(storage.pid_path)
    if pid is None:
        clear_pid_file(storage.pid_path)
        return BackgroundStopResult(None, False, "未发现后台定时检测进程")

    if not is_process_running(pid):
        clear_pid_file(storage.pid_path)
        return BackgroundStopResult(pid, False, f"未发现运行中的后台定时检测进程 PID={pid}，已清理 pid 文件")

    try:
        terminate_process(pid)
    except ProcessLookupError:
        clear_pid_file(storage.pid_path)
        return BackgroundStopResult(pid, False, f"后台定时检测进程不存在或已退出 PID={pid}，已清理 pid 文件")
    except PermissionError:
        return BackgroundStopResult(pid, False, f"停止后台定时检测进程权限不足 PID={pid}，请使用管理员权限重试")
    except Exception as exc:
        return BackgroundStopResult(pid, False, f"停止后台定时检测进程失败 PID={pid}: {exc}")
    deadline = time.time() + timeout
    while time.time() < deadline:
        if not is_process_running(pid):
            clear_pid_file(storage.pid_path)
            return BackgroundStopResult(pid, True, f"已停止后台定时检测进程 PID={pid}")
        time.sleep(0.1)

    return BackgroundStopResult(pid, True, f"已发送停止信号给后台定时检测进程 PID={pid}")


def get_scheduler_status(storage: AppStorage) -> BackgroundStatusResult:
    pid = read_pid_file(storage.pid_path)
    if pid is None:
        clear_pid_file(storage.pid_path)
        return BackgroundStatusResult(False, None, False, "后台定时检测未运行")

    if is_process_running(pid):
        return BackgroundStatusResult(True, pid, False, f"后台定时检测运行中 PID={pid}")

    clear_pid_file(storage.pid_path)
    return BackgroundStatusResult(False, pid, True, f"后台定时检测未运行，旧 pid 文件已清理 PID={pid}")


def read_pid_file(pid_path: Path) -> Optional[int]:
    try:
        payload = json.loads(pid_path.read_text(encoding="utf-8"))
        pid = int(payload["pid"])
    except (FileNotFoundError, KeyError, TypeError, ValueError, json.JSONDecodeError):
        return None
    return pid if pid > 0 else None


def write_pid_file(pid_path: Path, pid: int) -> None:
    pid_path.parent.mkdir(parents=True, exist_ok=True)
    pid_path.write_text(json.dumps({"pid": int(pid)}, ensure_ascii=False) + "\n", encoding="utf-8")


def clear_pid_file(pid_path: Path, expected_pid: Optional[int] = None) -> None:
    if expected_pid is not None and read_pid_file(pid_path) != expected_pid:
        return
    try:
        pid_path.unlink()
    except FileNotFoundError:
        pass


def is_process_running(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name == "nt":
        return _is_process_running_windows(pid)
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def terminate_process(pid: int) -> None:
    if os.name == "nt":
        _terminate_process_windows(pid)
        return
    os.kill(pid, signal.SIGTERM)


def _is_process_running_windows(pid: int) -> bool:
    completed = subprocess.run(
        ["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV", "/NH"],
        capture_output=True,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        return False

    for raw_line in completed.stdout.splitlines():
        line = raw_line.strip()
        if not line or not line.startswith('"'):
            continue
        row = next(csv_reader([line]), [])
        if len(row) < 2:
            continue
        try:
            if int(row[1]) == pid:
                return True
        except ValueError:
            continue
    return False


def _terminate_process_windows(pid: int) -> None:
    completed = subprocess.run(
        ["taskkill", "/PID", str(pid), "/T"],
        capture_output=True,
        text=True,
        check=False,
    )
    if completed.returncode == 0:
        return

    detail = "\n".join(part.strip() for part in (completed.stdout, completed.stderr) if part and part.strip())
    lowered = detail.lower()
    if any(token in lowered for token in ("not found", "no running instance", "no tasks are running")):
        raise ProcessLookupError(pid)
    if any(token in detail for token in ("找不到", "不存在")):
        raise ProcessLookupError(pid)
    if "access is denied" in lowered or any(token in detail for token in ("拒绝访问", "存取被拒")):
        raise PermissionError(detail or f"Access denied when stopping PID={pid}")
    raise RuntimeError(detail or f"taskkill failed with exit code {completed.returncode}")
