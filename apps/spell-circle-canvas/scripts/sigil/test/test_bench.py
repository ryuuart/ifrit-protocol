"""The fps lane's handling of the window's rows, without a window.

A fixture stands in for Grimoire: a script that answers `--window-bench`
with one WINDOW row per sketch it was asked for, in the order asked, in a
window of the size the test states, and writes down the command line it
was given.
"""

import contextlib
import io
import json
import stat
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from sigil import bench, tree

GRIMOIRE = """\
#!{python}
import json, os, sys
arguments = sys.argv[1:]
here = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(here, "asked.json"), "w") as handle:
    json.dump(arguments, handle)
with open(os.path.join(here, "window.txt")) as handle:
    window = handle.read().strip()
stems = [arguments[at + 1] for at, word in enumerate(arguments) if word == "--sketch"]
for stem in stems:
    print("WINDOW " + stem + " window=" + window + " canvas=900x640 "
          "kind=scene2d fps=119.8 work=0.40ms p99=1.10ms draw=0.39ms "
          "submit=0.20ms headroom=2400.0 open=0.05s warmMax=2.0ms "
          "power=AC gpuBusy=10%")
"""


class Fixture:
    """A temporary project with the fixture Grimoire in it."""

    def __init__(self, window):
        self.work = tempfile.TemporaryDirectory(prefix="sigil-fps-")
        self.root = Path(self.work.name)
        self.binary = self.root / "Grimoire"
        self.binary.write_text(GRIMOIRE.format(python=sys.executable))
        self.binary.chmod(self.binary.stat().st_mode | stat.S_IXUSR)
        (self.root / "window.txt").write_text(window)
        (self.root / "bench").mkdir()

    def run(self, *arguments):
        patches = [
            mock.patch.object(tree, "grimoire", lambda *a, **k: self.binary),
            mock.patch.object(tree, "PROJECT_DIR", self.root),
            mock.patch.object(bench, "screen_is_locked", lambda: False),
        ]
        printed = io.StringIO()
        with contextlib.ExitStack() as stack:
            for patch in patches:
                stack.enter_context(patch)
            stack.enter_context(contextlib.redirect_stdout(printed))
            status = bench.main(["--lane", "fps", *arguments])
        return status, printed.getvalue()

    def asked(self):
        return json.loads((self.root / "asked.json").read_text())

    def close(self):
        self.work.cleanup()


class TheLaneRunsOverTheStemsNamed(unittest.TestCase):
    def test_each_stem_is_asked_for_in_the_order_named_and_read_back(self):
        fixture = Fixture("1440x900@2")
        self.addCleanup(fixture.close)
        stems = ["alpha_ground", "bousen", "alpha_ground"]
        status, printed = fixture.run("--sketch", *stems, "--rebase")
        self.assertEqual(status, 0, printed)
        asked = fixture.asked()
        self.assertEqual(
            [asked[at + 1] for at, word in enumerate(asked) if word == "--sketch"],
            stems,
        )
        self.assertIn("presented  2 sketches", printed)
        self.assertNotIn("asked for", printed)

    def test_a_window_other_than_the_one_asked_for_is_said(self):
        fixture = Fixture("1440x600@4")
        self.addCleanup(fixture.close)
        status, printed = fixture.run("--sketch", "alpha_ground", "--rebase")
        self.assertEqual(status, 0, printed)
        self.assertIn("the window is 1440x600@4, asked for 1440x900@2", printed)


if __name__ == "__main__":
    unittest.main()
