"""Native value interoperability and ownership across Python drawing calls."""

import runpy
import tempfile
import unittest
from pathlib import Path

from sigil import compose, image, material, motion, skia, weave
from sigil.geometry import mesh
from sigil.material import field
from sigil.sketch import render_file

Paint = material.Paint


class Values(unittest.TestCase):
    def test_pixel_image_copies_contiguous_buffer(self):
        pixels = bytearray([255, 0, 0, 255, 0, 128, 255, 255])
        native = image.from_rgba(pixels, 2, 1)
        pixels[:] = bytes(8)
        self.assertEqual(native.rgba(), bytes([255, 0, 0, 255, 0, 128, 255, 255]))
        self.assertEqual(image.decode(image.encode(native)).rgba(), native.rgba())
        with self.assertRaises(ValueError):
            image.from_rgba(bytes(3), 1, 1)

    def test_native_path_boolean_preserves_hole(self):
        outer = skia.Path.Rect((0, 0, 100, 100))
        hole = skia.Path.Circle(50, 50, 20)
        ring = skia.pathOp(outer, hole, skia.PathOp.Difference)
        self.assertTrue(ring.contains(10, 10))
        self.assertFalse(ring.contains(50, 50))
        self.assertTrue(ring.offset(100, 0).contains(110, 10))

    def test_material_copies_keep_independent_uniforms(self):
        source = skia.RuntimeEffect.MakeForShader(
            "uniform float4 tone; half4 main(float2 p) { return half4(tone); }"
        )
        paint = Paint.sksl(source, {"tone": [1, 0, 0, 1]})
        copy = paint.copy()
        copy.uniform("tone", [0, 1, 0, 1])
        self.assertNotEqual(paint, copy)
        self.assertFalse(paint.isAnimated())
        self.assertFalse(Paint.recipe(field.noise(0.02)).isNone())
        with self.assertRaises(ValueError):
            Paint.sksl("invalid shader source")

    def test_native_type_accepts_relative_lengths(self):
        font = weave.Type()
        font.size = 20
        font.track = weave.em(0.05)
        self.assertEqual(font.size.value, 20)
        self.assertAlmostEqual(font.track.value, 0.05)

    def test_parametric_mesh_and_camera_use_native_geometry(self):
        plane = mesh.grid(5, 7, lambda u, v: (u * 10, v * 20, 0))
        self.assertGreater(plane.triangleCount(), 0)
        lo, hi = plane.bounds()
        self.assertEqual(lo, (0, 0, 0))
        self.assertEqual(hi, (10, 20, 0))
        camera = mesh.camera.Camera()
        center = camera.project((0, 0, 0), (640, 480))
        self.assertAlmostEqual(center.x, 320)
        self.assertAlmostEqual(center.y, 240)
        self.assertIsNone(camera.project((0, 0, 500), (640, 480)))

    def test_shader_slot_and_mesh_render_headlessly(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "scene.py"
            output = Path(folder) / "scene.png"
            source.write_text("""from sigil.sketch import sketch
from sigil.material import Paint
from sigil.geometry import mesh
@sketch(size=(96, 96), capture_at=0.02, background="#000000")
class Scene:
    def setup(self, ctx):
        self.fill = Paint.sksl("uniform shader source; half4 main(float2 p) { return source.eval(p); }").slot("source", Paint.solid("#22cc88"))
        self.body = mesh.box((-30,-30,-30), (30,30,30))
        self.camera = mesh.camera.Camera()
        self.camera.eye = (80, 60, 170)
        self.style = mesh.render.MeshStyle()
        self.style.baseColor = "#ffaa44"
    def draw(self, pen):
        pen.background(self.fill)
        mesh.render.drawMesh(pen, self.body, mesh.camera.place(), self.camera, self.style)
""")
            render_file(source, output)
            raster = image.load(output)
            pixels = raster.rgba()
            self.assertEqual(pixels[:4], bytes([34, 204, 136, 255]))
            center = (48 * 96 + 48) * 4
            self.assertNotEqual(pixels[center : center + 4], pixels[:4])


class Colors(unittest.TestCase):
    def test_every_role_union_runs_as_it_is_declared(self):
        # The fixture beside the typing checks is the declared surface of
        # the three colouring roles. Running it is the other half of the
        # promise: what the declarations allow, the bindings accept.
        fixture = Path(__file__).parent / "typing" / "colors.py"
        runpy.run_path(str(fixture), run_name="colors")

    def test_one_colour_class_reads_every_spelling(self):
        self.assertEqual(
            material.Color("#6e99bb"),
            material.Color(0x6E / 255, 0x99 / 255, 0xBB / 255),
        )
        read = compose.Fill.color((0.25, 0.5, 0.75)).colorValue
        self.assertIsInstance(read, material.Color)
        self.assertEqual(read, material.Color(0.25, 0.5, 0.75))
        # A colour returned by one library is a colour the next accepts.
        self.assertEqual(Paint.solid(read), Paint.solid((0.25, 0.5, 0.75)))
        with self.assertRaises(TypeError):
            compose.Fill.color(7)

    def test_a_flat_mark_refuses_what_it_cannot_hold(self):
        recipe = material.kit.unlit(material.kit.SurfaceParameters(baseColor="#e75a31"))
        unit = Paint.linearUnit((0, 0), (1, 0), [(0, "#000"), (1, "#fff")])
        # The messages are the refusal: falling back to the generic colour
        # error would say a colour is a string or a sequence, which tells
        # an author nothing about where the value they wrote does belong.
        with self.assertRaisesRegex(TypeError, "A material is not a flat fill"):
            compose.Fill(recipe)
        with self.assertRaisesRegex(TypeError, "geometry-dependent paint"):
            compose.Fill(unit)
        # A glyph outline is one such flat mark, and the node's own fill
        # is not, so the same value is refused at one and taken at the other.
        with self.assertRaisesRegex(TypeError, "geometry-dependent paint"):
            compose.text("words").textStroke(1, unit)
        # The same values are a surface paint, which is what the message
        # sends the author to.
        self.assertFalse(compose.SurfacePaint(recipe).none())
        self.assertIsInstance(compose.box().fill(recipe), compose.Element)
        self.assertIsInstance(compose.text("words").ink(unit), compose.Text)

    def test_an_ink_paint_refuses_what_it_cannot_store(self):
        ramp = Paint.linearUnit((0, 0), (1, 0), [(0, "#000"), (1, "#fff")])
        # An ink paint is one paint resolved without the tree, so a bound
        # fill says so rather than quietly dropping the ramp already set.
        # A custom-property reference is the ink lane's own spelling and
        # sets the ink from that property.
        with self.assertRaisesRegex(TypeError, "clear the paint with None"):
            compose.text("words").ink(ramp).ink(compose.Fill.currentInk())
        self.assertIsInstance(
            compose.text("words").ink(ramp).ink(None), compose.Text
        )
        self.assertIsInstance(
            compose.text("words").ink(ramp).ink(compose.var("accent")),
            compose.Text,
        )

    def test_an_ink_paint_takes_the_box_it_is_stretched_over(self):
        ramp = Paint.linearUnit((0, 0), (1, 0), [(0, "#000"), (1, "#fff")])
        for box in (
            compose.PaintBox.Element,
            compose.PaintBox.Subtree,
            compose.PaintBox.Canvas,
        ):
            with self.subTest(box=box):
                self.assertIsInstance(
                    compose.box().ink(ramp, box=box), compose.Element
                )

    def test_an_ink_paint_restarts_on_the_text_unit_it_names(self):
        # A text unit restarts the paint on each letter, word or line; the
        # element's own box, the default, lays it once across the passage.
        # The rule and the span take the same keyword the element does.
        ramp = Paint.linearUnit((0, 0), (1, 0), [(0, "#f00"), (1, "#00f")])
        for box in (
            compose.PaintBox.Element,
            compose.PaintBox.Glyph,
            compose.PaintBox.Cluster,
            compose.PaintBox.Word,
            compose.PaintBox.Line,
            compose.PaintBox.Sentence,
        ):
            with self.subTest(box=box):
                self.assertIsInstance(
                    compose.text("two words").ink(ramp, box=box), compose.Text
                )
        self.assertNotEqual(
            compose.rule(".chrome").ink(ramp, box=compose.PaintBox.Glyph),
            compose.rule(".chrome").ink(ramp),
        )
        self.assertIsInstance(
            compose.text("two words").span(
                weave.selectors.word(1),
                compose.SpanStyle().ink(ramp, box=compose.PaintBox.Word),
            ),
            compose.Text,
        )

    def test_a_colour_takes_a_box_and_ignores_it(self):
        # A colour has no unit square for a box to stretch.
        self.assertEqual(
            compose.rule(".red").ink("#ff0000", box=compose.PaintBox.Glyph),
            compose.rule(".red").ink("#ff0000"),
        )

    def test_a_fill_takes_the_box_it_is_stretched_over(self):
        ramp = Paint.linearUnit((0, 0), (1, 0), [(0, "#000"), (1, "#fff")])
        for box in (
            compose.PaintBox.Element,
            compose.PaintBox.Padding,
            compose.PaintBox.Content,
            compose.PaintBox.Canvas,
        ):
            with self.subTest(box=box):
                self.assertIsInstance(
                    compose.box().fill(ramp, box=box), compose.Element
                )
        # A box places a paint's unit square, so a surface with none to
        # place is applied whole, exactly as without the box.
        self.assertEqual(
            compose.rule(".x").fill(
                compose.Fill.currentInk(), box=compose.PaintBox.Canvas
            ),
            compose.rule(".x").fill(compose.Fill.currentInk()),
        )
        self.assertFalse(hasattr(compose, "PaintAnchor"))
        self.assertFalse(hasattr(compose, "BackgroundOrigin"))
        self.assertFalse(hasattr(compose.Property, "BackgroundOrigin"))

    def test_a_uniform_is_written_the_same_way_on_a_paint_and_an_effect(self):
        source = skia.RuntimeEffect.MakeForShader(
            "uniform float4 tint; half4 main(float2 p) { return half4(tint); }"
        )
        # A colour uniform, written as the colour class and as a CSS
        # string, is the same uniform on either seam.
        self.assertEqual(
            Paint.sksl(source).uniform("tint", "#ff0000"),
            Paint.sksl(source).uniform("tint", material.Color("#ff0000")),
        )
        self.assertEqual(
            material.Effect.shader(source).uniform("tint", "#ff0000"),
            material.Effect.shader(source).uniform("tint", material.Color("#ff0000")),
        )
        # …and so is a live scalar, which is what makes either animate.
        moving = skia.RuntimeEffect.MakeForShader(
            "uniform float amount; half4 main(float2 p) { return half4(amount); }"
        )
        output = motion.Output(0.0)
        self.assertTrue(Paint.sksl(moving).uniform("amount", output).isAnimated())
        self.assertTrue(
            material.Effect.shader(moving).uniform("amount", output).isAnimated()
        )


if __name__ == "__main__":
    unittest.main()
