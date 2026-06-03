from .models import User
from .storage import AppStorage


TD_COMPLETION_LIMIT = 32


def has_reached_td_limit(storage: AppStorage, user: User) -> bool:
    count = storage.get_last_count(user.student_id)
    return count is not None and count >= TD_COMPLETION_LIMIT


def td_limit_message(user: User) -> str:
    return f"用户 {user.student_id} TD 次数已达 {TD_COMPLETION_LIMIT}，跳过后续打卡"
