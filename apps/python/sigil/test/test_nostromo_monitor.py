"""The Nostromo monitor's tube reads warped neighbours and states how far,
so a capture of it is a pure function of its scene time."""

import tempfile
import unittest
from pathlib import Path

from sigil import media
from sigil.sketch import renderFile

SKETCH = (
    Path(__file__).resolve().parents[3]
    / "grimoire"
    / "sketches"
    / "nostromo_monitor"
    / "nostromo_monitor.py"
)


class NostromoMonitor(unittest.TestCase):
    def test_two_captures_of_the_tube_are_one_picture(self):
        with tempfile.TemporaryDirectory(prefix="nostromo ") as directory:
            root = Path(directory)
            captures = []
            for name in ("first.png", "second.png"):
                renderFile(SKETCH, root / name)
                captures.append(media.load(root / name).frameAt(0).image.rgba())
        self.assertGreater(len(captures[0]), 0)
        self.assertEqual(bytes(captures[0]), bytes(captures[1]))


if __name__ == "__main__":
    unittest.main()
