#!/usr/bin/env python3
"""Run one command in several processes at once, round after round.

Passes when every process of every round exits with status zero. What it
asks is whether copies of one program can run beside each other on a
machine whose facilities they share: a directory of publications, a
device, a port. A process that fails has its output printed whole.
"""

import argparse
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processes", type=int, default=2)
    parser.add_argument("--rounds", type=int, default=1)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    arguments = parser.parse_args()
    command = arguments.command
    if command and command[0] == "--":
        command = command[1:]
    if not command or arguments.processes < 1 or arguments.rounds < 1:
        parser.error("a command, at least one process and at least one round")

    failures = 0
    for round_number in range(arguments.rounds):
        running = [
            subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                errors="replace",
            )
            for _ in range(arguments.processes)
        ]
        for process_number, process in enumerate(running):
            output, _ = process.communicate()
            if process.returncode != 0:
                failures += 1
                print(
                    f"round {round_number + 1}, process {process_number + 1} "
                    f"exited with {process.returncode}:\n{output}"
                )
    total = arguments.processes * arguments.rounds
    print(f"{total - failures} of {total} processes passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
