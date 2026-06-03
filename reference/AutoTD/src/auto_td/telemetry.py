from __future__ import annotations

import hashlib
import hmac
import json
import platform
import secrets
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any, Optional, Protocol
from urllib import request
from urllib.error import HTTPError, URLError

from . import __version__
from .constants import BEIJING_TZ


DEFAULT_TELEMETRY_ENDPOINT = "https://autotd-telemetry.autotd-buaa.workers.dev"
DEFAULT_TIMEOUT_SECONDS = 2.0


class TelemetryTransport(Protocol):
    def post_json(
        self,
        url: str,
        payload: dict[str, Any],
        headers: Optional[dict[str, str]] = None,
        timeout: Optional[float] = None,
    ) -> dict[str, Any]:
        ...


class UrlJsonTransport:
    def post_json(
        self,
        url: str,
        payload: dict[str, Any],
        headers: Optional[dict[str, str]] = None,
        timeout: Optional[float] = None,
    ) -> dict[str, Any]:
        body = _json_body(payload)
        req = request.Request(
            url,
            data=body,
            method="POST",
            headers={
                "Content-Type": "application/json",
                "User-Agent": f"autotd/{__version__}",
                **(headers or {}),
            },
        )
        try:
            with request.urlopen(req, timeout=timeout or DEFAULT_TIMEOUT_SECONDS) as response:
                raw = response.read().decode("utf-8")
        except HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"telemetry HTTP {exc.code}: {detail}") from exc
        except URLError as exc:
            raise RuntimeError(f"telemetry network error: {exc.reason}") from exc
        return json.loads(raw or "{}")


def ensure_telemetry_state(storage, endpoint: Optional[str] = None) -> dict[str, Any]:
    state = storage.load_state()
    telemetry = state.setdefault("telemetry", {})
    changed = False
    if "enabled" not in telemetry:
        telemetry["enabled"] = True
        changed = True
    if "installation_id" not in telemetry:
        telemetry["installation_id"] = str(uuid.uuid4())
        changed = True
    if "installation_secret" not in telemetry:
        telemetry["installation_secret"] = secrets.token_hex(32)
        changed = True
    if "endpoint" not in telemetry or (endpoint is None and telemetry.get("endpoint") == "" and DEFAULT_TELEMETRY_ENDPOINT):
        telemetry["endpoint"] = endpoint if endpoint is not None else DEFAULT_TELEMETRY_ENDPOINT
        changed = True
    elif endpoint is not None:
        telemetry["endpoint"] = endpoint
        changed = True
    telemetry.setdefault("registered", False)
    telemetry.setdefault("last_sent_at", None)
    telemetry.setdefault("last_error", None)
    telemetry.setdefault("last_midnight_snapshot_date", None)
    if changed:
        storage.save_state(state)
    return telemetry


def enable_telemetry(storage, endpoint: Optional[str] = None) -> dict[str, Any]:
    state = storage.load_state()
    telemetry = state.setdefault("telemetry", {})
    if endpoint is not None:
        telemetry["endpoint"] = endpoint
    telemetry.setdefault("installation_id", str(uuid.uuid4()))
    telemetry.setdefault("installation_secret", secrets.token_hex(32))
    telemetry["enabled"] = True
    telemetry.setdefault("registered", False)
    telemetry["last_error"] = None
    telemetry.setdefault("last_sent_at", None)
    telemetry.setdefault("last_midnight_snapshot_date", None)
    storage.save_state(state)
    return telemetry


def disable_telemetry(storage) -> None:
    state = storage.load_state()
    telemetry = state.setdefault("telemetry", {})
    telemetry.setdefault("installation_id", str(uuid.uuid4()))
    telemetry.setdefault("installation_secret", secrets.token_hex(32))
    telemetry["enabled"] = False
    telemetry["last_error"] = None
    storage.save_state(state)
    queue_path = _queue_path(storage)
    if queue_path.exists():
        queue_path.unlink()


def get_telemetry_status(storage) -> dict[str, Any]:
    telemetry = ensure_telemetry_state(storage)
    return {
        "enabled": bool(telemetry.get("enabled", True)),
        "endpoint": telemetry.get("endpoint") or "",
        "installation_id": telemetry.get("installation_id") or "",
        "registered": bool(telemetry.get("registered", False)),
        "queue_length": queue_length(storage),
        "last_sent_at": telemetry.get("last_sent_at"),
        "last_error": telemetry.get("last_error"),
    }


def queue_length(storage) -> int:
    path = _queue_path(storage)
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8").splitlines() if line.strip())


def enqueue_install_initialized(storage, now: Optional[datetime] = None) -> Optional[dict[str, Any]]:
    return enqueue_snapshot_event(storage, "install_initialized", now=now)


def enqueue_run_requested(storage, once: bool, now: Optional[datetime] = None) -> Optional[dict[str, Any]]:
    return enqueue_event(storage, "run_requested", {"once": bool(once), **_snapshot_payload(storage)}, now=now)


def enqueue_daemon_started(storage, pid: int, now: Optional[datetime] = None) -> Optional[dict[str, Any]]:
    return enqueue_event(storage, "daemon_started", {"pid": int(pid), **_snapshot_payload(storage)}, now=now)


def enqueue_daemon_stopped(storage, pid: int, now: Optional[datetime] = None) -> Optional[dict[str, Any]]:
    return enqueue_event(storage, "daemon_stopped", {"pid": int(pid), **_snapshot_payload(storage)}, now=now)


def enqueue_stop_requested(storage, now: Optional[datetime] = None) -> Optional[dict[str, Any]]:
    return enqueue_event(storage, "stop_requested", _snapshot_payload(storage), now=now)


def enqueue_user_changed(
    storage,
    change_type: str,
    affected_student_id: str,
    now: Optional[datetime] = None,
) -> Optional[dict[str, Any]]:
    payload = {
        "change_type": change_type,
        "affected_student_id": str(affected_student_id),
        **_snapshot_payload(storage),
    }
    return enqueue_event(storage, "user_changed", payload, now=now)


def enqueue_snapshot_event(
    storage,
    event_type: str,
    now: Optional[datetime] = None,
) -> Optional[dict[str, Any]]:
    return enqueue_event(storage, event_type, _snapshot_payload(storage), now=now)


def enqueue_td_count_changed(
    storage,
    student_id: str,
    previous_count: Optional[int],
    new_count: int,
    now: Optional[datetime] = None,
    count_source: str = "observation",
) -> Optional[dict[str, Any]]:
    previous = int(previous_count) if previous_count is not None else None
    new = int(new_count)
    initial = previous is None
    decreased = previous is not None and new < previous
    source = str(count_source or "observation")
    delta = 0
    inferred_initial_delta = False
    if source == "td_exit" and not decreased:
        if previous is None:
            if new > 0:
                delta = 1
                inferred_initial_delta = True
        else:
            delta = max(0, new - previous)
    payload = {
        "student_id": str(student_id),
        "previous_count": previous,
        "new_count": new,
        "delta": delta,
        "initial_observation": initial,
        "inferred_initial_delta": inferred_initial_delta,
        "count_decreased": decreased,
        "count_source": source,
        **_snapshot_payload(storage),
    }
    return enqueue_event(storage, "td_count_changed", payload, now=now)


def enqueue_daily_midnight_snapshot(storage, now: Optional[datetime] = None) -> Optional[dict[str, Any]]:
    current = _local_now(now)
    day_key = current.date().isoformat()
    state = storage.load_state()
    telemetry = state.setdefault("telemetry", {})
    if telemetry.get("last_midnight_snapshot_date") == day_key:
        return None
    storage.save_state(state)
    event = enqueue_snapshot_event(storage, "daily_midnight_snapshot", now=current)
    state = storage.load_state()
    state.setdefault("telemetry", {})["last_midnight_snapshot_date"] = day_key
    storage.save_state(state)
    return event


def enqueue_event(
    storage,
    event_type: str,
    payload: dict[str, Any],
    now: Optional[datetime] = None,
) -> Optional[dict[str, Any]]:
    telemetry = ensure_telemetry_state(storage)
    if not telemetry.get("enabled", True):
        return None
    current = _local_now(now)
    event = {
        "event_id": str(uuid.uuid4()),
        "event_type": event_type,
        "installation_id": telemetry["installation_id"],
        "app_version": __version__,
        "platform": platform.platform(),
        "occurred_at": current.isoformat(timespec="seconds"),
        "event_day": current.date().isoformat(),
        "payload": payload,
    }
    _append_queue(storage, event)
    return event


def flush_telemetry_queue(
    storage,
    transport: Optional[TelemetryTransport] = None,
    timeout: float = DEFAULT_TIMEOUT_SECONDS,
) -> int:
    telemetry = ensure_telemetry_state(storage)
    if not telemetry.get("enabled", True):
        return 0
    events = _read_queue(storage)
    if not events:
        return 0
    endpoint = str(telemetry.get("endpoint") or "").rstrip("/")
    if not endpoint:
        _record_last_error(storage, "telemetry endpoint not configured")
        return 0
    transport = transport or UrlJsonTransport()

    try:
        if not telemetry.get("registered", False):
            telemetry = _register_installation(storage, transport, endpoint, telemetry, timeout)
        sent = 0
        remaining = []
        for event in events:
            body = {"event": event}
            try:
                _post_event(storage, transport, endpoint, telemetry, body, timeout)
                sent += 1
            except Exception as exc:
                if _is_unknown_installation_error(exc):
                    try:
                        _mark_unregistered(storage)
                        telemetry = ensure_telemetry_state(storage)
                        telemetry = _register_installation(storage, transport, endpoint, telemetry, timeout)
                        _post_event(storage, transport, endpoint, telemetry, body, timeout)
                        sent += 1
                        continue
                    except Exception as retry_exc:
                        remaining.append(event)
                        _record_last_error(storage, str(retry_exc))
                else:
                    remaining.append(event)
                    _record_last_error(storage, str(exc))
        _write_queue(storage, remaining)
        if sent:
            _record_sent(storage)
        return sent
    except Exception as exc:
        _record_last_error(storage, str(exc))
        return 0


def _registration_payload(storage, telemetry: dict[str, Any]) -> dict[str, Any]:
    return {
        "installation_id": telemetry["installation_id"],
        "installation_secret": telemetry["installation_secret"],
        "app_version": __version__,
        "platform": platform.platform(),
        "registered_at": _local_now(None).isoformat(timespec="seconds"),
        **_snapshot_payload(storage),
    }


def _register_installation(
    storage,
    transport: TelemetryTransport,
    endpoint: str,
    telemetry: dict[str, Any],
    timeout: float,
) -> dict[str, Any]:
    transport.post_json(
        _url(endpoint, "/v1/installations/register"),
        _registration_payload(storage, telemetry),
        timeout=timeout,
    )
    _mark_registered(storage)
    return ensure_telemetry_state(storage)


def _post_event(
    storage,
    transport: TelemetryTransport,
    endpoint: str,
    telemetry: dict[str, Any],
    body: dict[str, Any],
    timeout: float,
) -> None:
    del storage
    signature = _signature(telemetry["installation_secret"], body)
    transport.post_json(
        _url(endpoint, "/v1/events"),
        body,
        headers={"X-AutoTD-Installation": telemetry["installation_id"], "X-AutoTD-Signature": signature},
        timeout=timeout,
    )


def _snapshot_payload(storage) -> dict[str, Any]:
    users = []
    counts = storage.load_state().get("counts", {})
    for user in sorted(storage.list_users(), key=lambda item: item.student_id):
        count_info = counts.get(user.student_id) or {}
        users.append(
            {
                "student_id": user.student_id,
                "td_count": count_info.get("count"),
            }
        )
    return {
        "current_user_count": len(users),
        "users": users,
    }


def _queue_path(storage) -> Path:
    return getattr(storage, "telemetry_queue_path", storage.home / "telemetry_queue.jsonl")


def _append_queue(storage, event: dict[str, Any]) -> None:
    path = _queue_path(storage)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(event, ensure_ascii=False, sort_keys=True, separators=(",", ":")))
        f.write("\n")


def _read_queue(storage) -> list[dict[str, Any]]:
    path = _queue_path(storage)
    if not path.exists():
        return []
    events = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.strip():
            events.append(json.loads(line))
    return events


def _write_queue(storage, events: list[dict[str, Any]]) -> None:
    path = _queue_path(storage)
    if not events:
        if path.exists():
            path.unlink()
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        for event in events:
            f.write(json.dumps(event, ensure_ascii=False, sort_keys=True, separators=(",", ":")))
            f.write("\n")


def _mark_registered(storage) -> None:
    state = storage.load_state()
    state.setdefault("telemetry", {})["registered"] = True
    storage.save_state(state)


def _mark_unregistered(storage) -> None:
    state = storage.load_state()
    state.setdefault("telemetry", {})["registered"] = False
    storage.save_state(state)


def _is_unknown_installation_error(exc: Exception) -> bool:
    return "unknown_installation" in str(exc)


def _record_sent(storage) -> None:
    state = storage.load_state()
    telemetry = state.setdefault("telemetry", {})
    telemetry["last_sent_at"] = _local_now(None).isoformat(timespec="seconds")
    telemetry["last_error"] = None
    storage.save_state(state)


def _record_last_error(storage, message: str) -> None:
    state = storage.load_state()
    telemetry = state.setdefault("telemetry", {})
    telemetry["last_error"] = message
    storage.save_state(state)


def _url(endpoint: str, path: str) -> str:
    return endpoint.rstrip("/") + path


def _signature(secret: str, payload: dict[str, Any]) -> str:
    return hmac.new(secret.encode("utf-8"), _json_body(payload), hashlib.sha256).hexdigest()


def _json_body(payload: dict[str, Any]) -> bytes:
    return json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def _local_now(value: Optional[datetime]) -> datetime:
    if value is None:
        return datetime.now(BEIJING_TZ)
    if value.tzinfo is None:
        return value.replace(tzinfo=BEIJING_TZ)
    return value.astimezone(BEIJING_TZ)
