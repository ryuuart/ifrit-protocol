#!/usr/bin/env python3
"""The window answers input sent over its endpoint as a person's input.

The window mounts `session.pointer` and `session.key` on the endpoint it
opens, and hands each to its render thread, where the sketch on screen
reads it on its next frame. A file of functions with no `draw` calls its
input hooks from what that frame reads, so a press sent over the endpoint
to a real window has to reach the file's `mousePressed`.

So this opens one: a `--shot` run of such a file, which presents the
window for as long as the photograph takes, while a client attached to
the address the window prints presses the pointer on the canvas until the
file writes down that it heard the press. A machine where no window
opens, or whose window mounts no endpoint, says so and is skipped.

Usage (invoked by the build; the paths are all absolute):
  window_input.py --grimoire <Grimoire binary> --package <sigil package dir> \\
      --work <scratch dir>
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

# The exit status the build reads as a skip.
SKIPPED = 77

# How long the window is given to print its address, and how long a press
# is given to be heard once the client is attached.
ADDRESS_SECONDS = 120.0
HEARD_SECONDS = 30.0

SKETCH = """from pathlib import Path

from sigil.sketch.globals import *

size(160, 120)
HEARD = Path(__file__).with_name("heard.txt")


def setup():
    HEARD.write_text("")


def update(elapsed, ctx):
    pass


def mousePressed():
    with HEARD.open("a") as heard:
        heard.write(f"pressed {mouseX:g} {mouseY:g}\\n")
"""


class Output:
    """Everything a child process writes to one stream, read as it arrives
    on a thread of its own so the child never blocks on a full pipe."""

    def __init__(self, stream) -> None:
        self.lines: list[str] = []
        self._lock = threading.Lock()
        self._thread = threading.Thread(target=self._read, args=(stream,))
        self._thread.daemon = True
        self._thread.start()

    def _read(self, stream) -> None:
        for line in stream:
            with self._lock:
                self.lines.append(line)

    def text(self) -> str:
        with self._lock:
            return "".join(self.lines)

    def finish(self) -> None:
        self._thread.join(timeout=5)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grimoire", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()

    sys.path.insert(0, str(args.package))
    from sigil.protocol import Session, connect

    work = args.work
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    source = work / "pressed.py"
    source.write_text(SKETCH)
    heard = work / "heard.txt"
    shot = work / "window.png"

    command = [
        str(args.grimoire),
        "--state",
        str(work / "state"),
        str(source),
        "--shot",
        str(shot),
    ]
    print("$ " + " ".join(command), flush=True)
    window = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    output, errors = Output(window.stdout), Output(window.stderr)

    def finish_window() -> int:
        try:
            status = window.wait(timeout=120)
        except subprocess.TimeoutExpired:
            window.kill()
            status = window.wait()
        output.finish()
        errors.finish()
        sys.stdout.write(output.text())
        sys.stderr.write(errors.text())
        return status

    # THE ADDRESS THE WINDOW PRINTS once its endpoint listens.
    address = None
    deadline = time.monotonic() + ADDRESS_SECONDS
    while address is None and time.monotonic() < deadline:
        found = re.search(r"\[grimoire\] protocol at (ws://\S+)", errors.text())
        if found:
            address = found.group(1)
        elif window.poll() is not None:
            break
        else:
            time.sleep(0.05)
    if address is None:
        status = finish_window()
        print(
            f"the window printed no endpoint address (exit {status}): "
            "no window or no endpoint on this machine",
            flush=True,
        )
        sys.exit(SKIPPED)

    # A PRESS, again and again, until the file says it heard one. The
    # window's session may still be compiling the file when the client
    # attaches, and a press before the session stands is answered with a
    # refusal rather than lost silently, so the client keeps pressing.
    connection = connect(address, timeout=30)
    session = Session(connection)
    deadline = time.monotonic() + HEARD_SECONDS
    refusals: list[str] = []
    pressed = False
    while time.monotonic() < deadline and window.poll() is None:
        try:
            session.pointer(x=40, y=30, pressed=True)
            time.sleep(0.1)
            session.pointer(x=40, y=30, pressed=False)
        except Exception as refusal:  # noqa: BLE001 — every refusal is reported
            refusals.append(str(refusal))
        if heard.exists() and "pressed" in heard.read_text():
            pressed = True
            break
        time.sleep(0.1)
    try:
        connection.close()
    except Exception:  # noqa: BLE001 — the window may already have gone
        pass
    status = finish_window()

    if not pressed:
        last = refusals[-1] if refusals else "none"
        sys.exit(
            "the file never heard a press sent over the window's endpoint "
            f"(window exit {status}, {len(refusals)} refusals, last: {last})"
        )
    lines = heard.read_text().splitlines()
    if not lines or lines[0] != "pressed 40 30":
        sys.exit(f"the press was heard at the wrong point: {lines}")
    if status != 0:
        sys.exit(f"the window exited {status} after the press was heard")
    print(f"the window's endpoint pressed the file's pointer: {lines[0]}")


if __name__ == "__main__":
    main()
