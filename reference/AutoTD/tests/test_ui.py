import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from datetime import datetime
from importlib import resources
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from auto_td import cli
from auto_td.background import BackgroundStartResult, BackgroundStatusResult, BackgroundStopResult
from auto_td.constants import BEIJING_TZ
from auto_td.models import User
from auto_td.storage import AppStorage
from auto_td.telemetry import disable_telemetry
from auto_td.ui import UiService


class UiServiceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.home = Path(self.tmp.name) / "home"
        self.old_env = os.environ.get("AUTOTD_HOME")
        os.environ["AUTOTD_HOME"] = str(self.home)
        self.storage = AppStorage()
        self.storage.initialize()
        disable_telemetry(self.storage)
        self.storage.add_image(Path(__file__), name="in.py")
        self.storage.add_image(Path(__file__), name="out.py")

    def tearDown(self):
        if self.old_env is None:
            os.environ.pop("AUTOTD_HOME", None)
        else:
            os.environ["AUTOTD_HOME"] = self.old_env
        self.tmp.cleanup()

    def service(self) -> UiService:
        return UiService(storage_factory=lambda: self.storage)

    def test_state_payload_includes_users_counts_today_progress_and_process_status(self):
        self.storage.save_user(
            User(
                student_id="1001",
                card_id="CARD",
                entrance_machine_id=2,
                exit_machine_id=6,
                entrance_image="in.py",
                exit_image="out.py",
                rounds=3,
                wait_time_min=180,
                wait_time_max=240,
            )
        )
        self.storage.set_last_count("1001", 12, when=datetime(2026, 5, 27, 8, 0, tzinfo=BEIJING_TZ))
        today = datetime.now(BEIJING_TZ).date().isoformat()
        state = self.storage.load_state()
        state["daily_runs"] = {
            today: {
                "users": {
                    "1001": {
                        "status": "waiting",
                        "completed_rounds": 1,
                        "next_action": "exit",
                        "due_at": None,
                        "last_message": "入口完成，等待出口",
                    }
                }
            }
        }
        self.storage.save_state(state)

        with mock.patch(
            "auto_td.ui.get_scheduler_status",
            return_value=BackgroundStatusResult(True, 4321, False, "后台定时检测运行中 PID=4321"),
        ):
            payload = self.service().get_state()

        self.assertEqual(payload["background"]["pid"], 4321)
        self.assertTrue(payload["background"]["running"])
        self.assertEqual(payload["users"][0]["student_id"], "1001")
        self.assertEqual(payload["users"][0]["td_count"], 12)
        self.assertEqual(payload["users"][0]["today"]["status"], "waiting")
        self.assertEqual(payload["users"][0]["today"]["completed_rounds"], 1)
        self.assertEqual(payload["users"][0]["today"]["next_action"], "exit")

    def test_quick_add_delete_and_image_management(self):
        image_name = self.service().save_uploaded_image("photo.jpg", b"fake-image", overwrite=False)
        self.assertEqual(image_name, "photo.jpg")
        self.assertIn("photo.jpg", [image["name"] for image in self.service().list_images()])

        with mock.patch("auto_td.quick.random.choice", side_effect=lambda values: values[0]):
            added = self.service().add_user({"student_id": "1002", "campus": "学院路"})

        self.assertEqual(added["student_id"], "1002")
        self.assertEqual(self.storage.get_user("1002")["entrance_machine_id"], 2)

        self.assertTrue(self.service().delete_user("1002")["deleted"])
        self.assertIsNone(self.storage.get_user("1002"))
        self.assertTrue(self.service().delete_image("photo.jpg")["deleted"])
        self.assertNotIn("photo.jpg", self.storage.list_images())

    def test_run_control_starts_when_stopped_and_stops_when_running(self):
        service = self.service()
        stopped = BackgroundStatusResult(False, None, False, "后台定时检测未运行")
        running = BackgroundStatusResult(True, 4321, False, "后台定时检测运行中 PID=4321")

        with mock.patch("auto_td.ui.get_scheduler_status", return_value=stopped):
            with mock.patch(
                "auto_td.ui.start_scheduler_process",
                return_value=BackgroundStartResult(4321, self.storage.pid_path, False),
            ) as start:
                started = service.toggle_background()

        self.assertEqual(started["action"], "started")
        self.assertEqual(started["pid"], 4321)
        self.assertTrue(start.called)

        with mock.patch("auto_td.ui.get_scheduler_status", return_value=running):
            with mock.patch(
                "auto_td.ui.stop_scheduler_process",
                return_value=BackgroundStopResult(4321, True, "已停止后台定时检测进程 PID=4321"),
            ) as stop:
                stopped_payload = service.toggle_background()

        self.assertEqual(stopped_payload["action"], "stopped")
        self.assertEqual(stopped_payload["pid"], 4321)
        self.assertTrue(stop.called)

    def test_cli_ui_command_delegates_to_serve_ui(self):
        with mock.patch("auto_td.cli.serve_ui", return_value=0) as serve:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["ui", "--host", "127.0.0.1", "--port", "0", "--no-open"]), 0)

        args = serve.call_args.kwargs
        self.assertEqual(args["host"], "127.0.0.1")
        self.assertEqual(args["port"], 0)
        self.assertFalse(args["open_browser"])

    def test_static_ui_removes_motion_toggle_and_stacks_image_upload_controls(self):
        html = resources.files("auto_td").joinpath("ui_static/index.html").read_text(encoding="utf-8")
        css = resources.files("auto_td").joinpath("ui_static/styles.css").read_text(encoding="utf-8")
        js = resources.files("auto_td").joinpath("ui_static/app.js").read_text(encoding="utf-8")

        self.assertNotIn("reduce-motion", html)
        self.assertNotIn("减少动态", html)
        self.assertNotIn("reduceMotion", js)
        self.assertIn('class="panel image-panel"', html)
        self.assertIn('class="image-upload-panel"', html)
        self.assertIn(".image-panel .section-head", css)
        self.assertIn(".image-upload-panel", css)
        self.assertIn("grid-template-columns: minmax(12rem, 1fr) auto auto", css)
        self.assertIn("min-width: 0", css)


if __name__ == "__main__":
    unittest.main()
