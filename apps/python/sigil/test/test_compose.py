"""Native composition, model reconciliation, and typography contracts."""

import builtins
import tempfile
import unittest
from pathlib import Path

from sigil import compose as raw
from sigil import image, skia, weave
from sigil.compose import Align, Dimension, Fill, box, pct, stroke
from sigil.motion import Output
from sigil.sketch import render_file


class Compose(unittest.TestCase):
    def render(self, body, *, at=0.08):
        with tempfile.TemporaryDirectory() as folder:
            source, output = Path(folder) / "scene.py", Path(folder) / "scene.png"
            source.write_text(body)
            render_file(source, output, at=at)
            return image.load(output).rgba()

    def setUp(self):
        builtins._sigil_compose_contract = []
        self.addCleanup(delattr, builtins, "_sigil_compose_contract")

    def test_native_relative_dimensions_and_property_types(self):
        self.assertEqual(Dimension(weave.em(2)).unit, Dimension.Unit.Em)
        self.assertEqual(Dimension("45%"), pct(45))
        self.assertEqual(Dimension("auto").unit, Dimension.Unit.Auto)
        font = weave.Type(size=24, color="#aa2211", track=weave.em(0.1))
        self.assertEqual(font.size.value, 24)
        self.assertEqual(font.track.unit, weave.Length.Unit.Em)
        self.assertIsInstance(
            (box().width(pct(50)).padding(weave.em(1)).alignItems(Align.End)).stroke(
                stroke(2)
            ),
            raw.Element,
        )
        self.assertIsInstance(raw.box().fill(Fill.currentInk()), raw.Element)
        with self.assertRaisesRegex(TypeError, "Unknown Type field"):
            weave.Type(szie=10)

    def test_edge_dimensions_take_their_names_at_every_arity(self):
        for element in (
            box().padding(all=12),
            box().margin(horizontal=12, vertical=6),
            box().margin(left=1, top=2, right=3, bottom=4),
        ):
            self.assertIsInstance(element, raw.Element)
        # One, two and four dimensions are the whole vocabulary.
        with self.assertRaises(TypeError):
            box().padding(1, 2, 3)
        self.render("""import builtins
from sigil.compose import box, row
from sigil.sketch import sketch


def bar(key, element):
    return element.key(key).children([box().width(10).height(10)])


@sketch(size=(200, 60), capture_at=0)
class Edges:
    def setup(self, ctx):
        ctx.render(
            row(
                bar("named", box().padding(left=3, top=5, right=7, bottom=9)),
                bar("placed", box().padding(3, 5, 7, 9)),
                bar("pair", box().margin(horizontal=4, vertical=2)),
                bar("both", box().margin(4, 2)),
            )
        )

    def update(self, elapsed, ctx):
        if elapsed > 1 / 60:
            builtins._sigil_compose_contract = [
                ctx.composer.bounds(key)
                for key in ("named", "placed", "pair", "both")
            ]
""")
        named, placed, pair, both = builtins._sigil_compose_contract
        self.assertEqual(
            (named.width(), named.height()), (placed.width(), placed.height())
        )
        self.assertEqual((pair.width(), pair.height()), (both.width(), both.height()))
        # The left and right dimensions reach the measured width: 3 + 10 + 7.
        self.assertEqual(named.width(), 20)
        # A margin stands outside the box, so the pair keeps the child's width.
        self.assertEqual(pair.width(), 10)

    def test_animatable_fields_roundtrip_without_losing_live_output(self):
        source = Output(0.25)
        path = raw.MotionPath(skia.Path.Circle(0, 0, 40), t=source)
        retained = path.t
        path.t = retained
        mark = stroke(2, "#ff0000")
        mark.trimPhase = retained
        mark.dashPhaseBinding = path.t
        source.set(0.75)
        self.assertAlmostEqual(mark.trimPhase.value, 0.75)
        self.assertEqual(mark.dashPhaseBinding, retained)
        del source, path
        self.assertAlmostEqual(mark.trimPhase.value, 0.75)
        self.assertTrue(mark.isAnimated())

    def test_alignment_strings_and_enums_share_native_layout(self):
        def render(mode):
            return self.render(
                f"""from sigil.compose import box
from sigil import compose as raw
from sigil.sketch import sketch


@sketch(size=(24, 16), background="#000000")
class Scene:
    def setup(self, ctx):
        children = [
            raw.box().size(4, 4).fill("#ff0000"),
            raw.box().size(4, 4).fill("#00ff00"),
        ]
        if {mode!r} == "enum":
            root = (
                raw.box()
                .size(24, 16)
                .row()
                .alignItems(raw.Align.Auto)
                .justify(raw.Justify.SpaceBetween)
                .children(children)
            )
        elif {mode!r} == "string":
            root = (
                raw.box()
                .size(24, 16)
                .row()
                .alignItems("auto")
                .justify("space_between")
                .children(children)
            )
        else:
            root = (
                box()
                .width(24)
                .height(16)
                .alignItems("auto")
                .justify("space_between")
                .children(children)
            ).row()
        ctx.render(root)
""",
                at=0,
            )

        expected = render("enum")
        colors = {expected[index : index + 4] for index in range(0, len(expected), 4)}
        self.assertIn(b"\xff\x00\x00\xff", colors)
        self.assertIn(b"\x00\xff\x00\xff", colors)
        self.assertEqual(render("string"), expected)
        self.assertEqual(render("convenience"), expected)
        for method in ("alignItems", "alignSelf", "justify"):
            with (
                self.subTest(method=method),
                self.assertRaisesRegex(ValueError, "Unknown"),
            ):
                getattr(raw.box(), method)("sideways")

    def test_every_spelling_of_no_fill_clears_a_standing_one(self):
        # Three ways to write "no fill", over a fill that is already set.
        # A conversion that only APPLIES its value leaves the red standing
        # for two of them, and the three spellings quietly disagree.
        def render(empty):
            return self.render(
                f"""from sigil.compose import Fill, box
from sigil.material import Paint
from sigil.sketch import sketch


@sketch(size=(8, 8), background="#0000ff")
class Scene:
    def setup(self, ctx):
        ctx.render(box().width(8).height(8).fill("#ff0000").fill({empty}))
""",
                at=0,
            )

        for empty in ("None", "Fill.none()", "Paint()"):
            with self.subTest(empty=empty):
                self.assertEqual(render(empty)[:4], b"\x00\x00\xff\xff")
        # …and a fill that is not empty still reaches the node.
        self.assertEqual(render('"#00ff00"')[:4], b"\x00\xff\x00\xff")

    def test_rules_merge_native_partials_without_resetting_other_fields(self):
        sheet = weave.StyleSheet(
            [
                weave.rule("title").font(weave.Type(size=32, color="#ff3300")),
                weave.rule("title").font(weave.Type(track=2)),
            ]
        )
        self.assertEqual(len(sheet), 1)
        self.assertEqual(sheet["title"].shaping.fontSize, 32)
        self.assertEqual(sheet["title"].shaping.letterSpacing, 2)
        copy = sheet.find("title")
        copy.font(weave.Type(size=50))
        self.assertEqual(sheet["title"].shaping.fontSize, 32)
        block = weave.Block(
            leading=weave.Leading.multiple(1.5), alignment=weave.TextAlignment.Center
        )
        self.assertEqual(block.leading.value, 1.5)

    def test_saved_optional_and_vector_style_values_survive_replacement(self):
        font = weave.Type(
            size=20,
            track=2,
            wordSpacing=3,
            variations=[weave.FontVariation("wght", 600)],
            features=[weave.FontFeature("liga", 0)],
        )
        size, track, space = font.size, font.track, font.wordSpacing
        axis, feature = font.variations[0], font.features[0]
        font.size = None
        font.track = 7
        font.wordSpacing = None
        font.variations = []
        font.features = None
        del font
        self.assertEqual((size.value, track.value, space.value), (20, 2, 3))
        self.assertEqual(
            (axis.tag, axis.value, feature.tag, feature.value), ("wght", 600, "liga", 0)
        )
        block = weave.Block(leading=weave.Leading.multiple(1.5))
        leading = block.leading
        block.leading = None
        del block
        self.assertEqual(leading.value, 1.5)
        style = weave.ShapingStyle(variations=[axis], fontFeatures=[feature])
        saved = style.variations[0]
        style.variations = []
        del style
        self.assertEqual(saved.value, 600)
        layers = raw.LayerStyle()
        layers.over = [raw.Decoration(stroke(2, "#ffaa88"))]
        mark = layers.over[0]
        layers.over = []
        del layers
        self.assertFalse(mark.isAnimated())

    def test_abandoned_constructor_theme_closes_before_setup(self):
        self.render(
            """import builtins
from sigil.compose import box
from sigil.sketch import sketch, kit


@sketch(size=(8, 8))
class Scene:
    def __init__(self):
        self.initial = kit.theme().type.title.size
        custom = kit.house_theme()
        custom.type.title.size = 99
        self.abandoned = kit.provide(custom)
        self.abandoned.__enter__()

    def setup(self, ctx):
        builtins._sigil_compose_contract.append(
            (self.initial, kit.theme().type.title.size)
        )
        ctx.render((box().width(8).height(8)))
""",
            at=0,
        )
        before, after = builtins._sigil_compose_contract[0]
        self.assertEqual(before, after)
        self.assertNotEqual(after, 99)

    def test_memo_equal_models_prune_and_changed_models_rebuild(self):
        self.render("""from dataclasses import dataclass
import builtins
from sigil.compose import box, memo
from sigil.sketch import sketch


@dataclass(frozen=True)
class Model:
    color: str


@sketch(size=(32, 32), background="#000000")
class Scene:
    def setup(self, ctx):
        self.frame = 0

    def update(self, elapsed, ctx):
        self.frame += 1
        color = "#ff0000" if self.frame < 3 else "#00ff00"

        def describe(model):
            builtins._sigil_compose_contract.append(model.color)
            return box().width(32).height(32).fill(model.color)

        ctx.render((memo(Model(color), describe).key("subject")))
""")
        self.assertEqual(builtins._sigil_compose_contract, ["#ff0000", "#00ff00"])

    def test_memo_copies_model_before_author_mutation(self):
        pixels = self.render(
            """from sigil.compose import box, memo
from sigil.sketch import sketch


@sketch(size=(8, 8), background="#000000")
class Scene:
    def setup(self, ctx):
        model = {"color": "#ff0000"}
        node = memo(model, lambda value: box().width(8).height(8).fill(value["color"]))
        model["color"] = "#00ff00"
        ctx.render(node)
""",
            at=0,
        )
        self.assertEqual(pixels[:4], bytes((255, 0, 0, 255)))

    def test_memo_builder_traceback_recovers_on_next_render(self):
        with self.assertRaisesRegex(RuntimeError, "component exploded"):
            self.render(
                """from sigil.compose import memo
from sigil.sketch import sketch


@sketch(size=(8, 8))
class Scene:
    def setup(self, ctx):
        def describe(model):
            raise ValueError("component exploded")

        ctx.render(memo(1, describe))
""",
                at=0,
            )
        pixels = self.render(
            """from sigil.compose import box
from sigil.sketch import sketch


@sketch(size=(8, 8))
class Scene:
    def setup(self, ctx):
        ctx.render((box().width(8).height(8).fill("#223344")))
""",
            at=0,
        )
        self.assertEqual(pixels[:4], bytes((34, 51, 68, 255)))

    def test_retained_model_and_bound_builder_release_session_cycles(self):
        import gc

        self.render(
            """import builtins
import weakref
from sigil.compose import box, memo
from sigil.sketch import sketch


class Model:
    def __init__(self, owner):
        self.owner = owner

    def __deepcopy__(self, memo):
        return self

    def __eq__(self, other):
        return self is other


@sketch(size=(8, 8))
class Scene:
    def setup(self, ctx):
        builtins._sigil_compose_contract.append(weakref.ref(self))
        self.node = memo(Model(self), self.describe)
        ctx.render(self.node)

    def describe(self, model):
        return box().width(8).height(8).fill("#cc8844")
""",
            at=0,
        )
        gc.collect()
        self.assertIsNone(builtins._sigil_compose_contract[0]())

    def test_model_equality_exception_reaches_the_host_with_traceback(self):
        with self.assertRaisesRegex(RuntimeError, "model equality exploded"):
            self.render("""from sigil.compose import box, memo
from sigil.sketch import sketch


class Model:
    def __eq__(self, other):
        raise ValueError("model equality exploded")


@sketch(size=(8, 8))
class Scene:
    def setup(self, ctx):
        pass

    def update(self, elapsed, ctx):
        ctx.render((memo(Model(), lambda value: box().width(8).height(8))))
""")

    def test_native_styles_and_explicit_total_style_render_identically(self):
        pixels = self.render(
            """from sigil.compose import box, text
from sigil import compose
from sigil.sketch import sketch
from sigil.weave import Type, StyleSheet, rule, textStyle


@sketch(size=(128, 48), background="#000000")
class Scene:
    def setup(self, ctx):
        style = Type(size=30, color="#ff0000")
        sheet = StyleSheet([rule("title").font(style)])
        children = [
            (text("HI").styleClass("title")),
            compose.text("HI", textStyle(style)),
        ]
        ctx.render(
            (
                box()
                .styleSheet(sheet)
                .children(
                    (
                        (
                            box()
                            .width(64)
                            .height(48)
                            .children(
                                [
                                    child,
                                ]
                            )
                        )
                        for child in children
                    )
                )
            ).row()
        )
""",
            at=0,
        )
        left = b"".join(pixels[y * 128 * 4 : (y * 128 + 64) * 4] for y in range(48))
        right = b"".join(
            pixels[(y * 128 + 64) * 4 : (y + 1) * 128 * 4] for y in range(48)
        )
        self.assertEqual(left, right)
        self.assertTrue(any(left[::4]))


if __name__ == "__main__":
    unittest.main()
