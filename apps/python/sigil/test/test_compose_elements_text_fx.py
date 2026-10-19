"""The text effect family: effects a track runs, and the verbs that run them."""

import shutil
import tempfile
import unittest
from pathlib import Path

from sigil import compose, media
from sigil.compose import textFx
from sigil.sketch import renderFile

INSTRUMENTS = (
    Path(__file__).resolve().parents[3] / "spell-circle-canvas/src/test/assets"
)

# Every letter of the instrument sans stands on one advance and every letter
# of one case on one outline, so a scramble of capitals through small letters
# substitutes every letter it churns and shows each substitution.
HEAD = """from sigil import compose, motion, skia, weave
from sigil.compose import box, text, textFx
from sigil.compose.textFx import GlyphModifier
from sigil.sketch import sketch
from sigil.weave import Type
"""


class TextEffects(unittest.TestCase):
    def render(self, body, size=(200, 60)):
        """The pixels of a sketch whose `setup` renders `body`, an
        expression that may name `sans` and `variable`, the instrument
        faces, set at 24 pixels in white."""
        width, height = size
        source = (
            HEAD
            + f"""@sketch(size=({width}, {height}), background="#000000")
class Scene:
    def setup(self, ctx):
        hub = ctx.assets.hub()
        sans = Type(size=24, color="#ffffff",
                    face=hub.load(skia.Typeface, ctx.local("data/Sans.ttf")))
        variable = Type(size=24, color="#ffffff",
                        face=hub.load(skia.Typeface, ctx.local("data/Variable.ttf")))
        ctx.render({body})
"""
        )
        with tempfile.TemporaryDirectory() as folder:
            data = Path(folder) / "data"
            data.mkdir()
            for face in ("Sans.ttf", "Variable.ttf"):
                shutil.copy(INSTRUMENTS / face, data / face)
            path = Path(folder) / "effects.py"
            output = Path(folder) / "effects.png"
            path.write_text(source)
            renderFile(path, output, at=0)
            return media.load(output).frameAt(0).image.rgba()

    @staticmethod
    def count(pixels, matches):
        """How many pixels `matches` answers yes for, given (r, g, b)."""
        return sum(
            1
            for index in range(0, len(pixels), 4)
            if matches(pixels[index], pixels[index + 1], pixels[index + 2])
        )

    def test_an_effect_is_a_value_that_compares_by_what_built_it(self):
        self.assertEqual(textFx.scramble("01", 6), textFx.scramble("01", 6))
        self.assertNotEqual(textFx.scramble("01", 6), textFx.scramble("01", 7))
        # A scramble substitutes in place; a wave moves its glyphs.
        self.assertFalse(textFx.scramble().displaces())
        self.assertTrue(textFx.waveLoop().displaces())
        stop = textFx.GlyphModifier(colorMultiplier=compose.Fill.currentInk())
        self.assertEqual(stop.colorMultiplier, compose.Fill.currentInk())
        stop.colorAdd = compose.Fill.var("flash")
        self.assertEqual(stop.colorAdd, compose.Fill.var("flash"))
        with self.assertRaises(TypeError):
            stop.colorScreen = compose.Fill.none()
        table = textFx.tween(
            from_=textFx.GlyphModifier(alpha=0),
            keyframes=[
                textFx.GlyphModifierKeyframe(stop, duration=0.3),
                textFx.GlyphModifier(),
            ],
        )
        self.assertEqual(
            table,
            textFx.tween(
                from_=textFx.GlyphModifier(alpha=0),
                keyframes=[
                    textFx.GlyphModifierKeyframe(stop, duration=0.3),
                    textFx.GlyphModifier(),
                ],
            ),
        )
        phase = textFx.scramble().until(0.4).crossfade(0.1)
        self.assertAlmostEqual(phase.endsAt, 0.4, places=6)
        self.assertAlmostEqual(phase.overlap, 0.1, places=6)
        track = compose.Track(effect=textFx.hold(textFx.scramble()))
        self.assertEqual(track.effect, textFx.hold(textFx.scramble()))
        leaf = compose.text("HOLD")
        self.assertIs(leaf.textFx(textFx.scramble(), progress=0.5), leaf)

    def test_a_scramble_track_resolves_its_letters_over_its_progress(self):
        def scrambled(progress):
            return self.render(
                'text("MMMMMM").font(sans).textFx('
                f'textFx.scramble("abcdefghijkl", 6), progress={progress})'
            )

        # A run under a track stands on whole pixels, so the letters it
        # lands on are the ones a track that changes nothing draws.
        plain = self.render('text("MMMMMM").font(sans).textFx(textFx.tween())')
        self.assertNotEqual(scrambled(0), plain, "no letter churned at the start")
        self.assertEqual(scrambled(1), plain, "a letter had not landed by the end")

    def test_a_table_naming_the_ink_in_force_follows_the_sheet(self):
        def page(classes):
            return self.render(
                """box().applyStyleSheet(compose.StyleSheet([
            compose.rule(".red").ink("#ff0000"),
            compose.rule(".blue").ink("#0000ff")]))
            .children(box().styleClass("%s").padding(10).children(
                text("IIII", weave.textStyle(sans)).textFx(textFx.tween(
                    keyframes=[GlyphModifier(colorMultiplier=compose.Fill.currentInk())]))))"""
                % classes
            )

        def red(r, g, b):
            return r > 200 and g < 40 and b < 40

        def blue(r, g, b):
            return b > 200 and r < 40 and g < 40

        # The letters are set white by their own style, not by the cascade's
        # ink, and multiplied toward the ink the sheet sets, which the table
        # names rather than states.
        reddened = page("red")
        self.assertGreater(self.count(reddened, red), 30)
        blued = page("blue")
        self.assertGreater(self.count(blued, blue), 30)
        self.assertEqual(self.count(blued, red), 0)

    def test_a_sequence_runs_its_effects_in_order(self):
        def at(progress):
            # One letter on a schedule with no stagger: its local time is
            # the master progress.
            pixels = self.render(
                """text("I").font(sans).textFx(textFx.sequence(
                textFx.tween(from_=GlyphModifier(colorMultiplier="#ff0000"),
                             to=GlyphModifier(colorMultiplier="#ff0000")).until(0.5),
                textFx.tween(from_=GlyphModifier(colorMultiplier="#0000ff"),
                             to=GlyphModifier(colorMultiplier="#0000ff"))),
                tween=motion.Tween(duration=1), progress=%s)"""
                % progress
            )
            return (
                self.count(pixels, lambda r, g, b: r > 200 and b < 40),
                self.count(pixels, lambda r, g, b: b > 200 and r < 40),
            )

        early_red, early_blue = at(0.25)
        late_red, late_blue = at(0.75)
        self.assertGreater(early_red, 10)
        self.assertEqual(early_blue, 0)
        self.assertGreater(late_blue, 10)
        self.assertEqual(late_red, 0)

    def test_a_variable_axis_effect_drives_the_axis(self):
        def inked(pixels):
            return self.count(pixels, lambda r, g, b: r > 128)

        rest = inked(self.render('text("IIII").font(variable)'))
        graded = self.render(
            'text("IIII").font(variable).textFx(textFx.variableAxis("GRAD", 150))'
        )
        self.assertGreater(inked(graded), rest, "the grade moved no ink")
        # A drive held at the same coordinate is the same picture.
        self.assertEqual(
            self.render('text("IIII").font(variable).variationDrive("GRAD", 150)'),
            graded,
        )

    def test_a_wave_loop_comes_back_round(self):
        def at(progress):
            return self.render(
                'text("WAVES").font(sans).textFx(textFx.waveLoop(0.15), '
                f"tween=motion.Tween(), progress={progress})"
            )

        self.assertEqual(at(0), at(1))
        self.assertNotEqual(at(0), at(0.25))

    def test_enter_as_an_effect_is_the_entrance_verbs_picture(self):
        self.assertEqual(
            textFx.entrance(textFx.rise(20)).effect, textFx.enter(textFx.rise(20))
        )
        # A stock entrance carries the track's own timing, so the effect on
        # a default track runs the same schedule as the verb.
        verb = self.render(
            'text("RISE").font(sans).textFx(textFx.entrance(textFx.rise(20), '
            "compose.Track(progress=0.3)))"
        )
        effect = self.render(
            'text("RISE").font(sans).textFx(textFx.enter(textFx.rise(20)), progress=0.3)'
        )
        self.assertEqual(verb, effect)
        self.assertNotEqual(effect, self.render('text("RISE").font(sans)'))

    def test_an_attached_mark_stands_on_its_unit(self):
        pixels = self.render(
            'text("AAAA BBBB").font(sans).textAttach(weave.selectors.words(1, 2), '
            'box().width(compose.pct(100)).height(compose.pct(100)).fill("#ff0000"))'
        )

        def red_in(left, right):
            return any(
                pixels[(y * 200 + x) * 4] > 200 and pixels[(y * 200 + x) * 4 + 1] < 40
                for y in range(60)
                for x in range(left, right)
            )

        # Four letters of 14.4 pixels and a space of 7.2 open the line.
        self.assertFalse(red_in(0, 50))
        self.assertTrue(red_in(70, 110))


class MountEntrance(unittest.TestCase):
    """A bare text entrance plays once when its leaf mounts, over the span
    its schedule takes, and an exit runs it back to its start; a progress
    the track states stands as written. Each case reads the picture at a
    moment of the scene's clock: the leaf mounts in `setup`, at zero."""

    # Four letters of 450ms, each opening 30ms after the one before.
    SPAN = 0.54

    def render(self, body, at, leave=None):
        """The pixels at `at` seconds of a sketch whose `setup` renders
        `body`, an expression that may name `sans`, the instrument face set
        at 24 pixels in white; past `leave` seconds it renders an empty box
        in its place."""
        source = (
            HEAD
            + f"""@sketch(size=(200, 60), background="#000000")
class Scene:
    def setup(self, ctx):
        hub = ctx.assets.hub()
        sans = Type(size=24, color="#ffffff",
                    face=hub.load(skia.Typeface, ctx.local("data/Sans.ttf")))
        self.left = False
        ctx.render(box().children([{body}]))

    def update(self, elapsed, ctx):
        if {leave!r} is not None and elapsed >= {leave!r} and not self.left:
            self.left = True
            ctx.render(box())
"""
        )
        with tempfile.TemporaryDirectory() as folder:
            data = Path(folder) / "data"
            data.mkdir()
            shutil.copy(INSTRUMENTS / "Sans.ttf", data / "Sans.ttf")
            path = Path(folder) / "entrance.py"
            output = Path(folder) / "entrance.png"
            path.write_text(source)
            renderFile(path, output, at=at)
            return media.load(output).frameAt(0).image.rgba()

    @staticmethod
    def bright(pixels):
        """How many pixels stand nearly white: a glyph home and opaque."""
        return TextEffects.count(pixels, lambda r, g, b: min(r, g, b) > 200)

    BARE = 'text("RISE").font(sans).textFx(textFx.entrance(textFx.rise(20)))'
    # The same effect on a track at rest, which nothing plays.
    REST = 'text("RISE").font(sans).textFx(textFx.enter(textFx.rise(20)), progress=1)'

    # The sketch is 200 pixels wide; the instrument's letters stand on an
    # advance of 14.4 pixels at 24, so the first letter's ink lies in the
    # first 15 columns and the last letter's between columns 43 and 58.
    WIDTH = 200
    FIRST_LETTER = range(0, 15)
    LAST_LETTER = range(43, 58)

    @classmethod
    def columns(cls, pixels, span):
        """The pixels of the columns in `span`, row by row."""
        return b"".join(
            pixels[(row * cls.WIDTH + span.start) * 4:(row * cls.WIDTH + span.stop) * 4]
            for row in range(len(pixels) // (cls.WIDTH * 4))
        )

    def test_a_bare_entrance_plays_once_over_its_span(self):
        rest = self.render(self.REST, at=1.0)
        self.assertGreater(self.bright(rest), 0)
        # One frame in, every glyph is below its rest and still faint.
        self.assertEqual(self.bright(self.render(self.BARE, at=1 / 60)), 0)
        # The letters land in turn: at 0.28 s the first letter, 0.62 of its
        # way through an ease-out-expo rise of 20 pixels, is within a
        # third of a pixel of home, which a glyph's vertical place rounds
        # away, while the last, which started 90 ms later, is still a pixel
        # short of it.
        landing = self.render(self.BARE, at=0.28)
        self.assertEqual(
            self.columns(landing, self.FIRST_LETTER),
            self.columns(rest, self.FIRST_LETTER),
        )
        self.assertNotEqual(
            self.columns(landing, self.LAST_LETTER),
            self.columns(rest, self.LAST_LETTER),
        )
        # Every letter is home once the span closes, and stays home.
        self.assertEqual(self.render(self.BARE, at=self.SPAN + 0.02), rest)
        self.assertEqual(self.render(self.BARE, at=self.SPAN * 3), rest)

    def test_a_progress_the_track_states_stands(self):
        self.assertTrue(textFx.entrance(textFx.rise(20)).progressOverSpan)
        held = textFx.entrance(textFx.rise(20), compose.Track(progress=0.5))
        self.assertFalse(held.progressOverSpan)
        # Long past the span the glyphs still stand where 0.5 puts them.
        self.assertEqual(
            self.render(
                'text("RISE").font(sans).textFx(textFx.entrance(textFx.rise(20), '
                "compose.Track(progress=0.5)))",
                at=1.0,
            ),
            self.render(
                'text("RISE").font(sans).textFx(textFx.enter(textFx.rise(20)), '
                "progress=0.5)",
                at=1.0,
            ),
        )
        # A motion of its own keeps its own two seconds.
        driven = (
            'text("RISE").font(sans).textFx(textFx.entrance(textFx.rise(20), '
            "compose.Track(progress=motion.animate(from_=0, to=1, duration=2, "
            "ease=motion.ease.linear))))"
        )
        rest = self.render(self.REST, at=1.0)
        self.assertNotEqual(self.render(driven, at=self.SPAN + 0.1), rest)
        self.assertEqual(self.render(driven, at=2.05), rest)

    def test_an_exit_runs_a_bare_entrance_back_to_its_start(self):
        leaving = (
            'text("RISE").font(sans).key("title").exit()'
            ".textFx(textFx.entrance(textFx.rise(20)))"
        )
        rest = self.render(self.REST, at=1.0)
        self.assertEqual(self.render(leaving, at=0.58, leave=0.6), rest)
        # On its way back, then all but back at its start, then gone.
        self.assertNotEqual(self.render(leaving, at=0.8, leave=0.6), rest)
        self.assertGreater(self.bright(self.render(leaving, at=0.8, leave=0.6)), 0)
        self.assertEqual(self.bright(self.render(leaving, at=1.1, leave=0.6)), 0)
        gone = self.render(leaving, at=1.2, leave=0.6)
        self.assertEqual(TextEffects.count(gone, lambda r, g, b: max(r, g, b) > 0), 0)


if __name__ == "__main__":
    unittest.main()
