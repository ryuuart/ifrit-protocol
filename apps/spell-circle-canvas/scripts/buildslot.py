#!/usr/bin/env python3
"""One builder at a time in the shared build tree.

    buildslot.py build <target> [<target> ...] [--who <name>]
    buildslot.py run --who <name> -- <command> [<argument> ...]
    buildslot.py wait <job>
    buildslot.py status

`build` runs `cmake --build build --config Release --target ...` from the
application root. `run` runs any other command that writes into the build
tree or needs it quiet (ctest, a headless Grimoire sweep, stub generation).
A job that runs past --limit seconds (5400 for build, 1500 for run) is stopped
with everything it launched and exits 124. A job whose leader exits while
processes it launched still hold its output is given a short grace, then
finishes with the leader's exit status and a note in its log. Either one
waits for the slot, an exclusive lock that admits one holder and keeps no
order among the jobs waiting for it; it runs DETACHED from the calling shell
so a tool timeout cannot orphan ninja, and the caller follows the log. When the caller's own time runs out before the job finishes it
prints the job name; `wait <job>` picks the log back up. Exit status is the
job's exit status, or 75 when the job is still running.

Each log keeps at most 2 MiB: its first 256 KiB and its latest output.
The newest 24 completed log/status pairs remain; queued and running jobs
are never removed by retention. A job whose runner was killed is abandoned:
`status` says so, and retention removes it as it removes a completed job.
Copy evidence elsewhere before it expires.
"""

import argparse
import fcntl
import json
import os
import selectors
import signal
import subprocess
import sys
import time
from pathlib import Path

APPLICATION = (
    Path(__file__).resolve().parents[1]
)  # apps/spell-circle-canvas, whichever checkout holds this file
STATE = Path(__file__).resolve().parent.parent / "buildslot"
LOCK = STATE / "slot.lock"
FOLLOW_SECONDS = 530
MAX_LOG_BYTES = 2 * 1024 * 1024
HEAD_BYTES = 256 * 1024
COMPLETED_JOBS = 24
FOLLOW_BYTES = 64 * 1024
TERM_GRACE_SECONDS = 20
LINGER_GRACE_SECONDS = 2.0
TRUNCATION_MARKER = b"\n== output truncated: first and latest output retained ==\n"


def job_paths(job: str) -> tuple[Path, Path]:
    return STATE / f"{job}.log", STATE / f"{job}.json"


def liveness_path(job: str) -> Path:
    # The runner holds an exclusive lock on this file from before its first
    # status until after its last; the lock goes when its process does, so
    # it cannot be mistaken for a later process that reuses the id.
    return STATE / f"{job}.alive"


def read_status(path: Path):
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError):
        return None


def abandoned(status_path: Path, status: dict) -> bool:
    """Whether a queued or running job's runner is gone."""
    if status.get("state") not in ("queued", "running"):
        return False
    job = status.get("job")
    if not isinstance(job, str):
        job = status_path.stem
    try:
        descriptor = os.open(liveness_path(job), os.O_RDONLY)
    except FileNotFoundError:
        current = read_status(status_path)
        if not isinstance(current, dict) or current.get("state") not in (
            "queued",
            "running",
        ):
            return False
        # A job recorded before runners held a liveness lock: its process id
        # is all there is to go on, and without one nothing can be said.
        process = current.get("pid")
        if not isinstance(process, int):
            return False
        try:
            os.kill(process, 0)
        except ProcessLookupError:
            return True
        except PermissionError:
            return False
        return False
    except OSError:
        return False
    try:
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        return False
    finally:
        os.close(descriptor)
    # The runner writes its final status before it lets the lock go.
    current = read_status(status_path)
    return isinstance(current, dict) and current.get("state") in ("queued", "running")


def write_status(path: Path, status: dict) -> None:
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(status))
    temporary.replace(path)


class BoundedLog:
    def __init__(self, stream):
        self.stream = stream
        self.head = bytearray()
        self.tail = bytearray()
        self.total = 0
        self.truncated = False
        self.dirty = False
        self.next_flush = 0.0

    def append(self, data: bytes) -> None:
        self.total += len(data)
        count = min(HEAD_BYTES - len(self.head), len(data))
        self.head.extend(data[:count])
        self.tail.extend(data[count:])
        maximum_tail = MAX_LOG_BYTES - HEAD_BYTES
        if self.truncated:
            maximum_tail -= len(TRUNCATION_MARKER)
        if len(self.tail) > maximum_tail:
            maximum_tail = MAX_LOG_BYTES - HEAD_BYTES - len(TRUNCATION_MARKER)
            del self.tail[:-maximum_tail]
            self.truncated = True
        self.dirty = True

    def flush(self, *, force: bool = False) -> None:
        now = time.monotonic()
        if not self.dirty or (not force and now < self.next_flush):
            return
        self.stream.seek(0)
        self.stream.write(self.head)
        if self.truncated:
            self.stream.write(TRUNCATION_MARKER)
        self.stream.write(self.tail)
        self.stream.truncate()
        self.stream.flush()
        self.dirty = False
        self.next_flush = now + 1.0


def stop_group(child: subprocess.Popen, signal_number: signal.Signals) -> None:
    try:
        os.killpg(child.pid, signal_number)
    except ProcessLookupError:
        pass


def drain(child: subprocess.Popen, log: BoundedLog, limit: float) -> int:
    assert child.stdout is not None
    deadline = time.monotonic() + limit
    timed_out = False
    kill_at = None
    drain_until = None
    leader_exited_at = None
    lingering = False
    with selectors.DefaultSelector() as ready:
        os.set_blocking(child.stdout.fileno(), False)
        ready.register(child.stdout, selectors.EVENT_READ)
        try:
            while child.poll() is None or ready.get_map():
                now = time.monotonic()
                if not timed_out and now >= deadline:
                    timed_out = True
                    stop_group(child, signal.SIGTERM)
                    kill_at = now + TERM_GRACE_SECONDS
                if kill_at is not None and now >= kill_at:
                    # The leader may have exited while a descendant holds stdout.
                    stop_group(child, signal.SIGKILL)
                    kill_at = None
                    drain_until = now + 1.0
                if drain_until is not None and now >= drain_until:
                    break
                # A process the leader launched may keep the output open after
                # the leader is done; the job is the leader's, and the slot is
                # not held for what it left behind.
                if leader_exited_at is None and child.poll() is not None:
                    leader_exited_at = now
                if (
                    not timed_out
                    and leader_exited_at is not None
                    and now - leader_exited_at >= LINGER_GRACE_SECONDS
                ):
                    lingering = True
                    break
                if ready.get_map():
                    for key, _ in ready.select(timeout=0.1):
                        try:
                            data = os.read(key.fd, 64 * 1024)
                        except BlockingIOError:
                            continue
                        if data:
                            log.append(data)
                        else:
                            ready.unregister(key.fileobj)
                else:
                    time.sleep(0.1)
                log.flush()
        finally:
            if timed_out or child.poll() is None:
                stop_group(child, signal.SIGKILL)
            child.stdout.close()
            child.wait()
    if lingering:
        log.append(
            b"== the leader exited while processes it launched still held its "
            b"output; they were left running\n"
        )
    return 124 if timed_out else child.returncode


def prune_finished_jobs() -> None:
    # Called with the slot locked. Completed, readable pairs are eligible, and
    # so are abandoned jobs, aged from when they were last heard of.
    finished = []
    for path in STATE.glob("*.json"):
        status = read_status(path)
        if not isinstance(status, dict):
            continue
        if (
            status.get("state") == "finished"
            and isinstance(status.get("finished"), (int, float))
            and path.with_suffix(".log").is_file()
        ):
            finished.append((status["finished"], path))
        elif abandoned(path, status):
            heard = status.get("started", status.get("queued", 0))
            if not isinstance(heard, (int, float)):
                heard = 0
            finished.append((heard, path))
    for _, path in sorted(finished, reverse=True)[COMPLETED_JOBS:]:
        path.with_suffix(".log").unlink(missing_ok=True)
        path.with_suffix(".alive").unlink(missing_ok=True)
        path.unlink(missing_ok=True)


def runner(job: str, limit: float, command: list[str]) -> int:
    log_path, status_path = job_paths(job)
    liveness = open(liveness_path(job), "w")
    fcntl.flock(liveness, fcntl.LOCK_EX)
    try:
        return run_holding_liveness(job, limit, command, log_path, status_path)
    finally:
        liveness_path(job).unlink(missing_ok=True)
        liveness.close()


def run_holding_liveness(
    job: str, limit: float, command: list[str], log_path: Path, status_path: Path
) -> int:
    status = {
        "job": job,
        "command": command,
        "state": "queued",
        "queued": time.time(),
        "pid": os.getpid(),
    }
    write_status(status_path, status)
    with open(LOCK, "w") as lock, open(log_path, "w+b") as stream:
        fcntl.flock(lock, fcntl.LOCK_EX)
        status.update(state="running", started=time.time(), pid=os.getpid())
        write_status(status_path, status)
        log = BoundedLog(stream)
        log.append(f"== slot acquired: {' '.join(command)}\n".encode())
        log.flush(force=True)
        # The child leads its own process group so that a job which outlives
        # its limit is stopped together with everything it launched; a hung
        # window capture would otherwise hold the slot for every other agent.
        try:
            child = subprocess.Popen(
                command,
                cwd=APPLICATION,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            exit_status = drain(child, log, limit)
        except OSError as error:
            log.append(f"== could not run: {error}\n".encode())
            exit_status = 127
        if exit_status == 124:
            log.append(
                f"== stopped: the job ran past its limit of {limit:g} seconds\n".encode()
            )
        log.append(f"== finished with exit status {exit_status}\n".encode())
        log.flush(force=True)
        status.update(state="finished", finished=time.time(), exit=exit_status)
        status.update(log_truncated=log.truncated, output_bytes=log.total)
        write_status(status_path, status)
        prune_finished_jobs()
    return exit_status


def follow(job: str) -> int:
    log_path, status_path = job_paths(job)
    started = time.monotonic()
    deadline = started + FOLLOW_SECONDS
    announced_wait = False
    while time.monotonic() < deadline:
        try:
            status = json.loads(status_path.read_text())
        except FileNotFoundError:
            if time.monotonic() - started >= 5:
                print(f"[{job}] job is unavailable or no longer retained")
                return 2
            time.sleep(3)
            continue
        except ValueError:
            print(f"[{job}] job status is unusable")
            return 2
        if (
            not isinstance(status, dict)
            or status.get("state") not in ("queued", "running", "finished")
            or (
                status.get("state") == "finished"
                and not isinstance(status.get("exit"), int)
            )
        ):
            print(f"[{job}] job status is unusable")
            return 2
        if status.get("state") == "finished":
            failed = status["exit"] != 0
            keep = 160 if failed else 25
            try:
                with log_path.open("rb") as stream:
                    if status.get("log_truncated"):
                        head = stream.read(FOLLOW_BYTES).decode(errors="replace")
                        print("\n".join(head.splitlines()[:20]))
                        print(
                            f"[{job}] output truncated; showing retained head and tail"
                        )
                    stream.seek(0, os.SEEK_END)
                    stream.seek(max(0, stream.tell() - FOLLOW_BYTES))
                    tail = stream.read(FOLLOW_BYTES).decode(errors="replace")
                    print("\n".join(tail.splitlines()[-keep:]))
            except FileNotFoundError:
                print(f"[{job}] completed log is no longer retained")
            return status["exit"]
        if abandoned(status_path, status):
            print(f"[{job}] job was abandoned: its runner is gone")
            return 2
        if status.get("state") == "queued" and not announced_wait:
            print(f"[{job}] waiting for the build slot", flush=True)
            announced_wait = True
        time.sleep(3)
    print(f"[{job}] still running; continue with: buildslot.py wait {job}")
    return 75


def start(job: str, limit: float, command: list[str]) -> int:
    STATE.mkdir(parents=True, exist_ok=True)
    subprocess.Popen(
        [sys.executable, __file__, "--runner", job, str(limit), "--", *command],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    time.sleep(0.5)
    return follow(job)


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] == "--runner":
        return runner(sys.argv[2], float(sys.argv[3]), sys.argv[5:])

    parser = argparse.ArgumentParser(description=__doc__)
    verbs = parser.add_subparsers(dest="verb", required=True)
    build = verbs.add_parser("build")
    build.add_argument("targets", nargs="+")
    build.add_argument("--who", default="builder")
    build.add_argument(
        "--limit", type=float, default=5400, help="seconds before the job is stopped"
    )
    run = verbs.add_parser("run")
    run.add_argument("--who", default="runner")
    run.add_argument(
        "--limit", type=float, default=1500, help="seconds before the job is stopped"
    )
    run.add_argument("command", nargs=argparse.REMAINDER)
    wait = verbs.add_parser("wait")
    wait.add_argument("job")
    verbs.add_parser("status")
    options = parser.parse_args()

    if options.verb == "status":
        for status_path in sorted(STATE.glob("*.json")):
            try:
                status = json.loads(status_path.read_text())
            except (OSError, ValueError):
                continue
            if not isinstance(status, dict) or status.get("state") not in (
                "queued",
                "running",
            ):
                continue
            job, command = status.get("job"), status.get("command")
            if (
                isinstance(job, str)
                and isinstance(command, list)
                and all(isinstance(argument, str) for argument in command)
            ):
                state = (
                    "abandoned" if abandoned(status_path, status) else status["state"]
                )
                print(state, job, " ".join(command)[:120])
        return 0
    if options.verb == "wait":
        return follow(options.job)

    stamp = time.strftime("%H%M%S")
    job = f"{options.who}-{stamp}-{os.getpid()}"
    if options.verb == "build":
        command = [
            "cmake",
            "--build",
            "build",
            "--config",
            "Release",
            "--target",
            *options.targets,
        ]
    else:
        command = list(options.command)
        if command and command[0] == "--":
            command = command[1:]
        if not command:
            parser.error("run needs a command after --")
    return start(job, options.limit, command)


if __name__ == "__main__":
    sys.exit(main())
