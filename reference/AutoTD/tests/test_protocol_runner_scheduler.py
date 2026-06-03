import contextlib
import io
import json
import logging
import os
import sys
import tempfile
import unittest
from datetime import date, datetime, timezone
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from auto_td.client import CheckResponse, TDClient, extract_exercise_count
from auto_td.constants import BEIJING_TZ
from auto_td.logging_utils import setup_logging
from auto_td.models import User
from auto_td.runner import run_all_users
from auto_td.scheduler import run_forever, run_scheduler_once
from auto_td.storage import AppStorage


class FakeClient:
    def __init__(self, outcomes):
        self.outcomes = list(outcomes)
        self.check_calls = []
        self.upload_calls = []

    def check(self, user, machine_id, timestamp=None):
        self.check_calls.append((user.student_id, machine_id))
        outcome = self.outcomes.pop(0)
        if isinstance(outcome, Exception):
            raise outcome
        return outcome

    def upload_photo(self, machine_id, photo, timestamp=None):
        self.upload_calls.append((machine_id, photo))
        return {"status": "success", "name": "uploaded.jpg"}

    def query_count(self, user, machine_id=None, timestamp=None):
        self.check_calls.append((user.student_id, machine_id))
        return 7


class RefusedSocket:
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        return False

    def settimeout(self, _timeout):
        pass

    def connect(self, _address):
        raise ConnectionRefusedError(61, "Connection refused")


class ProtocolRunnerSchedulerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.home = Path(self.tmp.name) / "home"
        self.old_env = os.environ.get("AUTOTD_HOME")
        os.environ["AUTOTD_HOME"] = str(self.home)
        self.storage = AppStorage()
        self.storage.initialize()
        self.storage.save_config(
            {
                "type": 1,
                "schoolno": "10006",
                "eventno": "802",
                "server": {"ip": "127.0.0.1", "port": 8888},
                "machine": [
                    {"id": 2, "machinesn": "IN", "location": "入口"},
                    {"id": 6, "machinesn": "OUT", "location": "出口"},
                ],
            }
        )
        (self.home / "images" / "in.jpg").write_bytes(b"in")
        (self.home / "images" / "out.jpg").write_bytes(b"out")

    def tearDown(self):
        logger = logging.getLogger("auto_td")
        for handler in logger.handlers:
            handler.close()
        logger.handlers.clear()
        logger.addHandler(logging.NullHandler())
        logger.propagate = False
        if self.old_env is None:
            os.environ.pop("AUTOTD_HOME", None)
        else:
            os.environ["AUTOTD_HOME"] = self.old_env
        self.tmp.cleanup()

    def add_user(self, student_id="1001", rounds=1, wait_time_min=0, wait_time_max=0):
        user = User(
            student_id=student_id,
            card_id=hex(int(student_id))[2:].upper(),
            entrance_machine_id=2,
            exit_machine_id=6,
            entrance_image="in.jpg",
            exit_image="out.jpg",
            rounds=rounds,
            wait_time_min=wait_time_min,
            wait_time_max=wait_time_max,
        )
        self.storage.save_user(user)
        return user

    def telemetry_events(self):
        if not self.storage.telemetry_queue_path.exists():
            return []
        return [
            json.loads(line)
            for line in self.storage.telemetry_queue_path.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]

    def test_extract_exercise_count_accepts_chinese_colon_and_ascii_colon(self):
        self.assertEqual(extract_exercise_count("刷卡成功, 本学期锻炼次数:7"), 7)
        self.assertEqual(extract_exercise_count("刷卡成功，本学期锻炼次数：12"), 12)
        self.assertIsNone(extract_exercise_count("刷卡成功"))

    def test_runner_continues_after_user_failure_and_records_count(self):
        self.add_user("1001")
        self.add_user("1002")
        client = FakeClient(
            [
                RuntimeError("boom"),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:7", 7),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:8", 8),
            ]
        )

        with contextlib.redirect_stdout(io.StringIO()):
            result = run_all_users(self.storage, client=client, sleeper=lambda _seconds: None)

        self.assertEqual(result.total, 2)
        self.assertEqual(result.success_count, 1)
        self.assertEqual(result.failure_count, 1)
        self.assertEqual(self.storage.get_last_count("1002"), 8)

    def test_only_successful_exit_checks_count_telemetry_delta(self):
        self.add_user("1001", rounds=2)
        client = FakeClient(
            [
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:1", 1),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:2", 2),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:3", 3),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:4", 4),
            ]
        )

        with contextlib.redirect_stdout(io.StringIO()):
            result = run_all_users(self.storage, client=client, sleeper=lambda _seconds: None)

        self.assertEqual(result.success_count, 1)
        events = [event for event in self.telemetry_events() if event["event_type"] == "td_count_changed"]
        self.assertEqual([event["payload"]["delta"] for event in events], [0, 1, 0, 1])
        self.assertEqual(
            [event["payload"]["count_source"] for event in events],
            ["td_entrance", "td_exit", "td_entrance", "td_exit"],
        )

    def test_runner_skips_user_when_cached_td_count_reaches_limit(self):
        self.add_user("1001", rounds=2)
        self.storage.set_last_count("1001", 32)
        client = FakeClient([])

        with contextlib.redirect_stdout(io.StringIO()):
            result = run_all_users(self.storage, client=client, sleeper=lambda _seconds: None)

        self.assertEqual(result.total, 1)
        self.assertEqual(result.success_count, 1)
        self.assertEqual(client.check_calls, [])
        self.assertIn("TD 次数已达 32", result.results[0].message)
        events = self.telemetry_events()
        self.assertEqual([event["event_type"] for event in events], ["td_count_changed", "td_limit_reached"])
        self.assertEqual(events[-1]["payload"]["users"], [{"student_id": "1001", "td_count": 32}])

    def test_runner_stops_next_round_after_exit_reaches_td_limit(self):
        self.add_user("1001", rounds=2)
        self.storage.set_last_count("1001", 31)
        client = FakeClient(
            [
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:31", 31),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:32", 32),
            ]
        )

        with contextlib.redirect_stdout(io.StringIO()):
            result = run_all_users(self.storage, client=client, sleeper=lambda _seconds: None)

        self.assertEqual(result.success_count, 1)
        self.assertEqual(client.check_calls, [("1001", 2), ("1001", 6)])
        events = self.telemetry_events()
        count_events = [event for event in events if event["event_type"] == "td_count_changed"]
        self.assertEqual([event["payload"]["delta"] for event in count_events], [0, 0, 1])
        self.assertEqual(events[-1]["event_type"], "td_limit_reached")
        self.assertEqual(events[-1]["payload"]["users"], [{"student_id": "1001", "td_count": 32}])

    def test_runner_stops_after_entrance_reports_td_limit(self):
        self.add_user("1001", rounds=1)
        client = FakeClient([CheckResponse(True, "刷卡成功, 本学期锻炼次数:32", 32)])

        with contextlib.redirect_stdout(io.StringIO()):
            result = run_all_users(self.storage, client=client, sleeper=lambda _seconds: None)

        self.assertEqual(result.success_count, 1)
        self.assertEqual(client.check_calls, [("1001", 2)])
        self.assertEqual(client.upload_calls, [(2, b"in")])
        self.assertEqual(self.storage.get_last_count("1001"), 32)
        events = self.telemetry_events()
        self.assertEqual([event["event_type"] for event in events], ["td_count_changed", "td_limit_reached"])
        self.assertEqual(events[0]["payload"]["count_source"], "td_entrance")
        self.assertEqual(events[0]["payload"]["delta"], 0)
        self.assertEqual(events[-1]["payload"]["users"], [{"student_id": "1001", "td_count": 32}])

    def test_query_count_uses_check_request_without_uploading_photo(self):
        user = self.add_user("1001")
        client = FakeClient([])

        count = client.query_count(user, machine_id=user.entrance_machine_id)

        self.assertEqual(count, 7)
        self.assertEqual(client.check_calls, [("1001", 2)])
        self.assertEqual(client.upload_calls, [])

    def test_td_client_query_count_sends_only_check_request_type(self):
        user = self.add_user("1001")
        client = TDClient(self.storage.load_config())
        request_types = []

        def fake_request(_data, request_type):
            request_types.append(request_type)
            return {"status": "success", "srvresp": "刷卡成功, 本学期锻炼次数:7"}

        client.request = fake_request

        self.assertEqual(client.query_count(user), 7)
        self.assertEqual(request_types, [80])

    def test_connection_refused_includes_endpoint_and_config_hint(self):
        config = self.storage.load_config()
        config["server"]["ip"] = "127.0.0.1"
        config["server"]["port"] = 8888
        client = TDClient(config, socket_factory=lambda *_args, **_kwargs: RefusedSocket())

        with self.assertRaises(ConnectionError) as caught:
            client.request(b"{}", 80)

        message = str(caught.exception)
        self.assertIn("127.0.0.1:8888", message)
        self.assertIn("~/.autoTD/config.json", message)
        self.assertIn("校园网", message)

    def test_scheduler_interleaves_users_by_due_time_and_waits_after_exit(self):
        self.add_user("1001", rounds=2, wait_time_min=10, wait_time_max=10)
        self.add_user("1002", rounds=1, wait_time_min=10, wait_time_max=10)
        self.storage.update_settings(
            {
                "schedule": {
                    "poll_seconds": 60,
                    "windows": ["07:30-10:00", "11:30-14:00", "15:30-20:00"],
                }
            }
        )
        client = FakeClient(
            [
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:1", 1),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:1", 1),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:2", 2),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:2", 2),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:3", 3),
            ]
        )

        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 8, 0, 0, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 10,
        )
        self.assertEqual(client.check_calls, [("1001", 2), ("1002", 2)])

        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 8, 0, 5, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 10,
        )
        self.assertEqual(client.check_calls, [("1001", 2), ("1002", 2)])

        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 8, 0, 10, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 10,
        )
        self.assertEqual(client.check_calls, [("1001", 2), ("1002", 2), ("1001", 6), ("1002", 6)])
        day = self.storage.load_state()["daily_runs"]["2026-04-28"]["users"]
        self.assertEqual(day["1001"]["status"], "waiting")
        self.assertEqual(day["1001"]["next_action"], "entrance")
        self.assertEqual(day["1001"]["completed_rounds"], 1)
        self.assertEqual(day["1002"]["status"], "completed")

        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 8, 0, 20, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 10,
        )
        self.assertEqual(
            client.check_calls,
            [("1001", 2), ("1002", 2), ("1001", 6), ("1002", 6), ("1001", 2)],
        )

    def test_scheduler_discards_due_exit_outside_window_and_restarts_next_window(self):
        self.add_user("1001", rounds=1, wait_time_min=120, wait_time_max=120)
        client = FakeClient(
            [
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:1", 1),
                CheckResponse(True, "刷卡成功, 本学期锻炼次数:2", 2),
            ]
        )

        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 9, 59, 30, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 120,
        )
        self.assertEqual(client.check_calls, [("1001", 2)])

        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 10, 2, 0, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 120,
        )
        self.assertEqual(client.check_calls, [("1001", 2)])
        state = self.storage.load_state()["daily_runs"]["2026-04-28"]["users"]["1001"]
        self.assertEqual(state["status"], "pending")
        self.assertEqual(state["next_action"], "entrance")
        self.assertEqual(state["completed_rounds"], 0)

        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 11, 30, 0, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 120,
        )
        self.assertEqual(client.check_calls, [("1001", 2), ("1001", 2)])

    def test_scheduler_removes_deleted_user_before_next_due_action(self):
        self.add_user("1001", wait_time_min=10, wait_time_max=10)
        client = FakeClient([CheckResponse(True, "刷卡成功, 本学期锻炼次数:1", 1)])

        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 8, 0, 0, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 10,
        )
        self.storage.delete_user("1001")
        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 8, 0, 10, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 10,
        )

        self.assertEqual(client.check_calls, [("1001", 2)])
        self.assertNotIn("1001", self.storage.load_state()["daily_runs"]["2026-04-28"]["users"])

    def test_scheduler_records_error_status_until_restarted(self):
        self.add_user("1001")
        client = FakeClient([RuntimeError("network down")])

        run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 8, 0, 0, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 0,
        )

        state = self.storage.load_state()["daily_runs"]["2026-04-28"]["users"]["1001"]
        self.assertEqual(state["status"], "error")
        self.assertIn("network down", state["last_error"])

    def test_scheduler_skips_user_when_cached_td_count_reaches_limit_and_queues_snapshot(self):
        self.add_user("1001", rounds=3)
        self.storage.set_last_count("1001", 32)
        client = FakeClient([])

        result = run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 8, 0, 0, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 0,
        )

        self.assertEqual(result.success_count, 1)
        self.assertEqual(client.check_calls, [])
        state = self.storage.load_state()["daily_runs"]["2026-04-28"]["users"]["1001"]
        self.assertEqual(state["status"], "completed")
        self.assertIn("TD 次数已达 32", state["last_message"])
        events = self.telemetry_events()
        self.assertEqual([event["event_type"] for event in events], ["td_count_changed", "td_limit_reached"])
        self.assertEqual(events[-1]["payload"]["users"], [{"student_id": "1001", "td_count": 32}])

    def test_scheduler_stops_after_entrance_reports_td_limit(self):
        self.add_user("1001", rounds=3)
        client = FakeClient([CheckResponse(True, "刷卡成功, 本学期锻炼次数:32", 32)])

        result = run_scheduler_once(
            self.storage,
            now=datetime(2026, 4, 28, 8, 0, 0, tzinfo=BEIJING_TZ),
            client=client,
            wait_seconds=lambda _user: 120,
        )

        self.assertEqual(result.success_count, 1)
        self.assertEqual(client.check_calls, [("1001", 2)])
        self.assertEqual(client.upload_calls, [(2, b"in")])
        state = self.storage.load_state()["daily_runs"]["2026-04-28"]["users"]["1001"]
        self.assertEqual(state["status"], "completed")
        self.assertIsNone(state["due_at"])
        self.assertIn("TD 次数已达 32", state["last_message"])
        events = self.telemetry_events()
        self.assertEqual([event["event_type"] for event in events], ["td_count_changed", "td_limit_reached"])
        self.assertEqual(events[0]["payload"]["count_source"], "td_entrance")
        self.assertEqual(events[0]["payload"]["delta"], 0)
        self.assertEqual(events[-1]["payload"]["users"], [{"student_id": "1001", "td_count": 32}])

    def test_run_forever_sends_midnight_snapshot_only_when_process_crosses_date(self):
        times = iter(
            [
                datetime(2026, 5, 5, 23, 59, 50, tzinfo=BEIJING_TZ),
                datetime(2026, 5, 5, 23, 59, 50, tzinfo=BEIJING_TZ),
                datetime(2026, 5, 6, 0, 0, 10, tzinfo=BEIJING_TZ),
            ]
        )
        stop_calls = {"count": 0}

        def stop_requested():
            stop_calls["count"] += 1
            return stop_calls["count"] > 2

        with mock.patch("auto_td.scheduler.run_scheduler_once") as run_once:
            with mock.patch("auto_td.scheduler.enqueue_daily_midnight_snapshot") as daily_snapshot:
                with mock.patch("auto_td.scheduler.flush_telemetry_queue"):
                    with mock.patch("auto_td.scheduler._sleep_with_stop"):
                        run_forever(self.storage, stop_requested=stop_requested, now_provider=lambda: next(times))

        self.assertEqual(run_once.call_count, 2)
        daily_snapshot.assert_called_once()
        self.assertEqual(daily_snapshot.call_args.kwargs["now"], datetime(2026, 5, 6, 0, 0, 10, tzinfo=BEIJING_TZ))

    def test_logging_creates_daytime_log_file(self):
        now = datetime(2026, 4, 28, 8, 0, tzinfo=timezone.utc)
        with contextlib.redirect_stdout(io.StringIO()):
            logger = setup_logging(self.storage, now=now)
            logger.info("用户 1001 成功")

        log_path = self.home / "logs" / "2026-04-28-daytime-log.txt"
        self.assertTrue(log_path.exists())
        self.assertIn("用户 1001 成功", log_path.read_text(encoding="utf-8"))

    def test_logging_rotates_to_machine_date_while_process_keeps_running(self):
        current_date = {"value": date(2026, 4, 28)}
        logger = setup_logging(self.storage, stream=False, date_provider=lambda: current_date["value"])

        logger.info("first day")
        current_date["value"] = date(2026, 4, 29)
        logger.info("second day")

        first = self.home / "logs" / "2026-04-28-daytime-log.txt"
        second = self.home / "logs" / "2026-04-29-daytime-log.txt"
        self.assertIn("first day", first.read_text(encoding="utf-8"))
        self.assertIn("second day", second.read_text(encoding="utf-8"))
        self.assertNotIn("second day", first.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
