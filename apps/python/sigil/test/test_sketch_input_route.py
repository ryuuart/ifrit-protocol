"""Pointer and key input driven through the protocol: a client puts the
pointer over a running sketch with `session.pointer` and sends a key with
`session.key`, and a file of functions with no `draw` reads both and calls
its input hooks on the next step."""

import tempfile
import unittest
from pathlib import Path

from sigil.protocol import Clock, Session
from sigil.protocol.clock import Policy
from sigil.protocol.shared import ProtocolError
from sigil.testing import InProcess

SKETCH = """from pathlib import Path

from sigil.sketch.globals import *

size(64, 48)
HEARD = Path(__file__).with_name("heard.txt")


def setup():
    HEARD.write_text("")


def update(elapsed, ctx):
    pass


def mousePressed():
    with HEARD.open("a") as heard:
        heard.write(f"pressed {mouseX:g} {mouseY:g}\\n")


def mouseReleased():
    with HEARD.open("a") as heard:
        heard.write(f"released {mouseX:g} {mouseY:g}\\n")


def keyPressed():
    with HEARD.open("a") as heard:
        heard.write(f"key {key} {keyCode}\\n")
"""


class InputRoute(unittest.TestCase):
    def test_a_press_through_the_route_calls_the_sketchs_hooks(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "pressed.py"
            source.write_text(SKETCH)
            heard = root / "heard.txt"
            host = InProcess(root / "state")
            clock, session = Clock(host), Session(host)
            clock.setPolicy(policy=Policy.Advance)
            session.open(sketch=str(source))
            clock.step(seconds=1 / 60)
            self.assertEqual(heard.read_text(), "")

            session.pointer(x=12, y=30, pressed=True)
            clock.step(seconds=1 / 60)
            self.assertEqual(heard.read_text(), "pressed 12 30\n")

            session.pointer(x=20, y=10, pressed=False)
            session.key(name="a", code=65, pressed=True)
            clock.step(seconds=1 / 60)
            self.assertEqual(
                heard.read_text(),
                "pressed 12 30\nreleased 20 10\nkey a 65\n",
            )

    def test_input_without_a_session_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            session = Session(InProcess(Path(folder) / "state"))
            with self.assertRaises(ProtocolError):
                session.pointer(x=1, y=2, pressed=True)


if __name__ == "__main__":
    unittest.main()
