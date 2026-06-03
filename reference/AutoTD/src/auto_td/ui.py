from __future__ import annotations

import json
import mimetypes
import tempfile
import webbrowser
from dataclasses import dataclass
from datetime import datetime
from email import policy
from email.parser import BytesParser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from importlib import resources
from pathlib import Path
from typing import Any, Callable, Optional
from urllib.parse import quote, unquote, urlparse

from .background import get_scheduler_status, start_scheduler_process, stop_scheduler_process
from .constants import BEIJING_TZ
from .logging_utils import setup_logging
from .quick import build_quick_user
from .runner import run_all_users
from .scheduler import clear_today_errors
from .storage import AppStorage
from .telemetry import enqueue_run_requested, enqueue_stop_requested, enqueue_user_changed, flush_telemetry_queue


StorageFactory = Callable[[], AppStorage]


@dataclass(frozen=True)
class UploadedFile:
    filename: str
    content: bytes
    overwrite: bool = False


class UiService:
    def __init__(self, storage_factory: StorageFactory = AppStorage):
        self.storage_factory = storage_factory

    def get_state(self) -> dict[str, Any]:
        storage = self._storage()
        state = storage.load_state()
        today_key = datetime.now(BEIJING_TZ).date().isoformat()
        today_states = state.get("daily_runs", {}).get(today_key, {}).get("users", {})
        counts = state.get("counts", {})
        background = get_scheduler_status(storage)
        users = []
        for user in sorted(storage.list_users(), key=lambda item: item.student_id):
            count_info = counts.get(user.student_id) or {}
            users.append(
                {
                    "student_id": user.student_id,
                    "card_id": user.card_id,
                    "entrance_machine_id": user.entrance_machine_id,
                    "exit_machine_id": user.exit_machine_id,
                    "rounds": user.rounds,
                    "td_count": count_info.get("count"),
                    "td_count_updated_at": count_info.get("updated_at"),
                    "today": _today_payload(today_states.get(user.student_id), user.rounds),
                }
            )
        return {
            "home": str(storage.home),
            "refreshed_at": datetime.now(BEIJING_TZ).isoformat(timespec="seconds"),
            "background": {
                "running": background.running,
                "pid": background.pid,
                "stale": background.stale,
                "message": background.message,
            },
            "users": users,
            "images": self.list_images(),
        }

    def list_images(self) -> list[dict[str, Any]]:
        storage = self._storage()
        images = []
        for name in storage.list_images():
            path = storage.image_path(name)
            stat = path.stat()
            images.append(
                {
                    "name": name,
                    "size": stat.st_size,
                    "url": f"/images/{quote(name)}",
                    "updated_at": datetime.fromtimestamp(stat.st_mtime, BEIJING_TZ).isoformat(timespec="seconds"),
                }
            )
        return images

    def save_uploaded_image(self, filename: str, content: bytes, overwrite: bool = False) -> str:
        if not content:
            raise ValueError("图片内容不能为空")
        with tempfile.NamedTemporaryFile(delete=False) as tmp:
            tmp.write(content)
            tmp_path = Path(tmp.name)
        try:
            return self._storage().add_image(tmp_path, name=Path(filename).name, overwrite=overwrite)
        finally:
            try:
                tmp_path.unlink()
            except FileNotFoundError:
                pass

    def delete_image(self, name: str) -> dict[str, Any]:
        deleted = self._storage().delete_image(name)
        return {"deleted": deleted, "name": name}

    def add_user(self, payload: dict[str, Any]) -> dict[str, Any]:
        student_id = str(payload.get("student_id", "")).strip()
        campus = str(payload.get("campus", "")).strip()
        card_id = str(payload.get("card_id", "") or "").strip()
        if not student_id:
            raise ValueError("学号不能为空")
        if campus not in ("沙河", "学院路"):
            raise ValueError("校区只能选择 沙河 或 学院路")
        storage = self._storage()
        user = build_quick_user(storage, student_id, campus, card_id=card_id)
        storage.save_user(user)
        enqueue_user_changed(storage, "add", user.student_id)
        flush_telemetry_queue(storage)
        return user.to_dict()

    def delete_user(self, student_id: str) -> dict[str, Any]:
        storage = self._storage()
        deleted = storage.delete_user(student_id)
        if not deleted:
            raise KeyError(f"用户不存在: {student_id}")
        enqueue_user_changed(storage, "delete", student_id)
        flush_telemetry_queue(storage)
        return {"deleted": True, "student_id": str(student_id)}

    def toggle_background(self) -> dict[str, Any]:
        storage = self._storage()
        status = get_scheduler_status(storage)
        if status.running:
            enqueue_stop_requested(storage)
            flush_telemetry_queue(storage)
            result = stop_scheduler_process(storage)
            return {"action": "stopped", "pid": result.pid, "message": result.message, "stopped": result.stopped}

        clear_today_errors(storage)
        enqueue_run_requested(storage, once=False)
        flush_telemetry_queue(storage)
        result = start_scheduler_process(storage)
        return {
            "action": "already_running" if result.already_running else "started",
            "pid": result.pid,
            "message": f"后台定时检测已在运行 PID={result.pid}"
            if result.already_running
            else f"后台定时检测已启动 PID={result.pid}",
        }

    def run_once(self) -> dict[str, Any]:
        storage = self._storage()
        enqueue_run_requested(storage, once=True)
        logger = setup_logging(storage)
        result = run_all_users(storage, logger=logger)
        flush_telemetry_queue(storage)
        return {
            "total": result.total,
            "success": result.success_count,
            "failure": result.failure_count,
            "results": [
                {
                    "index": item.index,
                    "student_id": item.student_id,
                    "success": item.success,
                    "message": item.message,
                }
                for item in result.results
            ],
        }

    def _storage(self) -> AppStorage:
        storage = self.storage_factory()
        if not storage.home.exists() or not storage.config_path.exists() or not storage.state_path.exists():
            storage.initialize()
        return storage


def serve_ui(host: str = "127.0.0.1", port: int = 8765, open_browser: bool = True) -> int:
    service = UiService()
    handler = _make_handler(service)
    server = ThreadingHTTPServer((host, int(port)), handler)
    url = f"http://{host}:{server.server_address[1]}"
    print(f"autoTD UI running at {url}")
    if open_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nautoTD UI stopped")
    finally:
        server.server_close()
    return 0


def _today_payload(user_state: Optional[dict[str, Any]], rounds: int) -> dict[str, Any]:
    if not user_state:
        return {
            "status": "pending",
            "completed_rounds": 0,
            "rounds": rounds,
            "next_action": "entrance",
            "due_at": None,
            "message": "今日尚未开始",
        }
    return {
        "status": str(user_state.get("status", "pending")),
        "completed_rounds": int(user_state.get("completed_rounds", 0)),
        "rounds": rounds,
        "next_action": user_state.get("next_action") or "-",
        "due_at": user_state.get("due_at"),
        "message": str(user_state.get("last_error") or user_state.get("last_message") or ""),
    }


def _make_handler(service: UiService) -> type[BaseHTTPRequestHandler]:
    class UiRequestHandler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:
            parsed = urlparse(self.path)
            try:
                if parsed.path == "/":
                    self._send_static("index.html", "text/html; charset=utf-8")
                elif parsed.path == "/static/styles.css":
                    self._send_static("styles.css", "text/css; charset=utf-8")
                elif parsed.path == "/static/app.js":
                    self._send_static("app.js", "application/javascript; charset=utf-8")
                elif parsed.path == "/api/state":
                    self._send_json(service.get_state())
                elif parsed.path == "/api/images":
                    self._send_json({"images": service.list_images()})
                elif parsed.path.startswith("/images/"):
                    self._send_image(unquote(parsed.path.removeprefix("/images/")))
                else:
                    self._send_error(HTTPStatus.NOT_FOUND, "资源不存在")
            except Exception as exc:
                self._send_exception(exc)

        def do_POST(self) -> None:
            parsed = urlparse(self.path)
            try:
                if parsed.path == "/api/users":
                    self._send_json({"user": service.add_user(self._read_json())}, status=HTTPStatus.CREATED)
                elif parsed.path == "/api/images":
                    upload = self._read_multipart_upload()
                    name = service.save_uploaded_image(upload.filename, upload.content, overwrite=upload.overwrite)
                    self._send_json({"image": name}, status=HTTPStatus.CREATED)
                elif parsed.path == "/api/background/toggle":
                    self._send_json(service.toggle_background())
                elif parsed.path == "/api/run-once":
                    self._send_json(service.run_once())
                else:
                    self._send_error(HTTPStatus.NOT_FOUND, "资源不存在")
            except Exception as exc:
                self._send_exception(exc)

        def do_DELETE(self) -> None:
            parsed = urlparse(self.path)
            try:
                if parsed.path.startswith("/api/users/"):
                    self._send_json(service.delete_user(unquote(parsed.path.removeprefix("/api/users/"))))
                elif parsed.path.startswith("/api/images/"):
                    self._send_json(service.delete_image(unquote(parsed.path.removeprefix("/api/images/"))))
                else:
                    self._send_error(HTTPStatus.NOT_FOUND, "资源不存在")
            except Exception as exc:
                self._send_exception(exc)

        def log_message(self, _format: str, *_args: Any) -> None:
            return

        def _read_json(self) -> dict[str, Any]:
            length = int(self.headers.get("Content-Length", "0") or "0")
            if length <= 0:
                return {}
            raw = self.rfile.read(length).decode("utf-8")
            return json.loads(raw or "{}")

        def _read_multipart_upload(self) -> UploadedFile:
            content_type = self.headers.get("Content-Type", "")
            length = int(self.headers.get("Content-Length", "0") or "0")
            if not content_type.startswith("multipart/form-data") or length <= 0:
                raise ValueError("请上传 multipart/form-data 图片")
            raw = self.rfile.read(length)
            message = BytesParser(policy=policy.default).parsebytes(
                f"Content-Type: {content_type}\r\nMIME-Version: 1.0\r\n\r\n".encode("utf-8") + raw
            )
            filename = ""
            content = b""
            overwrite = False
            for part in message.iter_parts():
                name = part.get_param("name", header="content-disposition")
                if name == "image":
                    filename = part.get_filename() or "image"
                    content = part.get_payload(decode=True) or b""
                elif name == "overwrite":
                    overwrite = (part.get_content() or "").strip().lower() in ("1", "true", "yes", "on")
            if not filename:
                raise ValueError("缺少图片文件")
            return UploadedFile(filename=filename, content=content, overwrite=overwrite)

        def _send_static(self, name: str, content_type: str) -> None:
            data = resources.files("auto_td").joinpath("ui_static", name).read_bytes()
            self._send_bytes(data, content_type=content_type)

        def _send_image(self, name: str) -> None:
            image_name = Path(name).name
            if image_name != name:
                self._send_error(HTTPStatus.BAD_REQUEST, "图片名称不能包含目录")
                return
            path = service._storage().image_path(image_name)
            if not path.exists():
                self._send_error(HTTPStatus.NOT_FOUND, "图片不存在")
                return
            content_type = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
            self._send_bytes(path.read_bytes(), content_type=content_type)

        def _send_json(self, payload: dict[str, Any], status: HTTPStatus = HTTPStatus.OK) -> None:
            body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
            self._send_bytes(body, status=status, content_type="application/json; charset=utf-8")

        def _send_error(self, status: HTTPStatus, message: str) -> None:
            self._send_json({"error": message}, status=status)

        def _send_exception(self, exc: Exception) -> None:
            if isinstance(exc, KeyError):
                self._send_error(HTTPStatus.NOT_FOUND, str(exc).strip("'"))
            elif isinstance(exc, (FileExistsError, FileNotFoundError, ValueError)):
                self._send_error(HTTPStatus.BAD_REQUEST, str(exc))
            else:
                self._send_error(HTTPStatus.INTERNAL_SERVER_ERROR, str(exc))

        def _send_bytes(
            self,
            data: bytes,
            status: HTTPStatus = HTTPStatus.OK,
            content_type: str = "application/octet-stream",
        ) -> None:
            self.send_response(int(status))
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

    return UiRequestHandler
