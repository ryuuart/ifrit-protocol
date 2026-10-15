"""Native mixed text, paragraph values and rendered Compose typography."""

import tempfile
import unittest
from pathlib import Path

from _sigil import weave as native_weave
from sigil import compose, geometry, material, media, skia, weave
from sigil.compose import SpanStyle, Text, TextPath, frame, text
from sigil.compose import selectors as composition_selectors
from sigil.motion import animatable
from sigil.sketch import renderFile


class Typography(unittest.TestCase):
    def render(self, source, at=0):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "typography.py"
            output = Path(folder) / "typography.png"
            path.write_text(source)
            renderFile(path, output, at=at)
            return media.load(output).frameAt(0).image.rgba()

    def test_rich_runs_keep_inheritance_and_resolve_names_by_value(self):
        self.assertIs(weave.RichText, native_weave.RichText)
        base = weave.textStyle(weave.Type(size=24, color="#ffffff"))
        passage = (
            weave.rich(base)
            .add("日本語 ")
            .add("accent", name="accent")
            .add(" superscript", type=weave.Type(size=weave.em(0.5)))
        )
        sheet = weave.TypeSheet().set("accent", weave.Type(color="#ff0000"))
        self.assertIs(passage.styles(sheet), passage)
        self.assertTrue(passage.hasBase())
        self.assertTrue(passage.hasStyles())
        runs = passage.runs()
        self.assertEqual(runs[0].utf8, "日本語 ")
        self.assertEqual(runs[1].styleName, "accent")
        self.assertEqual(runs[1].style.shaping.fontSize, 24)
        self.assertEqual(runs[2].style.shaping.fontSize, 12)
        self.assertFalse(runs[2].total)
        sheet.set("accent", weave.Type(size=90))
        self.assertEqual(passage.runs()[1].style.shaping.fontSize, 24)
        snapshot = passage.copy()
        passage.add(" more").slot(name="mark", size=(14, 9), baselineDrop=2)
        self.assertEqual(len(snapshot.runs()), 3)
        self.assertEqual(len(runs), 3)
        self.assertEqual(passage.runs()[-1].slotSize, (14, 9))
        inherited = weave.rich().add("inherited", type=weave.Type(weight=700))
        self.assertFalse(inherited.hasBase())
        self.assertEqual(inherited.runs()[0].over.weight, 700)

    def test_optional_paragraph_settings_survive_replacement_as_copies(self):
        tabs = weave.TabStopOptions(
            stops=(weave.TabStop(position=90, align=weave.TabStop.Align.Character),),
            interval=48,
        )
        block = weave.ParagraphBlock(
            justification=weave.JustificationOptions(spaceStretch=0.8),
            hyphenation=weave.HyphenationOptions(consecutiveLimit=2),
            tabStops=tabs,
            kinsoku=weave.KinsokuTable(notLineStart="、。", notLineEnd="（"),
        )
        saved = block.justification
        saved.spaceStretch = 0.3
        self.assertAlmostEqual(block.justification.spaceStretch, 0.8)
        block.justification = None
        self.assertAlmostEqual(saved.spaceStretch, 0.3)
        block.justification = saved
        self.assertEqual(block.hyphenation.consecutiveLimit, 2)
        self.assertEqual(block.tabStops.stops[0].position, 90)
        self.assertEqual(block.kinsoku.notLineStart, "、。")
        heading = weave.ParagraphStyle(
            keep=weave.KeepOptions(withNext=True),
            initial=weave.InitialLetter(lines=3, margin=5),
            spaceAfter=12,
        )
        story = weave.Story(weave.rich().add("Heading\nBody")).paragraphs((heading,))
        saved_blocks = story.blocks()
        saved_blocks[0].keep.withNext = False
        self.assertTrue(story.blocks()[0].keep.withNext)
        story.paragraphs(())
        self.assertEqual(saved_blocks[0].spaceAfter, 12)
        self.assertEqual(story.blocks(), [])
        self.assertIsInstance(frame(story), Text)

    def test_unicode_tables_and_paint_lists_preserve_native_value_semantics(self):
        table = weave.MojikumiTable()
        members = table.members
        members[2] = "（「"
        table.members = tuple(members)
        self.assertEqual(table.classOf("（"), weave.MojikumiClass.Opening)
        room = table.room
        room[2][1] = 0.25
        table.room = tuple(tuple(row) for row in room)
        self.assertAlmostEqual(table.room[2][1], 0.25)
        with self.assertRaises(TypeError):
            table.members = ("too short",)
        underline = weave.Decoration(color="#aabbcc", thickness=2)
        outline = skia.Paint()
        outline.setColor("#000000")
        layer = weave.PaintLayer(paint=outline, offset=(2, 3))
        font = weave.Type(decorations=(underline,), underlays=(layer,))
        saved = font.decorations
        saved[0].thickness = 10
        self.assertEqual(font.decorations[0].thickness, 2)
        font.decorations = ()
        self.assertEqual(font.decorations, [])
        font.decorations = None
        self.assertIsNone(font.decorations)
        self.assertEqual(
            (font.underlays[0].offset.x, font.underlays[0].offset.y), (2, 3)
        )

    def test_a_paint_style_holds_its_foreground_material_and_its_copies_share_it(self):
        style = weave.PaintStyle()
        self.assertIsNone(style.foregroundMaterial)
        gold = material.Material("#d4a017")
        style.foregroundMaterial = gold
        self.assertEqual(style.foregroundMaterial, gold)
        self.assertEqual(
            weave.PaintStyle(foregroundMaterial=gold).foregroundMaterial, gold
        )
        copied = style.copy()
        self.assertEqual(copied, style)
        style.foregroundMaterial = None
        self.assertIsNone(style.foregroundMaterial)
        self.assertEqual(copied.foregroundMaterial, gold)
        self.assertNotEqual(copied, style)

    def test_vector_fields_read_back_the_point_they_were_given(self):
        def pair(point):
            return (point.x, point.y)

        layer = weave.PaintLayer(offset=(2, 3))
        self.assertEqual(pair(layer.offset), (2, 3))
        layer.offset = (-4, 5)
        self.assertEqual(pair(layer.offset), (-4, 5))
        blurred = weave.PaintLayer.blurred(skia.Paint(), 2, (1, 7))
        self.assertEqual(pair(blurred.offset), (1, 7))
        exclusion = weave.Exclusion(offset=(6, -1))
        self.assertEqual(pair(exclusion.offset), (6, -1))
        initial = weave.PlacedInitial(baseline=(12, 40))
        self.assertEqual(pair(initial.baseline), (12, 40))

    def test_an_outline_turns_its_corners_by_a_geometry_join(self):
        def outline(*join):
            return weave.kit.outline("#000000", 3.0, *join)

        round_join = outline(geometry.path.Join.Round)
        self.assertEqual(outline(), round_join, "a round join by default")
        self.assertNotEqual(outline(geometry.path.Join.Bevel), round_join)

    def test_a_tracked_style_takes_its_colour_in_any_spelling(self):
        def tracked(color):
            return weave.kit.tracked(None, 12.0, color, 40.0)

        red = tracked(material.Color(1.0, 0.0, 0.0, 1.0))
        self.assertEqual(tracked("#ff0000"), red)
        self.assertEqual(tracked((1.0, 0.0, 0.0)), red)
        self.assertNotEqual(tracked("#00ff00"), red)

    def test_selectors_and_path_progress_keep_native_types_and_named_inputs(self):
        selection = weave.selectors.words(start=0, end=2) | weave.selectors.text(
            text="日本語"
        )
        selection = selection & ~composition_selectors.style(name="muted")
        node = text(weave.rich().add("日本語 and signals")).span(
            where=selection,
            style=SpanStyle().font(weave.Type(weight=700)),
        )
        progress = animatable(0.25)
        path = TextPath(
            path=skia.Path.Circle(50, 50, 40),
            at=progress,
            align=TextPath.Align.Center,
            exactTangent=True,
        )
        retained = path.at
        progress.set(0.75)
        del progress
        self.assertAlmostEqual(retained.value, 0.75)
        self.assertIs(node.textOnPath(path=path), node)
        self.assertIs(node.paragraphStyles(names=("heading", "body")), node)
        self.assertIs(node.paragraphStyles(blocks=(weave.ParagraphStyle(),)), node)

    def test_a_run_lays_out_along_a_curve_built_from_geometry_alone(self):
        # An arch through three points, made by sigil.geometry with no Skia
        # spelling, carries the run: ink rises to the arch's crown and
        # leaves the straight line between its two feet empty.
        pixels = self.render("""from sigil.compose import TextPath, box, text
from sigil.geometry import path
from sigil.sketch import sketch
from sigil.weave import Type
@sketch(size=(240, 120), background="#000000")
class Scene:
    def setup(self, ctx):
        arch = path.curveThrough([(20, 100), (120, 30), (220, 100)])
        ctx.render(box().width(240).height(120).children(
            text("MMMMMMMMMMMMMMMMMMMMMMMM").font(Type(size=16, color="#ffffff"))
            .textOnPath(path=TextPath(path=arch, align=TextPath.Align.Center))))
""")
        width = 240

        def inked(x0, y0, x1, y1):
            return sum(
                pixels[(y * width + x) * 4] > 128
                for y in range(y0, y1)
                for x in range(x0, x1)
            )

        self.assertGreater(inked(100, 10, 140, 45), 20)
        self.assertEqual(inked(100, 80, 140, 110), 0)

    def test_a_native_entrance_reveals_the_glyphs_in_turn(self):
        from sigil.compose import textFx

        rise = textFx.rise(30)
        self.assertEqual(rise.from_, textFx.Displaced(dy=30))
        typed = textFx.entrance(textFx.typeOn())
        self.assertIsInstance(typed, compose.Track)
        self.assertEqual(typed, textFx.entrance(textFx.typeOn()))

        def revealed(track, at=0):
            pixels = self.render(
                f"""from sigil import compose
from sigil.compose import box, text, textFx
from sigil.sketch import sketch
from sigil.weave import Type
@sketch(size=(240, 40), background="#000000")
class Scene:
    def setup(self, ctx):
        track = {track}
        ctx.render(box().width(240).height(40).children(
            text("MMMMMMMMMMMM").font(Type(size=16, color="#ffffff")).textFx(track)))
""",
                at=at,
            )
            columns = [
                any(pixels[(y * 240 + x) * 4] > 128 for y in range(40))
                for x in range(240)
            ]
            inked = [x for x, ink in enumerate(columns) if ink]
            return (min(inked), max(inked)) if inked else None

        def held(progress):
            return (
                f"textFx.entrance(textFx.typeOn(), compose.Track(progress={progress}))"
            )

        self.assertIsNone(revealed(held(0)))
        # Left at its default, the progress is the entrance's own: nothing
        # when the text mounts, every glyph once the 780ms span has passed.
        self.assertIsNone(revealed("textFx.entrance(textFx.typeOn())"))
        whole = revealed("textFx.entrance(textFx.typeOn())", at=1.0)
        half = revealed(held(0.5))
        self.assertIsNotNone(whole)
        self.assertIsNotNone(half)
        assert whole is not None and half is not None
        # The first glyphs stand where they stand at rest; the last have
        # not yet appeared.
        self.assertEqual(half[0], whole[0])
        self.assertLess(half[1], whole[1] - 10)

    def test_a_span_states_the_font_and_ink_verbs_over_its_range(self):
        stated = SpanStyle().fontWeight(700).letterSpacing(1).ink("#ff0000")
        self.assertIsInstance(stated, SpanStyle)
        leaf = text("alpha beta")
        self.assertIs(leaf.span(weave.selectors.words(0, 1), stated), leaf)
        for retired in ("spanPaint", "spanStyle"):
            self.assertFalse(hasattr(Text, retired), retired)
        for boxVerb in ("padding", "children", "maxTextLines"):
            self.assertFalse(hasattr(SpanStyle, boxVerb), boxVerb)

    def test_mixed_runs_and_inline_slot_render_in_inherited_type(self):
        pixels = self.render("""from sigil.compose import box, text
from sigil.sketch import sketch
from sigil.weave import Type, rich
@sketch(size=(240, 72), background="#000000")
class Scene:
    def setup(self, ctx):
        passage = rich().add("RED", Type(color="#ff0000")).add(" ").slot("mark", (20, 20)).add(" GREEN", Type(color="#00ff00"))
        ctx.render(text(passage).width(240).font(Type(size=24)).children(box().key("mark").fill("#0000ff")))
""")
        colors = list(zip(pixels[0::4], pixels[1::4], pixels[2::4]))
        self.assertTrue(any(r > 150 and g < 20 and b < 20 for r, g, b in colors))
        self.assertTrue(any(g > 150 and r < 20 and b < 20 for r, g, b in colors))
        self.assertGreater(
            sum(b > 200 and r < 20 and g < 20 for r, g, b in colors), 200
        )

    def test_a_text_leaf_hyphenates_with_the_held_english_table(self):
        english = weave.kit.englishHyphenator()
        self.assertIs(english, weave.kit.englishHyphenator())
        options = weave.HyphenationOptions(patterns=english)
        self.assertIs(options.patterns, english)
        self.assertIsNone(weave.HyphenationOptions().patterns)

        # The table the options hold breaks a word at the end of a line.
        words = "specimen " * 6
        paragraph = weave.Paragraph(words, weave.Type(size=20, language="en-US"))
        layout = weave.layoutParagraph(
            weave.FontContext(),
            paragraph,
            weave.BlockFlow((0, 0, 70, 400)),
            weave.ParagraphLayoutOptions(hyphenation=options),
        )
        ends = [line.textEnd for line in layout.lineMetrics(paragraph)]
        self.assertTrue(
            any(
                0 < end < len(words) and words[end - 1] != " " and words[end] != " "
                for end in ends
            ),
            "no line ended inside a word",
        )

        # And a text leaf's hyphens lane carries it: a leaf filling a
        # canvas narrower than the word, justified, ends a line in a hyphen
        # under the table and never under its typed hyphens alone.
        source = """from sigil.compose import text
from sigil.sketch import sketch
from sigil.weave import HyphenationOptions, TextAlignment, Type, kit
@sketch(size=(70, 300), background="#000000")
class Scene:
    def setup(self, ctx):
        ctx.render(text("specimen " * 4).textAlign(TextAlignment.Justify).font(Type(size=20, color="#ffffff", language="en-US")).hyphens(HyphenationOptions(%s)))
"""
        self.assertTrue(
            self.aLineEndsInAHyphen(
                self.render(source % "patterns=kit.englishHyphenator()"), 70
            ),
            "no line of the leaf ended in a hyphen",
        )
        self.assertFalse(
            self.aLineEndsInAHyphen(self.render(source % ""), 70),
            "a line ended in a hyphen with no table to break by",
        )

    @staticmethod
    def aLineEndsInAHyphen(pixels, width):
        """Whether the last glyph of some line of light text on a dark
        ground is a hyphen: a mark much wider than it is tall, standing
        clear of the line's top and bottom."""
        height = len(pixels) // (4 * width)

        def lit(x, y):
            return pixels[(y * width + x) * 4] > 128

        rows = [y for y in range(height) if any(lit(x, y) for x in range(width))]
        bands = []
        for y in rows:
            if bands and y == bands[-1][1] + 1:
                bands[-1][1] = y
            else:
                bands.append([y, y])
        for top, bottom in bands:

            def column(x, top=top, bottom=bottom):
                return [y for y in range(top, bottom + 1) if lit(x, y)]

            right = max(x for x in range(width) if column(x))
            left = right
            while left > 0 and column(left - 1):
                left -= 1
            marked = [y for x in range(left, right + 1) for y in column(x)]
            markHeight = max(marked) - min(marked) + 1
            markWidth = right - left + 1
            if (
                markWidth >= 2 * markHeight
                and min(marked) > top + 2
                and max(marked) < bottom - 2
            ):
                return True
        return False

    def test_story_overflow_reaches_second_frame(self):
        pixels = self.render("""from sigil.compose import box, frame
from sigil.sketch import sketch
from sigil.weave import Story, Type, rich, textStyle
@sketch(size=(280, 100), background="#000000")
class Scene:
    def setup(self, ctx):
        article = Story(rich(textStyle(Type(size=20, color="#ffffff"))).add("One two three four five six seven eight nine ten eleven twelve thirteen fourteen."))
        ctx.render(box().row().gap(20).children(frame(article).key("a").textThreadTo("b").width(130).height(60), frame(article).key("b").width(130).height(100)))
""")

        def ink(left, right):
            return sum(
                pixels[(y * 280 + x) * 4] > 80
                for y in range(100)
                for x in range(left, right)
            )

        self.assertGreater(ink(0, 130), 100)
        self.assertGreater(ink(150, 280), 100)


if __name__ == "__main__":
    unittest.main()


class GlyphShadowsAndPageDirection(unittest.TestCase):
    """A glow each glyph casts, which fades with its glyph; and a page that
    states the direction its bidirectional analysis resolves against."""

    def render(self, source):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "shadow.py"
            output = Path(folder) / "shadow.png"
            path.write_text(source)
            renderFile(path, output, at=0)
            return media.load(output).frameAt(0).image.rgba()

    def glowBeside(self, opacity):
        """The green the glow leaves just left of the letter's stem, with
        the glyph held at `opacity` by an entrance track."""
        pixels = self.render(f"""from sigil import compose
from sigil.compose import TextShadow, box, text, textFx
from sigil.sketch import sketch
from sigil.weave import Type
@sketch(size=(160, 120), background="#000000")
class Scene:
    def setup(self, ctx):
        held = textFx.DisplacedTween(from_=textFx.Displaced(opacity={opacity}, fadeOver=0))
        track = textFx.entrance(held, compose.Track(progress=0))
        ctx.render(box().width(160).height(120).padding(30).children(
            text("I").font(Type(size=64, color="#ffffff"))
            .textShadow(TextShadow(blur=6, color="#00ff00")).textFx(track)))
""")
        width, row = 160, 70
        stem = next(x for x in range(width) if pixels[(row * width + x) * 4] > 10)
        return pixels[(row * width + stem - 4) * 4 + 1]

    def test_a_glow_fades_with_its_glyph(self):
        full = self.glowBeside(1)
        self.assertGreater(full, 40, "no glow stands beside the glyph")
        self.assertAlmostEqual(self.glowBeside(0.25), full * 0.25, delta=3)

    def test_a_shadow_is_coloured_by_a_colour_term(self):
        shadow = compose.TextShadow(offset=(4, 0), blur=2)
        self.assertEqual(shadow.color, compose.Fill.currentInk())
        shadow.color = compose.Fill.var("accent")
        self.assertEqual(shadow.color, compose.Fill.var("accent"))
        shadow.color = "#ff0000"
        self.assertEqual(shadow.color, compose.Fill.color("#ff0000"))
        with self.assertRaises(TypeError):
            shadow.color = material.Paint.linearGradient(
                (0, 0), (1, 0), [(0, "#000000"), (1, "#ffffff")]
            )
        leaf = text("I")
        self.assertIs(leaf.textShadow([shadow, compose.TextShadow(blur=6)]), leaf)

    def test_a_leaf_answers_the_style_the_cascade_sets_it_in(self):
        page = (
            compose.box().font(weave.Type(size=34)).children(text("Wave").key("hero"))
        )
        style = compose.textStyleOf(page, "hero")
        self.assertIsNotNone(style)
        assert style is not None
        self.assertEqual(style.shaping.fontSize, 34)
        self.assertIsNone(compose.textStyleOf(page, "absent"))

    def test_a_page_resolves_its_levels_against_its_stated_base(self):
        folio = "۱۵۲۵ · Folio 22v"
        fonts = weave.FontContext()
        paragraph = weave.Paragraph(folio, weave.Type(size=16))
        self.assertEqual(
            paragraph.baseDirection(), weave.unicode.BaseDirection.AutoLeftToRight
        )

        def levels(base):
            runs = weave.unicode.bidi(folio, base)
            paragraph.ensureAnalyzed(fonts)
            for word in paragraph.words():
                if word.textEnd <= word.textBegin:
                    continue
                expected = next(
                    run.level for run in runs if run.start <= word.textBegin < run.end
                )
                self.assertEqual(word.bidiLevel, expected, word.textBegin)
            return [word.bidiLevel for word in paragraph.words()]

        automatic = levels(weave.unicode.BaseDirection.AutoLeftToRight)
        before = paragraph.wordRevision()
        paragraph.setBaseDirection(weave.unicode.BaseDirection.RightToLeft)
        self.assertNotEqual(paragraph.wordRevision(), before)
        self.assertNotEqual(levels(weave.unicode.BaseDirection.RightToLeft), automatic)
        block = weave.ParagraphBlock(
            baseDirection=weave.unicode.BaseDirection.RightToLeft
        )
        self.assertEqual(block.baseDirection, weave.unicode.BaseDirection.RightToLeft)
        self.assertTrue(hasattr(weave.ParagraphField, "BaseDirection"))

    def test_a_typeface_answers_its_variation_axes(self):
        face = weave.typeface("Helvetica Neue")
        axes = face.variationAxes()
        self.assertIsInstance(axes, list)
        for axis in axes:
            self.assertIsInstance(axis, weave.VariationAxis)
            self.assertEqual(len(axis.tag), 4)
            self.assertLessEqual(axis.minimum, axis.maximum)
