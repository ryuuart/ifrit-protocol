"""The cue_deck sketch reads a Cue FlatBuffer off its door and draws it.

The sketch's own files are copied beside a probe that subclasses it, so
the probe's session reaches the sketch's binary schema and recording at
the same names the sketch asks for. The probe delivers one Cue, built by
the sketch's sender from the modules flatc generated, through the door's
inlet — the way a transport delivers a datagram — and the test reads back
what the door decoded and what the still shows.
"""

import builtins
import importlib.util
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
COLOR = (1.0, 0.0, 0.5)  # a colour no other part of the page is drawn in
COLOR_BYTES = (255, 0, 128)

PROBE = """
import builtins
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
        payload = (HERE / "cue.bin").read_bytes()
        if payload:
            inletOf(ctx.assets.hub().listen(deck.DOOR)).deliver(payload, arrivedAt=0)

    def update(self, elapsed, ctx):
        super().update(elapsed, ctx)
        builtins._cue_deck = (
            self.door.latest().payload.to_python(),
            self.shown.revision,
            self.shown.undecodable,
        )
"""


def sender_module():
    """The sketch's sender, imported with the generated CueDeck package
    beside it on the path its own run would have."""
    sys.path.insert(0, str(SKETCH))
    try:
        specification = importlib.util.spec_from_file_location(
            "cue_deck_sender", SKETCH / "sender.py"
        )
        module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(module)
        return module
    finally:
        sys.path.remove(str(SKETCH))


@unittest.skipUnless(
    SCHEMA.exists(), "cue_deck.bfbs is written by the SigilSketchSchemas target"
)
@unittest.skipUnless(
    importlib.util.find_spec("flatbuffers"), "the flatbuffers package is not installed"
)
class CueDeckDoor(unittest.TestCase):
    def setUp(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_cue_deck", None))
        self.sender = sender_module()

    def render(self, payload: bytes):
        """The probe's still at the first moment, before the recording has
        played anything, and what the door read by then."""
        builtins.__dict__.pop("_cue_deck", None)
        with tempfile.TemporaryDirectory(prefix="cue deck ") as directory:
            root = Path(directory)
            shutil.copy(SKETCH / "cue_deck.py", root)
            shutil.copy(SCHEMA, root)
            shutil.copytree(SKETCH / "data", root / "data")
            (root / "cue.bin").write_bytes(payload)
            (root / "probe.py").write_text(textwrap.dedent(PROBE))
            render_file(root / "probe.py", root / "still.png", at=0)
            image = media.decode(media.Image, (root / "still.png").read_bytes())
            return builtins._cue_deck, image.frameAt(0).image

    def test_a_delivered_cue_is_read_through_the_schema_and_drawn(self):
        cue = {
            "show": "PROBE SHOW",
            "channels": [("KEY", 1.0), ("FILL", 0.25)],
            "color": COLOR,
            "markers": [("DROP", 8.0)],
            "beat": 6.5,
        }
        (decoded, revision, undecodable), image = self.render(self.sender.encode(cue))
        self.assertEqual(revision, 1)
        self.assertEqual(undecodable, 0)
        self.assertEqual(decoded["show"], "PROBE SHOW")
        self.assertEqual(
            [(channel["name"], channel["level"]) for channel in decoded["channels"]],
            [("KEY", 1.0), ("FILL", 0.25)],
        )
        self.assertEqual(decoded["color"], {"r": 1.0, "g": 0.0, "b": 0.5})
        self.assertEqual(decoded["markers"], [{"name": "DROP", "beat": 8.0}])
        self.assertEqual(decoded["beat"], 6.5)
        # THE PAGE SHOWS IT: the full KEY meter and the quarter FILL meter
        # are filled in the cue's colour, which nothing else on the page is.
        self.assertGreater(self.pixels_of(image, COLOR_BYTES), 64 * 250)

    def test_an_arrival_that_is_no_cue_is_counted_and_not_drawn(self):
        (decoded, revision, undecodable), image = self.render(b"\x01\x02\x03 not a cue")
        self.assertEqual(revision, 1)
        self.assertEqual(undecodable, 1)
        self.assertIsNone(decoded)
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
