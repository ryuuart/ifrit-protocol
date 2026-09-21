"""Paint programs, the context they are lent, keyed shapes and the two leaves.

The values are asked directly; whatever needs a paint call is asked inside a
sketch rendered headless, which hands its readings back through `builtins`
because a paint context and a lent canvas do not outlive the call they were
made for.
"""

import builtins
import copy
import gc
import tempfile
import textwrap
import unittest
import weakref
from pathlib import Path

from _sigil import compose as native
from sigil import image, skia, weave
from sigil.sketch import render_file

RESULTS = "_sigil_paint_program_results"


def outline(width, height):
    return skia.Path.Oval((0, 0, width, height))


class PaintProgramValues(unittest.TestCase):
    def test_a_paint_context_is_lent_and_never_built(self):
        with self.assertRaises(TypeError):
            native.PaintContext()

    def test_the_promotion_policy_names_its_three_rules(self):
        self.assertEqual(
            set(native.PromotionPolicy.__members__), {"Off", "ByCost", "Eager"}
        )

    def test_key_state_is_a_record_that_copies_what_it_holds(self):
        state = native.KeyState(pressed=True, key="a", keyCode=65, down=[65, 16])
        self.assertEqual(
            (state.pressed, state.key, state.keyCode, state.down),
            (True, "a", 65, [65, 16]),
        )
        state.down.append(1)
        self.assertEqual(state.down, [65, 16])
        for duplicate in (state.copy(), copy.copy(state), copy.deepcopy(state)):
            self.assertIsNot(duplicate, state)
            self.assertEqual(duplicate.key, "a")
        self.assertEqual(native.KeyState().down, [])
        with self.assertRaisesRegex(TypeError, "Unknown KeyState field"):
            native.KeyState(presed=True)

    def test_the_pointer_record_reads_a_point_from_any_spelling(self):
        pointer = native.PaintContext.Pointer(at=(3, 4), pressed=True)
        self.assertEqual((pointer.at.x, pointer.at.y, pointer.pressed), (3, 4, True))
        pointer.at = skia.Point(5, 6)
        self.assertEqual(copy.deepcopy(pointer).at.y, 6)
        self.assertFalse(native.PaintContext.Pointer().pressed)
        with self.assertRaisesRegex(TypeError, "Unknown Pointer field"):
            native.PaintContext.Pointer(where=(0, 0))

    def test_a_program_is_a_callable_naming_at_most_what_it_is_offered(self):
        for factory in (native.custom, native.pen, native.graphics):
            with self.subTest(factory=factory.__name__):
                with self.assertRaises(TypeError):
                    factory(None)
                with self.assertRaises(TypeError):
                    factory("key", None)
                with self.assertRaisesRegex(TypeError, "cannot accept"):
                    factory(lambda first, second, third: None)
                for program in (
                    lambda: None,
                    lambda first: None,
                    lambda first, context: None,
                    print,
                ):
                    self.assertIsInstance(factory(program), native.Element)
                    self.assertIsInstance(factory("key", program), native.Element)

    def test_every_program_factory_takes_its_inputs_by_name(self):
        def program(first):
            pass

        self.assertIsInstance(native.custom(program=program), native.Element)
        self.assertIsInstance(
            native.custom(key="mark", program=program), native.Element
        )
        for factory in (native.pen, native.graphics):
            self.assertIsInstance(
                factory(program=program, cache=native.Cache.Texture), native.Element
            )
            self.assertIsInstance(
                factory(key="mark", program=program, cache=native.Cache.None_),
                native.Element,
            )

    def test_a_keyed_shape_compares_by_its_key_and_nothing_else(self):
        first = native.keyedShape((6.0, "round"), outline)
        second = native.keyedShape((6.0, "round"), lambda width, height: skia.Path())
        self.assertTrue(first.comparable())
        self.assertEqual(first, second)
        self.assertNotEqual(first, native.keyedShape((7.0, "round"), outline))
        self.assertNotEqual(first, native.keyedShape("round", outline))
        self.assertFalse(native.shape(outline).comparable())
        self.assertNotEqual(native.shape(outline), native.shape(outline))

    def test_a_keyed_shape_generates_over_the_box_or_over_nothing(self):
        sized = native.keyedShape(key=1, function=outline)
        self.assertEqual(sized.path(30, 20).getBounds().width(), 30)
        fixed = native.keyedShape("fixed", lambda: skia.Path.Rect((0, 0, 10, 10)))
        self.assertEqual(fixed.path(30, 20).getBounds().width(), 10)
        with self.assertRaisesRegex(TypeError, "width and height, or nothing"):
            native.keyedShape(1, lambda width: skia.Path())
        with self.assertRaises(TypeError):
            native.keyedShape(1, None)

    def test_a_key_that_cannot_be_compared_says_so(self):
        class Incomparable:
            def __eq__(self, other):
                raise ValueError("this key refuses comparison")

        with self.assertRaisesRegex(RuntimeError, "refuses comparison"):
            native.keyedShape(Incomparable(), outline) == native.keyedShape(
                Incomparable(), outline
            )

    def test_an_element_takes_the_keyed_spelling_beside_the_shape_itself(self):
        element = native.box().width(20).height(20)
        self.assertIs(element.shape((1, 2), outline), element)
        self.assertIs(element.shape(key="oval", function=outline), element)
        self.assertIs(element.shape(native.keyedShape(3, outline)), element)
        self.assertIs(element.shape(outline), element)

    def test_gradients_are_fills_checked_before_they_are_built(self):
        ramp = native.linearGradient((0, 0), (10, 0), ["#ff0000", (0, 0, 1)])
        self.assertIsInstance(ramp, native.Fill)
        self.assertNotEqual(ramp, native.Fill.none())
        named = native.linearGradient(
            from_=(0, 0), to=(10, 0), colors=["#ff0000", "#0000ff"], stops=[0, 1]
        )
        self.assertIsInstance(named, native.Fill)
        glow = native.radialGradient(
            center=skia.Point(5, 5), radius=5, colors=["#ffffff", "#000000"]
        )
        self.assertIsInstance(glow, native.Fill)
        self.assertIsInstance(native.box().fill(glow), native.Element)
        with self.assertRaisesRegex(ValueError, "one position per colour"):
            native.linearGradient((0, 0), (10, 0), ["#fff", "#000"], [0.5])
        with self.assertRaisesRegex(ValueError, "needs a colour"):
            native.radialGradient((0, 0), 4, [])

    def test_a_custom_property_fill_is_named_by_reference_or_by_name(self):
        self.assertEqual(
            native.Fill.var("accent"), native.Fill.var(native.var("accent"))
        )
        self.assertEqual(native.Fill.var(name="accent"), native.Fill.var("accent"))
        self.assertNotEqual(native.Fill.var("accent"), native.Fill.var("ground"))

    def test_the_image_leaf_takes_a_decoded_asset_or_a_picture(self):
        picture = image.from_rgba(bytes([255, 0, 0, 255]) * 16, 4, 4)
        asset = image.decodeAsset(image.encode(picture))
        self.assertIsInstance(native.image(asset), native.Image)
        self.assertIsInstance(native.image(asset=asset), native.Image)
        self.assertIsInstance(native.image(picture), native.Image)
        with self.assertRaises(TypeError):
            native.image("not an image")

    def test_the_paragraph_leaf_takes_a_paragraph_and_its_layout_options(self):
        paragraph = weave.Paragraph("Held words", weave.Type(size=14))
        options = weave.ParagraphLayoutOptions()
        for element in (
            native.text(paragraph),
            native.text(paragraph, options),
            native.text(paragraph=paragraph, options=None),
        ):
            self.assertIsInstance(element, native.Text)
        self.assertIsInstance(native.text("words"), native.Text)
        with self.assertRaises(TypeError):
            native.text(paragraph, 3)
        with self.assertRaises(TypeError):
            native.text(3)


class PaintProgramRenders(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="sigil_paint_programs_")
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        setattr(builtins, RESULTS, self.results)
        self.addCleanup(delattr, builtins, RESULTS)

    def render(self, body, *, size, at=0):
        """Render @p body as a sketch's methods and answer the decoded frame."""
        source = Path(self.directory.name) / "scene.py"
        output = Path(self.directory.name) / "scene.png"
        source.write_text(
            "import builtins\n"
            "import weakref\n"
            "from _sigil import compose\n"
            "from sigil import image, material, skia, weave\n"
            "from sigil.sketch import sketch\n"
            f"results = builtins.{RESULTS}\n"
            f"@sketch(size={size!r}, background='#000000', capture_at=0)\n"
            "class Scene:\n"
            + textwrap.indent(textwrap.dedent(body).strip("\n"), "    ")
            + "\n"
        )
        render_file(source, output, at=at)
        return image.decode(output.read_bytes())

    def pixel(self, picture, x, y, size):
        """The colour at @p x, @p y of a canvas @p size wide, at any density."""
        column = x * picture.width() // size[0]
        row = y * picture.height() // size[1]
        offset = 4 * (row * picture.width() + column)
        return tuple(picture.rgba()[offset : offset + 3])

    def test_a_program_draws_whichever_of_its_parameters_it_names(self):
        size = (60, 20)
        picture = self.render(
            """
            def setup(self, ctx):
                def red():
                    paint = skia.Paint()
                    paint.setAntiAlias(False)
                    paint.setColor('#ff0000')
                    return paint

                def nothing():
                    results['nothing'] = True

                def canvas_alone(canvas):
                    canvas.drawRect((0, 0, 20, 20), red())

                def canvas_and_context(canvas, context):
                    paint = red()
                    paint.setColor('#0000ff')
                    box = (0, 0, context.size.width(), context.size.height())
                    canvas.drawRect(box, paint)
                    results['lent'] = (canvas, context)

                ctx.render(
                    compose.box(
                        compose.custom(nothing).size(20, 20),
                        compose.custom(canvas_alone).size(20, 20),
                        compose.custom('both', canvas_and_context).size(20, 20),
                    ).row().size(60, 20)
                )
            """,
            size=size,
        )
        self.assertTrue(self.results["nothing"])
        self.assertEqual(self.pixel(picture, 10, 10, size), (0, 0, 0))
        self.assertEqual(self.pixel(picture, 30, 10, size), (255, 0, 0))
        self.assertEqual(self.pixel(picture, 50, 10, size), (0, 0, 255))
        canvas, context = self.results["lent"]
        with self.assertRaisesRegex(RuntimeError, "no longer inside its paint call"):
            canvas.save()
        with self.assertRaisesRegex(RuntimeError, "no longer inside its paint call"):
            context.size

    def test_the_paint_context_reads_what_the_tree_resolved_at_the_node(self):
        self.render(
            """
            def setup(self, ctx):
                def read(canvas, context):
                    bounds = context.outline.getBounds()
                    table = context.vars
                    results.update(
                        size=(context.size.width(), context.size.height()),
                        outline=(bounds.width(), bounds.height()),
                        silhouette=isinstance(context.silhouette, skia.Path),
                        elapsed=context.elapsedSeconds,
                        scale=context.contentScale,
                        animating=context.animating,
                        root=(context.rootSize.width(), context.rootSize.height()),
                        ink=context.ink == material.Color('#00ff00'),
                        font=context.font.size.value,
                        accent=table.find('accent') == material.Color('#ff0000'),
                        reference=table.find(compose.var('accent')) is not None,
                        measure=table.find('measure'),
                        missing=table.find('missing'),
                        named='accent' in table and 'missing' not in table,
                        count=(len(table), table.empty(), len(table.entries())),
                        entry=[
                            value
                            for name, value in table.entries()
                            if name == compose.var('measure')
                        ],
                        copied=table == table.copy(),
                        pointer=(context.pointer.at.x, context.pointer.pressed),
                        keys=context.keys,
                        borrowed=(context.borrowed, context.borrowedPath('nobody').isEmpty()),
                        matrix=isinstance(context.toRoot, skia.Matrix),
                        density=context.bakeDensity,
                        promotion=context.promotion,
                    )

                ctx.render(
                    compose.box(compose.custom(read).size(24, 12))
                    .size(48, 24)
                    .ink('#00ff00')
                    .font(weave.Type(size=17))
                    .var('accent', '#ff0000')
                    .var('measure', 12)
                )
            """,
            size=(48, 24),
        )
        results = self.results
        self.assertEqual(results["size"], (24, 12))
        self.assertEqual(results["outline"], (24, 12))
        self.assertTrue(results["silhouette"])
        self.assertGreaterEqual(results["elapsed"], 0)
        self.assertGreater(results["scale"], 0)
        self.assertIsInstance(results["animating"], bool)
        self.assertEqual(results["root"], (48, 24))
        self.assertTrue(results["ink"])
        self.assertEqual(results["font"], 17)
        self.assertTrue(results["accent"])
        self.assertTrue(results["reference"])
        self.assertIsInstance(results["measure"], native.Dimension)
        self.assertEqual(results["measure"].value, 12)
        self.assertIsNone(results["missing"])
        self.assertTrue(results["named"])
        count, empty, entries = results["count"]
        self.assertGreaterEqual(count, 2)
        self.assertEqual((empty, entries), (False, count))
        self.assertEqual(results["entry"], [results["measure"]])
        self.assertTrue(results["copied"])
        self.assertFalse(results["pointer"][1])
        self.assertIsInstance(results["keys"], native.KeyState)
        self.assertEqual((results["keys"].pressed, results["keys"].down), (False, []))
        self.assertEqual(results["borrowed"], ([], True))
        self.assertTrue(results["matrix"])
        self.assertGreaterEqual(results["density"], 0)
        self.assertIsInstance(results["promotion"], native.PromotionPolicy)

    def test_a_pen_program_names_the_pen_the_context_or_neither(self):
        self.render(
            """
            def setup(self, ctx):
                def nothing():
                    results['nothing'] = True

                def pen_alone(pen):
                    results['alone'] = (pen.width, pen.height)

                def pen_and_context(pen, context):
                    results['both'] = (pen.width, context.size.width())
                    results['lent'] = (pen, context)

                def kept(pen, context):
                    results['kept'] = (context.size.width(), context.size.height())

                ctx.render(
                    compose.box(
                        compose.box(compose.pen(nothing)).size(10, 10),
                        compose.box(compose.pen('alone', pen_alone)).size(20, 10),
                        compose.box(compose.pen(pen_and_context)).size(30, 10),
                        compose.box(compose.graphics('kept', kept)).size(40, 10),
                    ).size(40, 40)
                )
            """,
            size=(40, 40),
        )
        self.assertTrue(self.results["nothing"])
        self.assertEqual(self.results["alone"], (20, 10))
        self.assertEqual(self.results["both"], (30, 30))
        self.assertEqual(self.results["kept"], (40, 10))
        pen, context = self.results["lent"]
        with self.assertRaises(RuntimeError):
            pen.width
        with self.assertRaisesRegex(RuntimeError, "no longer inside its paint call"):
            context.ink

    def patched(self, leaf, *, prelude=""):
        """How many nodes a second, identical describe of @p leaf patched."""
        self.render(
            f"""
            def setup(self, ctx):
                {prelude}
                self.tree = lambda: compose.box(({leaf}))
                self.described = False
                ctx.composer.render(self.tree())

            def update(self, elapsed, ctx):
                if elapsed > 1 / 60 and not self.described:
                    self.described = True
                    ctx.composer.render(self.tree())
                    results['patched'] = ctx.composer.stats().patchedNodes
            """,
            size=(40, 40),
            at=0.1,
        )
        return self.results.pop("patched")

    def test_a_key_is_what_lets_a_paint_program_prune(self):
        keyed = "compose.custom('mark', lambda canvas: None).size(20, 20)"
        keyless = "compose.custom(lambda canvas: None).size(20, 20)"
        self.assertEqual(self.patched(keyed), 0)
        self.assertGreaterEqual(self.patched(keyless), 1)
        for factory in ("pen", "graphics"):
            with self.subTest(factory=factory):
                leaf = f"compose.{factory}('mark', lambda pen: None)"
                self.assertEqual(self.patched(leaf), 0)

    def test_a_key_is_what_lets_a_generated_shape_prune(self):
        oval = "lambda width, height: skia.Path.Oval((0, 0, width, height))"
        keyed = f"compose.box().size(20, 20).shape((20, 'oval'), {oval})"
        keyless = f"compose.box().size(20, 20).shape({oval})"
        self.assertEqual(self.patched(keyed), 0)
        self.assertGreaterEqual(self.patched(keyless), 1)

    def test_a_keyed_shape_is_the_outline_the_node_is_filled_to(self):
        size = (40, 40)
        picture = self.render(
            """
            def setup(self, ctx):
                oval = lambda width, height: skia.Path.Oval((0, 0, width, height))
                ctx.render(
                    compose.box(
                        compose.box().size(40, 40).fill('#ff0000').shape('oval', oval)
                    )
                )
            """,
            size=size,
        )
        self.assertEqual(self.pixel(picture, 20, 20, size), (255, 0, 0))
        self.assertEqual(self.pixel(picture, 1, 1, size), (0, 0, 0))

    def test_one_asset_is_one_identity_in_every_describe(self):
        held = (
            "asset = image.decodeAsset(image.encode("
            "image.from_rgba(bytes([255, 0, 0, 255]) * 64, 8, 8)))"
        )
        self.assertEqual(
            self.patched("compose.image(asset).size(8, 8)", prelude=held), 0
        )
        decoded = (
            "compose.image(image.decodeAsset(image.encode("
            "image.from_rgba(bytes([255, 0, 0, 255]) * 64, 8, 8)))).size(8, 8)"
        )
        self.assertGreaterEqual(self.patched(decoded), 1)

    def test_the_two_leaves_draw_what_they_were_given(self):
        size = (120, 40)
        picture = self.render(
            """
            def setup(self, ctx):
                asset = image.decodeAsset(image.encode(
                    image.from_rgba(bytes([255, 0, 0, 255]) * 64, 8, 8)))
                paragraph = weave.Paragraph(
                    'Held words', weave.Type(size=16, color='#ffffff'))
                results['paragraph'] = paragraph
                ctx.render(
                    compose.box(
                        compose.image(asset).size(8, 8),
                        compose.text(
                            paragraph, weave.ParagraphLayoutOptions()
                        ).width(112),
                    ).row().size(120, 40)
                )
            """,
            size=size,
        )
        self.assertEqual(self.pixel(picture, 4, 4, size), (255, 0, 0))
        pixels = picture.rgba()
        stride = 4 * picture.width()
        first = 9 * picture.width() // size[0]
        lit = sum(
            1
            for row in range(picture.height())
            for column in range(first, picture.width())
            if pixels[row * stride + 4 * column] > 128
        )
        self.assertGreater(lit, 0, "the paragraph leaf drew no glyphs")
        # The paragraph is still the author's own once the session is gone.
        self.assertEqual(self.results["paragraph"].text(), "Held words")

    def test_a_program_is_released_when_its_session_closes(self):
        self.render(
            """
            def setup(self, ctx):
                class Painter:
                    def paint(self, canvas):
                        results['painted'] = True

                    def outline(self, width, height):
                        return skia.Path.Rect((0, 0, width, height))

                painter = Painter()
                results['painter'] = weakref.ref(painter)
                results['element'] = compose.custom(painter.paint).size(10, 10)
                results['shape'] = compose.keyedShape(painter, painter.outline)
                ctx.render(compose.box(results['element']))
            """,
            size=(20, 20),
        )
        self.assertTrue(self.results["painted"])
        self.assertIsInstance(self.results["element"], native.Element)
        gc.collect()
        self.assertIsNone(
            self.results["painter"](),
            "a retained element must not keep its program's owner alive",
        )
        # A key whose lifetime has closed equals nothing, its own copy aside.
        shape = self.results["shape"]
        self.assertEqual(shape, native.Shape(shape))
        self.assertNotEqual(shape, native.keyedShape(1, outline))


if __name__ == "__main__":
    unittest.main()
