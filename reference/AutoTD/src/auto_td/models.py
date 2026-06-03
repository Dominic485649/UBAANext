from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional


@dataclass(frozen=True)
class User:
    student_id: str
    card_id: str
    entrance_machine_id: int
    exit_machine_id: int
    entrance_image: str
    exit_image: str
    rounds: int = 3
    wait_time_min: int = 180
    wait_time_max: int = 240

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "User":
        student_id = str(data.get("student_id", "")).strip()
        if not student_id:
            raise ValueError("student_id 不能为空")
        card_id = str(data.get("card_id", "") or "").strip()
        if not card_id:
            card_id = hex(int(student_id))[2:].upper()
        return cls(
            student_id=student_id,
            card_id=card_id.upper(),
            entrance_machine_id=_read_int(data, "entrance_machine_id"),
            exit_machine_id=_read_int(data, "exit_machine_id"),
            entrance_image=_image_name(data.get("entrance_image") or data.get("entrance_photo_path")),
            exit_image=_image_name(data.get("exit_image") or data.get("exit_photo_path")),
            rounds=_read_int(data, "rounds", 3),
            wait_time_min=_read_int(data, "wait_time_min", 180),
            wait_time_max=_read_int(data, "wait_time_max", 240),
        ).validated()

    def validated(self) -> "User":
        if self.rounds <= 0:
            raise ValueError("rounds 必须大于 0")
        if self.wait_time_min < 0 or self.wait_time_max < 0:
            raise ValueError("wait_time_min 和 wait_time_max 不能为负数")
        if self.wait_time_min > self.wait_time_max:
            raise ValueError("wait_time_min 必须小于等于 wait_time_max")
        if not self.entrance_image or not self.exit_image:
            raise ValueError("入口和出口图片不能为空")
        return self

    def to_dict(self) -> Dict[str, Any]:
        return {
            "student_id": self.student_id,
            "card_id": self.card_id,
            "entrance_machine_id": self.entrance_machine_id,
            "exit_machine_id": self.exit_machine_id,
            "entrance_image": self.entrance_image,
            "exit_image": self.exit_image,
            "rounds": self.rounds,
            "wait_time_min": self.wait_time_min,
            "wait_time_max": self.wait_time_max,
        }


@dataclass(frozen=True)
class CheckResponse:
    success: bool
    server_message: str
    count: Optional[int] = None


@dataclass(frozen=True)
class UserResult:
    index: int
    student_id: str
    success: bool
    message: str


@dataclass(frozen=True)
class BatchResult:
    results: List[UserResult]

    @property
    def total(self) -> int:
        return len(self.results)

    @property
    def success_count(self) -> int:
        return sum(1 for result in self.results if result.success)

    @property
    def failure_count(self) -> int:
        return self.total - self.success_count


def _read_int(data: Dict[str, Any], key: str, default: Optional[int] = None) -> int:
    if key not in data:
        if default is None:
            raise ValueError(f"{key} 不能为空")
        return default
    try:
        return int(data[key])
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{key} 必须是整数") from exc


def _image_name(value: Any) -> str:
    if value is None:
        return ""
    return Path(str(value)).name
