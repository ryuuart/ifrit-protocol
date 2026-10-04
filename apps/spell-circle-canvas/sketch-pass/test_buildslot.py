#!/usr/bin/env python3
"""Bounded builder output and retention, isolated from the shared build tree."""

import contextlib
import fcntl
import importlib.util
import io
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest import mock

HELPER = Path(__file__).with_name("buildslot.py")
spec = importlib.util.spec_from_file_location("buildslot", HELPER)
buildslot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(buildslot)


class BuildslotTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="buildslot-test-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.state = self.root / "buildslot"
        self.state.mkdir()
        self.globals = mock.patch.multiple(
            buildslot,
            APPLICATION=self.root,
            STATE=self.state,
            LOCK=self.state / "slot.lock",
            TERM_GRACE_SECONDS=0.2,
        )
        self.globals.start()
        self.addCleanup(self.globals.stop)

    def run_job(self, name, source, limit=5):
        return buildslot.runner(name, limit, [sys.executable, "-c", source])

    def test_large_unbroken_output_keeps_head_tail_stderr_and_exit(self):
        source = (
            "import os\n"
            "os.write(1, b'HEAD_DIAGNOSTIC\\n')\n"
            "for _ in range(128): os.write(1, b'x' * 65536)\n"
            "os.write(2, b'\\nTAIL_DIAGNOSTIC\\n')\n"
            "os._exit(37)\n"
        )
        self.assertEqual(self.run_job("noisy", source), 37)
        log, status = buildslot.job_paths("noisy")
        data = log.read_bytes()
        self.assertLessEqual(len(data), buildslot.MAX_LOG_BYTES)
        self.assertIn(b"HEAD_DIAGNOSTIC", data)
        self.assertIn(buildslot.TRUNCATION_MARKER, data)
        self.assertTrue(
            data.endswith(b"TAIL_DIAGNOSTIC\n== finished with exit status 37\n")
        )
        result = json.loads(status.read_text())
        self.assertEqual(result["state"], "finished")
        self.assertEqual(result["exit"], 37)
        self.assertTrue(result["log_truncated"])
        self.assertGreater(result["output_bytes"], buildslot.MAX_LOG_BYTES)

    def test_short_output_is_exact_and_full_capacity_needs_no_marker(self):
        stream = io.BytesIO()
        log = buildslot.BoundedLog(stream)
        message = b"one diagnostic\n"
        log.append(message)
        log.flush(force=True)
        self.assertEqual(stream.getvalue(), message)
        log.append(b"x" * (buildslot.MAX_LOG_BYTES - len(message)))
        log.flush(force=True)
        self.assertEqual(len(stream.getvalue()), buildslot.MAX_LOG_BYTES)
        self.assertFalse(log.truncated)

    def test_timeout_stops_descendants_even_after_the_leader_exits(self):
        # The worker ignores TERM and keeps the output pipe open after its parent.
        worker = (
            "import pathlib, signal, time\n"
            "signal.signal(signal.SIGTERM, signal.SIG_IGN)\n"
            "heartbeat = pathlib.Path('heartbeat')\n"
            "while True:\n"
            " heartbeat.write_text(str(time.monotonic()))\n"
            " print('worker alive', flush=True)\n"
            " time.sleep(0.02)\n"
        )
        for exits_immediately in (False, True):
            with self.subTest(leader_exits_before_timeout=exits_immediately):
                heartbeat = self.root / "heartbeat"
                heartbeat.unlink(missing_ok=True)
                source = (
                    "import os, signal, subprocess, sys, time\n"
                    "print('LEADER=' + str(os.getpid()), flush=True)\n"
                    f"subprocess.Popen([sys.executable, '-c', {worker!r}])\n"
                    "signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))\n"
                    + ("time.sleep(0.1)\n" if exits_immediately else "time.sleep(30)\n")
                )
                name = f"timeout-{exits_immediately}"
                start = time.monotonic()
                try:
                    self.assertEqual(self.run_job(name, source, limit=0.3), 124)
                    self.assertLess(time.monotonic() - start, 3)
                    self.assertTrue(heartbeat.exists())
                    final = heartbeat.read_text()
                    time.sleep(0.1)
                    self.assertEqual(heartbeat.read_text(), final)
                    log, status = buildslot.job_paths(name)
                    self.assertIn(b"ran past its limit", log.read_bytes())
                    self.assertEqual(json.loads(status.read_text())["exit"], 124)
                finally:
                    # Clean up a worker even if a regression fails before group cleanup.
                    log, _ = buildslot.job_paths(name)
                    if log.exists():
                        for line in log.read_text().splitlines():
                            if line.startswith("LEADER="):
                                try:
                                    os.killpg(int(line[7:]), signal.SIGKILL)
                                except ProcessLookupError:
                                    pass

    def test_a_leader_that_exits_is_not_held_by_what_it_left_running(self):
        # The worker keeps the output pipe open for a while after its parent.
        worker = (
            "import time\n"
            "end = time.monotonic() + 5\n"
            "while time.monotonic() < end:\n"
            " print('worker alive', flush=True)\n"
            " time.sleep(0.05)\n"
        )
        source = (
            "import os, subprocess, sys\n"
            "print('LEADER=' + str(os.getpid()), flush=True)\n"
            f"subprocess.Popen([sys.executable, '-c', {worker!r}])\n"
            "sys.exit(5)\n"
        )
        start = time.monotonic()
        try:
            with mock.patch.object(buildslot, "LINGER_GRACE_SECONDS", 0.3):
                self.assertEqual(self.run_job("lingering", source, limit=30), 5)
            self.assertLess(time.monotonic() - start, 3)
            log, status = buildslot.job_paths("lingering")
            self.assertIn(b"they were left running", log.read_bytes())
            self.assertEqual(json.loads(status.read_text())["exit"], 5)
        finally:
            log, _ = buildslot.job_paths("lingering")
            if log.exists():
                for line in log.read_text().splitlines():
                    if line.startswith("LEADER="):
                        try:
                            os.killpg(int(line[7:]), signal.SIGKILL)
                        except ProcessLookupError:
                            pass

    def test_a_killed_runner_is_abandoned_and_retention_removes_it(self):
        helper = self.root / "sketch-pass" / "buildslot.py"
        helper.parent.mkdir()
        shutil.copyfile(HELPER, helper)
        name = "killed"
        _, status = buildslot.job_paths(name)
        with buildslot.LOCK.open("w") as slot:
            # Holding the slot keeps the runner queued behind it.
            fcntl.flock(slot, fcntl.LOCK_EX)
            runner = subprocess.Popen(
                [
                    sys.executable,
                    str(helper),
                    "--runner",
                    name,
                    "5",
                    "--",
                    sys.executable,
                    "-c",
                    "pass",
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            try:
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline and not status.exists():
                    time.sleep(0.02)
                self.assertTrue(status.exists(), "the runner never queued")
                queued = json.loads(status.read_text())
                self.assertEqual(queued["state"], "queued")
                self.assertFalse(buildslot.abandoned(status, queued))
                output = io.StringIO()
                with (
                    mock.patch.object(sys, "argv", [str(HELPER), "status"]),
                    contextlib.redirect_stdout(output),
                ):
                    self.assertEqual(buildslot.main(), 0)
                self.assertTrue(output.getvalue().startswith("queued killed"))
            finally:
                runner.kill()
                runner.wait()
            self.assertTrue(buildslot.abandoned(status, queued))
            output = io.StringIO()
            with (
                mock.patch.object(sys, "argv", [str(HELPER), "status"]),
                contextlib.redirect_stdout(output),
            ):
                self.assertEqual(buildslot.main(), 0)
            self.assertTrue(output.getvalue().startswith("abandoned killed"))
            with contextlib.redirect_stdout(io.StringIO()) as followed:
                self.assertEqual(buildslot.follow(name), 2)
            self.assertIn("abandoned", followed.getvalue())
            # Retention removes it as it removes a completed job.
            with mock.patch.object(buildslot, "COMPLETED_JOBS", 0):
                buildslot.prune_finished_jobs()
            self.assertFalse(status.exists())
            self.assertFalse(buildslot.liveness_path(name).exists())

    def test_a_dead_process_id_abandons_a_job_without_a_liveness_lock(self):
        finished = subprocess.Popen([sys.executable, "-c", "pass"])
        finished.wait()
        _, status = buildslot.job_paths("unlocked")
        record = {"state": "running", "job": "unlocked", "pid": finished.pid}
        status.write_text(json.dumps(record))
        self.assertTrue(buildslot.abandoned(status, record))
        record["pid"] = os.getpid()
        status.write_text(json.dumps(record))
        self.assertFalse(buildslot.abandoned(status, record))

    def test_retention_removes_only_old_completed_pairs(self):
        count = buildslot.COMPLETED_JOBS + 3
        for index in range(count):
            log, status = buildslot.job_paths(f"complete-{index:03}")
            log.write_text("completed output")
            status.write_text(json.dumps({"state": "finished", "finished": index}))
        for state in ("queued", "running"):
            log, status = buildslot.job_paths(state)
            log.write_text("active output")
            status.write_text(json.dumps({"state": state, "finished": -1000}))
        (self.state / "corrupt.json").write_text("not JSON")
        (self.state / "corrupt.log").write_text("do not guess its state")
        (self.state / "unmatched.log").write_text("no status")
        with buildslot.LOCK.open("w") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            buildslot.prune_finished_jobs()
        for index in range(count):
            log, status = buildslot.job_paths(f"complete-{index:03}")
            self.assertEqual(log.exists(), index >= 3)
            self.assertEqual(status.exists(), index >= 3)
        for name in ("queued", "running", "corrupt"):
            log, status = buildslot.job_paths(name)
            self.assertTrue(log.exists())
            self.assertTrue(status.exists())
        self.assertTrue((self.state / "unmatched.log").exists())

    def test_follow_reads_a_bounded_tail_and_preserves_evicted_exit_status(self):
        log, status = buildslot.job_paths("evicted")
        log.write_bytes(b"x" * (buildslot.MAX_LOG_BYTES * 3) + b"\nLAST_ERROR\n")
        status.write_text(json.dumps({"state": "finished", "exit": 9}))
        original_open = Path.open
        requests = []

        class Reader:
            def __init__(self, stream):
                self.stream = stream

            def __enter__(self):
                return self

            def __exit__(self, *_):
                self.stream.close()

            def seek(self, *args):
                return self.stream.seek(*args)

            def tell(self):
                return self.stream.tell()

            def read(self, size=-1):
                requests.append(size)
                if size < 0 or size > buildslot.FOLLOW_BYTES:
                    raise AssertionError("follower read unbounded output")
                return self.stream.read(size)

        def opened(path, *args, **kwargs):
            stream = original_open(path, *args, **kwargs)
            return Reader(stream) if path == log else stream

        output = io.StringIO()
        with (
            mock.patch.object(Path, "open", opened),
            contextlib.redirect_stdout(output),
        ):
            self.assertEqual(buildslot.follow("evicted"), 9)
        self.assertIn("LAST_ERROR", output.getvalue())
        self.assertEqual(requests, [buildslot.FOLLOW_BYTES])
        log.unlink()
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(buildslot.follow("evicted"), 9)

    def test_detached_job_finishes_after_the_calling_process_is_killed(self):
        helper = self.root / "sketch-pass" / "buildslot.py"
        helper.parent.mkdir()
        shutil.copyfile(HELPER, helper)
        caller = subprocess.Popen(
            [
                sys.executable,
                str(helper),
                "run",
                "--who",
                "detached",
                "--limit",
                "5",
                "--",
                sys.executable,
                "-c",
                "import time; time.sleep(0.5); print('DETACHED_TAIL'); raise SystemExit(17)",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            deadline = time.monotonic() + 3
            status = None
            while time.monotonic() < deadline:
                paths = list(self.state.glob("detached-*.json"))
                if paths:
                    candidate = json.loads(paths[0].read_text())
                    if candidate.get("state") == "running":
                        status = paths[0]
                        break
                time.sleep(0.02)
            self.assertIsNotNone(status, "the detached job never acquired its slot")
            caller.kill()
            caller.wait()
            while time.monotonic() < deadline:
                result = json.loads(status.read_text())
                if result.get("state") == "finished":
                    break
                time.sleep(0.02)
            self.assertEqual(result["state"], "finished")
            self.assertEqual(result["exit"], 17)
            self.assertIn("DETACHED_TAIL", status.with_suffix(".log").read_text())
        finally:
            if caller.poll() is None:
                caller.kill()
                caller.wait()

    def test_status_and_follow_handle_valid_but_malformed_json(self):
        malformed = (
            None,
            [],
            12,
            "finished",
            {},
            {"state": []},
            {"state": "finished"},
            {"state": "finished", "exit": "zero"},
        )
        for index, value in enumerate(malformed):
            name = f"malformed-{index}"
            _, status = buildslot.job_paths(name)
            status.write_text(json.dumps(value))
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(buildslot.follow(name), 2)
            self.assertIn("status is unusable", output.getvalue())
        for index, value in enumerate(
            (
                {"state": "queued"},
                {"state": "running", "job": 12, "command": []},
                {"state": "queued", "job": "bad", "command": [None]},
            )
        ):
            _, status = buildslot.job_paths(f"incomplete-{index}")
            status.write_text(json.dumps(value))
        _, valid = buildslot.job_paths("valid")
        valid.write_text(
            json.dumps({"state": "queued", "job": "valid", "command": ["python3"]})
        )
        output = io.StringIO()
        with (
            mock.patch.object(sys, "argv", [str(HELPER), "status"]),
            contextlib.redirect_stdout(output),
        ):
            self.assertEqual(buildslot.main(), 0)
        self.assertEqual(output.getvalue(), "queued valid python3\n")


if __name__ == "__main__":
    unittest.main()
