"""The bloom specimen shows the filter a native text leaf wears: a cell of
the specimen and a bare text leaf given the same parameter set paint the
same pixels, and each halo stands on its own when the other's strength is
zero."""

import importlib.util
import json
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import media
from sigil.sketch import renderFile

SKETCH = (
    Path(__file__).resolve().parents[3]
    / "grimoire"
    / "sketches"
    / "python_bloom_specimen.py"
)

# The specimen read in this process for its configurations and constants.
_specification = importlib.util.spec_from_file_location("bloom_specimen", SKETCH)
specimen = importlib.util.module_from_spec(_specification)
sys.modules[_specification.name] = specimen
_specification.loader.exec_module(specimen)

COMPARED = "fine.Even"
NEAR_ALONE = "halo.Near halo alone"
BROAD_ALONE = "halo.Broad halo alone"
CONFIGURATIONS = {
    COMPARED: specimen.FINE_TYPE["Even"],
    NEAR_ALONE: specimen.HALOS["Near halo alone"],
    BROAD_ALONE: specimen.HALOS["Broad halo alone"],
}

LOAD_SPECIMEN = """
import importlib.util
import json
import sys
from pathlib import Path

_specification = importlib.util.spec_from_file_location("bloom_specimen_probed", SKETCH)
specimen = importlib.util.module_from_spec(_specification)
sys.modules[_specification.name] = specimen
_specification.loader.exec_module(specimen)
"""

# The specimen's own page, reporting where each probed subject was laid out.
PROBE = LOAD_SPECIMEN + """
from sigil.sketch import sketch


@sketch(size=specimen.SIZE, captureAt=0.0, theme=specimen.THEME)
class Probe:
    def setup(self):
        return specimen.page()

    def update(self, elapsed, ctx):
        found = {}
        for key in KEYS:
            rect = ctx.composer.bounds(key)
            if rect is not None:
                found[key] = [rect.left(), rect.top(), rect.width(), rect.height()]
        Path(__file__).with_suffix(".json").write_text(json.dumps(found))
"""

# Bare text leaves on the well's ground, each written out by hand with the
# configuration it is given, at the place the specimen laid its cell out.
LEAVES = LOAD_SPECIMEN + """
from sigil.compose import box, text
from sigil.material import BloomOptions, Filter
from sigil.sketch import sketch


def leaf(configuration, left, top):
    optics = Filter() if configuration is None else Filter.bloom(BloomOptions(**configuration))
    return (
        text(specimen.FINE_WORDS, size=specimen.FINE_SIZE, color=specimen.LUMINOUS_INK)
        .fontFamily(specimen.MONOSPACE)
        .letterSpacing(specimen.FINE_TRACKING)
        .filter(optics)
        .absolute()
        .left(left)
        .top(top)
    )


@sketch(size=specimen.SIZE, background=specimen.WELL_GROUND, captureAt=0.0)
class Leaves:
    def setup(self):
        width, height = specimen.SIZE
        return box(
            *[leaf(configuration, left, top) for configuration, left, top in PLACED]
        ).width(width).height(height)
"""

# How far past a subject's laid-out box its halo is compared: inside the
# well that holds it, which is wider and taller than the line it centres.
REACH_ACROSS = 16
REACH_DOWN = 40


class BloomSpecimen(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory(prefix="bloom specimen ")
        root = Path(cls.folder.name)
        cls.width = int(specimen.SIZE[0])
        cls.page = cls.render(root / "specimen.png", source=SKETCH)
        cls.bounds = cls.probe(root)
        placed = [
            (CONFIGURATIONS[key], cls.bounds[key][0], cls.bounds[key][1])
            for key in CONFIGURATIONS
        ]
        cls.leaves = cls.render(root / "leaves.png", LEAVES, PLACED=placed)
        bare = [(None, left, top) for _, left, top in placed]
        cls.bare = cls.render(root / "bare.png", LEAVES, PLACED=bare)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    @classmethod
    def render(cls, output, body=None, source=None, **values):
        if source is None:
            source = output.with_suffix(".py")
            preamble = f"SKETCH = {str(SKETCH)!r}\n"
            preamble += "".join(f"{name} = {value!r}\n" for name, value in values.items())
            source.write_text(preamble + textwrap.dedent(body))
        renderFile(source, output, at=0)
        frame = media.load(output).frameAt(0).image
        return frame.rgba()

    @classmethod
    def probe(cls, root):
        source = root / "probe.py"
        source.write_text(
            f"SKETCH = {str(SKETCH)!r}\nKEYS = {list(CONFIGURATIONS)!r}\n"
            + textwrap.dedent(PROBE)
        )
        probed = cls.render(root / "probe.png", source=source)
        cls.probed = probed
        return json.loads(source.with_suffix(".json").read_text())

    def region(self, key, across=REACH_ACROSS, down=REACH_DOWN):
        left, top, width, height = self.bounds[key]
        return (
            int(left) - across,
            int(top) - down,
            int(left + width) + 1 + across,
            int(top + height) + 1 + down,
        )

    def channels(self, pixels, region):
        left, top, right, bottom = region
        for y in range(top, bottom):
            row = (y * self.width + left) * 4
            yield from pixels[row : row + (right - left) * 4]

    def lit(self, pixels, region, beyond=4):
        """How many channels in the region stand more than `beyond` code
        values above the same channel without the filter."""
        return sum(
            1
            for filtered, bare in zip(
                self.channels(pixels, region), self.channels(self.bare, region)
            )
            if filtered > bare + beyond
        )

    def test_the_probe_paints_the_specimen(self):
        self.assertEqual(set(self.bounds), set(CONFIGURATIONS))
        self.assertEqual(self.page, self.probed)

    def test_a_cell_and_a_native_text_leaf_with_one_configuration_match(self):
        region = self.region(COMPARED)
        differences = [
            abs(cell - leaf)
            for cell, leaf in zip(
                self.channels(self.page, region), self.channels(self.leaves, region)
            )
        ]
        self.assertLessEqual(max(differences), 1)
        # The configuration lights the region, so the match is of light.
        self.assertGreater(self.lit(self.page, region), 500)

    def test_each_halo_stands_alone_when_the_other_strength_is_zero(self):
        for key in (NEAR_ALONE, BROAD_ALONE):
            with self.subTest(key=key):
                self.assertGreater(self.lit(self.page, self.region(key)), 500)

    def test_only_the_broad_halo_reaches_past_the_near_one(self):
        # A band above the line, clear of the near Gaussian's three sigma
        # and of the dilation, inside the broad one's reach.
        def band(key):
            left, top, width, _ = self.bounds[key]
            return (int(left), int(top) - 30, int(left + width), int(top) - 12)

        self.assertEqual(self.lit(self.page, band(NEAR_ALONE), beyond=1), 0)
        self.assertGreater(self.lit(self.page, band(BROAD_ALONE), beyond=1), 200)


if __name__ == "__main__":
    unittest.main()
