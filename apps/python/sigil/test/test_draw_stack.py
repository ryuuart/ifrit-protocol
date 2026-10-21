"""A stack of pixel layers held in Python: drawn on with no frame open,
composited bottom to top with each layer's blend, opacity, placement, mask
and clip, shown inline as PNG, put down by a sketch's pen and drawn every
frame by a window."""

import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import compose, draw, live, media
from support.rendering import pixel, renderedImage


def at(picture, x, y):
    """The RGBA colour of a ``skia.Image`` at pixel ``x``, ``y``."""
    return pixel(picture, x, y)


def filled(layer, colour, x, y, width, height):
    """Draws one rectangle of ``colour`` on ``layer``."""
    with layer.pen() as pen:
        pen.noStroke()
        pen.fill(colour)
        pen.rect(x, y, width, height)


class StackContracts(unittest.TestCase):
    def test_a_stack_of_one_layer_is_that_layers_drawing(self):
        stack = draw.Stack((48, 32))
        ink = stack.layer("ink")
        with ink.pen() as pen:
            pen.background("#102030")
            pen.stroke("#ffcc00")
            pen.strokeWeight(3)
            pen.fill("#ff3366")
            pen.circle(24, 16, 20)
            pen.line(0, 0, 48, 32)
        composite = stack.image()
        self.assertEqual((composite.width(), composite.height()), (48, 32))
        self.assertEqual(composite.rgba(), ink.image().rgba())

    def test_asking_for_a_name_again_answers_the_same_layer(self):
        stack = draw.Stack(64, 64)
        rings = stack.layer("rings")
        stack.layer("field")
        self.assertEqual(stack.layer("rings"), rings)
        self.assertEqual(stack.names, ["rings", "field"])
        self.assertEqual([layer.name for layer in stack.layers], ["rings", "field"])
        self.assertEqual(len(stack), 2)
        self.assertIn("rings", stack)
        self.assertIn(rings, stack)
        self.assertEqual(stack.find("field"), stack.layer("field"))
        self.assertIsNone(stack.find("absent"))
        self.assertTrue(stack.reorder(rings, 1))
        self.assertEqual(stack.names, ["field", "rings"])
        self.assertTrue(stack.reorder("rings", 0))
        self.assertEqual(stack.names, ["rings", "field"])
        self.assertFalse(stack.reorder("absent", 0))

    def test_a_removed_layer_says_so(self):
        stack = draw.Stack((16, 16))
        gone = stack.layer("gone")
        self.assertTrue(stack.remove("gone"))
        self.assertFalse(stack.remove("gone"))
        self.assertNotIn(gone, stack)
        with self.assertRaisesRegex(RuntimeError, "removed from its stack"):
            gone.opacity = 0.5
        again = stack.layer("gone")
        self.assertNotEqual(again, gone)

    def test_a_layer_blended_remove_erases_what_lies_beneath_where_it_has_ink(self):
        stack = draw.Stack((40, 20))
        filled(stack.layer("ground"), "#ff0000", 0, 0, 40, 20)
        cut = stack.layer("cut")
        filled(cut, "#ffffff", 0, 0, 20, 20)
        cut.blend("remove")
        self.assertEqual(cut.blend(), draw.REMOVE)
        picture = stack.image()
        self.assertEqual(at(picture, 5, 10)[3], 0)
        self.assertEqual(at(picture, 30, 10), (255, 0, 0, 255))
        cut.hide()
        self.assertFalse(cut.isVisible)
        self.assertEqual(at(stack.image(), 5, 10), (255, 0, 0, 255))

    def test_blend_takes_the_pens_constants_and_their_words(self):
        layer = draw.Stack((8, 8)).layer("layer")
        self.assertEqual(layer.blend(), draw.BLEND)
        layer.blend("Hard Light")
        self.assertEqual(layer.blend(), draw.HARD_LIGHT)
        layer.blend(draw.MULTIPLY)
        self.assertEqual(layer.blend(), draw.MULTIPLY)
        with self.assertRaisesRegex(ValueError, "blend words"):
            layer.blend("smudge")
        with self.assertRaisesRegex(ValueError, "blend words"):
            layer.blend(draw.CENTER)

    def test_a_layer_masked_by_another_shows_only_where_that_one_has_ink(self):
        stack = draw.Stack((40, 20))
        shown = stack.layer("shown")
        filled(shown, "#00ff00", 0, 0, 40, 20)
        stencil = stack.layer("stencil")
        filled(stencil, "#000000", 20, 0, 20, 20)
        stencil.hide()
        shown.mask(stencil)
        self.assertEqual(shown.mask(), stencil)
        picture = stack.image()
        self.assertEqual(at(picture, 5, 10)[3], 0)
        self.assertEqual(at(picture, 30, 10), (0, 255, 0, 255))
        # Read by its luminance, black ink hides as surely as no ink does.
        shown.mask(stencil, channel="luminance")
        self.assertEqual(at(stack.image(), 30, 10)[3], 0)
        shown.mask(None)
        self.assertIsNone(shown.mask())
        self.assertEqual(at(stack.image(), 5, 10), (0, 255, 0, 255))
        with self.assertRaisesRegex(ValueError, "alpha"):
            shown.mask(stencil, channel="red")

    def test_removing_a_mask_layer_removes_the_mask(self):
        stack = draw.Stack((20, 20))
        shown = stack.layer("shown")
        filled(shown, "#0000ff", 0, 0, 20, 20)
        shown.mask(stack.layer("stencil"))
        self.assertEqual(at(stack.image(), 10, 10)[3], 0)
        stack.remove("stencil")
        self.assertIsNone(shown.mask())
        self.assertEqual(at(stack.image(), 10, 10), (0, 0, 255, 255))

    def test_a_clip_shape_confines_the_layer(self):
        stack = draw.Stack((40, 20))
        shown = stack.layer("shown")
        filled(shown, "#ffffff", 0, 0, 40, 20)
        shown.clip(lambda pen: pen.rect(0, 0, 20, 20))
        self.assertTrue(shown.isClipped)
        picture = stack.image()
        self.assertEqual(at(picture, 5, 10), (255, 255, 255, 255))
        self.assertEqual(at(picture, 30, 10)[3], 0)
        shown.clip(lambda pen: pen.rect(0, 0, 20, 20), invert=True)
        self.assertEqual(at(stack.image(), 5, 10)[3], 0)
        shown.clip(None)
        self.assertFalse(shown.isClipped)

    def test_a_clip_shape_that_raises_raises_from_the_composite(self):
        stack = draw.Stack((8, 8))
        layer = stack.layer("layer")

        def broken(pen):
            raise KeyError("no outline")

        layer.clip(broken)
        with self.assertRaises(KeyError):
            stack.image()

    def test_opacity_scales_the_layer_and_is_clamped(self):
        stack = draw.Stack((10, 10))
        layer = stack.layer("layer")
        filled(layer, "#ffffff", 0, 0, 10, 10)
        layer.opacity = 0.5
        self.assertAlmostEqual(at(stack.image(), 5, 5)[3], 128, delta=1)
        layer.opacity = 4
        self.assertEqual(layer.opacity, 1.0)
        with self.assertRaises(ValueError):
            layer.opacity = float("nan")

    def test_a_placement_moves_the_layer_and_reads_back(self):
        stack = draw.Stack((40, 20))
        layer = stack.layer("layer")
        filled(layer, "#ff00ff", 0, 0, 10, 20)
        layer.transform(translate=(20, 0))
        self.assertEqual(layer.placement.translation, (20.0, 0.0))
        picture = stack.image()
        self.assertEqual(at(picture, 5, 10)[3], 0)
        self.assertEqual(at(picture, 25, 10), (255, 0, 255, 255))
        layer.transform(scale=2, origin=(0, 10))
        self.assertEqual(layer.placement.scale, (2.0, 2.0))
        self.assertEqual(layer.placement.translation, (0.0, 0.0))
        self.assertEqual(at(stack.image(), 15, 10), (255, 0, 255, 255))
        other = stack.layer("other")
        other.transform(layer.placement)
        self.assertEqual(other.placement, layer.placement)
        layer.resetMatrix()
        self.assertEqual(layer.placement, draw.LayerTransform())
        other.transform()
        self.assertEqual(other.placement, draw.LayerTransform())

    def test_a_still_stack_is_composited_once(self):
        stack = draw.Stack((10, 10))
        filled(stack.layer("layer"), "#ffffff", 0, 0, 10, 10)
        stack.image()
        made = stack.revision
        stack.image()
        stack._repr_png_()
        self.assertEqual(stack.revision, made)
        stack.layer("layer").opacity = 0.25
        stack.image()
        self.assertEqual(stack.revision, made + 1)

    def test_a_compose_element_bakes_into_a_layer(self):
        stack = draw.Stack((60, 40))
        layer = stack.layer("baked")
        layer.bake(compose.box().width(30).height(20).fill("#3366ff"))
        picture = stack.image()
        self.assertEqual(at(picture, 10, 10), (0x33, 0x66, 0xFF, 255))
        self.assertEqual(at(picture, 50, 30)[3], 0)
        # A second bake clears the first.
        layer.bake(compose.box().width(10).height(10).fill("#ffffff"), (60, 40))
        self.assertEqual(at(stack.image(), 20, 15)[3], 0)

    def test_a_frame_is_open_once_and_its_pen_ends_with_it(self):
        layer = draw.Stack((8, 8)).layer("layer")
        frame = layer.pen()
        with frame as pen:
            with self.assertRaisesRegex(RuntimeError, "open frame"):
                with layer.pen():
                    pass
        with self.assertRaisesRegex(RuntimeError, "no longer inside"):
            pen.circle(4, 4, 4)

    def test_repr_png_is_the_stack_at_its_canvas_size(self):
        stack = draw.Stack((30, 20))
        stack.setDensityFloor(2)
        filled(stack.layer("layer"), "#ff8800", 0, 0, 30, 20)
        self.assertEqual(stack.image().width(), 60)
        for picture in (stack._repr_png_(), stack.layer("layer")._repr_png_()):
            self.assertEqual(picture[:8], b"\x89PNG\r\n\x1a\n")
            decoded = media.decode(media.Image, picture)
            self.assertEqual(decoded.size(), (30, 20))


class StackOnAPen(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)

    def render(self, source):
        path = Path(self.directory.name) / "stacked.py"
        path.write_text(textwrap.dedent(source))
        return renderedImage(path, Path(self.directory.name) / "stacked.png")

    def test_a_sketch_puts_a_stack_down_as_its_pen_draws_the_same_layer(self):
        drawing = """
            pen.noStroke()
            pen.fill('#ff3366')
            pen.rect(4, 4, 20, 12)
            pen.fill('#33ccff')
            pen.rect(20, 8, 20, 16)
        """
        stacked = self.render(
            """
            from sigil import draw
            from sigil.sketch import sketch

            @sketch(size=(48, 32), background="#000000")
            class Stacked:
                def setup(self, ctx):
                    self.stack = draw.Stack((48, 32))
                    with self.stack.layer("ink").pen() as pen:
            """
            + textwrap.indent(textwrap.dedent(drawing), " " * 24)
            + """
                def draw(self, pen):
                    pen.stack(self.stack)
            """
        )
        direct = self.render(
            """
            from sigil.sketch import sketch

            @sketch(size=(48, 32), background="#000000")
            class Direct:
                def draw(self, pen):
            """
            + textwrap.indent(textwrap.dedent(drawing), " " * 20)
        )
        self.assertEqual(
            (stacked.width(), stacked.height()), (direct.width(), direct.height())
        )
        for x, y in ((10, 10), (30, 18), (2, 2), (44, 28)):
            self.assertEqual(
                pixel(stacked, x * stacked.width() // 48, y * stacked.height() // 32),
                pixel(direct, x * direct.width() // 48, y * direct.height() // 32),
            )

    def test_a_layer_drawn_again_in_a_later_frame_is_what_the_next_frame_shows(self):
        picture = self.render(
            """
            from sigil import draw
            from sigil.sketch import sketch

            @sketch(size=(20, 20), background="#000000", captureAt=1)
            class Redrawn:
                def setup(self, ctx):
                    self.stack = draw.Stack((20, 20))

                def draw(self, pen):
                    with self.stack.layer("ink").pen() as ink:
                        ink.background("#00ff00" if pen.frameCount > 1 else "#ff0000")
                    pen.stack(self.stack)
            """
        )
        self.assertEqual(pixel(picture, picture.width() // 2, picture.height() // 2)[:3], (0, 255, 0))

    def test_composite_on_a_pen_places_the_stack(self):
        picture = self.render(
            """
            from sigil import draw
            from sigil.sketch import sketch

            @sketch(size=(40, 20), background="#000000")
            class Placed:
                def setup(self, ctx):
                    self.stack = draw.Stack((20, 20))
                    with self.stack.layer("ink").pen() as pen:
                        pen.background("#00ff00")

                def draw(self, pen):
                    self.stack.composite(pen, 20, 0)
            """
        )
        scale = picture.width() // 40
        self.assertEqual(pixel(picture, 5 * scale, 10 * scale)[:3], (0, 0, 0))
        self.assertEqual(pixel(picture, 30 * scale, 10 * scale)[:3], (0, 255, 0))


class StackInAWindow(unittest.TestCase):
    def test_a_window_draws_the_stack_it_shows_every_frame(self):
        window = live.open(size=(40, 20), headless=True, background="#000000")
        self.addCleanup(window.close)
        stack = draw.Stack((40, 20))
        self.assertIs(window.show(stack), stack)
        self.assertIs(window.sketch, stack)
        filled(stack.layer("ground"), "#ff0000", 0, 0, 40, 20)
        for _ in range(3):
            self.assertTrue(window.pump())
        with tempfile.TemporaryDirectory() as directory:
            still = window.still(Path(directory) / "red.png")
            picture = media.decode(media.Image, Path(still).read_bytes()).frameAt(0).image
            self.assertEqual(pixel(picture, 30, 10), (255, 0, 0, 255))
            cut = stack.layer("cut")
            filled(cut, "#ffffff", 0, 0, 20, 20)
            cut.blend("remove")
            for _ in range(3):
                self.assertTrue(window.pump())
            still = window.still(Path(directory) / "cut.png")
            picture = media.decode(media.Image, Path(still).read_bytes()).frameAt(0).image
            self.assertEqual(pixel(picture, 5, 10)[:3], (0, 0, 0))
            self.assertEqual(pixel(picture, 30, 10), (255, 0, 0, 255))


if __name__ == "__main__":
    unittest.main()
