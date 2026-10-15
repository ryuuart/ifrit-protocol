"""The wardnet_live sketch reads each kind of message through either door and shows it.

The sketch's own files are copied beside a probe that subclasses it,
with the show's recording replaced by the silent one, so nothing arrives
but what the probe delivers. The probe delivers one message of each kind
through the inlets of the two doors, the way a transport delivers them,
and the test reads back the model the handlers wrote, the keyed nodes
the page laid out for it, and the meter's bar in the still.
"""

import builtins
import json
import shutil
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import media
from sigil.sketch import renderFile

SKETCH = Path(__file__).resolve().parents[3] / "grimoire" / "sketches" / "wardnet_live"
SETTLED = 1.5  # seconds: every entrance has played and the meter has eased
ACCENT_BYTES = (0x59, 0xCB, 0xE3)  # the meter's ink below the breach line

# ONE MESSAGE OF EACH KIND, and the door each is delivered at. A message
# no handler names is delivered too, and is counted rather than drawn.
ARRIVALS = [
    ("websocket", {"kind": "row", "tag": "WARN", "body": "probe row on the websocket"}),
    ("websocket", {"kind": "alert", "text": "PROBE ALERT", "level": "breach"}),
    ("udp", {"kind": "meter", "load": 0.7}),
    (
        "udp",
        {
            "kind": "card",
            "title": "PROBE CARD",
            "value": "7 / 9",
            "note": "a probe's note",
        },
    ),
    ("udp", {"kind": "row", "tag": "INFO", "body": "probe row over udp"}),
    ("udp", {"kind": "weather", "cloud": 0.4}),
]

PROBE = """
import builtins
import json
from pathlib import Path

from sigil.io.testing import inletOf
from sigil.sketch import sketch

from . import wardnet_live as console

HERE = Path(__file__).resolve().parent


@sketch(size=(console.WIDTH, console.HEIGHT), captureAt=0)
class Probe(console.WardNetLive):
    def setup(self, ctx):
        super().setup(ctx)
        hub = ctx.assets.hub()
        doors = {"udp": console.UDP_DOOR, "websocket": console.websocket_door(ctx)}
        for door, message in json.loads((HERE / "arrivals.json").read_text()):
            inletOf(hub.listen(doors[door])).deliver(json.dumps(message).encode(), arrivedAt=0)

    def update(self, elapsed, ctx):
        super().update(elapsed, ctx)
        keys = ("row1", "row2", "card:PROBE CARD", "alert", "meter-bar")
        bounds = {key: self.composer.bounds(key) for key in keys}
        builtins._wardnet_live = {
            "rows": self.rows,
            "alert": self.alert,
            "cards": self.cards,
            "target": self.target,
            "load": self.load.value,
            "strangers": self.strangers,
            "revisions": {name: state.revision for name, state in self.shown.items()},
            "bounds": {
                key: None if rect is None else (rect.left(), rect.top(), rect.right(), rect.bottom())
                for key, rect in bounds.items()
            },
        }
"""


class WardNetLiveDoors(unittest.TestCase):
    def setUp(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_wardnet_live", None))

    def render(self):
        """The probe's still once everything delivered has settled, and
        what the page read by then."""
        builtins.__dict__.pop("_wardnet_live", None)
        with tempfile.TemporaryDirectory(prefix="wardnet live ") as directory:
            root = Path(directory)
            shutil.copy(SKETCH / "wardnet_live.py", root)
            shutil.copytree(SKETCH / "pages", root / "pages")
            shutil.copytree(SKETCH / "data", root / "data")
            shutil.copy(
                SKETCH / "data" / "silence.feed", root / "data" / "wardnet.feed"
            )
            (root / "arrivals.json").write_text(json.dumps(ARRIVALS))
            (root / "probe.py").write_text(textwrap.dedent(PROBE))
            renderFile(root / "probe.py", root / "still.png", at=SETTLED)
            image = media.decode(media.Image, (root / "still.png").read_bytes())
            return builtins._wardnet_live, image.frameAt(0).image

    def test_every_kind_through_either_door_reaches_the_page(self):
        seen, image = self.render()

        # THE HANDLERS: each kind wrote its part of the model, whichever
        # door it came through. Rows are numbered as they are read; two
        # doors' arrivals in one frame are read door by door.
        self.assertEqual([row[0] for row in seen["rows"]], [1, 2])
        self.assertEqual(
            sorted((tag, body) for _, tag, body in seen["rows"]),
            [("INFO", "probe row over udp"), ("WARN", "probe row on the websocket")],
        )
        self.assertEqual(tuple(seen["alert"]), ("PROBE ALERT", "breach"))
        self.assertEqual(
            {title: tuple(values) for title, values in seen["cards"].items()},
            {"PROBE CARD": ("7 / 9", "a probe's note", "clear")},
        )
        self.assertEqual(seen["revisions"], {"UDP": 4, "WEBSOCKET": 2})
        self.assertEqual(seen["strangers"], 1)

        # THE METER EASED: the target is the load sent, and the live value
        # the bar follows has reached it by the settled moment.
        self.assertAlmostEqual(seen["target"], 0.7)
        self.assertAlmostEqual(seen["load"], 0.7, places=3)

        # THE PAGE LAID THEM OUT: every keyed node the messages made stands
        # inside the canvas.
        for key, rect in seen["bounds"].items():
            with self.subTest(key=key):
                self.assertIsNotNone(rect)
                left, top, right, bottom = rect
                self.assertGreater(right, left)
                self.assertGreater(bottom, top)
                self.assertGreaterEqual(left, 0)
                self.assertLessEqual(bottom, image.height())

        # THE STILL SHOWS THE LOAD: along the middle of the bar's track,
        # the meter's ink runs across seven tenths of it. The alert's glow
        # beneath lifts the ink a little, so a pixel is the ink within a
        # tolerance rather than exactly.
        left, top, right, bottom = seen["bounds"]["meter-bar"]
        inked = self.inked(
            image, ACCENT_BYTES, round(left), round(right), round((top + bottom) / 2)
        )
        self.assertAlmostEqual(inked / (right - left), 0.7, delta=0.02)

    @staticmethod
    def inked(image, color, left, right, line):
        """How many pixels from `left` to `right` on one line are `color`,
        each channel within 16 of it."""
        rgba = image.rgba()
        width = image.width()
        return sum(
            1
            for x in range(left, right)
            if all(
                abs(rgba[(line * width + x) * 4 + channel] - color[channel]) <= 16
                for channel in range(3)
            )
        )


if __name__ == "__main__":
    unittest.main()
