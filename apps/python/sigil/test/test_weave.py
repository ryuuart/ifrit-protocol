"""Native mixed text, paragraph values and rendered Compose typography."""

import tempfile
import unittest
from pathlib import Path

from _sigil import weave as native_weave
from sigil import geometry, image, material, skia, weave
from sigil.compose import SpanStyle, Text, TextPath, frame, text
from sigil.compose import selectors as composition_selectors
from sigil.motion import Output
from sigil.sketch import render_file


class Typography(unittest.TestCase):
    def render(self, source):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "typography.py"
            output = Path(folder) / "typography.png"
            path.write_text(source)
            render_file(path, output, at=0)
            return image.load(output).rgba()

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
        progress = Output(0.25)
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
