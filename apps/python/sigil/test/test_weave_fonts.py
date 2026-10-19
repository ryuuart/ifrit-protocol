"""A font file is a resource, and its family is a family: loaded once
through a session's hub, it is set by name with `fontFamily`, it is the
face Skia draws and the face Weave's font context answers, and it stands
in `weave.fonts` beside the installed families, marked bundled.

The catalogue's installed half asks this machine's own families."""

import json
import shutil
import tempfile
import unittest
from pathlib import Path

from sigil import media, weave
from sigil.sketch import renderFile

INSTRUMENTS = (
    Path(__file__).resolve().parents[3] / "spell-circle-canvas/src/test/assets"
)

SCENE = """import json
from sigil import compose, weave
from sigil.sketch import sketch

HOW = {how!r}
REPORT = {report!r}


@sketch(size=(320, 64), background="#000000", captureAt=0)
class Lettering:
    def setup(self, ctx):
        face = ctx.assets.hub().load(weave.Face, ctx.local("data/Face.ttf"))
        word = compose.text("Waveform").fontSize(36).ink("#ffffff")
        if HOW == "family":
            word = word.fontFamily(face.familyName())
        elif HOW == "face":
            word = word.font(weave.Type(face=face))
        elif HOW == "stated":
            word = word.fontFamily(face)
        ctx.render(compose.box(word))
        family = weave.fonts.family(face.familyName(), ctx.fonts)
        with open(REPORT, "w") as report:
            json.dump(
                {{
                    "family": face.familyName(),
                    "typeface": face.typeface().familyName(),
                    "context": ctx.fonts.familyTypeface(face.familyName()) == face,
                    "listed": weave.fonts.families(ctx.fonts)[0],
                    "bundled": family.bundled,
                    "uri": family.styles[0].uri,
                    "found": weave.fonts.find(face.familyName(), context=ctx.fonts) == face,
                    "repr": repr(family),
                }},
                report,
            )
"""


class BundledType(unittest.TestCase):
    def render(self, how):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / "lettering"
            (root / "data").mkdir(parents=True)
            shutil.copy(INSTRUMENTS / "Sans.ttf", root / "data" / "Face.ttf")
            report = Path(folder) / "report.json"
            source = root / "lettering.py"
            source.write_text(SCENE.format(how=how, report=str(report)))
            output = Path(folder) / "lettering.png"
            renderFile(source, output, at=0)
            pixels = media.load(output).frameAt(0).image.rgba()
            return pixels, json.loads(report.read_text())

    def test_a_bundled_family_is_set_by_name_in_the_held_face(self):
        named, report = self.render("family")
        faced, _ = self.render("face")
        unnamed, _ = self.render("default")
        self.assertEqual(named, faced)
        self.assertNotEqual(named, unnamed)
        # fontFamily(face) is the face itself.
        stated, _ = self.render("stated")
        self.assertEqual(stated, faced)
        self.assertEqual(report["family"], "Sigil Instrument Sans")

    def test_the_held_face_reaches_skia_weave_and_the_catalogue(self):
        _, report = self.render("family")
        self.assertEqual(report["typeface"], report["family"])
        self.assertTrue(report["context"])
        self.assertTrue(report["found"])
        self.assertEqual(report["listed"], report["family"])
        self.assertTrue(report["bundled"])
        self.assertTrue(report["uri"].startswith("sketch://"), report["uri"])
        self.assertTrue(report["uri"].endswith("/data/Face.ttf"), report["uri"])
        self.assertIn("bundled", report["repr"])


class Catalogue(unittest.TestCase):
    def test_the_installed_families_are_listed(self):
        names = weave.fonts.families()
        self.assertIn("Georgia", names)
        self.assertIsNone(weave.fonts.family("No Family Is Named This"))

    def test_a_family_reads_as_its_styles_with_their_weight_names(self):
        georgia = weave.fonts.family("Georgia")
        self.assertIsInstance(georgia, weave.fonts.Family)
        self.assertFalse(georgia.bundled)
        bold_italic = [
            style
            for style in georgia.styles
            if style.weight == 700 and style.slant == weave.FaceSlant.Italic
        ]
        self.assertEqual(len(bold_italic), 1)
        self.assertEqual(bold_italic[0].weightName, "Bold")
        self.assertTrue(bold_italic[0].name)
        self.assertIn("700 Bold", repr(bold_italic[0]))
        self.assertIn("Georgia", repr(georgia))
        face = weave.fonts.find("Georgia", bold_italic[0].faceStyle())
        self.assertEqual(face.familyName(), "Georgia")
        self.assertEqual(weave.fonts.weightName(600), "Semi Bold")

    def test_an_owned_context_adopts_a_face_by_hand(self):
        fonts = weave.FontContext()
        face = weave.Face(weave.fonts.find("Georgia").typeface())
        fonts.adopt(face, "res://Georgia.ttf")
        self.assertEqual(fonts.adoptedFaces()[0].origin, "res://Georgia.ttf")
        self.assertEqual(fonts.familyTypeface("Georgia"), face)


if __name__ == "__main__":
    unittest.main()
