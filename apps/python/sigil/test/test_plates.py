"""Two directories of plates compare as rows a script reads, not as text."""

import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil.sketch import PlateOutcome, PlateSide, compare, render_file

SCENE = textwrap.dedent("""
    from sigil.sketch import sketch

    @sketch(size=(48, 32), background="#101418", capture_at=0.5)
    class Scene:
        def draw(self, pen):
            pen.noStroke()
            pen.fill(220, 120, 40)
            pen.rect(pen.millis() / 50, 8, 12, 16)
""")


class PlateComparisons(unittest.TestCase):
    def test_rows_carry_the_distances_and_what_could_not_be_measured(self):
        with tempfile.TemporaryDirectory(prefix="sigil plates ") as directory:
            root = Path(directory)
            source = root / "scene.py"
            source.write_text(SCENE)
            first, second, moved = root / "first", root / "second", root / "moved"
            for plates in (first, second, moved):
                plates.mkdir()
            render_file(source, first / "plate_scene.png")
            render_file(source, second / "plate_scene.png")
            render_file(source, moved / "plate_scene.png", at=0.0)
            render_file(source, first / "plate_alone.png")

            comparison = compare(first, second)
            self.assertEqual(comparison.refusal, "")
            self.assertEqual(comparison.status(), 1)
            rows = {plate.name: plate for plate in comparison.plates}
            self.assertEqual(rows["scene"].outcome, PlateOutcome.Compared)
            self.assertEqual(rows["scene"].worst, 0)
            self.assertEqual(rows["alone"].outcome, PlateOutcome.Missing)
            self.assertEqual(rows["alone"].side, PlateSide.Second)
            self.assertEqual(str(rows["alone"]), "missing alone second")
            self.assertTrue(str(rows["scene"]).startswith("compared scene mean 0.0000"))

            absent = compare(first, root / "absent")
            self.assertEqual(absent.status(), 2)
            self.assertTrue(absent.refusal)

            (first / "plate_alone.png").unlink()
            drift = compare(first, moved)
            self.assertEqual(drift.status(), 0)
            (row,) = drift.plates
            self.assertGreater(row.worst, 0)
            self.assertEqual(row.worst, max(row.worstOverContent, row.worstOverGraze))


if __name__ == "__main__":
    unittest.main()
