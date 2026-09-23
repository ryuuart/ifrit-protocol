"""The facts a node states, and the operators that read them.

A node states typed facts about itself and says nothing about who reads
them. An operator does the reading: an ARRANGING one places the node's
direct children during layout, by a fact rather than by an index, and an
ADDING one runs once layout has settled and builds elements from what it
finds. Both are values, and their equality is what lets the node they
were applied on prune.

The first cases ask the values everything that needs no layout: how a
table states a fact and reads it back in the type it was stated in, what
an operator is built from, what it refuses, and the equality rule a
Python operator lives under — a class that states an equality prunes its
node, and a plain one is the escape hatch that never does. The rest
draw, because an arrangement and a scope are answers of a layout and
layout runs inside a draw.
"""

import builtins
import copy
import dataclasses
import tempfile
import textwrap
import unittest
from pathlib import Path

from _sigil import compose as native
from sigil import compose, image, skia
from sigil.sketch import render_file

RESULTS = "_sigil_operator_results"

# Every name this package adds to the compose module.
NAMES = (
    "Arrangement",
    "Attributes",
    "Operator",
    "Scope",
    "drawWith",
    "point",
)


@dataclasses.dataclass(frozen=True)
class Stated:
    """An arranging operator whose class states an equality of its own."""

    inset: float = 4.0

    def arrange(self, arrangement):
        for child in arrangement.children:
            child.place(
                (self.inset, self.inset, child.size.width(), child.size.height())
            )


class Plain:
    """The same arrangement from a class that states none."""

    def __init__(self, inset=4.0):
        self.inset = inset

    def arrange(self, arrangement):
        for child in arrangement.children:
            child.place(
                (self.inset, self.inset, child.size.width(), child.size.height())
            )


class Values(unittest.TestCase):
    """The operator seam as values, before any layout has run."""

    def test_the_package_adds_these_names_to_the_compose_module(self):
        for name in NAMES:
            with self.subTest(name=name):
                self.assertTrue(hasattr(compose, name))
        self.assertTrue(hasattr(compose.connect, "Between"))
        self.assertTrue(hasattr(compose.connect, "ByLane"))

    def test_a_fact_reads_back_in_the_type_it_was_stated_in(self):
        facts = compose.Attributes()
        facts.set("tier", 2)
        facts.set("weight", 1.5)
        facts.set("name", "gateway")
        facts.set("calls", ["ledger", "cache"])
        facts.set("offsets", [1.5, -2.0])
        facts.set("live", True)
        facts.set("at", skia.Point(3, 4))
        self.assertEqual(facts.get("tier"), 2)
        self.assertEqual(facts.get("weight"), 1.5)
        self.assertEqual(facts.get("name"), "gateway")
        self.assertEqual(facts.get("calls"), ["ledger", "cache"])
        self.assertEqual(facts.get("offsets"), [1.5, -2.0])
        self.assertIs(facts.get("live"), True)
        self.assertEqual((facts.get("at").x, facts.get("at").y), (3, 4))
        # A name nothing stated is nothing, and asking is silent.
        self.assertIsNone(facts.get("missing"))
        self.assertFalse(facts.has("missing"))
        self.assertNotIn("missing", facts)
        self.assertIn("tier", facts)
        self.assertEqual(len(facts), 7)
        self.assertFalse(facts.empty())
        self.assertEqual(
            facts.names(),
            ["tier", "weight", "name", "calls", "offsets", "live", "at"],
        )
        # A later fact under the same name replaces the earlier one.
        facts.set("tier", 3)
        self.assertEqual(facts.get("tier"), 3)
        self.assertEqual(len(facts), 7)

    def test_a_bool_is_a_bool_and_a_whole_number_is_not_a_number(self):
        # The type is part of the fact, and Python's bool is its own type
        # rather than the whole number it also is.
        facts = compose.Attributes()
        facts.set("live", True)
        facts.set("tier", 2)
        self.assertIs(facts.get("live"), True)
        self.assertNotIsInstance(facts.get("tier"), bool)

    def test_a_sequence_of_numbers_is_a_list_and_a_point_is_a_point(self):
        # Two floats read as a point as readily as as a list, so only a
        # point states a point; an empty sequence has to pick, and picks
        # the list of strings.
        facts = compose.Attributes()
        facts.set("pair", [3.0, 4.0])
        facts.set("none", [])
        self.assertEqual(facts.get("pair"), [3.0, 4.0])
        self.assertEqual(facts.get("none"), [])

    def test_a_table_refuses_a_value_it_cannot_state(self):
        facts = compose.Attributes()
        for value in (None, object(), {"a": 1}, ["one", 2]):
            with self.subTest(value=value), self.assertRaises(TypeError):
                facts.set("bad", value)

    def test_two_tables_are_equal_in_any_order(self):
        first = compose.Attributes()
        first.set("tier", 2)
        first.set("name", "gateway")
        second = compose.Attributes()
        second.set("name", "gateway")
        second.set("tier", 2)
        third = compose.Attributes()
        third.set("tier", 2)
        third.set("name", "ledger")
        self.assertEqual(first, second)
        self.assertNotEqual(first, third)
        # A copy is its own table: writing to one leaves the other standing.
        clone = copy.copy(first)
        clone.set("tier", 9)
        self.assertEqual(first.get("tier"), 2)
        self.assertEqual(clone.get("tier"), 9)
        self.assertEqual(copy.deepcopy(first), first)

    def test_a_table_and_a_fact_reach_every_kind_of_node(self):
        facts = compose.Attributes()
        facts.set("tier", 1)
        for node in (
            compose.box(),
            compose.text("words"),
            compose.point(),
        ):
            with self.subTest(node=type(node).__name__):
                self.assertIs(node.attribute("tier", 1), node)
                self.assertIs(node.attributes(facts), node)
                self.assertIs(node.operators([Stated()]), node)

    def test_a_fact_refuses_anything_but_a_table_of_facts(self):
        for value in (None, {"tier": 1}, "facts"):
            with self.subTest(value=value), self.assertRaises(TypeError):
                compose.box().attributes(value)

    def test_an_operator_is_built_from_every_stock_value(self):
        for value in (
            compose.layouts.Radial(),
            compose.layouts.Grid(),
            compose.layouts.Diagonal(),
            compose.layouts.BaselineGrid(),
            compose.layouts.Jittered(),
            compose.layouts.Jitter(),
            compose.layouts.AlongPath(),
        ):
            with self.subTest(value=type(value).__name__):
                built = compose.Operator(value)
                self.assertTrue(built)
                self.assertTrue(built.arranges())
                self.assertFalse(built.adds())
        for value in (
            compose.connect.Between(from_="gateway", to="ledger"),
            compose.connect.ByLane(lane="calls"),
        ):
            with self.subTest(value=type(value).__name__):
                built = compose.Operator(value)
                self.assertTrue(built.adds())
                self.assertFalse(built.arranges())
                self.assertTrue(built.comparable())
        # An operator built from an operator is that same operator.
        one = compose.Operator(compose.layouts.Radial())
        self.assertEqual(compose.Operator(one), one)

    def test_an_operator_refuses_a_value_that_neither_arranges_nor_adds(self):
        class Neither:
            pass

        for value in (None, object(), Neither(), "radial"):
            with self.subTest(value=value), self.assertRaises(TypeError):
                compose.Operator(value)

    def test_an_operator_refuses_a_value_that_does_both(self):
        class Both:
            def arrange(self, arrangement):
                pass

            def add(self, scope):
                pass

        with self.assertRaises(TypeError):
            compose.Operator(Both())

    def test_an_operator_states_where_its_additions_paint_and_what_dresses_them(self):
        built = compose.Operator(compose.connect.ByLane(lane="calls"))
        self.assertIs(built.zIndex(-1), built)
        self.assertIs(built.styleClass("wire"), built)
        # The properties take part in equality, so an operator dressed
        # differently is a different operator.
        plain = compose.Operator(compose.connect.ByLane(lane="calls"))
        self.assertNotEqual(built, plain)
        self.assertEqual(
            compose.Operator(compose.connect.ByLane(lane="calls"))
            .zIndex(-1)
            .styleClass("wire"),
            built,
        )

    def test_a_python_operator_is_equal_only_where_its_class_states_an_equality(self):
        self.assertEqual(compose.Operator(Stated()), compose.Operator(Stated()))
        self.assertNotEqual(
            compose.Operator(Stated()), compose.Operator(Stated(inset=9.0))
        )
        # A class that states none equals nothing but its own copies.
        one = compose.Operator(Plain())
        self.assertNotEqual(one, compose.Operator(Plain()))
        self.assertEqual(one, copy.copy(one))
        self.assertTrue(one.comparable())

    def test_an_operator_asks_for_the_content_minima_only_where_it_says_so(self):
        class Measuring:
            readsChildMinSizes = True

            def arrange(self, arrangement):
                pass

        self.assertTrue(compose.Operator(Measuring()).readsChildMinSizes())
        self.assertFalse(compose.Operator(Plain()).readsChildMinSizes())

    def test_a_program_that_is_not_callable_is_refused(self):
        for value in (None, 3, "program"):
            with self.subTest(value=value), self.assertRaises(TypeError):
                compose.drawWith(value)
        self.assertTrue(compose.drawWith(lambda pen, scope: None).adds())
        self.assertTrue(compose.drawWith("traffic", lambda pen, scope: None).adds())

    def test_a_layout_takes_a_stock_scheme_an_operator_and_a_python_operator(self):
        child = compose.box().key("one").width(4).height(4)
        for scheme in (
            compose.layouts.Radial(),
            compose.Operator(compose.layouts.Radial()),
            Stated(),
        ):
            with self.subTest(scheme=type(scheme).__name__):
                self.assertIsInstance(compose.layout(scheme, child), compose.Element)
                self.assertIsInstance(compose.layout(scheme, [child]), compose.Element)

    def test_a_node_carrying_an_operator_prunes_only_where_the_operator_is_equal(self):
        # The whole tree is described twice, identically, so the only
        # node that can patch is the one the operator was applied on.
        def tree(operator):
            return (
                compose.box()
                .key("root")
                .width(80)
                .height(80)
                .operators([operator])
                .children([compose.box().key("one").width(8).height(8)])
            )

        for factory, patched in ((Stated, 0), (Plain, 1)):
            with self.subTest(operator=factory.__name__):
                composer = compose.Composer()
                composer.setSize((80, 80))
                composer.render(tree(factory()))
                composer.render(tree(factory()))
                self.assertEqual(composer.stats().patchedNodes, patched)


class Session(unittest.TestCase):
    """A scene rendered through the native host, which lends the canvas."""

    SIZE = (200, 200)

    PRELUDE = textwrap.dedent(
        f"""
        import builtins
        import math
        from _sigil import compose
        from sigil import skia, weave
        from sigil.sketch import sketch
        results = builtins.{RESULTS}

        def dot(hour):
            return (
                compose.box().key('h%d' % hour).attribute('hour', hour)
                .width(10).height(10).fill('#ff0000')
            )

        def node(key, left, top):
            return (
                compose.box().key(key).absolute().left(left).top(top)
                .width(10).height(10).fill('#0000ff')
            )

        def show(pen, tree):
            # An arrangement and a scope are answers of a layout, and the
            # first pass may read a rect still settling, so the tree is
            # drawn once to settle and once to be read.
            probe = compose.Composer()
            probe.setSize({SIZE!r})
            probe.render(tree)
            probe.draw(pen.canvas())
            pen.background('#000000')
            probe.draw(pen.canvas())
            return probe

        def centre(probe, key):
            rect = probe.bounds(key)
            return None if rect is None else (rect.centerX(), rect.centerY())
        """
    )

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="sigil_operators_")
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        setattr(builtins, RESULTS, self.results)
        self.addCleanup(delattr, builtins, RESULTS)

    def render(self, body, prelude=""):
        """Render @p body as a sketch's methods and answer the decoded frame."""
        source = Path(self.directory.name) / "scene.py"
        output = Path(self.directory.name) / "scene.png"
        source.write_text(
            self.PRELUDE
            + textwrap.dedent(prelude)
            + f"\n@sketch(size={self.SIZE!r}, background='#000000', capture_at=0)\n"
            "class Scene:\n"
            + textwrap.indent(textwrap.dedent(body).strip("\n"), "    ")
            + "\n"
        )
        render_file(source, output, at=0)
        return image.decode(output.read_bytes())

    def frame(self, drawing, prelude=""):
        """Render @p drawing as the body of a sketch's draw."""
        return self.render(
            "def draw(self, pen):\n"
            "    pen.background('#000000')\n"
            + textwrap.indent(textwrap.dedent(drawing).strip("\n"), "    "),
            prelude,
        )

    def assertNear(self, actual, expected):
        """Every number of @p actual beside its peer, within a pixel."""
        self.assertIsNotNone(actual)
        for value, wanted in zip(actual, expected, strict=True):
            self.assertAlmostEqual(value, wanted, delta=1.0)

    def green(self, picture, x, y):
        """Whether the canvas is pen green at @p x, @p y, at any density."""
        column = x * picture.width() // self.SIZE[0]
        row = y * picture.height() // self.SIZE[1]
        offset = 4 * (row * picture.width() + column)
        red, greenness, blue = picture.rgba()[offset : offset + 3]
        return greenness > 200 and red < 80 and blue < 80


@unittest.skipUnless(hasattr(native, "Composer"), "no composer to read a layout from")
class Arranging(Session):
    def test_an_arranging_operator_places_by_a_fact_not_by_index(self):
        # Written out of order and with hours missing: 3 lands at three
        # o'clock whatever its place in the list, which a scheme told only
        # the index could not do.
        self.frame(
            """
            probe = show(pen, compose.box().width(200).height(200)
                .operators([AroundRing()])
                .children([dot(6), dot(3), dot(12)]))
            for key in ('h12', 'h3', 'h6'):
                results[key] = centre(probe, key)
            """,
            """
            class AroundRing:
                fraction = 0.8

                def arrange(self, arrangement):
                    box = arrangement.box
                    middle = (box.centerX(), box.centerY())
                    radius = min(middle) * self.fraction
                    for child in arrangement.children:
                        hour = child.number('hour') or 0.0
                        degrees = hour * 30.0 - 90.0
                        radians = math.radians(degrees)
                        child.centreAt((middle[0] + radius * math.cos(radians),
                                        middle[1] + radius * math.sin(radians)))
                        child.turn(degrees + 90.0)
            """,
        )
        self.assertNear(self.results["h12"], (100, 20))
        self.assertNear(self.results["h3"], (180, 100))
        self.assertNear(self.results["h6"], (100, 180))

    def test_a_later_operator_sees_where_the_earlier_one_left_each_child(self):
        self.frame(
            """
            probe = show(pen, compose.box().width(200).height(200)
                .operators([Corner(), Nudge()])
                .children([dot(3)]))
            results['h3'] = centre(probe, 'h3')
            results['read'] = builtins.__dict__.get('_operator_read')
            """,
            """
            class Corner:
                def arrange(self, arrangement):
                    for child in arrangement.children:
                        child.place((20, 20, child.size.width(), child.size.height()))

            class Nudge:
                def arrange(self, arrangement):
                    for child in arrangement.children:
                        builtins._operator_read = (child.rect.left(), child.rect.top())
                        child.rect = child.rect.MakeXYWH(
                            child.rect.left() + 7, child.rect.top() - 3,
                            child.rect.width(), child.rect.height())
            """,
        )
        self.assertEqual(self.results["read"], (20.0, 20.0))
        self.assertNear(self.results["h3"], (32, 22))

    def test_an_arrangement_reports_the_children_it_measured(self):
        self.frame(
            """
            probe = show(pen, compose.box().width(200).height(200)
                .operators([Report()]).children([dot(1), dot(2)]))
            """,
            """
            class Report:
                def arrange(self, arrangement):
                    child = arrangement.children[0]
                    results['count'] = len(arrangement.children)
                    results['box'] = (arrangement.box.width(), arrangement.box.height())
                    results['size'] = (child.size.width(), child.size.height())
                    results['area'] = child.area
                    results['fact'] = child.attribute('hour')
                    results['turn'] = child.turnDegrees
                    results['minSizesMeasured'] = arrangement.minSizesMeasured
                    results['facts'] = child.attributes.names()
            """,
        )
        self.assertEqual(self.results["count"], 2)
        self.assertEqual(self.results["box"], (200.0, 200.0))
        self.assertEqual(self.results["size"], (10.0, 10.0))
        self.assertEqual(self.results["area"], "")
        self.assertEqual(self.results["fact"], 1)
        self.assertEqual(self.results["turn"], 0.0)
        self.assertFalse(self.results["minSizesMeasured"])
        self.assertEqual(self.results["facts"], ["hour"])

    def test_an_operator_that_asks_is_handed_the_content_minima(self):
        self.frame(
            """
            show(pen, compose.box().width(200).height(200)
                .operators([Measuring()]).children([dot(1)]))
            """,
            """
            class Measuring:
                readsChildMinSizes = True

                def arrange(self, arrangement):
                    results['minSizesMeasured'] = arrangement.minSizesMeasured
                    results['minSize'] = (arrangement.children[0].minSize.width(),
                                          arrangement.children[0].minSize.height())
            """,
        )
        self.assertTrue(self.results["minSizesMeasured"])
        self.assertEqual(self.results["minSize"], (10.0, 10.0))

    def test_an_arrangement_refuses_every_reading_once_its_call_has_returned(self):
        self.frame(
            """
            show(pen, compose.box().width(200).height(200)
                .operators([Escape()]).children([dot(1)]))
            try:
                builtins._operator_escaped.box
            except RuntimeError as refusal:
                results['refusal'] = str(refusal)
            try:
                builtins._operator_escaped_child.size
            except RuntimeError as refusal:
                results['childRefusal'] = str(refusal)
            """,
            """
            class Escape:
                def arrange(self, arrangement):
                    builtins._operator_escaped = arrangement
                    builtins._operator_escaped_child = arrangement.children[0]
            """,
        )
        self.assertIn("no longer inside", self.results["refusal"])
        self.assertIn("no longer inside", self.results["childRefusal"])


@unittest.skipUnless(hasattr(native, "Composer"), "no composer to read a layout from")
class Adding(Session):
    def test_an_adding_operator_attaches_to_a_node_and_to_the_scope(self):
        self.frame(
            """
            probe = show(pen, compose.box().width(200).height(200)
                .applyStyleSheet(compose.StyleSheet([compose.rule('.service')]))
                .operators([Label()])
                .children([node('gateway', 20, 30).styleClass('service'),
                           node('ledger', 120, 90)]))
            results['tag'] = centre(probe, 'tag')
            results['mark'] = centre(probe, 'mark')
            """,
            """
            class Label:
                def add(self, scope):
                    results['keys'] = [one.key for one in scope.nodes()]
                    results['box'] = (scope.box.width(), scope.box.height())
                    results['absent'] = scope.find('missing')
                    results['classes'] = scope.nodes()[0].classes
                    results['dressed'] = [one.key for one in scope.withClass('service')]
                    gateway = scope.find('gateway')
                    results['bounds'] = (gateway.bounds.left(), gateway.bounds.top())
                    local = gateway.toLocal((30, 45))
                    results['local'] = (local.x, local.y)
                    results['outline'] = gateway.outline.isEmpty()
                    results['hasClass'] = (gateway.hasClass('service'),
                                           gateway.hasClass('other'))
                    # About one node, attached to it, in its own coordinates.
                    gateway.attach(compose.box().key('tag').absolute()
                        .left(2).top(2).width(4).height(4).fill('#00ff00'))
                    # About the scope, attached to it, in the scope's.
                    scope.attach(compose.box().key('mark').absolute()
                        .left(60).top(60).width(4).height(4).fill('#00ff00'))
            """,
        )
        self.assertEqual(self.results["keys"], ["gateway", "ledger"])
        self.assertEqual(self.results["box"], (200.0, 200.0))
        self.assertIsNone(self.results["absent"])
        self.assertEqual(self.results["classes"], ["service"])
        self.assertEqual(self.results["dressed"], ["gateway"])
        self.assertEqual(self.results["bounds"], (20.0, 30.0))
        self.assertEqual(self.results["local"], (10.0, 15.0))
        self.assertFalse(self.results["outline"])
        self.assertEqual(self.results["hasClass"], (True, False))
        self.assertNear(self.results["tag"], (24, 34))
        self.assertNear(self.results["mark"], (62, 62))

    def test_a_lane_wires_every_pairing_the_nodes_state(self):
        self.frame(
            """
            gateway = (node('gateway', 20, 30)
                .attribute('calls', ['ledger', 'missing']))
            probe = show(pen, compose.box().width(200).height(200)
                .operators([compose.Operator(compose.connect.ByLane(
                    lane='calls', wire=compose.stroke(3, '#ff0000'))).zIndex(-1)])
                .children([gateway, node('ledger', 120, 90)]))
            wire = probe.bounds('gateway->ledger')
            results['wire'] = None if wire is None else (wire.width() > 0, wire.height() > 0)
            results['missing'] = probe.bounds('gateway->missing')
            """
        )
        self.assertEqual(self.results["wire"], (True, True))
        # A name the scope does not carry is skipped, silently.
        self.assertIsNone(self.results["missing"])

    def test_a_stated_pairing_wires_the_two_keys_it_names(self):
        self.frame(
            """
            probe = show(pen, compose.box().width(200).height(200)
                .operators([compose.connect.Between(
                    from_='gateway', to='ledger',
                    wire=compose.stroke(3, '#ff0000'))])
                .children([node('gateway', 20, 30), node('ledger', 120, 90)]))
            wire = probe.bounds('gateway->ledger')
            results['wire'] = None if wire is None else (wire.width() > 0, wire.height() > 0)
            """
        )
        self.assertEqual(self.results["wire"], (True, True))

    def test_a_program_draws_the_scope_with_one_pen(self):
        picture = self.frame(
            """
            show(pen, compose.box().width(200).height(200)
                .operators([compose.drawWith('traffic', drawTraffic)])
                .children([node('gateway', 20, 20), node('ledger', 160, 160)]))
            """,
            """
            def drawTraffic(pen, scope):
                results['drawn'] = [one.key for one in scope.nodes()]
                results['box'] = (scope.box.width(), scope.box.height())
                pen.stroke('#00ff00')
                pen.strokeWeight(6)
                first, second = scope.nodes()
                pen.line(first.bounds.centerX(), first.bounds.centerY(),
                         second.bounds.centerX(), second.bounds.centerY())
            """,
        )
        self.assertEqual(self.results["drawn"], ["gateway", "ledger"])
        self.assertEqual(self.results["box"], (200.0, 200.0))
        self.assertTrue(self.green(picture, 100, 100))
        self.assertFalse(self.green(picture, 100, 20))

    def test_a_program_naming_only_the_pen_is_still_drawn_with(self):
        picture = self.frame(
            """
            show(pen, compose.box().width(200).height(200)
                .operators([compose.drawWith(drawBand)])
                .children([node('gateway', 20, 20)]))
            """,
            """
            def drawBand(pen):
                pen.noStroke()
                pen.fill('#00ff00')
                pen.rect(0, 100, 200, 20)
            """,
        )
        self.assertTrue(self.green(picture, 100, 110))


if __name__ == "__main__":
    unittest.main()
