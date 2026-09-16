"""Native value interoperability and ownership across Python drawing calls."""

import tempfile
import unittest
from pathlib import Path

from sigil import image, skia, weave
from sigil.geometry import mesh
from sigil.material import field
from sigil.material import skia as material_skia
from sigil.sketch import render_file

Paint = material_skia.Paint


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
from sigil.material import skia
from sigil.geometry import mesh
Paint = skia.Paint
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


if __name__ == "__main__":
    unittest.main()
