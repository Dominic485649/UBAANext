import logging
import sys
from datetime import date, datetime
from typing import Callable, Optional

from .constants import BEIJING_TZ
from .storage import AppStorage


class DailyFileHandler(logging.Handler):
    def __init__(self, storage: AppStorage, date_provider: Callable[[], date]):
        super().__init__()
        self.storage = storage
        self.date_provider = date_provider
        self.current_date: Optional[date] = None
        self.file_handler: Optional[logging.FileHandler] = None

    def emit(self, record: logging.LogRecord) -> None:
        try:
            self._ensure_handler()
            assert self.file_handler is not None
            self.file_handler.emit(record)
        except Exception:
            self.handleError(record)

    def setFormatter(self, fmt: logging.Formatter | None) -> None:  # noqa: N802
        super().setFormatter(fmt)
        if self.file_handler is not None:
            self.file_handler.setFormatter(fmt)

    def close(self) -> None:
        if self.file_handler is not None:
            self.file_handler.close()
            self.file_handler = None
        super().close()

    def _ensure_handler(self) -> None:
        today = self.date_provider()
        if self.file_handler is not None and self.current_date == today:
            return
        if self.file_handler is not None:
            self.file_handler.close()
        self.current_date = today
        self.storage.logs_dir.mkdir(parents=True, exist_ok=True)
        log_path = self.storage.logs_dir / f"{today.isoformat()}-daytime-log.txt"
        self.file_handler = logging.FileHandler(log_path, encoding="utf-8")
        if self.formatter is not None:
            self.file_handler.setFormatter(self.formatter)


def setup_logging(
    storage: AppStorage,
    now: datetime | None = None,
    stream: bool = True,
    date_provider: Callable[[], date] | None = None,
) -> logging.Logger:
    fixed_now = now
    now = now or datetime.now(BEIJING_TZ)
    storage.logs_dir.mkdir(parents=True, exist_ok=True)
    if date_provider is None:
        if fixed_now is not None:
            date_provider = lambda: now.astimezone(BEIJING_TZ).date()
        else:
            date_provider = lambda: datetime.now().astimezone().date()

    logger = logging.getLogger("auto_td")
    logger.setLevel(logging.INFO)
    logger.handlers.clear()
    formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")

    if stream:
        stream_handler = logging.StreamHandler(sys.stdout)
        stream_handler.setFormatter(formatter)
        logger.addHandler(stream_handler)

    file_handler = DailyFileHandler(storage, date_provider)
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)

    return logger
