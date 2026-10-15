"""Exits: a keyed element that states exit() leaves as it entered.

A scene renders two rows, the upper one fading in and stating `exit()`;
a moment later it renders the lower row alone. The still taken before
the change shows both, the one taken while the upper row leaves shows it
half faded and still in its place, and the one taken after its exit has
landed shows the lower row moved up into the place it left.
"""

import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import compose, media
from sigil.sketch import renderFile

SCENE = """
from sigil import compose, motion
from sigil.motion import ease
from sigil.sketch import sketch


def page(leaving):
    rows = []
    if leaving:
        rows.append(
            compose.box()
            .key("leave")
            .width(40)
            .height(10)
            .fill("#ff0000")
            .opacity(motion.animate(from_=0, to=1, duration=0.3, ease=ease.linear))
            .exit()
        )
    rows.append(compose.box().key("stay").width(40).height(10).fill("#00ff00"))
    return compose.box(*rows)


@sketch(size=(64, 24), background="#000000", captureAt=0)
class Scene:
    def setup(self, ctx):
        self.left = False
        ctx.render(page(True))

    def update(self, elapsed, ctx):
        if elapsed >= 0.45 and not self.left:
            self.left = True
            ctx.render(page(False))
"""


class Exits(unittest.TestCase):
    def still(self, at):
        with tempfile.TemporaryDirectory(prefix="sigil_exit_") as folder:
            entry, output = Path(folder) / "scene.py", Path(folder) / "scene.png"
            entry.write_text(textwrap.dedent(SCENE))
            renderFile(entry, output, at=at)
            image = media.load(output).frameAt(0).image
            pixels, width = image.rgba(), image.width()
        return lambda x, y: tuple(pixels[(y * width + x) * 4 : (y * width + x) * 4 + 4])

    def test_the_word_is_on_every_node_and_hands_the_node_back(self):
        row = compose.box().key("row")
        self.assertIs(row.exit(), row)
        self.assertIs(row.exit(False), row)
        leaf = compose.text("leaving")
        self.assertIs(leaf.exit(enabled=True), leaf)

    def test_a_leaving_row_keeps_its_place_and_plays_its_entrance_backwards(self):
        before = self.still(0.4)
        self.assertEqual(before(20, 5), (255, 0, 0, 255))
        self.assertEqual(before(20, 15), (0, 255, 0, 255))

        leaving = self.still(0.6)
        red = leaving(20, 5)[0]
        self.assertGreater(red, 60)
        self.assertLess(red, 200)
        # The lower row has not moved up into the place being left.
        self.assertEqual(leaving(20, 15), (0, 255, 0, 255))

        after = self.still(1.0)
        self.assertEqual(after(20, 5), (0, 255, 0, 255))
        self.assertEqual(after(20, 15), (0, 0, 0, 255))


if __name__ == "__main__":
    unittest.main()
