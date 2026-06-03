import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from auto_td.background import get_scheduler_status, is_process_running, start_scheduler_process, stop_scheduler_process
from auto_td.storage import AppStorage


class FakeProcess:
    pid = 4321


class BackgroundProcessTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.home = Path(self.tmp.name) / "home"
        self.old_env = os.environ.get("AUTOTD_HOME")
        os.environ["AUTOTD_HOME"] = str(self.home)
        self.storage = AppStorage()
        self.storage.initialize()

    def tearDown(self):
        if self.old_env is None:
            os.environ.pop("AUTOTD_HOME", None)
        else:
            os.environ["AUTOTD_HOME"] = self.old_env
        self.tmp.cleanup()

    def test_start_scheduler_process_launches_detached_worker_and_writes_pid(self):
        with mock.patch("auto_td.background.is_process_running", return_value=False):
            with mock.patch("auto_td.background.subprocess.Popen", return_value=FakeProcess()) as popen:
                result = start_scheduler_process(self.storage)

        self.assertEqual(result.pid, 4321)
        self.assertFalse(result.already_running)
        self.assertEqual(result.pid_path, self.storage.pid_path)
        self.assertEqual(json.loads(self.storage.pid_path.read_text(encoding="utf-8"))["pid"], 4321)
        args, kwargs = popen.call_args
        self.assertEqual(args[0], [sys.executable, "-m", "auto_td.cli", "--daemon-worker"])
        self.assertIs(kwargs["stdin"], subprocess.DEVNULL)
        self.assertIs(kwargs["stdout"], subprocess.DEVNULL)
        self.assertIs(kwargs["stderr"], subprocess.DEVNULL)
        self.assertTrue(kwargs["start_new_session"])

    def test_stop_scheduler_process_terminates_pid_and_removes_pid_file(self):
        self.storage.pid_path.write_text(json.dumps({"pid": 4321}), encoding="utf-8")

        with mock.patch("auto_td.background.is_process_running", side_effect=[True, False]):
            with mock.patch("auto_td.background.terminate_process") as terminate:
                result = stop_scheduler_process(self.storage)

        self.assertTrue(result.stopped)
        self.assertEqual(result.pid, 4321)
        terminate.assert_called_once_with(4321)
        self.assertFalse(self.storage.pid_path.exists())

    def test_get_scheduler_status_reports_running_process(self):
        self.storage.pid_path.write_text(json.dumps({"pid": 4321}), encoding="utf-8")

        with mock.patch("auto_td.background.is_process_running", return_value=True):
            result = get_scheduler_status(self.storage)

        self.assertTrue(result.running)
        self.assertEqual(result.pid, 4321)
        self.assertFalse(result.stale)
        self.assertIn("后台定时检测运行中 PID=4321", result.message)

    def test_get_scheduler_status_cleans_stale_pid_file(self):
        self.storage.pid_path.write_text(json.dumps({"pid": 4321}), encoding="utf-8")

        with mock.patch("auto_td.background.is_process_running", return_value=False):
            result = get_scheduler_status(self.storage)

        self.assertFalse(result.running)
        self.assertEqual(result.pid, 4321)
        self.assertTrue(result.stale)
        self.assertFalse(self.storage.pid_path.exists())
        self.assertIn("pid 文件已清理", result.message)

    def test_is_process_running_on_windows_uses_tasklist(self):
        output = '"python.exe","4321","Console","1","12,000 K"\n'
        completed = subprocess.CompletedProcess(args=["tasklist"], returncode=0, stdout=output, stderr="")

        with mock.patch("auto_td.background.os.name", "nt"):
            with mock.patch("auto_td.background.subprocess.run", return_value=completed) as run_cmd:
                with mock.patch("auto_td.background.os.kill", side_effect=AssertionError("os.kill should not be called")):
                    self.assertTrue(is_process_running(4321))

        run_cmd.assert_called_once()

    def test_stop_scheduler_process_on_windows_calls_taskkill(self):
        self.storage.pid_path.write_text(json.dumps({"pid": 4321}), encoding="utf-8")
        tasklist_running = subprocess.CompletedProcess(
            args=["tasklist"],
            returncode=0,
            stdout='"python.exe","4321","Console","1","12,000 K"\n',
            stderr="",
        )
        taskkill_ok = subprocess.CompletedProcess(args=["taskkill"], returncode=0, stdout="SUCCESS", stderr="")
        tasklist_stopped = subprocess.CompletedProcess(
            args=["tasklist"],
            returncode=0,
            stdout='INFO: No tasks are running which match the specified criteria.\n',
            stderr="",
        )

        with mock.patch("auto_td.background.os.name", "nt"):
            with mock.patch(
                "auto_td.background.subprocess.run",
                side_effect=[tasklist_running, taskkill_ok, tasklist_stopped],
            ) as run_cmd:
                result = stop_scheduler_process(self.storage)

        self.assertTrue(result.stopped)
        self.assertEqual(result.pid, 4321)
        self.assertIn("已停止后台定时检测进程", result.message)
        self.assertFalse(self.storage.pid_path.exists())
        self.assertEqual(run_cmd.call_count, 3)
        self.assertEqual(run_cmd.call_args_list[1].args[0][:2], ["taskkill", "/PID"])

    def test_stop_scheduler_process_on_windows_handles_permission_denied(self):
        self.storage.pid_path.write_text(json.dumps({"pid": 4321}), encoding="utf-8")
        tasklist_running = subprocess.CompletedProcess(
            args=["tasklist"],
            returncode=0,
            stdout='"python.exe","4321","Console","1","12,000 K"\n',
            stderr="",
        )
        taskkill_denied = subprocess.CompletedProcess(
            args=["taskkill"],
            returncode=1,
            stdout="ERROR: Access is denied.\n",
            stderr="",
        )

        with mock.patch("auto_td.background.os.name", "nt"):
            with mock.patch("auto_td.background.subprocess.run", side_effect=[tasklist_running, taskkill_denied]):
                result = stop_scheduler_process(self.storage)

        self.assertFalse(result.stopped)
        self.assertEqual(result.pid, 4321)
        self.assertIn("权限不足", result.message)
        self.assertTrue(self.storage.pid_path.exists())

    def test_get_scheduler_status_on_windows_cleans_stale_pid_file(self):
        self.storage.pid_path.write_text(json.dumps({"pid": 4321}), encoding="utf-8")
        tasklist_stopped = subprocess.CompletedProcess(
            args=["tasklist"],
            returncode=0,
            stdout='INFO: No tasks are running which match the specified criteria.\n',
            stderr="",
        )

        with mock.patch("auto_td.background.os.name", "nt"):
            with mock.patch("auto_td.background.subprocess.run", return_value=tasklist_stopped):
                result = get_scheduler_status(self.storage)

        self.assertFalse(result.running)
        self.assertEqual(result.pid, 4321)
        self.assertTrue(result.stale)
        self.assertFalse(self.storage.pid_path.exists())
        self.assertIn("pid 文件已清理", result.message)


if __name__ == "__main__":
    unittest.main()
