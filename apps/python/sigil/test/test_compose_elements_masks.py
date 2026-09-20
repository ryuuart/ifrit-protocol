"""Masks: a selection of a node's paint, and the gate it arrives through.

A mask pairs which of a node's outputs it applies to with how that paint
is cut, and the two are independent values. The cases below ask the
region, the selection and the gate everything that needs no canvas:
what each factory builds, what compares, what a read hands back and what
a copy shares. They then go to a session for what only pixels can say:
that a gate is a show set, that its complement is the other half, that
two masks intersect, and that a selection leaves the rest of the node
alone. The last case keeps a gate past the session it was made in.
"""

import builtins
import copy
import gc
import tempfile
import textwrap
import unittest
from pathlib import Path

from _sigil import compose as native
from sigil import compose, image, material, motion, skia
from sigil.compose import box, spans
from sigil.sketch import render_file

by = native.by
parts = native.parts
Gate = native.Gate
Parts = native.Parts
Region = native.Region
Paint = material.Paint

# Every gate factory, with the kind, side and channel it builds.
COVERAGE_GATES = (
    ("alpha", False, "Alpha"),
    ("alphaOut", True, "Alpha"),
    ("luma", False, "Luma"),
    ("lumaOut", True, "Luma"),
)


class Spellings(unittest.TestCase):
    def test_the_package_names_what_the_extension_registers(self):
        for name in ("Region", "Parts", "Gate"):
            with self.subTest(name=name):
                self.assertIs(getattr(compose, name), getattr(native, name))
        self.assertIs(compose.by.edge, by.edge)
        self.assertIs(compose.parts.named, parts.named)

    def test_each_name_reports_the_module_an_author_imports(self):
        for value in (
            Region,
            Region.Kind,
            Parts,
            Parts.Bits,
            Gate,
            Gate.Kind,
            Gate.Channel,
        ):
            with self.subTest(value=value.__qualname__):
                self.assertEqual(value.__module__, "sigil.compose")
        self.assertEqual(by.edge.__module__, "sigil.compose.by")
        self.assertEqual(parts.named.__module__, "sigil.compose.parts")

    def test_the_modules_carry_the_native_factories(self):
        for name in ("all", "marks", "surface", "content", "children", "named"):
            with self.subTest(name=name):
                self.assertTrue(callable(getattr(parts, name)))
        for name in (
            "spans",
            "edge",
            "shape",
            "outside",
            "alpha",
            "alphaOut",
            "luma",
            "lumaOut",
        ):
            with self.subTest(name=name):
                self.assertTrue(callable(getattr(by, name)))

    def test_the_nested_enumerations_carry_the_native_values(self):
        self.assertEqual(list(Region.Kind.__members__), ["Own", "Rect", "Oval", "Path"])
        self.assertEqual(
            list(Parts.Bits.__members__),
            ["kSurface", "kMarks", "kContent", "kChildren", "kAll"],
        )
        self.assertEqual(
            list(Gate.Kind.__members__), ["Spans", "Edge", "Shape", "Coverage"]
        )
        self.assertEqual(list(Gate.Channel.__members__), ["Alpha", "Luma"])


class Regions(unittest.TestCase):
    def test_each_factory_builds_its_kind(self):
        self.assertEqual(Region().kind(), Region.Kind.Own)
        self.assertEqual(Region.own().kind(), Region.Kind.Own)
        self.assertEqual(Region.rect((0, 0, 10, 10)).kind(), Region.Kind.Rect)
        self.assertEqual(Region.oval((0, 0, 10, 10)).kind(), Region.Kind.Oval)
        self.assertEqual(
            Region.path(skia.Path.Circle(5, 5, 5)).kind(), Region.Kind.Path
        )

    def test_a_rect_is_a_rect_value_or_four_numbers_and_takes_its_name(self):
        written = Region.rect((2, 3, 10, 20))
        self.assertEqual(written, Region.rect([2, 3, 10, 20]))
        self.assertEqual(written, Region.rect(skia.Rect(2, 3, 10, 20)))
        self.assertEqual(written, Region.rect(bounds=(2, 3, 10, 20)))
        self.assertEqual(
            Region.oval(bounds=(2, 3, 10, 20)), Region.oval((2, 3, 10, 20))
        )
        with self.assertRaises((TypeError, RuntimeError)):
            Region.rect("wide")

    def test_regions_compare_by_kind_and_by_what_the_kind_reads(self):
        self.assertEqual(Region.own(), Region.own())
        self.assertEqual(Region(), Region.own())
        self.assertNotEqual(Region.own(), Region.rect((0, 0, 10, 10)))
        self.assertNotEqual(Region.rect((0, 0, 10, 10)), Region.oval((0, 0, 10, 10)))
        self.assertNotEqual(Region.rect((0, 0, 10, 10)), Region.rect((0, 0, 10, 11)))
        circle = skia.Path.Circle(5, 5, 5)
        self.assertEqual(Region.path(circle), Region.path(path=circle))
        self.assertNotEqual(Region.path(circle), Region.path(skia.Path.Circle(5, 5, 6)))

    def test_a_region_resolves_to_the_path_it_covers(self):
        own = skia.Path.Rect((0, 0, 40, 30))
        bounds = Region.own().resolve(own).getBounds()
        self.assertEqual((bounds.width(), bounds.height()), (40, 30))
        bounds = Region.rect((2, 3, 10, 20)).resolve(ownShape=own).getBounds()
        self.assertEqual(
            (bounds.left(), bounds.top(), bounds.right(), bounds.bottom()),
            (2, 3, 12, 23),
        )
        oval = Region.oval((0, 0, 20, 20)).resolve(own)
        self.assertTrue(oval.contains(10, 10))
        self.assertFalse(oval.contains(1, 1))
        circle = Region.path(skia.Path.Circle(5, 5, 5)).resolve(own)
        self.assertTrue(circle.contains(5, 5))
        self.assertFalse(circle.contains(20, 20))

    def test_a_region_copies_into_an_equal_value_of_its_own(self):
        region = Region.rect((0, 0, 10, 10))
        for duplicate in (region.copy(), copy.copy(region), copy.deepcopy(region)):
            self.assertIsNot(duplicate, region)
            self.assertEqual(duplicate, region)


class Selections(unittest.TestCase):
    def test_each_factory_holds_its_bit(self):
        bits = Parts.Bits
        self.assertEqual(parts.surface().bits, int(bits.kSurface))
        self.assertEqual(parts.marks().bits, int(bits.kMarks))
        self.assertEqual(parts.content().bits, int(bits.kContent))
        self.assertEqual(parts.children().bits, int(bits.kChildren))
        self.assertEqual(parts.all().bits, int(bits.kAll))
        self.assertEqual(
            int(bits.kAll),
            int(bits.kSurface | bits.kMarks | bits.kContent | bits.kChildren),
        )

    def test_a_named_selection_holds_no_bit_and_one_label(self):
        named = parts.named("rule")
        self.assertEqual(named.bits, 0)
        self.assertEqual(named.names, ["rule"])
        self.assertEqual(named, parts.named(name="rule"))
        self.assertTrue(named.selectsMark("rule"))
        self.assertFalse(named.selectsMark("other"))
        self.assertFalse(named.selectsMark(""))
        self.assertFalse(named.selects(Parts.Bits.kMarks))

    def test_a_union_joins_bits_and_appends_names(self):
        union = parts.content() | parts.surface() | parts.named("a") | parts.named("b")
        self.assertTrue(union.selects(Parts.Bits.kContent))
        self.assertTrue(union.selects(what=Parts.Bits.kSurface))
        self.assertFalse(union.selects(Parts.Bits.kChildren))
        self.assertEqual(union.names, ["a", "b"])
        self.assertFalse(union.isEverything())
        self.assertTrue((union | parts.all()).isEverything())

    def test_the_marks_bit_reaches_every_label(self):
        self.assertTrue(parts.marks().selectsMark("anything"))
        self.assertTrue(parts.marks().selectsMark(label=""))
        self.assertFalse(parts.surface().selectsMark("anything"))

    def test_a_selection_is_built_from_its_fields_and_compares_by_them(self):
        self.assertEqual(Parts(), Parts(bits=0, names=[]))
        self.assertEqual(Parts(Parts.Bits.kMarks), parts.marks())
        self.assertEqual(Parts(bits=0, names=["rule"]), parts.named("rule"))
        self.assertNotEqual(parts.marks(), parts.surface())
        self.assertNotEqual(parts.named("a"), parts.named("b"))
        with self.assertRaises(TypeError):
            Parts(bits="marks")

    def test_names_read_as_a_copy_and_are_assigned_whole(self):
        selection = parts.named("rule")
        selection.names.append("lost")
        self.assertEqual(selection.names, ["rule"])
        selection.names = ["rule", "kept"]
        self.assertEqual(selection.names, ["rule", "kept"])
        selection.bits = int(Parts.Bits.kContent)
        self.assertTrue(selection.selects(Parts.Bits.kContent))

    def test_a_selection_copies_into_an_equal_value_of_its_own(self):
        selection = parts.content() | parts.named("rule")
        for duplicate in (
            selection.copy(),
            copy.copy(selection),
            copy.deepcopy(selection),
        ):
            self.assertIsNot(duplicate, selection)
            self.assertEqual(duplicate, selection)
        duplicate = selection.copy()
        duplicate.names = []
        self.assertEqual(selection.names, ["rule"])


class Gates(unittest.TestCase):
    def test_a_gate_is_made_by_a_factory_alone(self):
        with self.assertRaises(TypeError):
            Gate()

    def test_a_spans_gate_carries_three_numbers_a_term(self):
        gate = by.spans(spans.upTo(0.5))
        self.assertEqual(gate.kind, Gate.Kind.Spans)
        self.assertEqual(gate.valueCount(), 3)
        both = by.spans(where=spans.upTo(0.25) | spans.range(0.5, 0.75))
        self.assertEqual(both.valueCount(), 6)
        self.assertIsInstance(both.where, compose.Spans)

    def test_an_edge_gate_carries_its_angle_and_one_number(self):
        gate = by.edge(90, 0.3)
        self.assertEqual(gate.kind, Gate.Kind.Edge)
        self.assertEqual(gate.angleDeg, 90)
        self.assertAlmostEqual(gate.fraction.value, 0.3)
        self.assertEqual(gate.valueCount(), 1)
        self.assertEqual(gate, by.edge(angleDeg=90, fraction=0.3))
        self.assertNotEqual(gate, by.edge(90, 0.4))
        self.assertNotEqual(gate, by.edge(180, 0.3))

    def test_an_edge_fraction_keeps_the_output_it_was_bound_to(self):
        source = motion.Output(0.25)
        gate = by.edge(0, source)
        self.assertAlmostEqual(gate.fraction.value, 0.25)
        source.set(0.75)
        self.assertAlmostEqual(gate.fraction.value, 0.75)
        retained = gate.fraction
        gate.fraction = 1.0
        self.assertAlmostEqual(gate.fraction.value, 1.0)
        gate.fraction = retained
        self.assertAlmostEqual(gate.fraction.value, 0.75)
        with self.assertRaises((TypeError, RuntimeError)):
            gate.fraction = "half"

    def test_a_shape_gate_and_its_complement_differ_only_by_side(self):
        region = Region.rect((0, 0, 10, 10))
        kept, cut = by.shape(region), by.outside(region=region)
        self.assertEqual(kept.kind, Gate.Kind.Shape)
        self.assertEqual(cut.kind, Gate.Kind.Shape)
        self.assertFalse(kept.outside)
        self.assertTrue(cut.outside)
        self.assertEqual(kept.region, region)
        self.assertEqual(cut.region, region)
        self.assertEqual(kept.valueCount(), 0)
        self.assertNotEqual(kept, cut)
        self.assertEqual(kept, by.shape(Region.rect((0, 0, 10, 10))))
        self.assertNotEqual(kept, by.shape(Region.own()))

    def test_each_coverage_gate_names_its_side_and_its_channel(self):
        paint = Paint.solid("#ffffff")
        for name, outside, channel in COVERAGE_GATES:
            with self.subTest(name=name):
                gate = getattr(by, name)(paint)
                self.assertEqual(gate.kind, Gate.Kind.Coverage)
                self.assertEqual(gate.outside, outside)
                self.assertEqual(gate.channel, getattr(Gate.Channel, channel))
                self.assertEqual(gate.valueCount(), 0)
                self.assertEqual(gate.coverage, paint)
                self.assertEqual(gate, getattr(by, name)(coverage=paint))
        self.assertNotEqual(by.alpha(paint), by.alphaOut(paint))
        self.assertNotEqual(by.alpha(paint), by.luma(paint))
        self.assertNotEqual(by.alpha(paint), by.alpha(Paint.solid("#000000")))
        with self.assertRaises(TypeError):
            by.alpha("#ffffff")

    def test_a_gate_without_a_coverage_paint_reads_none(self):
        gate = by.edge(0, 0.5)
        self.assertIsNone(gate.coverage)
        matte = by.alpha(Paint.solid("#ffffff"))
        matte.coverage = None
        self.assertIsNone(matte.coverage)
        matte.coverage = Paint.solid("#ff0000")
        self.assertEqual(matte.coverage, Paint.solid("#ff0000"))

    def test_fields_read_as_copies_and_are_assigned_whole(self):
        gate = by.shape(Region.own())
        self.assertIsNot(gate.region, gate.region)
        gate.region = Region.oval((0, 0, 4, 4))
        self.assertEqual(gate.region.kind(), Region.Kind.Oval)
        gate.outside = True
        self.assertEqual(gate, by.outside(Region.oval((0, 0, 4, 4))))
        wipe = by.edge(0, 0.5)
        wipe.angleDeg = 270
        self.assertEqual(wipe, by.edge(270, 0.5))
        matte = by.alpha(Paint.solid("#ffffff"))
        matte.channel = Gate.Channel.Luma
        self.assertEqual(matte, by.luma(Paint.solid("#ffffff")))
        reveal = by.spans(spans.upTo(0.5))
        reveal.where = spans.upTo(0.25) | spans.range(0.5, 0.75)
        self.assertEqual(reveal.valueCount(), 6)

    def test_a_gate_copies_into_an_equal_value_of_its_own(self):
        gate = by.edge(90, 0.3)
        for duplicate in (gate.copy(), copy.copy(gate), copy.deepcopy(gate)):
            self.assertIsNot(duplicate, gate)
            self.assertEqual(duplicate, gate)
        duplicate = gate.copy()
        duplicate.angleDeg = 0
        self.assertEqual(gate.angleDeg, 90)


class Verb(unittest.TestCase):
    def test_both_forms_answer_the_element_they_were_called_on(self):
        element = box()
        self.assertIs(element.mask(by.edge(0, 0.5)), element)
        self.assertIs(element.mask(parts.surface(), by.shape(Region.own())), element)
        self.assertIs(element.mask(with_=by.edge(0, 0.5)), element)
        self.assertIs(
            element.mask(what=parts.content(), with_=by.outside(Region.own())),
            element,
        )

    def test_the_verb_follows_a_reshape_in_one_line(self):
        element = box().width(10).height(10).mask(by.shape(Region.own())).fill("#fff")
        self.assertIsInstance(element, compose.Element)

    def test_a_mask_is_a_gate_or_a_selection_and_a_gate(self):
        with self.assertRaises(TypeError):
            box().mask(parts.all())
        with self.assertRaises(TypeError):
            box().mask(by.edge(0, 0.5), parts.all())
        with self.assertRaises(TypeError):
            box().mask("surface", by.edge(0, 0.5))


class Session(unittest.TestCase):
    """A scene rendered through the native host, read back pixel by pixel."""

    SIZE = 40
    LEFT, RIGHT = (5, 20), (35, 20)
    TOP_LEFT, BOTTOM_LEFT = (5, 5), (5, 35)
    WHITE, BLACK = (255, 255, 255, 255), (0, 0, 0, 255)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        builtins._sigil_mask_results = self.results
        self.addCleanup(delattr, builtins, "_sigil_mask_results")

    def render(self, element):
        """Render @p element as the one child of a sketch's root."""
        size, half = self.SIZE, self.SIZE // 2
        source = Path(self.directory.name) / "scene.py"
        output = Path(self.directory.name) / "scene.png"
        source.write_text(
            "import builtins\n"
            "from _sigil import compose as native\n"
            "from sigil import material, motion\n"
            "from sigil.compose import box, spans\n"
            "from sigil.sketch import sketch\n"
            "by, parts = native.by, native.parts\n"
            "Region, Paint = native.Region, material.Paint\n"
            "results = builtins._sigil_mask_results\n"
            "def plate():\n"
            f"    return box().width({size}).height({size}).fill('#ffffff')\n"
            f"LEFT_HALF = Region.rect((0, 0, {half}, {size}))\n"
            f"TOP_HALF = Region.rect((0, 0, {size}, {half}))\n"
            f"@sketch(size=({size}, {size}), background='#000000', capture_at=0)\n"
            "class Scene:\n"
            "    def setup(self, ctx):\n"
            "        ctx.render(box().children([\n"
            + textwrap.indent(textwrap.dedent(element).strip(), "            ")
            + "\n        ]))\n"
        )
        render_file(source, output, at=0)
        self.picture = image.decode(output.read_bytes())

    def pixel(self, point):
        """The colour at a canvas point, at whatever density the frame has."""
        x, y = point
        column = x * self.picture.width() // self.SIZE
        row = y * self.picture.height() // self.SIZE
        offset = 4 * (row * self.picture.width() + column)
        return tuple(self.picture.rgba()[offset : offset + 4])

    def test_an_unmasked_plate_covers_the_frame(self):
        self.render("plate()")
        for point in (self.LEFT, self.RIGHT, self.TOP_LEFT, self.BOTTOM_LEFT):
            with self.subTest(point=point):
                self.assertEqual(self.pixel(point), self.WHITE)

    def test_an_edge_gate_shows_the_fraction_before_the_edge(self):
        self.render("plate().mask(by.edge(0, 0.5))")
        self.assertEqual(self.pixel(self.LEFT), self.WHITE)
        self.assertEqual(self.pixel(self.RIGHT), self.BLACK)

    def test_a_spans_gate_shows_the_run_of_the_boundary_it_names(self):
        self.render("plate().mask(by.spans(spans.upTo(1.0)))")
        self.assertEqual(self.pixel(self.LEFT), self.WHITE)
        self.assertEqual(self.pixel(self.RIGHT), self.WHITE)
        self.render("plate().mask(by.spans(spans.upTo(0.0)))")
        self.assertEqual(self.pixel(self.LEFT), self.BLACK)
        self.assertEqual(self.pixel(self.RIGHT), self.BLACK)

    def test_a_shape_gate_keeps_its_region_and_outside_keeps_the_rest(self):
        self.render("plate().mask(by.shape(LEFT_HALF))")
        self.assertEqual(self.pixel(self.LEFT), self.WHITE)
        self.assertEqual(self.pixel(self.RIGHT), self.BLACK)
        self.render("plate().mask(by.outside(LEFT_HALF))")
        self.assertEqual(self.pixel(self.LEFT), self.BLACK)
        self.assertEqual(self.pixel(self.RIGHT), self.WHITE)

    def test_two_masks_on_one_node_intersect(self):
        self.render("plate().mask(by.shape(LEFT_HALF)).mask(by.shape(TOP_HALF))")
        self.assertEqual(self.pixel(self.TOP_LEFT), self.WHITE)
        self.assertEqual(self.pixel(self.BOTTOM_LEFT), self.BLACK)
        self.assertEqual(self.pixel(self.RIGHT), self.BLACK)

    def test_a_selection_leaves_the_rest_of_the_node_alone(self):
        self.render("plate().mask(parts.children(), by.shape(LEFT_HALF))")
        self.assertEqual(self.pixel(self.RIGHT), self.WHITE)
        self.render("plate().mask(parts.surface(), by.shape(LEFT_HALF))")
        self.assertEqual(self.pixel(self.LEFT), self.WHITE)
        self.assertEqual(self.pixel(self.RIGHT), self.BLACK)

    def test_a_coverage_gate_keeps_what_its_paint_covers(self):
        for gate, shown in (
            ("by.alpha(Paint.solid('#ffffff'))", True),
            ("by.alphaOut(Paint.solid('#ffffff'))", False),
            ("by.alpha(Paint.solid((0, 0, 0, 0)))", False),
            ("by.alphaOut(Paint.solid((0, 0, 0, 0)))", True),
            ("by.luma(Paint.solid('#ffffff'))", True),
            ("by.luma(Paint.solid('#000000'))", False),
            ("by.lumaOut(Paint.solid('#000000'))", True),
            ("by.lumaOut(Paint.solid('#ffffff'))", False),
        ):
            with self.subTest(gate=gate):
                self.render(f"plate().mask({gate})")
                self.assertEqual(
                    self.pixel(self.LEFT), self.WHITE if shown else self.BLACK
                )

    def test_a_gate_outlives_the_session_it_was_made_in(self):
        self.render("""
            plate().mask(
                results.setdefault('gate', by.edge(0, motion.Output(0.5)))
            )
        """)
        gc.collect()
        gate = self.results["gate"]
        self.assertEqual(gate.kind, Gate.Kind.Edge)
        self.assertEqual(gate.valueCount(), 1)
        self.assertAlmostEqual(gate.fraction.value, 0.5)
        self.assertEqual(gate.copy().angleDeg, 0)
        self.assertIsInstance(box().mask(gate), compose.Element)
        self.assertEqual(self.pixel(self.LEFT), self.WHITE)
        self.assertEqual(self.pixel(self.RIGHT), self.BLACK)


if __name__ == "__main__":
    unittest.main()
