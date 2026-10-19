"""The prompt ``sigil repl`` reads: lines typed at it run inside a sketch
running in Grimoire.

``SketchConsole`` reads input as Python's own interactive console does —
continuation lines, an expression told from statements, history through
``readline`` where the platform has it — and sends each complete input to
the host's ``session.evaluate``, which runs it inside the running sketch,
between two of its frames. It prints what the input evaluated to, what it
printed and, where it raised, the traceback. Three commands stand beside
Python: ``:reload`` imports the sketch's file again as saving it does,
``:still path.png`` photographs the sketch on screen through
``session.still`` and copies the picture to the path, and ``:quit`` leaves.
"""

from __future__ import annotations

import code
import collections.abc
import os
import shutil
import sys
import time
from pathlib import Path

from .protocol import ProtocolError, Session
from .protocol.session import EvaluateResult, EvaluationMode

BANNER = (
    "Python inside the running sketch: its module's names, `self` and `ctx`.\n"
    "Each input runs between two frames; the window shows it on the next.\n"
    ":reload imports the file again, :still path.png takes a picture, "
    ":quit leaves."
)


def evaluationMode(source: str) -> EvaluationMode:
    """Expression where @p source reads as one expression, statements
    otherwise — what decides whether a value is answered."""
    try:
        compile(source, "<prompt>", "eval")
    except SyntaxError:
        return EvaluationMode.Statements
    return EvaluationMode.Expression


class SketchConsole(code.InteractiveConsole):
    """A prompt whose every complete input runs inside the sketch the host's
    session holds, through @p session. @p sketch is the file the running
    sketch was imported from, which ``:reload`` touches; @p write is where
    everything the prompt says goes."""

    def __init__(
        self,
        session: Session,
        *,
        sketch: Path | None = None,
        write: collections.abc.Callable[[str], object] = sys.stdout.write,
    ) -> None:
        super().__init__(filename="<sigil>")
        self.session = session
        self.sketch = sketch
        self._out = write
        self.finished = False
        """Whether ``:quit`` was asked."""

    def write(self, data: str) -> None:
        """Where the console's own messages go: the same place as the
        rest."""
        self._out(data)

    def say(self, text: str) -> None:
        """One line of the prompt's own."""
        self._out(text if text.endswith("\n") else text + "\n")

    # --- reading -----------------------------------------------------------

    def push(self, line: str, filename: str | None = None, _symbol: str = "single") -> bool:
        """One line typed: a command where it opens a fresh input with a
        colon, otherwise Python, gathered until it is complete."""
        if not self.buffer and line.strip().startswith(":"):
            self.command(line.strip())
            return False
        return super().push(line)

    def runsource(self, source: str, filename: str = "<input>", symbol: str = "single") -> bool:
        """Sends @p source once it is complete; true while more is
        wanted."""
        try:
            compiled = self.compile(source, filename, symbol)
        except (OverflowError, SyntaxError, ValueError):
            # Text that cannot be complete is sent as it stands, so the
            # sketch's interpreter reports it in its own words.
            compiled = True
        if compiled is None:
            return True
        self.evaluate(source)
        return False

    # --- answering ---------------------------------------------------------

    def evaluate(self, source: str) -> EvaluateResult | None:
        """Runs @p source inside the sketch and prints what came back;
        nothing where the host refused it."""
        try:
            result = self.session.evaluate(code=source, mode=evaluationMode(source))
        except ProtocolError as error:
            self.say(f"sigil: {error.error.message}")
            return None
        if result.output:
            self._out(result.output)
        if result.error is not None:
            self._out(result.error.traceback or f"{result.error.type}: {result.error.message}\n")
        elif result.value:
            self.say(result.value)
        return result

    def command(self, line: str) -> None:
        """One of the prompt's own commands."""
        word, _, rest = line.partition(" ")
        argument = rest.strip()
        if word == ":quit":
            self.finished = True
        elif word == ":reload":
            self.reload()
        elif word == ":still":
            self.still(argument)
        elif word == ":help":
            self.say(BANNER)
        else:
            self.say(f"sigil: no command {word}; :reload, :still path.png and :quit are")

    def reload(self, patience: float = 10.0) -> None:
        """Imports the sketch's file again, as saving it does — what the
        prompt changed goes, and the file as written runs from its start —
        and waits, up to @p patience seconds, until the host runs the new
        import, so the next input reaches it."""
        if self.sketch is None or not self.sketch.is_file():
            self.say("sigil: the running sketch names no file this prompt can reload")
            return
        before = self.module()
        os.utime(self.sketch)
        deadline = time.monotonic() + patience
        while time.monotonic() < deadline:
            time.sleep(0.05)
            now = self.module()
            if now is not None and now != before:
                self.say(f"reloaded {self.sketch.name}")
                return
        self.say(
            f"sigil: {self.sketch.name} did not load again, and the sketch that "
            "was running runs on; Grimoire says why"
        )

    def module(self) -> str | None:
        """The module the running sketch's class was imported as, which a
        new import of its file names afresh; nothing where it cannot be
        read."""
        try:
            result = self.session.evaluate(
                code="type(self).__module__", mode=EvaluationMode.Expression
            )
        except ProtocolError:
            return None
        return None if result.error is not None else result.value

    def still(self, destination: str) -> None:
        """Photographs the sketch on screen and copies the picture to
        @p destination."""
        if not destination:
            self.say("sigil: :still takes the path to write the picture to")
            return
        try:
            taken = self.session.still(density=2.0)
        except ProtocolError as error:
            self.say(f"sigil: {error.error.message}")
            return
        target = Path(destination).expanduser()
        try:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(taken.path, target)
        except OSError as error:
            self.say(f"sigil: the still at {taken.path} could not be copied: {error}")
            return
        self.say(f"{target} ({taken.width} x {taken.height})")

    # --- the loop ----------------------------------------------------------

    def run(self, read: collections.abc.Callable[[str], str] = input) -> None:
        """Reads lines with @p read until ``:quit`` or the end of input."""
        try:
            import readline  # noqa: F401  (history and line editing)
        except ImportError:
            pass
        self.say(BANNER)
        more = False
        while not self.finished:
            try:
                line = read("... " if more else ">>> ")
            except EOFError:
                self._out("\n")
                break
            except KeyboardInterrupt:
                self._out("\nKeyboardInterrupt\n")
                self.resetbuffer()
                more = False
                continue
            try:
                more = self.push(line)
            except KeyboardInterrupt:
                # The host still runs what was sent; only the wait is given up.
                self._out("\nKeyboardInterrupt\n")
                self.resetbuffer()
                more = False
