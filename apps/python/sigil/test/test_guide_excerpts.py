"""A guide's Python code is an excerpt of the sketch it names.

Every guide under the documentation's `guides/` whose prose names a sketch
file, written as a repository path in backticks, shows that sketch in
pieces: each ```python block's lines, read without their indentation,
stand in the sketch or sketches the guide names, so a guide never teaches
code the sketch does not run. A block that is deliberately not from the
sketch opens with the comment line `# not from the sketch` and is skipped.
"""

import re
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[4]
GUIDES = REPOSITORY / "apps/spell-circle-canvas/docs/guides"
SKETCH_PATH = re.compile(r"`(apps/grimoire/sketches/[\w/.-]+\.py)`")
PYTHON_BLOCK = re.compile(r"```python\n(.*?)```", re.S)
NOT_FROM_THE_SKETCH = "# not from the sketch"


class GuideExcerpts(unittest.TestCase):
    def test_every_python_block_is_lines_of_the_sketch_its_guide_names(self):
        checked = 0
        for guide in sorted(GUIDES.glob("*.md")):
            text = guide.read_text()
            named = sorted(set(SKETCH_PATH.findall(text)))
            if not named:
                continue
            lines = set()
            for path in named:
                sketch = REPOSITORY / path
                self.assertTrue(sketch.is_file(), f"{guide.name} names {path}, which is gone")
                lines |= {line.strip() for line in sketch.read_text().splitlines()}
            for number, block in enumerate(PYTHON_BLOCK.findall(text), start=1):
                body = [line.strip() for line in block.splitlines() if line.strip()]
                if not body or body[0] == NOT_FROM_THE_SKETCH:
                    continue
                checked += 1
                missing = [line for line in body if line not in lines]
                with self.subTest(guide=guide.name, block=number):
                    self.assertEqual(
                        missing, [], f"python block {number} of {guide.name} is not in {named}"
                    )
        self.assertGreater(checked, 0, "no guide names a sketch")


if __name__ == "__main__":
    unittest.main()
