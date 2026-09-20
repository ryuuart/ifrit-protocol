"""The draw runtime seam, drawPanel and the environment.

Splitting the render tier out of the file that also bound arrangement
moves no name: the light, the shading modes, the style and the two draws
are the spellings an author had before, reading the same inputs. The
cases below are that promise, and then the tier as it now stands: who
performs a draw, the sky a surface sees past the lights, the shading
terms themselves, and the plane a 2D drawing lands on.

A PANEL'S CANVAS HAS ITS ORIGIN AT THE PANEL'S CENTRE, with y down as
any Skia canvas has, and `width` and `height` on the pen it hands over
are the panel's size in WORLD units. Every drawing case below sizes its
panel from the camera's own frustum extent at the plate, so the panel
fills the picture exactly and a pixel can be asked what it holds.
"""

import builtins
import copy
import math
import tempfile
import textwrap
import unittest
from pathlib import Path

from _sigil.geometry.mesh import render as native
from sigil import image, skia
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
    def test_every_name_the_module_carried_is_still_there(self):
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
        for name in (
            "Environment",
            "Runtime",
            "acosP",
            "atan2P",
            "attenuate",
            "backdropRay",
            "drawBackdrop",
            "drawPanel",
            "environmentBrdf",
            "environmentIrradiance",
            "environmentRadiance",
            "environmentSpecular",
            "equirectangularUv",
            "fresnelRough",
            "luminance",
            "refraction",
            "samplePanorama",
            "specularColor",
            "toneMap",
        ):
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
        self.assertEqual(style.metallic, 0)
        self.assertEqual(style.roughness, 0.5)
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
        style = render.MeshStyle(mode=render.Mode.Uv, roughness=0.125)
        self.assertEqual(style.mode, render.Mode.Uv)
        self.assertEqual(style.roughness, 0.125)
        self.assertNotEqual(style, render.MeshStyle())
        self.assertEqual(style, render.MeshStyle(mode=render.Mode.Uv, roughness=0.125))
        with self.assertRaises(TypeError):
            render.MeshStyle(glossiness=1)

    def test_the_four_dials_the_binding_had_no_spelling_for(self):
        style = render.MeshStyle(
            filter=skia.FilterMode.Nearest,
            primitiveColorLane="Color",
            environment=render.Environment(exposure=2),
            runtime=render.Runtime.cpu(),
        )
        self.assertEqual(style.filter, skia.FilterMode.Nearest)
        self.assertEqual(style.primitiveColorLane, "Color")
        self.assertEqual(style.environment.exposure, 2)
        self.assertNotEqual(style, render.MeshStyle())

    def test_a_style_copy_moves_on_its_own(self):
        style = render.MeshStyle(roughness=0.25)
        duplicate = copy.deepcopy(style)
        duplicate.roughness = 0.75
        self.assertEqual(style.roughness, 0.25)
        self.assertEqual(copy.copy(style), style)
        self.assertEqual(style.copy(), style)


class Sky(unittest.TestCase):
    def picture(self):
        return image.from_rgba(TEXELS, 2, 2)

    def assertReads(self, shown, asked):
        """The three channels, each the number beside it within a float."""
        for channel, (read, wanted) in enumerate(zip(shown, asked)):
            self.assertAlmostEqual(read, wanted, places=5, msg=(channel, shown))

    def test_an_environment_with_no_panorama_is_not_one(self):
        self.assertFalse(render.Environment().valid())
        self.assertTrue(render.Environment(levels=[self.picture()]).valid())
        self.assertTrue(render.Environment(irradiance=self.picture()).valid())
        self.assertIsNone(render.Environment().irradiance)
        self.assertEqual(render.Environment().levels, [])

    def test_nothing_falls_on_a_surface_under_no_sky(self):
        empty = render.Environment()
        self.assertEqual(render.environmentIrradiance(empty, (0, 1, 0)), (0, 0, 0))
        self.assertEqual(render.environmentRadiance(empty, (0, 0, 1), 0.5), (0, 0, 0))
        self.assertEqual(render.samplePanorama(None, (0.5, 0.5)), (0, 0, 0))

    def test_a_panorama_is_read_texel_by_texel(self):
        panorama = self.picture()
        # Four texels over the whole sphere put each texel's centre a
        # quarter of the way in, so these four readings are the texels
        # themselves rather than a blend of any two.
        for uv, texel in (
            ((0.25, 0.25), (1, 0, 0)),
            ((0.75, 0.25), (0, 1, 0)),
            ((0.25, 0.75), (0, 0, 1)),
            ((0.75, 0.75), (1, 1, 1)),
        ):
            self.assertReads(render.samplePanorama(panorama, uv), texel)
        # Azimuth wraps and the poles clamp: a turn and a quarter round
        # is the first texel again, and above the top row is the top row.
        for uv in ((1.25, 0.25), (0.25, -1)):
            self.assertReads(render.samplePanorama(panorama, uv), (1, 0, 0))

    def test_a_roughness_picks_among_the_levels(self):
        sharp = image.from_rgba(bytes([255, 0, 0, 255]) * 4, 2, 2)
        blurry = image.from_rgba(bytes([0, 255, 0, 255]) * 4, 2, 2)
        sky = render.Environment(levels=[sharp, blurry])
        ahead = (0, 0, -1)
        self.assertReads(render.environmentRadiance(sky, ahead, 0), (1, 0, 0))
        self.assertReads(render.environmentRadiance(sky, ahead, 1), (0, 1, 0))
        # A body whose roughness ramps must not step from one blur to the
        # next, so the two levels either side of the pick are mixed.
        self.assertReads(render.environmentRadiance(sky, ahead, 0.5), (0.5, 0.5, 0))
        # The bias moves the pick and nothing else.
        sky.roughnessBias = 1
        self.assertReads(render.environmentRadiance(sky, ahead, 0), (0, 1, 0))

    def test_a_second_sky_is_mixed_in_at_the_crossfade(self):
        red = image.from_rgba(bytes([255, 0, 0, 255]) * 4, 2, 2)
        green = image.from_rgba(bytes([0, 255, 0, 255]) * 4, 2, 2)
        sky = render.Environment(
            levels=[red],
            irradiance=red,
            nextLevels=[green],
            nextIrradiance=green,
            crossfade=1,
        )
        # Both panoramas are sampled and mixed rather than one being
        # rebuilt, which is what lets a sky change while the frame runs.
        self.assertReads(render.environmentRadiance(sky, (0, 0, -1), 0), (0, 1, 0))
        self.assertReads(render.environmentIrradiance(sky, (0, 1, 0)), (0, 1, 0))
        sky.crossfade = 0.5
        self.assertReads(render.environmentRadiance(sky, (0, 0, -1), 0), (0.5, 0.5, 0))
        self.assertReads(render.environmentIrradiance(sky, (0, 1, 0)), (0.5, 0.5, 0))

    def test_each_side_of_a_sky_is_scaled_by_its_own_dial(self):
        panorama = self.picture()
        plain = render.Environment(levels=[panorama], irradiance=panorama)
        bright = render.Environment(levels=[panorama], irradiance=panorama, intensity=2)
        ahead, up = (0, 0, -1), (0, 1, 0)
        for lit, dim in zip(
            render.environmentRadiance(bright, ahead, 0),
            render.environmentRadiance(plain, ahead, 0),
        ):
            self.assertAlmostEqual(lit, dim * 2, places=5)
        for lit, dim in zip(
            render.environmentIrradiance(bright, up),
            render.environmentIrradiance(plain, up),
        ):
            self.assertAlmostEqual(lit, dim * 2, places=5)
        # What a surface mirrors is the specular dial's; what falls on it
        # is the diffuse dial's, and neither reaches the other side.
        mirrorless = render.Environment(levels=[panorama], specular=0)
        self.assertEqual(render.environmentRadiance(mirrorless, ahead, 0), (0, 0, 0))
        unlit = render.Environment(irradiance=panorama, diffuse=0)
        self.assertEqual(render.environmentIrradiance(unlit, up), (0, 0, 0))

    def test_turning_the_sky_turns_what_a_direction_reads(self):
        panorama = self.picture()
        # The orientation takes a world direction into the panorama's own
        # frame, so a quarter turn about y reads world +x where the
        # unturned sky reads world -z.
        turned = render.Environment(
            levels=[panorama], orientation=((0, 0, -1), (0, 1, 0), (1, 0, 0))
        )
        flat = render.Environment(levels=[panorama])
        self.assertEqual(
            render.environmentRadiance(turned, (1, 0, 0), 0),
            render.environmentRadiance(flat, (0, 0, -1), 0),
        )
        self.assertNotEqual(
            render.environmentRadiance(turned, (1, 0, 0), 0),
            render.environmentRadiance(flat, (1, 0, 0), 0),
        )

    def test_a_sky_is_turned_by_three_columns(self):
        sky = render.Environment()
        self.assertEqual(sky.orientation, ((1, 0, 0), (0, 1, 0), (0, 0, 1)))
        # A quarter turn about y: the panorama's own x axis points along
        # world -z, which is where the first column says it points.
        sky.orientation = ((0, 0, -1), (0, 1, 0), (1, 0, 0))
        self.assertEqual(sky.orientation[0], (0, 0, -1))
        self.assertEqual(sky.orientation[2], (1, 0, 0))
        self.assertNotEqual(sky, render.Environment())
        self.assertEqual(
            render.Environment(orientation=((0, 0, -1), (0, 1, 0), (1, 0, 0))), sky
        )

    def test_an_environment_compares_and_copies_by_value(self):
        panorama = self.picture()
        sky = render.Environment(levels=[panorama], exposure=1.5, backdrop=0.5)
        self.assertEqual(
            sky, render.Environment(levels=[panorama], exposure=1.5, backdrop=0.5)
        )
        # A panorama compares by identity, so the same pixels decoded
        # twice are two skies.
        self.assertNotEqual(sky, render.Environment(levels=[self.picture()]))
        duplicate = copy.deepcopy(sky)
        duplicate.exposure = 3
        self.assertEqual(sky.exposure, 1.5)
        self.assertEqual(copy.copy(sky), sky)
        self.assertEqual(sky.copy(), sky)

    def test_a_ground_projection_only_bends_a_ray_from_inside_it(self):
        flat = render.Environment()
        self.assertEqual(render.backdropRay(flat, (0, 0, 0), (0, 0, -2)), (0, 0, -2))
        grounded = render.Environment(groundRadius=100)
        # An eye at the centre reads by direction whatever the radius,
        # which is what makes the two agree where they meet.
        self.assertEqual(
            render.backdropRay(grounded, (0, 0, 0), (0, 0, -1)), (0, 0, -1)
        )
        # An eye off centre and inside sees the sphere: the ray leaves it
        # a unit vector from the centre.
        bent = render.backdropRay(grounded, (50, 0, 0), (0, 0, -1))
        self.assertAlmostEqual(math.dist(bent, (0, 0, 0)), 1, places=4)
        self.assertGreater(bent[0], 0)


class ShadingTerms(unittest.TestCase):
    def test_a_radiance_far_over_white_still_lands_under_it(self):
        mapped = render.toneMap((100, 100, 100), 1)
        for channel in mapped:
            self.assertLess(channel, 1)
            self.assertGreater(channel, 0.98)
        self.assertEqual(render.toneMap((100, 100, 100), 0), (0, 0, 0))
        # The ratio is taken on luminance so that hue and saturation
        # survive the compression, which is also why one saturated
        # channel can still land above one.
        self.assertGreater(render.toneMap((100, 0, 0), 1)[0], 1)
        # The exposure multiplies before the curve, so doubling it is
        # one stop and nothing else.
        self.assertEqual(
            render.toneMap((0.25, 0.5, 1), 2), render.toneMap((0.5, 1, 2), 1)
        )

    def test_luminance_is_one_at_white(self):
        self.assertAlmostEqual(render.luminance((1, 1, 1)), 1, places=5)
        self.assertEqual(render.luminance((0, 0, 0)), 0)

    def test_the_zenith_reads_at_the_top_of_a_panorama(self):
        self.assertAlmostEqual(render.equirectangularUv((0, 1, 0))[1], 0, places=5)
        self.assertAlmostEqual(render.equirectangularUv((0, -1, 0))[1], 1, places=5)
        self.assertAlmostEqual(render.equirectangularUv((0, 0, -1))[0], 0.5, places=4)

    def test_the_polynomials_answer_what_the_transcendentals_do(self):
        for y, x in ((1, 1), (-1, 2), (0.25, -3), (-4, -0.5)):
            self.assertAlmostEqual(render.atan2P(y, x), math.atan2(y, x), delta=0.002)
        for value in (-1, -0.5, 0, 0.25, 1):
            self.assertAlmostEqual(render.acosP(value), math.acos(value), delta=0.002)

    def test_a_dielectric_shows_four_per_cent_and_a_metal_its_own_colour(self):
        for channel in render.specularColor((0.8, 0.2, 0.1), 0):
            self.assertAlmostEqual(channel, 0.04, places=5)
        for shown, asked in zip(
            render.specularColor((0.8, 0.2, 0.1), 1), (0.8, 0.2, 0.1)
        ):
            self.assertAlmostEqual(shown, asked, places=5)

    def test_fresnel_head_on_is_the_surface_reflectance_itself(self):
        for shown, asked in zip(
            render.fresnelRough((0.04, 0.04, 0.04), 1, 0.5), (0.04, 0.04, 0.04)
        ):
            self.assertAlmostEqual(shown, asked, places=6)
        # At the rim a smooth dielectric climbs toward white.
        self.assertGreater(render.fresnelRough((0.04, 0.04, 0.04), 0, 0)[0], 0.9)

    def test_the_split_sum_pair_scales_and_adds(self):
        scale, bias = render.environmentBrdf(0.25, 1)
        self.assertGreater(scale, 0)
        self.assertGreaterEqual(bias, 0)
        mirrored = render.environmentSpecular((1, 1, 1), (1, 1, 1), 0, 1)
        self.assertGreater(mirrored[0], 0.5)

    def test_nothing_is_taken_out_of_a_radiance_by_no_medium(self):
        self.assertEqual(render.attenuate((1, 0.5, 0.25), (2, 2, 2), 0), (1, 0.5, 0.25))
        thinned = render.attenuate((1, 1, 1), (1, 0, 0), 1)
        self.assertAlmostEqual(thinned[0], math.exp(-1), places=5)
        self.assertEqual(thinned[1], 1)

    def test_a_ray_past_total_internal_reflection_has_no_direction(self):
        self.assertEqual(render.refraction((1, 0, 0), (0, 1, 0), 2), (0, 0, 0))
        straight = render.refraction((0, -1, 0), (0, 1, 0), 1)
        self.assertAlmostEqual(straight[1], -1, places=5)


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
            "from sigil import image\n"
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
        return image.decode(output.read_bytes())

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
                             width=extent.width(), height=extent.height())
        """)
        extent = frustum()
        self.assertAlmostEqual(self.results["size"][0], extent.width(), places=3)
        self.assertAlmostEqual(self.results["size"][1], extent.height(), places=3)
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
            texture = image.from_rgba(bytes([255, 0, 0, 255] * 4), 2, 2)
            render.drawImagePanel(pen, texture, extent.width(), extent.height(),
                                  camera.Matrix(), camera.Camera(),
                                  runtime=render.Runtime.cpu())
        """)
        middle = self.pixel(picture, PLATE // 2, PLATE // 2)
        self.assertGreater(middle[0], 200)
        self.assertLess(middle[1], 60)

    def test_the_sky_itself_is_painted_where_it_is_shown_and_nowhere_else(self):
        # Every pixel of the plate reads the panorama along the ray the
        # eye looks through it, so a sky that is one colour everywhere
        # reaches every corner; at zero strength none of it is drawn.
        drawing = """
            sky = render.Environment(
                levels=[image.from_rgba(bytes([255, 0, 0, 255]) * 4, 2, 2)],
                backdrop=results['backdrop'])
            lens = camera.Camera()
            render.drawBackdrop(pen, sky, lens.projection(1), lens.view())
        """
        self.results["backdrop"] = 1
        shown = self.render(drawing)
        for spot in ((PLATE // 2, PLATE // 2), (1, 1), (PLATE - 2, PLATE - 2)):
            sky = self.pixel(shown, *spot)
            self.assertGreater(sky[0], 150, spot)
            self.assertLess(sky[1], 60, spot)
        self.results["backdrop"] = 0
        hidden = self.render(drawing)
        self.assertNotEqual(
            self.pixel(hidden, PLATE // 2, PLATE // 2),
            self.pixel(shown, PLATE // 2, PLATE // 2),
        )

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
            face = mesh.quad(extent.width(), extent.height())
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
            face = mesh.quad(extent.width(), extent.height())
            style = render.MeshStyle(lit=False, baseColor='#ffffff',
                                     backfaceCull=False,
                                     texture=image.from_rgba(results['texels'], 2, 2),
                                     filter=results['filter'])
            render.drawMesh(pen, face, camera.Matrix(), camera.Camera(), style)
        """
        self.results["texels"] = TEXELS
        centre, edge = PLATE // 4, PLATE // 2 - 2
        self.results["filter"] = skia.FilterMode.Nearest
        hard = self.render(drawing)
        self.assertEqual(self.pixel(hard, edge, centre), self.pixel(hard, 2, centre))
        self.results["filter"] = skia.FilterMode.Linear
        soft = self.render(drawing)
        self.assertNotEqual(self.pixel(soft, edge, centre), self.pixel(soft, 2, centre))


if __name__ == "__main__":
    unittest.main()
