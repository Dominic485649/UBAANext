import random
from typing import Any, Dict, Iterable, List

from .models import User
from .storage import AppStorage


CAMPUS_ALIASES = {
    "沙河": "shahe",
    "shahe": "shahe",
    "sh": "shahe",
    "学院路": "xueyuanlu",
    "學院路": "xueyuanlu",
    "本部": "xueyuanlu",
    "xueyuanlu": "xueyuanlu",
    "xyl": "xueyuanlu",
}


def build_quick_user(storage: AppStorage, student_id: str, campus: str, card_id: str = "") -> User:
    normalized = normalize_campus(campus)
    machines = storage.load_config().get("machine", [])
    entrance_pool = _machine_pool(machines, normalized, entrance=True)
    exit_pool = _machine_pool(machines, normalized, entrance=False)
    images = storage.list_images()
    if not entrance_pool:
        raise ValueError(f"未找到 {campus} 的入口机")
    if not exit_pool:
        raise ValueError(f"未找到 {campus} 的出口机")
    if not images:
        raise ValueError("没有可用图片，请先运行 autotd image add")

    return User.from_dict(
        {
            "student_id": student_id,
            "card_id": card_id or "",
            "entrance_machine_id": random.choice(entrance_pool)["id"],
            "exit_machine_id": random.choice(exit_pool)["id"],
            "entrance_image": random.choice(images),
            "exit_image": random.choice(images),
        }
    )


def normalize_campus(campus: str) -> str:
    key = campus.strip().lower()
    if key not in CAMPUS_ALIASES:
        raise ValueError("quick 只支持 沙河 或 学院路")
    return CAMPUS_ALIASES[key]


def _machine_pool(machines: Iterable[Dict[str, Any]], campus: str, *, entrance: bool) -> List[Dict[str, Any]]:
    return [
        machine
        for machine in sorted(machines, key=lambda item: int(item["id"]))
        if _is_campus_machine(machine, campus) and _is_direction_machine(machine, entrance=entrance)
    ]


def _is_campus_machine(machine: Dict[str, Any], campus: str) -> bool:
    location = str(machine.get("location", ""))
    if campus == "shahe":
        return "沙河" in location
    return "学院路" in location or "本部" in location


def _is_direction_machine(machine: Dict[str, Any], *, entrance: bool) -> bool:
    location = str(machine.get("location", ""))
    doortype = str(machine.get("doortype", ""))
    if entrance:
        return doortype == "1" or "入口" in location
    return doortype == "2" or "出口" in location
