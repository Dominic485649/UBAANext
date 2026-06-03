import os
from pathlib import Path

from .constants import APP_DIR_NAME


def get_app_home() -> Path:
    override = os.environ.get("AUTOTD_HOME")
    if override:
        return Path(override).expanduser().resolve()
    return (Path.home() / APP_DIR_NAME).resolve()
