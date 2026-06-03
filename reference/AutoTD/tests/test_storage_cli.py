import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from datetime import datetime
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from auto_td import cli
from auto_td.background import BackgroundStartResult, BackgroundStatusResult, BackgroundStopResult
from auto_td.constants import BEIJING_TZ
from auto_td.storage import AppStorage


class StorageAndCliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.home = Path(self.tmp.name) / "home"
        self.old_env = os.environ.get("AUTOTD_HOME")
        os.environ["AUTOTD_HOME"] = str(self.home)

    def tearDown(self):
        if self.old_env is None:
            os.environ.pop("AUTOTD_HOME", None)
        else:
            os.environ["AUTOTD_HOME"] = self.old_env
        self.tmp.cleanup()

    def make_source_project(self) -> Path:
        source = Path(self.tmp.name) / "legacy_autoTD"
        images = source / "images"
        images.mkdir(parents=True)
        (images / "in.jpg").write_bytes(b"in")
        (images / "out.jpg").write_bytes(b"out")
        (source / "config.json").write_text(
            json.dumps(
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
            ),
            encoding="utf-8",
        )
        (source / "user_config.jsonl").write_text(
            json.dumps(
                {
                    "student_id": "22375080",
                    "card_id": "",
                    "entrance_machine_id": 2,
                    "exit_machine_id": 6,
                    "rounds": 3,
                    "wait_time_min": 180,
                    "wait_time_max": 240,
                    "entrance_photo_path": "images/in.jpg",
                    "exit_photo_path": "images/out.jpg",
                }
            )
            + "\n",
            encoding="utf-8",
        )
        return source

    def test_init_from_legacy_project_creates_home_files_and_imports_users(self):
        source = self.make_source_project()

        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(cli.main(["init", "--from", str(source)]), 0)

        storage = AppStorage()
        self.assertTrue((self.home / "config.json").exists())
        self.assertTrue((self.home / "settings.json").exists())
        self.assertTrue((self.home / "state.json").exists())
        self.assertTrue((self.home / "images" / "in.jpg").exists())
        self.assertTrue((self.home / "images" / "out.jpg").exists())
        user = storage.get_user("22375080")
        self.assertEqual(user["card_id"], hex(int("22375080"))[2:].upper())
        self.assertEqual(user["entrance_image"], "in.jpg")
        self.assertIn("initialized", output.getvalue())

    def test_init_uses_real_td_server_template(self):
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["init"]), 0)

        config = AppStorage().load_config()
        self.assertEqual(config["server"]["ip"], "10.212.28.38")
        self.assertEqual(config["server"]["port"], 8888)

    def test_init_migrates_existing_localhost_server_default(self):
        storage = AppStorage()
        storage.initialize()
        config = storage.load_config()
        config["server"]["ip"] = "127.0.0.1"
        config["server"]["port"] = 8888
        storage.save_config(config)

        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["init"]), 0)

        migrated = AppStorage().load_config()
        self.assertEqual(migrated["server"]["ip"], "10.212.28.38")
        self.assertEqual(migrated["server"]["port"], 8888)

    def test_user_crud_commands_manage_users_json(self):
        storage = AppStorage()
        storage.initialize()
        storage.add_image(Path(__file__), name="in.py")
        storage.add_image(Path(__file__), name="out.py")

        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(
                cli.main(
                    [
                        "user",
                        "add",
                        "1001",
                        "--entrance",
                        "2",
                        "--exit",
                        "6",
                        "--entrance-image",
                        "in.py",
                        "--exit-image",
                        "out.py",
                    ]
                ),
                0,
            )
        self.assertEqual(storage.get_user("1001")["card_id"], hex(1001)[2:].upper())

        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["user", "update", "1001", "--card-id", "CARD", "--exit", "7"]), 0)
        self.assertEqual(storage.get_user("1001")["card_id"], "CARD")
        self.assertEqual(storage.get_user("1001")["exit_machine_id"], 7)

        listed = io.StringIO()
        with contextlib.redirect_stdout(listed):
            self.assertEqual(cli.main(["user", "list"]), 0)
        self.assertIn("1001", listed.getvalue())

        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["user", "delete", "1001"]), 0)
        self.assertIsNone(storage.get_user("1001"))

    def test_user_add_quick_shahe_picks_shahe_machines_and_images(self):
        storage = AppStorage()
        storage.initialize()
        storage.add_image(Path(__file__), name="image1.py")
        storage.add_image(Path(__file__), name="image2.py")
        config = storage.load_config()
        config["machine"] = [
            {"id": 2, "machinesn": "XL-IN", "location": "北航本部TD入口1", "doortype": "1"},
            {"id": 6, "machinesn": "XL-OUT", "location": "北航本部TD出口1", "doortype": "2"},
            {"id": 8, "machinesn": "SH-IN", "location": "北航沙河TD入口1", "doortype": "1"},
            {"id": 11, "machinesn": "SH-OUT", "location": "北航沙河TD出口1", "doortype": "2"},
        ]
        storage.save_config(config)

        with mock.patch("auto_td.quick.random.choice", side_effect=lambda values: values[0]):
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(cli.main(["user", "add", "1002", "--quick", "沙河"]), 0)

        user = storage.get_user("1002")
        self.assertEqual(user["entrance_machine_id"], 8)
        self.assertEqual(user["exit_machine_id"], 11)
        self.assertEqual(user["entrance_image"], "image1.py")
        self.assertEqual(user["exit_image"], "image1.py")
        self.assertEqual(user["rounds"], 3)
        self.assertEqual(user["wait_time_min"], 180)
        self.assertEqual(user["wait_time_max"], 240)

    def test_user_add_quick_xueyuanlu_picks_main_campus_machines(self):
        storage = AppStorage()
        storage.initialize()
        storage.add_image(Path(__file__), name="image1.py")
        config = storage.load_config()
        config["machine"] = [
            {"id": 2, "machinesn": "XL-IN", "location": "北航本部TD入口1", "doortype": "1"},
            {"id": 6, "machinesn": "XL-OUT", "location": "北航本部TD出口1", "doortype": "2"},
            {"id": 8, "machinesn": "SH-IN", "location": "北航沙河TD入口1", "doortype": "1"},
            {"id": 11, "machinesn": "SH-OUT", "location": "北航沙河TD出口1", "doortype": "2"},
        ]
        storage.save_config(config)

        with mock.patch("auto_td.quick.random.choice", side_effect=lambda values: values[0]):
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(cli.main(["user", "add", "1003", "--quick", "学院路"]), 0)

        user = storage.get_user("1003")
        self.assertEqual(user["entrance_machine_id"], 2)
        self.assertEqual(user["exit_machine_id"], 6)

    def test_image_add_rejects_duplicate_without_overwrite(self):
        image = Path(self.tmp.name) / "photo.jpg"
        image.write_bytes(b"first")
        storage = AppStorage()
        storage.initialize()

        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["image", "add", str(image)]), 0)
        with self.assertRaises(FileExistsError):
            storage.add_image(image)

    def test_run_without_once_starts_background_scheduler(self):
        storage = AppStorage()
        storage.initialize()

        with mock.patch("auto_td.cli.run_all_users") as run_all_users:
            with mock.patch("auto_td.cli.clear_today_errors") as clear_errors:
                with mock.patch(
                    "auto_td.cli.start_scheduler_process",
                    return_value=BackgroundStartResult(4321, storage.pid_path, False),
                ) as start_scheduler:
                    output = io.StringIO()
                    with contextlib.redirect_stdout(output):
                        self.assertEqual(cli.main(["run"]), 0)

        self.assertFalse(run_all_users.called)
        self.assertTrue(clear_errors.called)
        self.assertTrue(start_scheduler.called)
        self.assertFalse(AppStorage().load_settings()["schedule"].get("enabled", False))
        self.assertIn("后台定时检测已启动", output.getvalue())
        self.assertNotIn("total=", output.getvalue())

    def test_schedule_enable_disable_commands_are_removed(self):
        storage = AppStorage()
        storage.initialize()

        with self.assertRaises(SystemExit):
            cli.main(["schedule", "enable"])
        with self.assertRaises(SystemExit):
            cli.main(["schedule", "disable"])

    def test_schedule_show_hides_legacy_enabled_setting(self):
        storage = AppStorage()
        storage.initialize()
        settings = storage.load_settings()
        settings["schedule"]["enabled"] = True
        storage.save_settings(settings)

        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(cli.main(["schedule", "show"]), 0)

        self.assertNotIn("enabled", output.getvalue())

    def test_status_includes_today_user_error_state(self):
        storage = AppStorage()
        storage.initialize()
        today = datetime.now(BEIJING_TZ).date().isoformat()
        state = storage.load_state()
        state["daily_runs"] = {
            today: {
                "users": {
                    "1001": {
                        "status": "error",
                        "completed_rounds": 0,
                        "next_action": "entrance",
                        "due_at": None,
                        "last_error": "network down",
                        "last_message": "请求失败",
                    }
                }
            }
        }
        storage.save_state(state)

        with mock.patch(
            "auto_td.cli.get_scheduler_status",
            return_value=BackgroundStatusResult(True, 4321, False, "后台定时检测运行中 PID=4321"),
        ):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["status"]), 0)

        self.assertIn("后台定时检测运行中 PID=4321", output.getvalue())
        self.assertIn("1001", output.getvalue())
        self.assertIn("error", output.getvalue())
        self.assertIn("network down", output.getvalue())

    def test_top_level_stop_stops_background_scheduler(self):
        storage = AppStorage()
        storage.initialize()

        with mock.patch(
            "auto_td.cli.stop_scheduler_process",
            return_value=BackgroundStopResult(4321, True, "已停止后台定时检测进程 PID=4321"),
        ) as stop_scheduler:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["--stop"]), 0)

        self.assertTrue(stop_scheduler.called)
        self.assertIn("已停止后台定时检测进程 PID=4321", output.getvalue())

    def test_user_count_reads_cached_state_without_server_request(self):
        storage = AppStorage()
        storage.initialize()
        storage.set_last_count("1001", 9)

        with mock.patch("auto_td.cli.TDClient") as client_cls:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["user", "count", "1001"]), 0)

        self.assertFalse(client_cls.called)
        self.assertIn("1001: 本学期锻炼次数 9", output.getvalue())
        self.assertIn("缓存", output.getvalue())

    def test_user_count_refresh_queries_server_and_updates_cache(self):
        storage = AppStorage()
        storage.initialize()
        storage.add_image(Path(__file__), name="in.py")
        storage.add_image(Path(__file__), name="out.py")
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(
                cli.main(
                    [
                        "user",
                        "add",
                        "1001",
                        "--entrance",
                        "2",
                        "--exit",
                        "6",
                        "--entrance-image",
                        "in.py",
                        "--exit-image",
                        "out.py",
                    ]
                ),
                0,
            )

        client = mock.Mock()
        client.query_count.return_value = 12
        with mock.patch("auto_td.cli.TDClient", return_value=client):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["user", "count", "1001", "--refresh"]), 0)

        self.assertEqual(storage.get_last_count("1001"), 12)
        self.assertIn("1001: 本学期锻炼次数 12", output.getvalue())
        self.assertIn("服务器刷新", output.getvalue())

    def test_status_reports_background_scheduler_state(self):
        storage = AppStorage()
        storage.initialize()

        with mock.patch(
            "auto_td.cli.get_scheduler_status",
            return_value=BackgroundStatusResult(True, 4321, False, "后台定时检测运行中 PID=4321"),
        ) as get_status:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["status"]), 0)

        self.assertTrue(get_status.called)
        self.assertIn("后台定时检测运行中 PID=4321", output.getvalue())


if __name__ == "__main__":
    unittest.main()
