"""The plates verb's selection and its raster smoke's exit rule, without a
renderer.

A fixture stands in for Grimoire: a script that answers `--catalog` with
rows a test writes and `--headless <dir> … --sketch <name>` by writing a
plate whose bytes say which sketch and which rasteriser drew it, or by
failing for a sketch the test names. The comparison is the sketch
library's and needs the built extension, so the smoke reads a fixture
comparison instead.
"""

import contextlib
import io
import json
import os
import stat
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

from sigil import plates, tree

GRIMOIRE = """\
#!{python}
import json, os, sys
arguments = sys.argv[1:]
here = os.path.dirname(os.path.abspath(__file__))
if "--catalog" in arguments:
    with open(os.path.join(here, "rows.json")) as handle:
        for row in json.load(handle):
            print(json.dumps(row))
    sys.exit(0)
directory = arguments[arguments.index("--headless") + 1]
name = arguments[arguments.index("--sketch") + 1]
with open(os.path.join(here, "failing.json")) as handle:
    if name in json.load(handle):
        print("the sketch threw during setup", file=sys.stderr)
        sys.exit(1)
rasteriser = "device" if "--gpu" in arguments else "raster"
with open(os.path.join(directory, "plate_" + name + ".png"), "w") as plate:
    plate.write(name + " on the " + rasteriser)
"""


def row(filed_name, stem, kind="scene2d", available=True, reason=""):
    return {
        "filedName": filed_name,
        "key": stem,
        "kind": kind,
        "available": available,
        "reason": reason,
    }


# A Python sketch is filed under the NAME it states, which differs from
# its file stem; a compiled one is filed under its stem.
ROWS = [
    row("cue_desk_live", "cue_deck"),
    row("astral_tome", "astral_tome"),
    row("first_light", "first_light", kind="scene3d"),
]


class Fixture:
    """A temporary build tree with the fixture Grimoire in it."""

    def __init__(self, rows, failing=()):
        self.work = tempfile.TemporaryDirectory(prefix="sigil-plates-")
        self.root = Path(self.work.name)
        self.binary = self.root / "Grimoire"
        self.binary.write_text(GRIMOIRE.format(python=sys.executable))
        self.binary.chmod(self.binary.stat().st_mode | stat.S_IXUSR)
        (self.root / "rows.json").write_text(json.dumps(rows))
        (self.root / "failing.json").write_text(json.dumps(list(failing)))

    def run(self, *arguments, comparison=None):
        """The verb's exit status and what it printed, with the build tree,
        the binary and, when given, the comparison stood in for."""
        patches = [
            mock.patch.object(tree, "build_dir", lambda preset="main": self.root),
            mock.patch.object(tree, "grimoire", lambda *a, **k: self.binary),
        ]
        if comparison is not None:
            patches.append(mock.patch.object(plates, "compared", comparison))
        printed = io.StringIO()
        with contextlib.ExitStack() as stack:
            for patch in patches:
                stack.enter_context(patch)
            stack.enter_context(contextlib.redirect_stdout(printed))
            status = plates.main(["--jobs", "2", *arguments])
        return status, printed.getvalue()

    def manifest(self):
        return plates.baseline.read_manifest(
            self.root / plates.DEVICE_MANIFEST.format(config="Release")
        )

    def close(self):
        self.work.cleanup()


def distance(mean, p99, worst):
    return types.SimpleNamespace(mean=mean, p99=p99, worst=worst)


class SelectionAnswersToTheStemAndTheFiledName(unittest.TestCase):
    def setUp(self):
        self.fixture = Fixture(ROWS)

    def tearDown(self):
        self.fixture.close()

    def test_resolve_takes_either_spelling(self):
        listing = plates.catalog_listing(
            "\n".join(json.dumps(r) for r in ROWS), plates.KINDS
        )
        chosen, unknown = plates.resolve(
            ["cue_deck", "astral_tome", "cue_desk_live", "nothing"], listing
        )
        self.assertEqual(chosen, ["cue_desk_live", "astral_tome", "cue_desk_live"])
        self.assertEqual(unknown, ["nothing"])

    def test_the_verb_selects_a_python_sketch_by_its_stem(self):
        status, printed = self.fixture.run(
            "--tier", "device", "--sketch", "cue_deck", "--rebase"
        )
        self.assertEqual(status, 0, printed)
        self.assertEqual(list(self.fixture.manifest()), ["cue_desk_live"])

    def test_the_verb_selects_by_the_filed_name(self):
        status, printed = self.fixture.run(
            "--tier", "device", "--scenes", "cue_desk_live", "first_light", "--rebase"
        )
        self.assertEqual(status, 0, printed)
        self.assertEqual(
            sorted(self.fixture.manifest()), ["cue_desk_live", "first_light"]
        )

    def test_a_name_that_selects_nothing_is_refused(self):
        with self.assertRaises(SystemExit) as refusal:
            self.fixture.run("--tier", "device", "--sketch", "cue")
        self.assertIn("not in the registry: cue", str(refusal.exception))


class TheDeviceTierIsTheReference(unittest.TestCase):
    def setUp(self):
        self.fixture = Fixture(ROWS)

    def tearDown(self):
        self.fixture.close()

    def test_every_kind_joins_the_device_manifest_and_a_second_sweep_is_identical(self):
        status, printed = self.fixture.run(
            "--tier", "device", "--kind", "scene2d", "--rebase"
        )
        self.assertEqual(status, 0, printed)
        status, printed = self.fixture.run(
            "--tier", "device", "--kind", "scene3d", "--rebase"
        )
        self.assertEqual(status, 0, printed)
        self.assertEqual(
            sorted(self.fixture.manifest()),
            ["astral_tome", "cue_desk_live", "first_light"],
        )
        status, printed = self.fixture.run("--tier", "device", "--kind", "scene2d")
        self.assertEqual(status, 0, printed)
        self.assertIn("2 byte-identical, 0 with a moved hash", printed)

    def test_a_moved_device_plate_fails_with_the_finding_marker(self):
        self.fixture.run("--tier", "device", "--sketch", "astral_tome", "--rebase")
        manifest = self.fixture.root / plates.DEVICE_MANIFEST.format(config="Release")
        manifest.write_text("0" * 64 + "  astral_tome\n")
        status, printed = self.fixture.run(
            "--tier", "device", "--sketch", "astral_tome"
        )
        self.assertEqual(status, 1)
        self.assertIn("MOVED  astral_tome", printed)
        self.assertIn("<-- FINDING", printed)

    def test_a_narrowed_sweep_keeps_the_plates_it_did_not_render(self):
        self.fixture.run("--tier", "device", "--rebase")
        status, printed = self.fixture.run(
            "--tier", "device", "--sketch", "astral_tome"
        )
        self.assertEqual(status, 0, printed)
        status, printed = self.fixture.run(
            "--tier", "device", "--scenes", "cue_desk_live", "first_light"
        )
        self.assertEqual(status, 0, printed)
        judged = self.fixture.root / "plates_Release" / "device" / "gpu"
        self.assertEqual(
            sorted(os.listdir(judged)),
            ["plate_astral_tome.png", "plate_cue_desk_live.png", "plate_first_light.png"],
        )

    def test_a_narrowed_sweep_leaves_no_earlier_plate_under_a_scene_that_failed(self):
        self.fixture.run("--tier", "device", "--rebase")
        self.fixture.run("--tier", "device", "--sketch", "astral_tome")
        (self.fixture.root / "failing.json").write_text(json.dumps(["astral_tome"]))
        status, printed = self.fixture.run(
            "--tier", "device", "--sketch", "astral_tome"
        )
        self.assertEqual(status, 1, printed)
        judged = self.fixture.root / "plates_Release" / "device" / "gpu"
        self.assertEqual(os.listdir(judged), [])

    def test_an_unnarrowed_sweep_holds_only_what_it_rendered(self):
        self.fixture.run("--tier", "device", "--rebase")
        judged = self.fixture.root / "plates_Release" / "device" / "gpu"
        judged.mkdir(parents=True, exist_ok=True)
        (judged / "plate_retired_sketch.png").write_text("an earlier run")
        status, printed = self.fixture.run("--tier", "device")
        self.assertEqual(status, 0, printed)
        self.assertNotIn("plate_retired_sketch.png", os.listdir(judged))

    def test_the_cpu_tier_refuses_to_rebase(self):
        with self.assertRaises(SystemExit):
            self.fixture.run("--tier", "cpu", "--rebase")


class TheRasterSmokeFailsOnlyForARenderThatFailed(unittest.TestCase):
    def comparison(self, rows):
        """A comparison that answers @p rows for every plate in the second
        directory, the device plates being the reference."""

        def compared(first, second):
            self.assertTrue(first.endswith(os.path.join("device", "baseline")))
            names = [
                name.removeprefix(plates.PLATE_PREFIX).removesuffix(".png")
                for name in os.listdir(second)
            ]
            return {name: rows[name] for name in names if name in rows}, {}, ""

        return compared

    def test_drift_past_the_tolerance_is_reported_and_passes(self):
        fixture = Fixture(ROWS)
        self.addCleanup(fixture.close)
        rows = {
            "cue_desk_live": distance(40.0, 255, 255),
            "astral_tome": distance(0.5, 3, 60),
        }
        status, printed = fixture.run("--tier", "cpu", comparison=self.comparison(rows))
        self.assertEqual(status, 0, printed)
        self.assertIn("OVER  cue_desk_live", printed)
        self.assertIn("WITHIN astral_tome", printed)
        self.assertIn("PASSED OVER 1 scene3d sketches", printed)
        self.assertNotIn("FINDING", printed)

    def test_a_sketch_that_fails_to_render_fails_the_run(self):
        fixture = Fixture(ROWS, failing=["astral_tome"])
        self.addCleanup(fixture.close)
        rows = {"cue_desk_live": distance(0.0, 0, 0)}
        status, printed = fixture.run("--tier", "cpu", comparison=self.comparison(rows))
        self.assertEqual(status, 1, printed)
        self.assertIn("RENDER FAILED  astral_tome", printed)
        self.assertIn("1 failed to render", printed)

    def test_no_device_plate_to_read_against_passes(self):
        with contextlib.redirect_stdout(io.StringIO()):
            status = plates.raster_smoke_report(
                ["astral_tome"], {}, {}, {}, "neither directory holds a plate"
            )
        self.assertEqual(status, 0)


if __name__ == "__main__":
    unittest.main()
