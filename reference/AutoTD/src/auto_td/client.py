import json
import re
import socket
import struct
import time
from datetime import datetime, timezone
from typing import Any, Callable, Dict, Optional

from .constants import BEIJING_TZ, DEFAULT_SOCKET_TIMEOUT, DEFAULT_WINDOWS
from .models import CheckResponse, User
from .scheduler import in_any_window


COUNT_RE = re.compile(r"本学期锻炼次数\s*[:：]\s*(\d+)")


class TDClient:
    def __init__(
        self,
        config: Dict[str, Any],
        socket_factory: Callable[..., socket.socket] = socket.socket,
    ):
        self.config = config
        self.server = config["server"]["ip"]
        self.port = int(config["server"]["port"])
        self.timeout = float(config.get("server", {}).get("timeout", DEFAULT_SOCKET_TIMEOUT))
        self.type = config["type"]
        self.schoolno = config["schoolno"]
        self.eventno = config["eventno"]
        self.machine = {int(machine["id"]): machine for machine in config["machine"]}
        self.socket_factory = socket_factory

    def request(self, data: bytes, request_type: int) -> Dict[str, Any]:
        header = struct.pack(">lB", len(data), request_type)
        with self.socket_factory(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(self.timeout)
            try:
                sock.connect((self.server, self.port))
            except (ConnectionRefusedError, TimeoutError, OSError) as exc:
                raise ConnectionError(self._connection_error_message(exc)) from exc
            sock.sendall(header + data)
            response_header = _recv_exact(sock, 5, "header")
            length, code = struct.unpack(">lB", response_header)
            if code != request_type:
                raise ValueError(f"Invalid response code: expected {request_type}, got {code}")
            if length <= 0:
                raise ValueError("Empty response")
            response = _recv_exact(sock, length, "body")
            return json.loads(response.decode("utf-8"))

    def check(
        self,
        user: User,
        machine_id: int,
        timestamp: Optional[float] = None,
        enforce_time_window: bool = True,
    ) -> CheckResponse:
        if timestamp is None:
            timestamp = time.time()
        if enforce_time_window and not _timestamp_in_allowed_windows(timestamp):
            dt_bj = datetime.fromtimestamp(timestamp, tz=timezone.utc).astimezone(BEIJING_TZ)
            return CheckResponse(
                False,
                f"非法时间，已取消。当前时间：{dt_bj.strftime('%Y-%m-%d %H:%M:%S')}",
                None,
            )

        payload = self.build_check_data(user, machine_id, timestamp)
        resp = self.request(json.dumps(payload).encode("utf-8"), 80)
        if resp["status"] != "success":
            raise ValueError(f"打卡请求失败: {resp['status']}")
        server_message = clean_server_message(resp.get("srvresp") or "")
        return CheckResponse(
            success="成功" in (resp.get("srvresp") or ""),
            server_message=server_message,
            count=extract_exercise_count(server_message),
        )

    def upload_photo(self, machine_id: int, photo: bytes, timestamp: Optional[float] = None) -> Dict[str, Any]:
        if timestamp is None:
            timestamp = time.time()
        machine = self.machine[machine_id]
        ts_ms = str(int(timestamp * 1000))
        photo_data = f"{machine['machinesn']}_{ts_ms}".encode("utf-8") + photo
        resp = self.request(photo_data, 100)
        if resp["status"] != "success":
            raise ValueError(f"照片上传失败: {resp['status']}")
        return resp

    def query_count(
        self,
        user: User,
        machine_id: Optional[int] = None,
        timestamp: Optional[float] = None,
    ) -> int:
        response = self.check(
            user,
            machine_id or user.entrance_machine_id,
            timestamp=timestamp,
            enforce_time_window=False,
        )
        if response.count is None:
            raise ValueError(f"服务器消息中没有锻炼次数: {response.server_message}")
        return response.count

    def build_check_data(self, user: User, machine_id: int, timestamp: float) -> Dict[str, Any]:
        machine = self.machine[machine_id]
        return {
            "cardno": user.card_id.upper(),
            "userno": user.student_id.upper(),
            "timestamp": str(int(timestamp * 1000)),
            "type": self.type,
            "eventno": self.eventno,
            "ln": str(machine["id"]),
            "sn": machine["machinesn"],
            "schoolno": self.schoolno,
        }

    def _connection_error_message(self, exc: OSError) -> str:
        endpoint = f"{self.server}:{self.port}"
        return (
            f"无法连接 TD 服务器 {endpoint}: {exc}. "
            "请确认已连接校园网或 VPN，并检查 ~/.autoTD/config.json 中的 server.ip/server.port。"
            "如果地址仍是 127.0.0.1，请重新导入或填写真实 TD 服务器配置。"
        )


def extract_exercise_count(message: str) -> Optional[int]:
    match = COUNT_RE.search(message)
    if not match:
        return None
    return int(match.group(1))


def clean_server_message(message: str) -> str:
    return message.strip().replace("\n \n", "\n").replace("\n", ", ")


def _recv_exact(sock: socket.socket, size: int, label: str) -> bytes:
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError(f"socket closed while reading {label}")
        data += chunk
    return data


def _timestamp_in_allowed_windows(timestamp: float) -> bool:
    dt_bj = datetime.fromtimestamp(timestamp, tz=timezone.utc).astimezone(BEIJING_TZ)
    return in_any_window(dt_bj, DEFAULT_WINDOWS)
