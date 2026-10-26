"""A font file is a resource, and its family is a family: loaded once
through a session's hub, it is set by name with `fontFamily`, it is the
face Skia draws and the face Weave's font context answers, and it stands
in `weave.fonts` beside the installed families, marked bundled, for as
long as the session that loaded it stands.

The catalogue's installed half asks this machine's own families, and
finds a face under the font's own name for its style, which is what a
prompt reads a family's styles by and sets text in."""

import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

from sigil import compose, live, media, weave
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


WINDOW_SKETCH = """
import json
from sigil import compose, weave
from sigil.sketch import sketch

REPORT = {report!r}
FAMILY = {family!r}


@sketch(size=(64, 32), background="#000000")
class {name}:
    def setup(self, ctx):
        family = FAMILY
        if not family:
            family = ctx.assets.hub().load(weave.Face, "res://Face.ttf").familyName()
        listed = weave.fonts.family(family, ctx.fonts)
        with open(REPORT, "w") as report:
            json.dump(
                {{
                    "family": family,
                    "found": ctx.fonts.familyTypeface(family) is not None,
                    "bundled": listed is not None and listed.bundled,
                }},
                report,
            )
        ctx.render(compose.box())
"""


def window_sketch(folder, name, family=""):
    """A ``@sketch`` class in a module of its own, and the file it reports
    to: with no ``family`` it loads the bundled face and reports that face's
    family, and with one it loads nothing and reports whether its session's
    font context answers that family."""
    report = Path(folder) / f"{name}.json"
    path = Path(folder) / f"window_fonts_{name.lower()}.py"
    path.write_text(WINDOW_SKETCH.format(report=str(report), family=family, name=name))
    specification = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(specification)
    sys.modules[path.stem] = module
    specification.loader.exec_module(module)
    return module.__sigil_sketch__, report


class WindowAdoptions(unittest.TestCase):
    """The windows of one interpreter share one font context, and a family
    a window's sketch loaded leaves it when that window closes."""

    def test_a_closed_windows_family_is_not_found_by_the_window_after_it(self):
        with tempfile.TemporaryDirectory() as folder:
            shutil.copy(INSTRUMENTS / "Sans.ttf", Path(folder) / "Face.ttf")
            loader, loaded = window_sketch(folder, "Loader")
            first = live.open(size=(64, 32), headless=True, assets=folder)
            try:
                first.show(loader)
                first.pump()
            finally:
                first.close()
            while_open = json.loads(loaded.read_text())
            probe, probed = window_sketch(folder, "Probe", while_open["family"])
            second = live.open(size=(64, 32), headless=True, assets=folder)
            try:
                second.show(probe)
                second.pump()
            finally:
                second.close()
            after_close = json.loads(probed.read_text())
        self.assertTrue(while_open["found"])
        self.assertTrue(while_open["bundled"])
        self.assertFalse(
            after_close["found"], "a closed window's family answered the next window"
        )
        self.assertFalse(after_close["bundled"])


def georgia_bold_italic_name():
    georgia = weave.fonts.family("Georgia")
    return next(
        style.name
        for style in georgia.styles
        if style.weight == 700 and style.slant == weave.FaceSlant.Italic
    )


class StyleByName(unittest.TestCase):
    def test_a_family_answers_a_style_by_the_fonts_own_name(self):
        georgia = weave.fonts.family("Georgia")
        named = georgia_bold_italic_name()
        style = georgia.style(named)
        self.assertEqual(style.weight, 700)
        self.assertEqual(style.slant, weave.FaceSlant.Italic)
        # Case and spaces aside.
        self.assertEqual(georgia.style(named.replace(" ", "").lower()), style)
        self.assertIsNone(georgia.style("No Style Is Named This"))

    def test_find_answers_the_face_under_the_style_name(self):
        named = georgia_bold_italic_name()
        face = weave.fonts.find("Georgia", named)
        self.assertEqual(face.familyName(), "Georgia")
        self.assertEqual(face.style().weight, 700)
        self.assertEqual(face.style().slant, weave.FaceSlant.Italic)
        self.assertEqual(weave.fonts.find("georgia", style=named.upper()), face)
        self.assertIsNone(weave.fonts.find("Georgia", "No Style Is Named This"))
        self.assertIsNone(weave.fonts.find("No Family Is Named This", named))

    def test_an_adopted_family_answers_its_style_names_alone(self):
        fonts = weave.FontContext()
        # Before anything is adopted, the context's manager answers.
        self.assertEqual(
            weave.fonts.find("Georgia", "Bold", fonts).style().weight, 700
        )
        held = weave.Face(weave.fonts.find("Georgia", "Regular").typeface())
        fonts.adopt(held, "res://Georgia.ttf")
        self.assertEqual(weave.fonts.find("Georgia", "regular", fonts), held)
        # The adopted family stands in front of the installed one, which
        # still has a bold the adopted one lacks.
        self.assertIsNone(weave.fonts.find("Georgia", "Bold", fonts))
        regular = weave.fonts.family("Georgia", fonts).style("Regular")
        self.assertEqual(regular.uri, "res://Georgia.ttf")

    def test_a_face_found_by_its_style_name_sets_text_at_a_prompt(self):
        named = weave.fonts.find("Georgia", georgia_bold_italic_name())
        styled = weave.fonts.find(
            "Georgia", weave.FaceStyle(weight=700, slant=weave.FaceSlant.Italic)
        )
        regular = weave.fonts.find("Georgia", "Regular")
        window = live.open(size=(320, 64), headless=True)
        with tempfile.TemporaryDirectory() as folder:

            def still(face, name):
                window.show(
                    compose.text("Waveform")
                    .fontFamily(face)
                    .fontSize(36)
                    .ink("#ffffff")
                )
                for _ in range(2):
                    window.pump()
                path = window.still(Path(folder) / name)
                return media.load(path).frameAt(0).image.rgba()

            try:
                by_name = still(named, "named.png")
                by_style = still(styled, "styled.png")
                upright = still(regular, "regular.png")
            finally:
                window.close()
        self.assertEqual(by_name, by_style)
        self.assertNotEqual(by_name, upright)

    def test_a_style_named_beside_the_family_is_set_when_the_cascade_runs(self):
        named = georgia_bold_italic_name()
        window = live.open(size=(320, 64), headless=True)
        with tempfile.TemporaryDirectory() as folder:

            def still(words, name):
                window.show(words.fontSize(36).ink("#ffffff"))
                for _ in range(2):
                    window.pump()
                path = window.still(Path(folder) / name)
                return media.load(path).frameAt(0).image.rgba()

            try:
                by_name = still(
                    compose.text("Waveform").fontFamily("Georgia", style=named),
                    "named.png",
                )
                held = still(
                    compose.text("Waveform").fontFamily(
                        weave.fonts.find("Georgia", named)
                    ),
                    "held.png",
                )
                # A weight written after the named style overrides its
                # traits, and the family is asked again at it.
                overridden = still(
                    compose.text("Waveform")
                    .fontFamily("Georgia", style=named)
                    .fontWeight(400),
                    "overridden.png",
                )
                italic = still(
                    compose.text("Waveform")
                    .fontFamily("Georgia")
                    .fontStyle(compose.FontStyle.Italic),
                    "italic.png",
                )
            finally:
                window.close()
        self.assertEqual(by_name, held)
        self.assertNotEqual(by_name, overridden)
        self.assertEqual(overridden, italic)

    def test_fontFamily_takes_the_style_by_keyword_alone(self):
        words = compose.text("Waveform")
        self.assertIs(words.fontFamily("Georgia", style="Bold"), words)
        with self.assertRaises(TypeError):
            compose.text("Waveform").fontFamily("Georgia", "Bold")


if __name__ == "__main__":
    unittest.main()
