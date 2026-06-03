import contextlib
import hmac
import io
import json
import os
import sys
import tempfile
import unittest
from datetime import datetime
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from auto_td import cli
from auto_td.background import BackgroundStartResult, BackgroundStopResult
from auto_td.constants import BEIJING_TZ
from auto_td.models import User
from auto_td.storage import AppStorage
from auto_td.telemetry import (
    disable_telemetry,
    enable_telemetry,
    enqueue_user_changed,
    flush_telemetry_queue,
    get_telemetry_status,
    queue_length,
)


class FakeTelemetryTransport:
    def __init__(self, fail=False):
        self.fail = fail
        self.calls = []

    def post_json(self, url, payload, headers=None, timeout=None):
        if self.fail:
            raise OSError("offline")
        self.calls.append(
            {
                "url": url,
                "payload": payload,
                "headers": dict(headers or {}),
                "timeout": timeout,
            }
        )
        return {"ok": True}


class UnknownInstallationOnceTransport:
    def __init__(self):
        self.calls = []
        self.rejected = False

    def post_json(self, url, payload, headers=None, timeout=None):
        self.calls.append(
            {
                "url": url,
                "payload": payload,
                "headers": dict(headers or {}),
                "timeout": timeout,
            }
        )
        if url.endswith("/v1/events") and not self.rejected:
            self.rejected = True
            raise RuntimeError('telemetry HTTP 401: {"error":"unknown_installation"}')
        return {"ok": True}


class TelemetryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.home = Path(self.tmp.name) / "home"
        self.old_env = os.environ.get("AUTOTD_HOME")
        os.environ["AUTOTD_HOME"] = str(self.home)
        self.storage = AppStorage()
        self.storage.initialize()
        (self.home / "images").mkdir(exist_ok=True)
        (self.home / "images" / "in.jpg").write_bytes(b"in")
        (self.home / "images" / "out.jpg").write_bytes(b"out")

    def tearDown(self):
        if self.old_env is None:
            os.environ.pop("AUTOTD_HOME", None)
        else:
            os.environ["AUTOTD_HOME"] = self.old_env
        self.tmp.cleanup()

    def add_user(self, student_id="1001"):
        user = User(
            student_id=student_id,
            card_id="CARD-SECRET",
            entrance_machine_id=2,
            exit_machine_id=6,
            entrance_image="in.jpg",
            exit_image="out.jpg",
            rounds=1,
            wait_time_min=0,
            wait_time_max=0,
        )
        self.storage.save_user(user)
        return user

    def read_queue(self):
        if not self.storage.telemetry_queue_path.exists():
            return []
        return [
            json.loads(line)
            for line in self.storage.telemetry_queue_path.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]

    def test_init_defaults_telemetry_enabled_and_queues_install_event(self):
        state = self.storage.load_state()

        self.assertTrue(state["telemetry"]["enabled"])
        self.assertTrue(state["telemetry"]["installation_id"])
        self.assertTrue(state["telemetry"]["installation_secret"])
        self.assertEqual(state["telemetry"]["endpoint"], "https://autotd-telemetry.autotd-buaa.workers.dev")
        self.assertEqual(queue_length(self.storage), 0)

        with mock.patch("auto_td.cli.flush_telemetry_queue") as flush:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["init"]), 0)

        queued = self.read_queue()
        self.assertEqual(queued[-1]["event_type"], "install_initialized")
        self.assertTrue(flush.called)
        self.assertIn("initialized", output.getvalue())

    def test_blank_legacy_endpoint_is_migrated_to_default_endpoint(self):
        state = self.storage.load_state()
        state["telemetry"]["endpoint"] = ""
        self.storage.save_state(state)

        status = get_telemetry_status(self.storage)

        self.assertEqual(status["endpoint"], "https://autotd-telemetry.autotd-buaa.workers.dev")

    def test_disable_stops_enqueueing_and_clears_existing_queue(self):
        self.add_user()
        enqueue_user_changed(self.storage, "add", "1001", now=datetime(2026, 5, 5, 8, 0, tzinfo=BEIJING_TZ))
        self.assertEqual(queue_length(self.storage), 1)

        disable_telemetry(self.storage)
        enqueue_user_changed(self.storage, "update", "1001", now=datetime(2026, 5, 5, 8, 1, tzinfo=BEIJING_TZ))

        status = get_telemetry_status(self.storage)
        self.assertFalse(status["enabled"])
        self.assertEqual(queue_length(self.storage), 0)

    def test_user_changed_event_contains_students_and_omits_private_config(self):
        self.add_user("1001")

        enqueue_user_changed(self.storage, "add", "1001", now=datetime(2026, 5, 5, 8, 0, tzinfo=BEIJING_TZ))

        event = self.read_queue()[0]
        text = json.dumps(event, ensure_ascii=False)
        self.assertEqual(event["event_type"], "user_changed")
        self.assertEqual(event["payload"]["change_type"], "add")
        self.assertEqual(event["payload"]["affected_student_id"], "1001")
        self.assertEqual(event["payload"]["current_user_count"], 1)
        self.assertEqual(event["payload"]["users"][0]["student_id"], "1001")
        self.assertNotIn("CARD-SECRET", text)
        self.assertNotIn("entrance_machine", text)
        self.assertNotIn("in.jpg", text)

    def test_observed_count_changes_do_not_create_delta_events(self):
        self.add_user("1001")

        self.storage.set_last_count("1001", 5, when=datetime(2026, 5, 5, 8, 0, tzinfo=BEIJING_TZ))
        self.storage.set_last_count("1001", 8, when=datetime(2026, 5, 5, 8, 1, tzinfo=BEIJING_TZ))
        self.storage.set_last_count("1001", 7, when=datetime(2026, 5, 5, 8, 2, tzinfo=BEIJING_TZ))

        events = self.read_queue()
        self.assertEqual([event["payload"]["delta"] for event in events], [0, 0, 0])
        self.assertTrue(events[0]["payload"]["initial_observation"])
        self.assertEqual([event["payload"]["count_source"] for event in events], ["observation", "observation", "observation"])
        self.assertTrue(events[2]["payload"]["count_decreased"])

    def test_exit_count_changes_create_delta_events(self):
        self.add_user("1001")

        self.storage.set_last_count(
            "1001",
            5,
            when=datetime(2026, 5, 5, 8, 0, tzinfo=BEIJING_TZ),
            count_source="td_entrance",
        )
        self.storage.set_last_count(
            "1001",
            6,
            when=datetime(2026, 5, 5, 8, 1, tzinfo=BEIJING_TZ),
            count_source="td_exit",
        )
        self.storage.set_last_count(
            "1001",
            7,
            when=datetime(2026, 5, 5, 8, 2, tzinfo=BEIJING_TZ),
            count_source="td_exit",
        )

        events = self.read_queue()
        self.assertEqual([event["payload"]["delta"] for event in events], [0, 1, 1])
        self.assertEqual([event["payload"]["count_source"] for event in events], ["td_entrance", "td_exit", "td_exit"])

    def test_flush_registers_installation_then_posts_signed_events(self):
        self.add_user("1001")
        enable_telemetry(self.storage, endpoint="https://example.test")
        enqueue_user_changed(self.storage, "add", "1001", now=datetime(2026, 5, 5, 8, 0, tzinfo=BEIJING_TZ))
        transport = FakeTelemetryTransport()

        sent = flush_telemetry_queue(self.storage, transport=transport)

        self.assertEqual(sent, 1)
        self.assertEqual(queue_length(self.storage), 0)
        self.assertEqual(transport.calls[0]["url"], "https://example.test/v1/installations/register")
        self.assertEqual(transport.calls[1]["url"], "https://example.test/v1/events")
        raw_event = json.dumps(transport.calls[1]["payload"], ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode(
            "utf-8"
        )
        secret = self.storage.load_state()["telemetry"]["installation_secret"].encode("utf-8")
        expected = hmac.new(secret, raw_event, "sha256").hexdigest()
        self.assertEqual(transport.calls[1]["headers"]["X-AutoTD-Signature"], expected)

    def test_flush_failure_preserves_queue_and_records_error(self):
        self.add_user("1001")
        enable_telemetry(self.storage, endpoint="https://example.test")
        enqueue_user_changed(self.storage, "add", "1001", now=datetime(2026, 5, 5, 8, 0, tzinfo=BEIJING_TZ))

        sent = flush_telemetry_queue(self.storage, transport=FakeTelemetryTransport(fail=True))

        status = get_telemetry_status(self.storage)
        self.assertEqual(sent, 0)
        self.assertEqual(queue_length(self.storage), 1)
        self.assertIn("offline", status["last_error"])

    def test_flush_reregisters_when_server_lost_installation(self):
        self.add_user("1001")
        enable_telemetry(self.storage, endpoint="https://example.test")
        state = self.storage.load_state()
        state["telemetry"]["registered"] = True
        self.storage.save_state(state)
        enqueue_user_changed(self.storage, "add", "1001", now=datetime(2026, 5, 5, 8, 0, tzinfo=BEIJING_TZ))
        transport = UnknownInstallationOnceTransport()

        sent = flush_telemetry_queue(self.storage, transport=transport)

        self.assertEqual(sent, 1)
        self.assertEqual(queue_length(self.storage), 0)
        self.assertEqual(
            [call["url"] for call in transport.calls],
            [
                "https://example.test/v1/events",
                "https://example.test/v1/installations/register",
                "https://example.test/v1/events",
            ],
        )

    def test_telemetry_cli_status_enable_disable_and_sync(self):
        with mock.patch("auto_td.cli.flush_telemetry_queue", return_value=0):
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(cli.main(["telemetry", "enable", "--endpoint", "https://example.test"]), 0)

        status_output = io.StringIO()
        with contextlib.redirect_stdout(status_output):
            self.assertEqual(cli.main(["telemetry", "status"]), 0)
        status = json.loads(status_output.getvalue())
        self.assertTrue(status["enabled"])
        self.assertEqual(status["endpoint"], "https://example.test")

        with mock.patch("auto_td.cli.flush_telemetry_queue", return_value=0):
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(cli.main(["telemetry", "sync"]), 0)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["telemetry", "disable"]), 0)
        self.assertFalse(get_telemetry_status(self.storage)["enabled"])

    def test_run_and_stop_commands_queue_telemetry_events(self):
        with mock.patch("auto_td.cli.flush_telemetry_queue"):
            with mock.patch(
                "auto_td.cli.start_scheduler_process",
                return_value=BackgroundStartResult(4321, self.storage.pid_path, False),
            ):
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(cli.main(["run"]), 0)
            with mock.patch(
                "auto_td.cli.stop_scheduler_process",
                return_value=BackgroundStopResult(4321, True, "stopped"),
            ):
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(cli.main(["--stop"]), 0)

        event_types = [event["event_type"] for event in self.read_queue()]
        self.assertIn("run_requested", event_types)
        self.assertIn("stop_requested", event_types)


if __name__ == "__main__":
    unittest.main()
