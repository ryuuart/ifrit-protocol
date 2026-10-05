"""The draw runtime seam and drawPanel.

The light, the shading modes, the style and the two draws are the
spellings an author draws a mesh with. The cases below are that promise,
and then who performs a draw and the plane a 2D drawing lands on.

A PANEL'S CANVAS HAS ITS ORIGIN AT THE PANEL'S CENTRE, with y down as
any Skia canvas has, and `width` and `height` on the pen it hands over
are the panel's size in WORLD units. Every drawing case below sizes its
panel from the camera's own frustum extent at the plate, so the panel
fills the picture exactly and a pixel can be asked what it holds.
"""

import builtins
import copy
import tempfile
import textwrap
import unittest
from pathlib import Path

from _sigil.geometry.mesh import render as native
from sigil import media
from sigil.geometry.mesh import camera, render
from sigil.sketch import render_file

PLATE = 32
# The smallest picture with a seam down the middle and another across it,
# in image order: red and green along the top row, blue and white under
# them, every texel opaque.
TEXELS = bytes(
    channel
    for texel in ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255))
    for channel in (*texel, 255)
)


def frustum():
    """The plane at the origin that fills a square plate exactly."""
    lens = camera.Camera()
    return lens.extentAt(lens.eye[2], 1)


class MovedNames(unittest.TestCase):
    def test_every_name_the_module_carries_is_there(self):
        for name in ("Light", "MeshStyle", "Mode", "drawImagePanel", "drawMesh"):
            self.assertTrue(hasattr(native, name), name)
            self.assertTrue(hasattr(render, name), name)

    def test_the_moved_classes_report_the_package_they_come_from(self):
        for value in (native.Light, native.MeshStyle, native.Mode):
            self.assertEqual(value.__module__, "sigil.geometry.mesh.render")

    def test_the_names_the_tier_gained_are_reached_by_their_public_name(self):
        # An author imports the public package, so a name the module
        # registers that the package does not re-export is a name nobody
        # can call.
        for name in ("Runtime", "Sampling", "drawPanel"):
            self.assertIs(getattr(render, name), getattr(native, name), name)
            self.assertIn(name, render.__all__)

    def test_a_light_is_still_built_by_position_and_by_keyword(self):
        lamp = render.Light((0, -1, 0), "#ff0000", 2)
        self.assertEqual(lamp.direction, (0, -1, 0))
        self.assertEqual(lamp.intensity, 2)
        self.assertEqual(lamp.color.r, 1)
        self.assertEqual(render.Light(intensity=2).direction, render.Light().direction)

    def test_a_style_still_stands_at_the_dials_it_stood_at(self):
        style = render.MeshStyle()
        self.assertEqual(style.mode, render.Mode.Lit)
        self.assertTrue(style.lit)
        self.assertTrue(style.backfaceCull)
        self.assertTrue(style.depthSort)
        self.assertFalse(style.tileTexture)
        self.assertIsNone(style.texture)
        self.assertEqual(style.specular, 0.5)
        self.assertEqual(style.shininess, 48)
        self.assertEqual(style.rim, 0.25)
        self.assertEqual(len(style.lights), 1)
        self.assertEqual(style.lights[0], render.Light())


class RuntimeSeam(unittest.TestCase):
    def test_the_built_in_executor_is_one_value(self):
        self.assertEqual(render.Runtime.cpu(), render.Runtime.cpu())
        self.assertTrue(render.Runtime.cpu())
        self.assertEqual(copy.deepcopy(render.Runtime.cpu()), render.Runtime.cpu())
        self.assertEqual(render.Runtime.cpu().copy(), render.Runtime.cpu())

    def test_a_style_carries_the_runtime_its_draw_executes_on(self):
        self.assertEqual(render.MeshStyle().runtime, render.Runtime.cpu())
        # The reason two default styles compare equal at all: the
        # executor inside them is the same value, not two of a kind.
        self.assertEqual(render.MeshStyle(), render.MeshStyle())
        self.assertEqual(
            render.MeshStyle(runtime=render.Runtime.cpu()), render.MeshStyle()
        )

    def test_a_style_is_built_from_keywords_and_compares_by_value(self):
        style = render.MeshStyle(mode=render.Mode.Uv, shininess=12)
        self.assertEqual(style.mode, render.Mode.Uv)
        self.assertEqual(style.shininess, 12)
        self.assertNotEqual(style, render.MeshStyle())
        self.assertEqual(style, render.MeshStyle(mode=render.Mode.Uv, shininess=12))
        with self.assertRaises(TypeError):
            render.MeshStyle(glossiness=1)

    def test_the_dials_beside_the_shading(self):
        style = render.MeshStyle(
            filter=render.Sampling.Nearest,
            primitiveColorLane="Color",
            runtime=render.Runtime.cpu(),
        )
        self.assertEqual(style.filter, render.Sampling.Nearest)
        self.assertEqual(style.primitiveColorLane, "Color")
        self.assertNotEqual(style, render.MeshStyle())

    def test_a_style_copy_moves_on_its_own(self):
        style = render.MeshStyle(rim=0.25)
        duplicate = copy.deepcopy(style)
        duplicate.rim = 0.75
        self.assertEqual(style.rim, 0.25)
        self.assertEqual(copy.copy(style), style)
        self.assertEqual(style.copy(), style)


class Panels(unittest.TestCase):
    """The plane a 2D drawing lands on, rendered through a real session."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        builtins._sigil_render_results = self.results
        self.addCleanup(delattr, builtins, "_sigil_render_results")

    def render(self, drawing):
        source = Path(self.directory.name) / "panel.py"
        output = Path(self.directory.name) / "panel.png"
        source.write_text(
            "import builtins\n"
            "from sigil import media\n"
            "from sigil.geometry import mesh\n"
            "from sigil.geometry.mesh import camera, render\n"
            "from sigil.sketch import sketch\n"
            "results = builtins._sigil_render_results\n"
            "def frustum():\n"
            "    lens = camera.Camera()\n"
            "    return lens.extentAt(lens.eye[2], 1)\n"
            f"@sketch(size=({PLATE}, {PLATE}), capture_at=0)\n"
            "class Panel:\n"
            "    def draw(self, pen):\n"
            + textwrap.indent(textwrap.dedent(drawing).strip(), "        ")
            + "\n"
        )
        render_file(source, output, at=0)
        return media.decode(media.Image, output.read_bytes()).frameAt(0).image

    def pixel(self, picture, x, y):
        offset = 4 * (y * picture.width() + x)
        return tuple(picture.rgba()[offset : offset + 4])

    def test_a_panel_hands_its_body_a_pen_sized_in_world_units(self):
        picture = self.render("""
            extent = frustum()
            def body(face):
                results['size'] = (face.width, face.height)
                results['face'] = face
                face.noStroke()
                face.fill('#ff0000')
                face.rect(-face.width / 2, -face.height / 2, face.width, face.height)
            render.drawPanel(pen, camera.Matrix(), camera.Camera(), body,
                             width=extent[0], height=extent[1])
        """)
        extent = frustum()
        self.assertAlmostEqual(self.results["size"][0], extent[0], places=3)
        self.assertAlmostEqual(self.results["size"][1], extent[1], places=3)
        for spot in ((PLATE // 2, PLATE // 2), (4, 4), (PLATE - 5, PLATE - 5)):
            red = self.pixel(picture, *spot)
            self.assertGreater(red[0], 200, spot)
            self.assertLess(red[1], 60, spot)

    def test_the_pen_a_panel_lent_cannot_draw_once_the_panel_is_drawn(self):
        self.render("""
            def body(face):
                results['face'] = face
            render.drawPanel(pen, camera.Matrix(), camera.Camera(), body,
                             width=100, height=100)
        """)
        with self.assertRaisesRegex(RuntimeError, "callback"):
            _ = self.results["face"].width

    def test_a_body_that_raises_leaves_the_canvas_where_it_found_it(self):
        picture = self.render("""
            def body(face):
                face.push()
                face.rotate(0.3)
                raise ValueError('the panel gave up')
            try:
                render.drawPanel(pen, camera.Matrix(), camera.Camera(), body,
                                 width=100, height=100)
            except RuntimeError as error:
                results['raised'] = str(error)
            pen.noStroke()
            pen.fill('#00ff00')
            pen.rect(0, 0, pen.width, pen.height)
        """)
        self.assertIn("the panel gave up", self.results["raised"])
        # The transform, the clip and the save the panel drew under are
        # all the executor's; a rect over the whole plate proves every one
        # of them came back, the abandoned push included.
        for spot in ((PLATE // 2, PLATE // 2), (2, 2), (PLATE - 3, PLATE - 3)):
            self.assertGreater(self.pixel(picture, *spot)[1], 200, spot)

    def test_an_image_panel_takes_the_runtime_it_draws_on(self):
        picture = self.render("""
            extent = frustum()
            texture = media.fromRgba(bytes([255, 0, 0, 255] * 4), 2, 2)
            render.drawImagePanel(pen, texture, extent[0], extent[1],
                                  camera.Matrix(), camera.Camera(),
                                  runtime=render.Runtime.cpu())
        """)
        middle = self.pixel(picture, PLATE // 2, PLATE // 2)
        self.assertGreater(middle[0], 200)
        self.assertLess(middle[1], 60)

    def test_a_triangle_naming_a_vertex_the_mesh_has_not_got_is_refused(self):
        # An index past the positions would be read inside the executor,
        # where nothing can answer for it, so the draw is refused at the
        # seam instead.
        self.render("""
            face = mesh.quad(100, 100)
            face.indices = [0, 1, face.vertexCount()]
            try:
                render.drawMesh(pen, face, camera.Matrix(), camera.Camera())
            except ValueError as error:
                results['refused'] = str(error)
        """)
        self.assertIn("outside the position array", self.results["refused"])

    def test_a_primitive_lane_tints_one_triangle_and_not_the_other(self):
        # The quad's two triangles meet along the diagonal from the
        # bottom-left corner to the top-right one, so these two spots
        # stand one in each.
        drawing = """
            extent = frustum()
            face = mesh.quad(extent[0], extent[1])
            face.setPrimitive('Color', [(1, 0, 0, 1), (0, 0, 1, 1)])
            style = render.MeshStyle(lit=False, baseColor='#ffffff',
                                     backfaceCull=False,
                                     primitiveColorLane=results['lane'])
            render.drawMesh(pen, face, camera.Matrix(), camera.Camera(), style)
        """
        self.results["lane"] = ""
        untinted = self.render(drawing)
        self.assertEqual(
            self.pixel(untinted, 6, 6), self.pixel(untinted, PLATE - 7, PLATE - 7)
        )
        self.results["lane"] = "Color"
        tinted = self.render(drawing)
        self.assertNotEqual(
            self.pixel(tinted, 6, 6), self.pixel(tinted, PLATE - 7, PLATE - 7)
        )

    def test_a_nearest_filter_keeps_a_texel_edge_hard(self):
        # Four texels over the whole plate put the seam down the middle,
        # with each texel's centre a quarter of the way in. A reading two
        # pixels short of the seam is a blend under a linear filter and
        # the texel itself under a nearest one.
        drawing = """
            extent = frustum()
            face = mesh.quad(extent[0], extent[1])
            style = render.MeshStyle(lit=False, baseColor='#ffffff',
                                     backfaceCull=False,
                                     texture=media.fromRgba(results['texels'], 2, 2),
                                     filter=results['filter'])
            render.drawMesh(pen, face, camera.Matrix(), camera.Camera(), style)
        """
        self.results["texels"] = TEXELS
        centre, edge = PLATE // 4, PLATE // 2 - 2
        self.results["filter"] = render.Sampling.Nearest
        hard = self.render(drawing)
        self.assertEqual(self.pixel(hard, edge, centre), self.pixel(hard, 2, centre))
        self.results["filter"] = render.Sampling.Linear
        soft = self.render(drawing)
        self.assertNotEqual(self.pixel(soft, edge, centre), self.pixel(soft, 2, centre))


if __name__ == "__main__":
    unittest.main()
