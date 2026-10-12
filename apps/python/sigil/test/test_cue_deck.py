"""The cue_deck sketch reads one cue through any of its three doors and draws it.

The sketch's own files are copied beside a probe that subclasses it, so
the probe's session reaches the sketch's binary schema and recordings at
the same names the sketch asks for. The probe delivers the datagrams the
sketch's sender makes of one cue — JSON text, OSC messages or a Cue
FlatBuffer — through the inlet of the door that wire is read at, the way
a transport delivers them, and the test reads back the cue the page drew,
the door it names, and what the still shows.
"""

import builtins
import importlib.util
import json
import shutil
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import media
from sigil.sketch import render_file

SKETCH = Path(__file__).resolve().parents[3] / "grimoire" / "sketches" / "cue_deck"
SCHEMA = SKETCH / "cue_deck.bfbs"
COLOR_BYTES = (255, 0, 128)  # a colour no other part of the page is drawn in

# ONE CUE, in the shape the sketch reads; every value is exact as a
# 32-bit float, so the OSC and FlatBuffer wires carry it unchanged.
CUE = {
    "show": "PROBE SHOW",
    "channels": [{"name": "KEY", "level": 1.0}, {"name": "FILL", "level": 0.25}],
    "color": {"r": 1.0, "g": 0.0, "b": 0.5},
    "markers": [{"name": "DROP", "beat": 8.0}],
    "beat": 6.5,
}

PROBE = """
import builtins
import json
from pathlib import Path

from sigil.io.testing import inletOf
from sigil.sketch import sketch

# A sketch's helpers are ordinary imports from the package its directory
# makes, so the sketch beside this probe is one.
from . import cue_deck as deck

HERE = Path(__file__).resolve().parent


@sketch(size=(deck.WIDTH, deck.HEIGHT), capture_at=0)
class Probe(deck.Deck):
    def setup(self, ctx):
        super().setup(ctx)
        arrivals = json.loads((HERE / "arrivals.json").read_text())
        inlet = inletOf(ctx.assets.hub().listen(getattr(deck, arrivals["door"])))
        for datagram in arrivals["datagrams"]:
            inlet.deliver(bytes.fromhex(datagram), arrivedAt=0)

    def update(self, elapsed, ctx):
        super().update(elapsed, ctx)
        door, cue = self.newest()
        builtins._cue_deck = (
            door,
            cue.to_python(),
            {name: state.revision for name, state in self.shown.items()},
            sum(state.undecodable for state in self.shown.values()),
        )
"""


def sender_module():
    """The sketch's sender, imported with the generated CueDeck package
    beside it on the path its own run would have."""
    sys.path.insert(0, str(SKETCH))
    specification = importlib.util.spec_from_file_location(
        "cue_deck_sender", SKETCH / "sender.py"
    )
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


@unittest.skipUnless(
    SCHEMA.exists(), "cue_deck.bfbs is written by the SigilSketchSchemas target"
)
class CueDeckDoors(unittest.TestCase):
    def setUp(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_cue_deck", None))
        self.sender = sender_module()
        self.addCleanup(sys.path.remove, str(SKETCH))

    def render(self, door: str, datagrams: list[bytes]):
        """The probe's still at the first moment, before the recording has
        played anything, with `datagrams` delivered at the door the sketch
        names `door`, and what the page read by then."""
        builtins.__dict__.pop("_cue_deck", None)
        with tempfile.TemporaryDirectory(prefix="cue deck ") as directory:
            root = Path(directory)
            shutil.copy(SKETCH / "cue_deck.py", root)
            shutil.copy(SCHEMA, root)
            shutil.copytree(SKETCH / "data", root / "data")
            arrivals = {"door": door, "datagrams": [each.hex() for each in datagrams]}
            (root / "arrivals.json").write_text(json.dumps(arrivals))
            (root / "probe.py").write_text(textwrap.dedent(PROBE))
            render_file(root / "probe.py", root / "still.png", at=0)
            image = media.decode(media.Image, (root / "still.png").read_bytes())
            return builtins._cue_deck, image.frameAt(0).image

    def assert_drawn(self, door, datagrams, named):
        (newest, cue, revisions, undecodable), image = self.render(door, datagrams)
        self.assertEqual(newest, named)
        self.assertEqual(revisions[named], len(datagrams))
        self.assertEqual(sum(revisions.values()), len(datagrams))
        self.assertEqual(undecodable, 0)
        # THE SAME CUE whichever wire carried it.
        self.assertEqual(cue, CUE)
        # THE PAGE SHOWS IT: the full KEY meter and the quarter FILL meter
        # are filled in the cue's colour, which nothing else on the page is.
        self.assertGreater(self.pixels_of(image, COLOR_BYTES), 64 * 250)

    def test_a_cue_as_json_text_is_read_without_a_schema_and_drawn(self):
        self.assert_drawn("JSON_DOOR", self.sender.json_datagrams(CUE), "JSON")

    def test_a_cue_as_osc_messages_is_gathered_and_drawn(self):
        self.assert_drawn("OSC_DOOR", self.sender.osc_datagrams(CUE), "OSC")

    @unittest.skipUnless(
        importlib.util.find_spec("flatbuffers"),
        "the flatbuffers package is not installed",
    )
    def test_a_cue_as_a_flatbuffer_is_read_through_the_schema_and_drawn(self):
        self.assert_drawn(
            "SCHEMA_DOOR", self.sender.flatbuffer_datagrams(CUE), "FLATBUFFER"
        )

    def test_an_arrival_that_is_no_cue_is_counted_and_not_drawn(self):
        (newest, cue, revisions, undecodable), image = self.render(
            "SCHEMA_DOOR", [b"\x01\x02\x03 not a cue"]
        )
        self.assertEqual(newest, "")
        self.assertIsNone(cue)
        self.assertEqual(revisions["FLATBUFFER"], 1)
        self.assertEqual(undecodable, 1)
        self.assertEqual(self.pixels_of(image, COLOR_BYTES), 0)

    @staticmethod
    def pixels_of(image, color):
        rgba = image.rgba()
        return sum(
            1
            for index in range(0, len(rgba), 4)
            if tuple(rgba[index : index + 3]) == color
        )


if __name__ == "__main__":
    unittest.main()
