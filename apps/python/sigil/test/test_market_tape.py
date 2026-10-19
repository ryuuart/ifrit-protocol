"""A sketch's event loop, and the market tape it feeds.

The loop tests render small sketches whose tasks report what they saw
through builtins, since a session's instance is the host's. The market
tape test copies the sketch's own files beside a probe that subclasses
it, so the probe's session replays the sketch's recordings at the names
the sketch asks for, and reads back the tape the page drew at its
capture moment.
"""

import builtins
import json
import shutil
import sys
import tempfile
import textwrap
import types
import unittest
from pathlib import Path

from sigil import data
from sigil.sketch import renderFile

SKETCH = (
    Path(__file__).resolve().parents[3]
    / "grimoire"
    / "sketches"
    / "python_market_tape"
)
CAPTURE = 12.0  # the market tape's declared capture moment

PROBE = """
import builtins

from sigil.sketch import sketch

from . import python_market_tape as tape


@sketch(size=(tape.WIDTH, tape.HEIGHT), captureAt=tape_capture)
class Probe(tape.MarketTape):
    def describe(self, seconds):
        builtins._sigil_market_tape = (
            seconds,
            list(self.trades),
            self.weather,
            (self.stream.state, self.stream.count),
            (self.forecast.state, self.forecast.count),
            self.tasks,
        )
        return super().describe(seconds)
"""


class Rendered(unittest.TestCase):
    def setUp(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_market_tape", None))

    def render(self, source, *, at=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entry = root / "scene.py"
            entry.write_text(textwrap.dedent(source))
            renderFile(entry, root / "frame.png", at=at)
        return builtins._sigil_market_tape


class EventLoop(Rendered):
    def test_a_task_runs_on_the_sketch_thread_and_the_next_description_reads_its_writes(
        self,
    ):
        seen = self.render(
            """
            import asyncio
            import builtins
            import threading

            from sigil.compose import box
            from sigil.sketch import sketch


            @sketch(size=(20, 20), captureAt=0.1)
            class Fed:
                def setup(self, ctx):
                    self.count = 0
                    self.seen = {"task": set(), "described": []}
                    builtins._sigil_market_tape = self.seen
                    self.loop.create_task(self.counting())

                async def counting(self):
                    while True:
                        self.seen["task"].add(threading.get_ident())
                        self.count += 1
                        await asyncio.sleep(0)

                def describe(self, seconds):
                    self.seen["described"].append((threading.get_ident(), self.count))
                    return box()
            """
        )
        described = seen["described"]
        threads = {thread for thread, count in described}
        counts = [count for thread, count in described]
        # One thread for the hooks and the task alike.
        self.assertEqual(len(threads), 1)
        self.assertEqual(seen["task"], threads)
        # The task resumes once a turn and a turn comes once a frame, so
        # every description reads the count one step on from the last.
        self.assertGreater(len(counts), 2)
        self.assertEqual(counts, list(range(len(counts))))

    def test_the_end_of_the_session_cancels_every_task_and_closes_the_loop(self):
        seen = self.render(
            """
            import asyncio
            import builtins

            from sigil.compose import box
            from sigil.sketch import sketch


            @sketch(size=(20, 20), captureAt=0.05)
            class Waiting:
                def setup(self, ctx):
                    self.seen = {"ended": None, "loop": self.loop}
                    builtins._sigil_market_tape = self.seen
                    self.loop.create_task(self.forever())
                    return box()

                async def forever(self):
                    try:
                        await asyncio.Event().wait()
                    except asyncio.CancelledError:
                        # A cancelled task gets its turns to unwind, so cleanup
                        # that awaits, like closing a socket, still runs.
                        await asyncio.sleep(0)
                        self.seen["ended"] = "cancelled"
                        raise
            """
        )
        self.assertEqual(seen["ended"], "cancelled")
        self.assertTrue(seen["loop"].is_closed())

    def declared(self, source):
        """The sketch class source declares, in a module of its own."""
        module = types.ModuleType(f"market_tape_probe_{len(source)}")
        sys.modules[module.__name__] = module
        self.addCleanup(sys.modules.pop, module.__name__)
        exec(textwrap.dedent(source), module.__dict__)
        return module.__sigil_sketch__

    def test_the_loop_is_the_sessions_and_a_class_of_its_own_keeps_its_own(self):
        plain = self.declared(
            """
            from sigil.sketch import sketch

            @sketch(size=(20, 20))
            class Plain:
                def describe(self, seconds):
                    return None
            """
        )
        with self.assertRaisesRegex(RuntimeError, "session's clock"):
            plain().loop
        owned = self.declared(
            """
            from sigil.sketch import sketch

            @sketch(size=(20, 20))
            class Owned:
                loop = "the class's own"

                def describe(self, seconds):
                    return None
            """
        )
        self.assertEqual(owned().loop, "the class's own")


class MarketTape(Rendered):
    def recording(self, name):
        """A recording as the hub decodes it, every arrival up to the
        capture moment."""
        text = (SKETCH / "data" / name).read_text()
        arrivals = data.decode(text, data.Dialect.Json).toPython()["arrivals"]
        return [arrival["message"] for arrival in arrivals if arrival["at"] <= CAPTURE]

    def test_a_capture_replays_the_recorded_tape_up_to_its_moment(self):
        with tempfile.TemporaryDirectory(prefix="market tape ") as directory:
            root = Path(directory)
            shutil.copy(SKETCH / "python_market_tape.py", root)
            shutil.copytree(SKETCH / "data", root / "data")
            (root / "probe.py").write_text(
                textwrap.dedent(PROBE).replace("tape_capture", repr(CAPTURE))
            )
            renderFile(root / "probe.py", root / "still.png")
            self.assertTrue((root / "still.png").stat().st_size > 0)
        seconds, trades, weather, stream, forecast, tasks = builtins._sigil_market_tape
        trades_recorded = self.recording("tape.json")
        readings = self.recording("weather.json")
        self.assertAlmostEqual(seconds, CAPTURE)
        # The tape holds the newest trades that had landed by the capture
        # moment, in the order they landed, and counts every one.
        self.assertGreater(len(trades_recorded), 0)
        self.assertEqual(trades, trades_recorded[-len(trades) :])
        self.assertEqual(len(trades), min(len(trades_recorded), 240))
        self.assertEqual(stream, ("replaying", len(trades_recorded)))
        self.assertEqual(weather, readings[-1]["current_weather"])
        self.assertEqual(forecast, ("ok", len(readings)))
        # A capture starts no task: nothing in it reaches the network.
        self.assertEqual(tasks, [])


if __name__ == "__main__":
    unittest.main()
