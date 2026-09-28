#!/usr/bin/env python3
"""One builder at a time in the shared build tree.

    buildslot.py build <target> [<target> ...] [--who <name>]
    buildslot.py run --who <name> -- <command> [<argument> ...]
    buildslot.py wait <job>
    buildslot.py status

`build` runs `cmake --build build --config Release --target ...` from the
application root. `run` runs any other command that writes into the build
tree or needs it quiet (ctest, a headless Sketchbook sweep, stub generation).
A job that runs past --limit seconds (5400 for build, 1500 for run) is stopped
with everything it launched and exits 124. Either one queues behind whoever holds the slot, runs DETACHED from the
calling shell so a tool timeout cannot orphan ninja, and the caller follows
the log. When the caller's own time runs out before the job finishes it
prints the job name; `wait <job>` picks the log back up. Exit status is the
job's exit status, or 75 when the job is still running.
"""

import argparse
import fcntl
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

APPLICATION = Path(__file__).resolve().parents[1]  # apps/spell-circle-canvas, whichever checkout holds this file
STATE = Path(__file__).resolve().parent.parent / "buildslot"
LOCK = STATE / "slot.lock"
FOLLOW_SECONDS = 530


def job_paths(job: str) -> tuple[Path, Path]:
    return STATE / f"{job}.log", STATE / f"{job}.json"


def runner(job: str, limit: float, command: list[str]) -> int:
    log_path, status_path = job_paths(job)
    status = {"job": job, "command": command, "state": "queued", "queued": time.time()}
    status_path.write_text(json.dumps(status))
    with open(LOCK, "w") as lock, open(log_path, "a") as log:
        fcntl.flock(lock, fcntl.LOCK_EX)
        status.update(state="running", started=time.time(), pid=os.getpid())
        status_path.write_text(json.dumps(status))
        log.write(f"== slot acquired: {' '.join(command)}\n")
        log.flush()
        # The child leads its own process group so that a job which outlives
        # its limit is stopped together with everything it launched; a hung
        # window capture would otherwise hold the slot for every other agent.
        child = subprocess.Popen(
            command, cwd=APPLICATION, stdout=log, stderr=subprocess.STDOUT, start_new_session=True
        )
        try:
            exit_status = child.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGTERM)
            try:
                child.wait(timeout=20)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
            exit_status = 124
            log.write(f"== stopped: the job ran past its limit of {int(limit)} seconds\n")
        status.update(state="finished", finished=time.time(), exit=exit_status)
        status_path.write_text(json.dumps(status))
        log.write(f"== finished with exit status {exit_status}\n")
    return exit_status


def follow(job: str) -> int:
    log_path, status_path = job_paths(job)
    deadline = time.time() + FOLLOW_SECONDS
    announced_wait = False
    while time.time() < deadline:
        status = json.loads(status_path.read_text()) if status_path.exists() else {}
        if status.get("state") == "finished":
            lines = log_path.read_text(errors="replace").splitlines()
            failed = status["exit"] != 0
            keep = 160 if failed else 25
            print("\n".join(lines[-keep:]))
            return status["exit"]
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
    build.add_argument("--limit", type=float, default=5400, help="seconds before the job is stopped")
    run = verbs.add_parser("run")
    run.add_argument("--who", default="runner")
    run.add_argument("--limit", type=float, default=1500, help="seconds before the job is stopped")
    run.add_argument("command", nargs=argparse.REMAINDER)
    wait = verbs.add_parser("wait")
    wait.add_argument("job")
    verbs.add_parser("status")
    options = parser.parse_args()

    if options.verb == "status":
        for status_path in sorted(STATE.glob("*.json")):
            status = json.loads(status_path.read_text())
            if status.get("state") != "finished":
                print(status["state"], status["job"], " ".join(status["command"])[:120])
        return 0
    if options.verb == "wait":
        return follow(options.job)

    stamp = time.strftime("%H%M%S")
    job = f"{options.who}-{stamp}-{os.getpid()}"
    if options.verb == "build":
        command = ["cmake", "--build", "build", "--config", "Release", "--target", *options.targets]
    else:
        command = list(options.command)
        if command and command[0] == "--":
            command = command[1:]
        if not command:
            parser.error("run needs a command after --")
    return start(job, options.limit, command)


if __name__ == "__main__":
    sys.exit(main())
