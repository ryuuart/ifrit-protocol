"""Native kit scope, value ownership and authoring prefix contracts."""

import builtins
import tempfile
import threading
import unittest
from pathlib import Path

from sigil import weave
from sigil.compose import Element, box, text
from sigil.compose import kit as neutral
from sigil.compose.layouts import Grid, fr, px
from sigil.native import sketch as raw
from sigil.sketch import kit, render_file


class Kit(unittest.TestCase):
    def render(self, source, at=0):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entry = root / "kit_fixture.py"
            output = root / "kit_fixture.png"
            entry.write_text(source)
            render_file(entry, output, at=at)
            self.assertTrue(output.is_file())

    def test_theme_is_a_native_value_and_reads_are_owned_snapshots(self):
        ambient = kit.theme()
        for factory, native in (
            (kit.house_theme, raw.kit.houseTheme),
            (kit.study_theme, raw.kit.studyTheme),
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
                with kit.provide(custom):
                    self.assertEqual(kit.theme(), raw.kit.theme())
                    self.assertEqual(kit.theme(), custom)
                    snapshot = kit.theme()
                    snapshot.spacing.cellGap += 1
                    self.assertEqual(kit.theme(), custom)
                self.assertEqual(kit.theme(), ambient)

    def test_theme_font_and_style_name_the_same_register_across_overloads(self):
        look = kit.house_theme()
        plain = look.font(line=look.type.title)
        self.assertIsInstance(plain, weave.Type)
        self.assertEqual(plain, look.font(look.type.title))
        inked = look.font(line=look.type.title, ink="#ffffff")
        self.assertIsInstance(inked, weave.Type)
        self.assertEqual(inked, look.font(look.type.title, "#ffffff"))
        styled = look.style(line=look.type.title, ink="#ffffff")
        self.assertIsInstance(styled, weave.TextStyle)

    def test_nested_providers_restore_the_original_theme_on_exception(self):
        original = kit.theme()
        outer = original.copy()
        outer.palette.ink = "#abcdef"
        inner = original.copy()
        inner.palette.ink = "#fedcba"
        with kit.provide(outer):
            with self.assertRaisesRegex(ValueError, "intentional"), kit.provide(inner):
                self.assertEqual(kit.theme(), inner)
                raise ValueError("intentional")
            self.assertEqual(kit.theme(), outer)
        self.assertEqual(kit.theme(), original)

    def test_provider_enter_close_and_thread_guards_preserve_native_scope(self):
        original = kit.theme()
        first = kit.provide(original)
        second = kit.provide(original)
        self.assertFalse(first.active)
        first.__enter__()
        second.__enter__()
        with self.assertRaisesRegex(RuntimeError, "reverse nesting"):
            first.close()
        second.close()
        errors = []

        def wrong_thread():
            try:
                first.close()
            except RuntimeError as error:
                errors.append(str(error))

        worker = threading.Thread(target=wrong_thread)
        worker.start()
        worker.join()
        self.assertEqual(len(errors), 1)
        self.assertIn("owning thread", errors[0])
        self.assertTrue(first.active)
        first.close()
        first.close()
        self.assertFalse(first.active)
        with self.assertRaisesRegex(RuntimeError, "only be entered once"):
            first.__enter__()
        self.assertEqual(kit.theme(), original)

    def test_optional_nested_values_survive_parent_replacement(self):
        well = kit.Well(content=kit.WellContent(across="end"))
        held = well.content
        well.content = None
        self.assertIsNotNone(held)
        held.across = "center"
        self.assertIsNone(well.content)
        well.content = held
        self.assertEqual(well.content.across, held.across)

    def test_abandoned_provider_on_a_finished_thread_is_closed(self):
        held = []

        def worker():
            provider = kit.provide(kit.house_theme())
            provider.__enter__()
            held.append(provider)

        thread = threading.Thread(target=worker)
        thread.start()
        thread.join()
        self.assertFalse(held[0].active)

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
            neutral.sheet(content, title="Neutral", margin_x=20), Element
        )
        with self.assertRaisesRegex(TypeError, "unknown Page property 'titlle'"):
            kit.page(content, titlle="typo")

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


@sketch(size=(160, 80), capture_at=0.05)
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
                        (box().key("rail").area("rail")),
                        (box().key("body").area("body")),
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
        self.assertEqual(props[0].noteGap, 4)
        props[0].noteGap = 18
        self.assertEqual(voice.noteGap, 900)

    def test_stage_uses_the_native_context_and_scoped_theme(self):
        self.render("""from sigil.compose import text
from sigil.sketch import kit, sketch


@sketch(size=(8, 8), capture_at=0)
class Sheet:
    def setup(self, ctx):
        look = kit.house_theme()
        look.palette.ground = "#123456"
        with kit.provide(look):
            kit.stage(ctx, size=(160, 100), capture_at=0, oversample=1)
            assert ctx.width == 160 and ctx.height == 100
            ctx.render(kit.page(text("Stage"), title="Native page"))
""")

    def test_callback_boundary_closes_an_abandoned_theme_provider(self):
        original = kit.theme()
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_kit_scope", None))
        self.render("""import builtins
from sigil.compose import box
from sigil.sketch import kit, sketch


@sketch(size=(32, 32), capture_at=0)
class LeakedScope:
    def setup(self, ctx):
        look = kit.house_theme()
        look.palette.ink = "#ff0000"
        builtins._sigil_kit_scope = kit.provide(look)
        builtins._sigil_kit_scope.__enter__()
        ctx.render((box().width(20).height(20)))
""")
        self.assertFalse(builtins._sigil_kit_scope.active)
        self.assertEqual(kit.theme(), original)

    def test_memo_restores_the_native_theme_after_the_author_scope_has_closed(self):
        original = kit.theme()
        builtins._sigil_kit_memo = []
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_kit_memo", None))
        self.render("""import builtins
from sigil.compose import box, memo
from sigil.sketch import kit, sketch


def describe(properties):
    builtins._sigil_kit_memo.append(kit.theme().type.title.size)
    return box().width(20).height(20).fill("#abcdef")


@sketch(size=(32, 32), capture_at=0)
class Deferred:
    def setup(self, ctx):
        look = kit.house_theme()
        look.type.title.size = 37
        with kit.provide(look):
            tree = memo(("held",), describe)
        assert kit.theme().type.title.size != 37
        ctx.render(tree)
""")
        self.assertEqual(builtins._sigil_kit_memo, [37])
        self.assertEqual(kit.theme(), original)

    def test_memo_cannot_close_a_provider_from_its_restored_environment(self):
        original = kit.theme()
        builtins._sigil_kit_scope_close = []
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_kit_scope_close", None))
        self.render("""import builtins
from sigil.compose import box, memo
from sigil.sketch import kit, sketch


@sketch(size=(8, 8), capture_at=0)
class DeferredClose:
    def setup(self, ctx):
        original = kit.theme()
        look = kit.house_theme()
        look.type.title.size = 99
        results = builtins._sigil_kit_scope_close
        with kit.provide(look) as provider:

            def describe(model):
                try:
                    provider.close()
                except RuntimeError as error:
                    results.append(str(error))
                else:
                    results.append(None)
                return box().width(8).height(8)

            ctx.render(memo(0, describe))
            results.append(provider.active)
            results.append(kit.theme() == look)
        results.append(kit.theme() == original)
""")
        rejection, still_active, still_scoped, restored = (
            builtins._sigil_kit_scope_close
        )
        self.assertIsInstance(rejection, str)
        self.assertTrue(still_active)
        self.assertTrue(still_scoped)
        self.assertTrue(restored)
        self.assertEqual(kit.theme(), original)

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
@sketch(size=(80, 48), capture_at=0)
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
