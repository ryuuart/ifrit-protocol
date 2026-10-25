"""Native kit scope, value ownership and authoring prefix contracts."""

import builtins
import copy
import tempfile
import unittest
from pathlib import Path

from _sigil import sketch as raw
from sigil import weave
from sigil.compose import Element, box, text
from sigil.compose import kit as neutral
from sigil.compose.layouts import Grid, fr, px
from sigil.sketch import kit, renderFile


class Kit(unittest.TestCase):
    def render(self, source, at=0):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entry = root / "kit_fixture.py"
            output = root / "kit_fixture.png"
            entry.write_text(source)
            renderFile(entry, output, at=at)
            self.assertTrue(output.is_file())

    def test_theme_is_a_native_value_and_reads_are_owned_snapshots(self):
        for factory, native in (
            (kit.houseTheme, raw.kit.houseTheme),
            (kit.studyTheme, raw.kit.studyTheme),
        ):
            with self.subTest(factory=factory.__name__):
                original = factory()
                self.assertIsInstance(original, raw.kit.Theme)
                self.assertEqual(original, native())
                custom = original.copy()
                custom.palette.ground = "#244466"
                custom.type.title.size += 1
                self.assertNotEqual(custom, original)
                fresh = factory()
                fresh.spacing.cellGap += 1
                self.assertEqual(factory(), original)
                sheet = custom.styleSheet()
                self.assertEqual(len(sheet), len(original.styleSheet()))
                self.assertEqual(len(custom.properties()), 1)
                self.assertNotEqual(custom.properties(), original.properties())

    def test_theme_answers_the_copy_protocol_a_memo_model_is_snapshotted_with(self):
        original = kit.houseTheme()
        for made in (copy.copy(original), copy.deepcopy(original)):
            self.assertIsInstance(made, raw.kit.Theme)
            self.assertEqual(made, original)
            made.spacing.cellGap += 1
            self.assertNotEqual(made, original)
        self.assertEqual(original, kit.houseTheme())

    def test_theme_font_and_style_name_the_same_register_across_overloads(self):
        look = kit.houseTheme()
        plain = look.font(line=look.type.title)
        self.assertIsInstance(plain, weave.Type)
        self.assertEqual(plain, look.font(look.type.title))
        inked = look.font(line=look.type.title, ink="#ffffff")
        self.assertIsInstance(inked, weave.Type)
        self.assertEqual(inked, look.font(look.type.title, "#ffffff"))
        styled = look.style(line=look.type.title, ink="#ffffff")
        self.assertIsInstance(styled, weave.TextStyle)

    def test_optional_nested_values_survive_parent_replacement(self):
        well = kit.Well(content=kit.WellContent(across="end"))
        held = well.content
        well.content = None
        self.assertIsNotNone(held)
        held.across = "center"
        self.assertIsNone(well.content)
        well.content = held
        self.assertEqual(well.content.across, held.across)

    def test_native_and_convenience_furniture_return_the_same_element_type(self):
        plate = kit.Well(width=120, height=80, content=kit.WellContent())
        specimen = kit.cell(text("A"), label="Letter", note="One voice", plate=plate)
        content = kit.cells([specimen, None, kit.caption(box(), label="Empty")])
        page = kit.page(content, title="Specimens", footer="Native furniture")
        self.assertIsInstance(page, Element)
        self.assertIsInstance(
            raw.kit.page(raw.kit.Page(title="Native"), content), Element
        )
        self.assertIsInstance(neutral.line(length=80, thickness=2), Element)
        self.assertIsInstance(
            neutral.sheet(content, title="Neutral", marginHorizontal=20), Element
        )
        with self.assertRaisesRegex(TypeError, "unknown Page property 'titlle'"):
            kit.page(content, titlle="typo")

    def test_kit_names_and_fields_are_spelled_as_the_native_kit_spells_them(self):
        from sigil.compose import document, layouts

        for module, names in (
            (
                kit,
                (
                    "featureTheme",
                    "houseTheme",
                    "studyTheme",
                    "houseFace",
                    "controlsOf",
                    "sectionHeader",
                    "panelGrid",
                ),
            ),
            (neutral, ("captionLabel", "captionNote", "panelGrid")),
            (document, ("listGap", "quoteInset")),
            (layouts, ("repeatTrack",)),
        ):
            for name in names:
                with self.subTest(module=module.__name__, name=name):
                    self.assertTrue(hasattr(module, name))
                    snake = "".join(
                        "_" + letter.lower() if letter.isupper() else letter
                        for letter in name
                    )
                    self.assertFalse(hasattr(module, snake))
                    self.assertNotIn(snake, module.__all__)
        self.assertIsInstance(kit.featureTheme(kit.Density.Spacious), raw.kit.Theme)
        self.assertEqual(kit.well(paddingVertical=6).__class__, Element)
        # A field is one spelling: the snake form of a camelCase field is
        # refused with the spelling it missed.
        for call, field in (
            (lambda: kit.well(padding_vertical=6), "paddingVertical"),
            (lambda: kit.panelGrid(box(), row_gap=4), "rowGap"),
            (lambda: neutral.sheet(box(), margin_horizontal=20), "marginHorizontal"),
            (lambda: neutral.panelGrid(box(), divider_width=1), "dividerWidth"),
        ):
            with (
                self.subTest(field=field),
                self.assertRaisesRegex(TypeError, f"spelled '{field}'"),
            ):
                call()

    def test_every_kind_of_node_is_a_child_of_a_kit_piece(self):
        # A typed leaf is a class of its own rather than a subclass of
        # Element, so a run that asked after Element alone refused a text
        # where the native children list takes one.
        leaf = text("A")
        self.assertNotIsInstance(leaf, Element)
        self.assertIsInstance(kit.cells(leaf, box()), Element)
        self.assertIsInstance(neutral.cells([leaf, "words", None]), Element)
        self.assertIsInstance(neutral.centred(leaf), Element)
        with self.assertRaisesRegex(TypeError, "children must be nodes"):
            neutral.cells(42)

    def test_grid_tracks_are_owned_values_and_place_named_native_cells(self):
        grid = Grid(columns=[px(40), fr()])
        track = grid.columns[0]
        grid.columns = []
        self.assertEqual(track, px(40))
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_kit_grid", None))
        self.render(
            """import builtins
from sigil.compose import box, layout
from sigil.compose.layouts import Grid, px, fr
from sigil.sketch import sketch


@sketch(size=(160, 80), captureAt=0.05)
class GridSheet:
    def setup(self, ctx):
        ctx.render(
            (
                layout(
                    Grid(
                        columns=[px(40), fr()],
                        rows=[fr()],
                        areas=["rail body"],
                        gap=(8, 0),
                    )
                )
                .width(160)
                .height(80)
                .children(
                    [
                        (box().key("rail").gridArea("rail")),
                        (box().key("body").gridArea("body")),
                    ]
                )
            )
        )

    def update(self, elapsed, ctx):
        if elapsed > 1 / 60:
            builtins._sigil_kit_grid = (
                ctx.composer.bounds("rail"),
                ctx.composer.bounds("body"),
            )
""",
            at=0.05,
        )
        rail, body = builtins._sigil_kit_grid
        self.assertEqual((rail.width(), body.left(), body.width()), (40, 48, 112))

    def test_line_parts_receive_owned_props_and_accept_argument_prefixes(self):
        props = []

        def label(words, voice):
            props.append(voice)
            return text(words)

        voice = neutral.Caption(label=label, note=lambda words: text(words))
        neutral.cell(box(), voice, label="Owned", note="Prefix")
        voice.noteGap = 900
        self.assertEqual(props[0].noteGap.value, 4)
        props[0].noteGap = 18
        self.assertEqual(voice.noteGap.value, 900)

    def test_stage_uses_the_native_context_and_sets_its_theme_at_the_root(self):
        builtins._sigil_kit_stage = []
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_kit_stage", None))
        self.render("""import builtins
from sigil.compose import text
from sigil.sketch import kit, sketch


@sketch(size=(8, 8), captureAt=0)
class Sheet:
    def setup(self, ctx):
        look = kit.houseTheme()
        look.palette.ground = "#123456"
        kit.stage(ctx, theme=look, size=(160, 100), captureAt=0, oversample=1)
        assert ctx.width == 160 and ctx.height == 100
        ctx.render(kit.page(text("Stage"), title="Native page"))
        builtins._sigil_kit_stage.append((ctx.width, ctx.height))
""")
        self.assertEqual(builtins._sigil_kit_stage, [(160, 100)])

    def test_a_nearer_sheet_states_a_subtree_look(self):
        self.render("""from sigil.compose import box
from sigil.sketch import kit, sketch


@sketch(size=(64, 64), captureAt=0, theme=kit.houseTheme())
class Nested:
    def setup(self, ctx):
        inset = kit.houseTheme()
        inset.palette.cellGround = "#254a48"
        ctx.render(
            box().children(
                kit.well(width=20, height=20),
                kit.well(width=20, height=20).applyStyleSheet(inset.properties()),
            )
        )
""")

    def test_decorator_adapts_setup_and_draw_argument_prefixes(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_prefix_calls", None))
        for setup_count in (0, 1):
            for draw_count in (0, 1, 2):
                with self.subTest(setup=setup_count, draw=draw_count):
                    builtins._sigil_prefix_calls = []
                    setup_args = ", ctx" if setup_count else ""
                    draw_args = ("", ", pen", ", pen, ctx")[draw_count]
                    check = "assert ctx.width == 80" if draw_count == 2 else "pass"
                    self.render(f"""import builtins
from sigil.sketch import sketch
@sketch(size=(80, 48), captureAt=0)
class Prefixes:
    def setup(self{setup_args}):
        self.ready = True
        builtins._sigil_prefix_calls.append("setup")
    def draw(self{draw_args}):
        assert self.ready
        {check}
        builtins._sigil_prefix_calls.append("draw")
""")
                    calls = builtins._sigil_prefix_calls
                    self.assertEqual(calls[0], "setup")
                    self.assertIn("draw", calls)


if __name__ == "__main__":
    unittest.main()
