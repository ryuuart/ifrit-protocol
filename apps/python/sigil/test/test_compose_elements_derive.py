"""The derive vocabulary: the values that ask where a keyed node landed.

A router says what path runs between two ends, an anchor is one stop of a
wire, a tether is where a box hangs off another, and a band is the ribbon
a spine sweeps out. The first cases ask the values everything that needs
no layout: how each is built, called by name, compared, copied and
refused. The band cases draw, because a swept region is an answer of a
layout and layout runs inside a draw.

A router built over a function equals only its own copies, and one built
over a value with a `route` method compares by that value's own equality.
Both are retained against the session that made them, so the last cases
ask what each answers once that session has closed.
"""

import builtins
import copy
import gc
import tempfile
import textwrap
import unittest
from pathlib import Path

import _sigil
from _sigil import compose as native
from sigil import compose, geometry, image, skia
from sigil.sketch import render_file

RESULTS = "_sigil_derive_results"

# Every name this package adds to the compose module.
NAMES = (
    "Across",
    "Anchor",
    "RailRouter",
    "Router",
    "Tether",
    "across",
    "band",
    "bandPointAt",
)


def segment(from_, to):
    """A straight path between the centres of two rects."""
    return (
        skia.PathBuilder()
        .moveTo(from_.centerX(), from_.centerY())
        .lineTo(to.centerX(), to.centerY())
        .detach()
    )


def polyline(points):
    """A straight path through every point of an anchor run."""
    return skia.PathBuilder().addPolygon(points, False).detach()


def edges(rect):
    return (rect.left(), rect.top(), rect.width(), rect.height())


class Bowed:
    """A route scheme: a value with `route` and an equality of its own."""

    def __init__(self, bulge):
        self.bulge = bulge
        self.asked = []

    def __eq__(self, other):
        return isinstance(other, Bowed) and other.bulge == self.bulge

    def route(self, from_, to):
        self.asked.append((edges(from_), edges(to)))
        return segment(from_, to)


class Threaded:
    """A run-route scheme: the same, over a resolved anchor run."""

    def __init__(self, radius):
        self.radius = radius

    def __eq__(self, other):
        return isinstance(other, Threaded) and other.radius == self.radius

    def route(self, anchors):
        return polyline(anchors)


class Spellings(unittest.TestCase):
    def test_the_package_names_what_the_extension_registers(self):
        for name in NAMES:
            with self.subTest(name=name):
                self.assertIs(getattr(compose, name), getattr(native, name))

    def test_the_alternatives_of_an_anchor_are_nested_in_it(self):
        self.assertTrue(isinstance(native.Anchor.OnNode, type))
        self.assertTrue(isinstance(native.Anchor.FreePoint, type))

    def test_the_profile_reading_stands_where_the_width_law_does(self):
        # The width law is a class of the geometry bindings; the reading
        # that answers one is offered exactly where it is registered.
        path = getattr(_sigil.geometry, "path", None)
        self.assertEqual(hasattr(native.Across, "profile"), hasattr(path, "Profile"))


class Routers(unittest.TestCase):
    def setUp(self):
        self.near = skia.Rect(0, 0, 10, 10)
        self.far = skia.Rect(40, 20, 10, 10)

    def test_an_empty_router_is_false_and_routes_nothing(self):
        for kind in (native.Router, native.RailRouter):
            with self.subTest(kind=kind.__name__):
                empty = kind()
                self.assertFalse(empty)
                self.assertFalse(empty.comparable())
                self.assertEqual(empty, kind())
        self.assertTrue(native.Router().route(self.near, self.far).isEmpty())
        self.assertTrue(native.RailRouter().route([(0, 0), (10, 0)]).isEmpty())

    def test_a_route_function_is_asked_with_copies_of_the_two_rects(self):
        asked = []

        def between(from_, to):
            asked.append((from_, to))
            return segment(from_, to)

        router = native.Router(between)
        self.assertTrue(router)
        bounds = router.route(self.near, self.far).getBounds()
        self.assertEqual(edges(bounds), (5, 5, 40, 20))
        from_, to = asked[0]
        self.assertIsInstance(from_, skia.Rect)
        self.assertIsInstance(to, skia.Rect)
        self.assertEqual((edges(from_), edges(to)), ((0, 0, 10, 10), (40, 20, 10, 10)))
        # What the function was handed is its own, not the caller's.
        self.assertIsNot(from_, self.near)

    def test_a_router_is_built_and_asked_by_name(self):
        router = native.Router(function=segment)
        path = router.route(from_=(0, 0, 10, 10), to=[40, 20, 10, 10])
        self.assertEqual(edges(path.getBounds()), (5, 5, 40, 20))
        rails = native.RailRouter(function=polyline)
        path = rails.route(anchors=[(0, 0), skia.Point(10, 0), [10, 30]])
        self.assertEqual(edges(path.getBounds()), (0, 0, 10, 30))
        # Any iterable of points is an anchor run.
        path = rails.route(point for point in ((0, 0), (8, 6)))
        self.assertEqual(edges(path.getBounds()), (0, 0, 8, 6))

    def test_a_rail_function_is_asked_with_a_list_of_points(self):
        asked = []

        def through(anchors):
            asked.append(anchors)
            return polyline(anchors)

        native.RailRouter(through).route([(1, 2), (3, 4)])
        self.assertIsInstance(asked[0], list)
        self.assertTrue(all(isinstance(each, skia.Point) for each in asked[0]))
        self.assertEqual([(each.x, each.y) for each in asked[0]], [(1, 2), (3, 4)])

    def test_a_function_router_equals_only_its_own_copies(self):
        for kind, function in (
            (native.Router, segment),
            (native.RailRouter, polyline),
        ):
            with self.subTest(kind=kind.__name__):
                router = kind(function)
                self.assertFalse(router.comparable())
                self.assertNotEqual(router, kind(function))
                self.assertNotEqual(router, kind())
                for duplicate in (
                    router.copy(),
                    copy.copy(router),
                    copy.deepcopy(router),
                ):
                    self.assertIsNot(duplicate, router)
                    self.assertEqual(duplicate, router)

    def test_a_scheme_router_compares_by_the_scheme(self):
        router = native.Router(Bowed(4))
        self.assertTrue(router)
        self.assertTrue(router.comparable())
        self.assertEqual(router, native.Router(scheme=Bowed(4)))
        self.assertNotEqual(router, native.Router(Bowed(5)))
        self.assertNotEqual(router, native.Router(segment))
        rails = native.RailRouter(Threaded(8))
        self.assertTrue(rails.comparable())
        self.assertEqual(rails, native.RailRouter(scheme=Threaded(8)))
        self.assertNotEqual(rails, native.RailRouter(Threaded(9)))

    def test_a_scheme_router_routes_through_its_method(self):
        scheme = Bowed(4)
        path = native.Router(scheme).route(self.near, self.far)
        self.assertEqual(edges(path.getBounds()), (5, 5, 40, 20))
        self.assertEqual(scheme.asked, [((0, 0, 10, 10), (40, 20, 10, 10))])
        path = native.RailRouter(Threaded(8)).route([(0, 0), (6, 8)])
        self.assertEqual(edges(path.getBounds()), (0, 0, 6, 8))

    def test_what_a_route_raises_is_carried_out(self):
        def blocked(from_, to):
            raise ValueError("no way through")

        with self.assertRaisesRegex(RuntimeError, "no way through"):
            native.Router(blocked).route(self.near, self.far)
        with self.assertRaisesRegex(RuntimeError, "answers a Path"):
            native.Router(lambda from_, to: None).route(self.near, self.far)
        with self.assertRaisesRegex(RuntimeError, "answers a Path"):
            native.RailRouter(lambda anchors: anchors).route([(0, 0)])

    def test_a_router_refuses_what_is_neither_a_function_nor_a_scheme(self):
        for kind in (native.Router, native.RailRouter):
            for value in (3, "arc", None, native.Router(), native.RailRouter()):
                with (
                    self.subTest(kind=kind.__name__, value=value),
                    self.assertRaises(TypeError),
                ):
                    kind(value)

    def test_a_router_asked_directly_refuses_what_is_no_rect_or_point(self):
        router = native.Router(segment)
        for value in (None, "near", (1, 2), 7):
            with self.subTest(value=value), self.assertRaises(TypeError):
                router.route(value, self.far)
        rails = native.RailRouter(polyline)
        for value in (None, "ab", 7, [(1, 2, 3)], ["a"]):
            with self.subTest(value=value), self.assertRaises(TypeError):
                rails.route(value)


class Anchors(unittest.TestCase):
    def test_the_bound_form_is_the_constructor(self):
        anchor = native.Anchor("port")
        self.assertIsInstance(anchor.where, native.Anchor.OnNode)
        self.assertEqual(anchor.where.key, "port")
        self.assertEqual((anchor.where.norm.x, anchor.where.norm.y), (0.5, 0.5))
        self.assertEqual(anchor.gap, 0)
        self.assertEqual(anchor.key(), "port")

        edge = native.Anchor("port", (1, 0.5), 4)
        self.assertEqual((edge.where.norm.x, edge.where.norm.y), (1, 0.5))
        self.assertEqual(edge.gap, 4)
        self.assertEqual(edge, native.Anchor(key="port", norm=[1, 0.5], gap=4))
        self.assertEqual(edge, native.Anchor.on("port", skia.Point(1, 0.5), 4))
        self.assertEqual(edge, native.Anchor.on(key="port", norm=(1, 0.5), gap=4))
        self.assertNotEqual(edge, anchor)

    def test_a_fresh_anchor_is_bound_to_no_node(self):
        anchor = native.Anchor()
        self.assertIsInstance(anchor.where, native.Anchor.OnNode)
        self.assertEqual(anchor.key(), "")
        self.assertEqual(anchor, native.Anchor(""))

    def test_the_free_form_is_a_point_bound_to_nothing(self):
        anchor = native.Anchor.at((30, 4), 2)
        self.assertIsInstance(anchor.where, native.Anchor.FreePoint)
        self.assertEqual((anchor.where.point.x, anchor.where.point.y), (30, 4))
        self.assertEqual(anchor.gap, 2)
        self.assertEqual(anchor.key(), "")
        self.assertEqual(anchor, native.Anchor.at(point=skia.Point(30, 4), gap=2))
        self.assertNotEqual(anchor, native.Anchor.at((30, 4)))

    def test_where_is_copied_on_read_and_changed_by_assignment(self):
        anchor = native.Anchor("port")
        read = anchor.where
        read.key = "elsewhere"
        self.assertEqual(anchor.key(), "port")
        anchor.where = native.Anchor.FreePoint(point=(3, 4))
        self.assertEqual(anchor.key(), "")
        self.assertEqual(anchor, native.Anchor.at((3, 4)))
        anchor.where = native.Anchor.OnNode(key="port", norm=(0, 1))
        self.assertEqual(anchor, native.Anchor("port", (0, 1)))
        anchor.gap = 6
        self.assertEqual(anchor, native.Anchor("port", (0, 1), 6))
        with self.assertRaises(TypeError):
            anchor.where = "port"

    def test_the_alternatives_are_records(self):
        node = native.Anchor.OnNode(key="port", norm=(1, 0))
        self.assertEqual(node, native.Anchor.OnNode(key="port", norm=skia.Point(1, 0)))
        self.assertNotEqual(node, native.Anchor.OnNode(key="port"))
        # A point read off a record is the record's own, so a coordinate
        # set on it is set on the record.
        node.norm.x = 0.25
        self.assertEqual(node.norm.x, 0.25)
        node.norm = (0.75, 1)
        self.assertEqual((node.norm.x, node.norm.y), (0.75, 1))

        free = native.Anchor.FreePoint(point=[3, 4])
        self.assertEqual(free, native.Anchor.FreePoint(point=(3, 4)))
        self.assertEqual((native.Anchor.FreePoint().point.x, free.point.x), (0, 3))
        with self.assertRaisesRegex(TypeError, "Unknown OnNode field"):
            native.Anchor.OnNode(node="port")
        with self.assertRaisesRegex(TypeError, "Unknown FreePoint field"):
            native.Anchor.FreePoint(norm=(0, 0))

    def test_anchors_and_their_alternatives_copy(self):
        for value in (
            native.Anchor("port", (1, 0.5), 4),
            native.Anchor.at((3, 4)),
            native.Anchor.OnNode(key="port"),
            native.Anchor.FreePoint(point=(3, 4)),
        ):
            for duplicate in (value.copy(), copy.copy(value), copy.deepcopy(value)):
                with self.subTest(value=type(value).__name__):
                    self.assertIsNot(duplicate, value)
                    self.assertEqual(duplicate, value)
        anchor = native.Anchor("port")
        duplicate = anchor.copy()
        duplicate.gap = 9
        self.assertEqual(anchor.gap, 0)

    def test_an_anchor_refuses_what_is_no_key_or_point(self):
        for arguments in ((3,), ("port", "right"), ("port", (1, 2, 3)), ("port", None)):
            with self.subTest(arguments=arguments), self.assertRaises(TypeError):
                native.Anchor(*arguments)
        with self.assertRaises(TypeError):
            native.Anchor.at("here")
        with self.assertRaises(TypeError):
            native.Anchor.OnNode(norm="right")


class Tethers(unittest.TestCase):
    def test_a_fresh_tether_hangs_centre_on_centre_of_no_node(self):
        tether = native.Tether()
        self.assertEqual(tether.key, "")
        self.assertEqual((tether.on.x, tether.on.y), (0.5, 0.5))
        self.assertEqual((tether.at.x, tether.at.y), (0.5, 0.5))
        self.assertEqual((tether.offset.x, tether.offset.y), (0, 0))
        self.assertEqual((tether.within.width(), tether.within.height()), (0, 0))
        self.assertEqual(tether.fallbacks, [])

    def test_a_tether_is_a_record_built_by_name(self):
        beside = native.Tether(key="dial", on=(1, 0.5), at=(0, 0.5))
        above = native.Tether(
            key="dial",
            on=(0.5, 0),
            at=skia.Point(0.5, 1),
            offset=[0, -4],
            within=(0, 0, 100, 80),
            fallbacks=[beside],
        )
        self.assertEqual(above.key, "dial")
        self.assertEqual((above.on.x, above.on.y), (0.5, 0))
        self.assertEqual((above.at.x, above.at.y), (0.5, 1))
        self.assertEqual((above.offset.x, above.offset.y), (0, -4))
        self.assertEqual(edges(above.within), (0, 0, 100, 80))
        self.assertEqual(above.fallbacks, [beside])

        assigned = native.Tether()
        assigned.key = "dial"
        assigned.on = (0.5, 0)
        assigned.at = (0.5, 1)
        assigned.offset = (0, -4)
        assigned.within = skia.Rect(0, 0, 100, 80)
        assigned.fallbacks = [beside]
        self.assertEqual(assigned, above)
        assigned.offset.y = -5
        self.assertNotEqual(assigned, above)
        with self.assertRaisesRegex(TypeError, "Unknown Tether field"):
            native.Tether(anchor="dial")

    def test_fallbacks_are_copied_on_read_and_changed_by_assignment(self):
        tether = native.Tether(key="dial")
        tether.fallbacks.append(native.Tether(key="dial", on=(1, 0.5)))
        self.assertEqual(tether.fallbacks, [])
        tether.fallbacks = (native.Tether(key="dial", on=(1, 0.5)),)
        self.assertEqual(len(tether.fallbacks), 1)
        read = tether.fallbacks
        read[0].key = "elsewhere"
        self.assertEqual(tether.fallbacks[0].key, "dial")
        with self.assertRaises(TypeError):
            tether.fallbacks = ["dial"]

    def test_place_is_the_pair_of_points_and_the_offset(self):
        tether = native.Tether(key="dial", on=(0.5, 0), at=(0.5, 1), offset=(0, -4))
        # x: 10 + 100 * 0.5 + 0 - 30 * 0.5; y: 20 + 50 * 0 - 4 - 10 * 1.
        self.assertEqual(
            edges(tether.place((10, 20, 100, 50), (30, 10))), (45, 6, 30, 10)
        )
        placed = tether.place(anchor=skia.Rect(10, 20, 100, 50), size=skia.Size(30, 10))
        self.assertEqual(edges(placed), (45, 6, 30, 10))
        beside = native.Tether(key="dial", on=(1, 0.5), at=(0, 0.5))
        self.assertEqual(
            edges(beside.place([10, 20, 100, 50], [30, 10])), (110, 40, 30, 10)
        )

    def test_tethers_copy(self):
        tether = native.Tether(
            key="dial", on=(0.5, 0), fallbacks=[native.Tether(key="dial")]
        )
        for duplicate in (tether.copy(), copy.copy(tether), copy.deepcopy(tether)):
            self.assertIsNot(duplicate, tether)
            self.assertEqual(duplicate, tether)
        duplicate = tether.copy()
        duplicate.key = "gauge"
        duplicate.on = (0, 0)
        self.assertEqual(tether.key, "dial")
        self.assertEqual((tether.on.x, tether.on.y), (0.5, 0))

    def test_a_tether_refuses_what_is_no_point_rect_or_size(self):
        with self.assertRaises(TypeError):
            native.Tether(on="up")
        with self.assertRaises(TypeError):
            native.Tether(within=(1, 2))
        tether = native.Tether(key="dial")
        for anchor, size in ((None, (1, 1)), ((0, 0, 1, 1), None), ("dial", (1, 1))):
            with self.subTest(anchor=anchor, size=size), self.assertRaises(TypeError):
                tether.place(anchor, size)


class Spines(unittest.TestCase):
    def assertPoint(self, point, x, y):
        self.assertAlmostEqual(point.x, x, places=3)
        self.assertAlmostEqual(point.y, y, places=3)

    def test_a_width_is_built_by_across_and_compared_by_its_profile(self):
        width = native.across(22)
        self.assertIsInstance(width, native.Across)
        self.assertEqual(width, native.across(pixels=22.0))
        self.assertNotEqual(width, native.across(14))
        for duplicate in (width.copy(), copy.copy(width), copy.deepcopy(width)):
            self.assertIsNot(duplicate, width)
            self.assertEqual(duplicate, width)
        # Only `across` installs what sweeps the width into a region.
        with self.assertRaises(TypeError):
            native.Across()
        with self.assertRaises(TypeError):
            native.across("wide")

    def test_a_band_is_its_own_leaf_over_its_spine(self):
        width = native.across(10)
        spine = skia.PathBuilder().addRect((20, 20, 100, 100)).detach()
        self.assertIsInstance(native.band(spine=spine, width=width), native.Band)
        for authored in (spine, lambda width, height: spine, compose.shape(spine)):
            with self.subTest(spine=type(authored).__name__):
                leaf = native.band(authored, width)
                self.assertIsInstance(leaf, native.Band)
        # The band's own verb, and every node verb, hand the band back, and
        # the leaf converts wherever a node is taken.
        leaf = native.band(spine, width)
        self.assertIs(leaf.bandAlignment(geometry.path.Formation.Outer), leaf)
        self.assertIs(leaf.fill("#ff0000"), leaf)
        self.assertIsInstance(native.Element(leaf), native.Element)
        native.stack(leaf)
        native.box().children(leaf)
        # A width is the value `across` builds, never a bare number.
        with self.assertRaises(TypeError):
            native.band(spine, 10)

    def test_positive_across_is_to_the_left_of_travel(self):
        spine = skia.PathBuilder().moveTo(0, 0).lineTo(100, 0).detach()
        on = native.bandPointAt(spine, 0.5, 0)
        self.assertIsInstance(on, skia.Point)
        self.assertPoint(on, 50, 0)
        # Travelling along +x with y pointing down, left is up.
        self.assertPoint(
            native.bandPointAt(spine=spine, along=0.25, acrossPx=10), 25, -10
        )
        self.assertPoint(native.bandPointAt(spine, 1.0, -10), 100, 10)
        # Past either end is the end.
        self.assertPoint(native.bandPointAt(spine, 2.0, 0), 100, 0)
        self.assertPoint(native.bandPointAt(spine, -1.0, 0), 0, 0)
        # A spine with no length has no point to answer.
        self.assertPoint(native.bandPointAt(skia.Path(), 0.5, 10), 0, 0)


class Session(unittest.TestCase):
    """A scene rendered through the native host, which lends the canvas."""

    SIZE = (64, 32)

    PRELUDE = textwrap.dedent(
        f"""
        import builtins
        import weakref
        from _sigil import compose
        from sigil import skia
        from sigil.sketch import sketch
        results = builtins.{RESULTS}

        def node(key, left, top, width=8, height=8):
            return (
                compose.box().key(key).absolute().left(left).top(top)
                .width(width).height(height).fill('#0000ff')
            )

        def wire(route):
            # Four pixels wide, so a pixel the stroke's centre line crosses
            # is covered whole at any slope.
            return route.absolute().inset(0).foreground(compose.stroke(4, '#ff0000'))

        def plate(*children):
            return compose.box().width(64).height(32).children(list(children))

        def segment(from_, to):
            return (
                skia.PathBuilder().moveTo(from_.centerX(), from_.centerY())
                .lineTo(to.centerX(), to.centerY()).detach()
            )

        def edges(rect):
            return (rect.left(), rect.top(), rect.width(), rect.height())

        def show(pen, tree):
            # A route and a tether are answers of a layout, and the first
            # one may be taken against a rect still settling, so the tree
            # is drawn once to settle and once to be read.
            probe = compose.Composer()
            probe.setSize((64, 32))
            probe.render(tree)
            probe.draw(pen.canvas())
            pen.background('#000000')
            probe.draw(pen.canvas())
            return probe
        """
    )

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="sigil_derive_")
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        setattr(builtins, RESULTS, self.results)
        self.addCleanup(delattr, builtins, RESULTS)

    def render(self, body):
        """Render @p body as a sketch's methods and answer the decoded frame."""
        source = Path(self.directory.name) / "scene.py"
        output = Path(self.directory.name) / "scene.png"
        source.write_text(
            self.PRELUDE
            + f"@sketch(size={self.SIZE!r}, background='#000000', capture_at=0)\n"
            "class Scene:\n"
            + textwrap.indent(textwrap.dedent(body).strip("\n"), "    ")
            + "\n"
        )
        render_file(source, output, at=0)
        return image.decode(output.read_bytes())

    def frame(self, drawing):
        """Render @p drawing as the body of a sketch's draw."""
        return self.render(
            "def draw(self, pen):\n"
            "    pen.background('#000000')\n"
            + textwrap.indent(textwrap.dedent(drawing).strip("\n"), "    ")
        )

    def assertNumbers(self, actual, expected):
        """Every number of @p actual beside its peer, within a layout's rounding."""
        self.assertEqual(len(actual), len(expected))
        for value, wanted in zip(actual, expected, strict=True):
            self.assertAlmostEqual(value, wanted, places=3)

    def red(self, picture, x, y):
        """Whether the canvas is stroke red at @p x, @p y, at any density."""
        column = x * picture.width() // self.SIZE[0]
        row = y * picture.height() // self.SIZE[1]
        offset = 4 * (row * picture.width() + column)
        red, green, blue = picture.rgba()[offset : offset + 3]
        return red > 200 and green < 80 and blue < 80


@unittest.skipUnless(hasattr(native, "Composer"), "no composer to read a layout from")
class Bands(Session):
    def side(self, formation):
        """The band's colour just outside and just inside its spine's top edge."""
        picture = self.frame(f"""
            from sigil import geometry
            spine = skia.PathBuilder().addRect((8, 8, 48, 16)).detach()
            show(pen, plate(
                compose.band(spine, compose.across(8)).absolute().inset(0)
                .bandAlignment(geometry.path.Formation.{formation})
                .fill('#ff0000')
            ))
        """)
        return self.red(picture, 32, 5), self.red(picture, 32, 10)

    def test_a_band_takes_the_declared_side_of_its_spine(self):
        # A rect spine runs clockwise on screen, so the left of travel along
        # its top edge is upward, outside the rect.
        self.assertEqual(self.side("Center"), (True, True))
        self.assertEqual(self.side("Outer"), (True, False))
        self.assertEqual(self.side("Inner"), (False, True))
        picture = self.frame("""
            show(pen, plate(compose.box().absolute().inset(0)))
        """)
        self.assertFalse(self.red(picture, 32, 5))


class Lifetimes(Session):
    def test_a_router_is_released_when_its_session_closes(self):
        self.render("""
            def setup(self, ctx):
                class Owner:
                    def between(self, from_, to):
                        results['asked'] = True
                        return segment(from_, to)

                class Scheme:
                    def __init__(self, owner):
                        self.owner = owner

                    def __eq__(self, other):
                        return True

                    def route(self, from_, to):
                        return self.owner.between(from_, to)

                owner = Owner()
                results['owner'] = weakref.ref(owner)
                results['function'] = compose.Router(owner.between)
                results['scheme'] = compose.Router(Scheme(owner))
                results['equal'] = results['scheme'] == compose.Router(Scheme(owner))
                # The router is asked while its session stands, which is
                # what the cases below contrast with: the same call once
                # the session that retained the function has closed.
                results['function'].route((4, 12, 8, 8), (52, 12, 8, 8))
                ctx.render(plate(node('a', 4, 12), node('b', 52, 12)))
        """)
        self.assertTrue(self.results["asked"])
        self.assertTrue(self.results["equal"])
        gc.collect()
        self.assertIsNone(
            self.results["owner"](),
            "a retained router must not keep its function's owner alive",
        )
        near, far = skia.Rect(0, 0, 10, 10), skia.Rect(40, 20, 10, 10)
        for name in ("function", "scheme"):
            router = self.results[name]
            with self.subTest(name=name):
                # The value outlives its session and refuses to route.
                self.assertTrue(router)
                with self.assertRaisesRegex(RuntimeError, "lifetime"):
                    router.route(near, far)
                self.assertEqual(router, router.copy())

        # A scheme whose lifetime has closed equals nothing, its own copy
        # aside, whatever its equality would have said.
        class Agreeable:
            def __eq__(self, other):
                return True

            def route(self, from_, to):
                return segment(from_, to)

        self.assertNotEqual(self.results["scheme"], native.Router(Agreeable()))


if __name__ == "__main__":
    unittest.main()
