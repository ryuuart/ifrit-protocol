"""Native retained World scenes, frame execution and Python value ownership."""

import gc
import tempfile
import threading
import unittest
import weakref
from pathlib import Path

from sigil import image, world
from sigil.geometry import mesh
from sigil.material import Material
from sigil.material import kit as surfaces
from sigil.motion import Output
from sigil.sketch import render_file


def camera() -> mesh.camera.Camera:
    result = mesh.camera.Camera()
    result.eye = (0, 0, 320)
    result.target = (0, 0, 0)
    return result


def body() -> world.Element:
    return (
        world.Element()
        .key("body")
        .mesh(mesh.box((-65, -35, -20), (65, 35, 20)))
        .fill(surfaces.unlit(surfaces.SurfaceParameters(baseColor="#e75a31")))
    )


class World(unittest.TestCase):
    def test_headless_scene_draws_native_geometry_and_image_owns_pixels(self):
        scene = world.Scene()
        scene.render(world.Frame(body()).camera(camera()))
        raster = scene.image((96, 96))
        pixels = raster.rgba()
        self.assertTrue(any(pixels[3::4]))
        self.assertEqual(pixels[:4], bytes(4))
        self.assertGreater(pixels[(48 * 96 + 48) * 4], 0)
        self.assertEqual(scene.stats.nodes, 1)
        del scene
        gc.collect()
        self.assertEqual(raster.rgba(), pixels)

    def test_children_forms_append_and_reordering_preserves_native_identity(self):
        shared = mesh.box((-10, -10, -10), (10, 10, 10))
        root = world.Element().key("root")
        for key, form in (
            ("a", "single"),
            ("b", "list"),
            ("c", "tuple"),
            ("d", "generator"),
        ):
            node = world.Element().key(key).mesh(shared)
            children = (
                node
                if form == "single"
                else [node]
                if form == "list"
                else (node,)
                if form == "tuple"
                else (item for item in [node])
            )
            self.assertIs(root.children(children), root)
        root.children(world.Element().key("e"), world.Element().key("f"))
        with self.assertRaises(TypeError):
            root.children(world.Element().key("uncommitted"), "wrong type")
        scene = world.Scene()
        scene.render(root)
        handles = {key: scene.handleOf(key) for key in "abcdef"}
        self.assertTrue(all(handles.values()))
        self.assertEqual(scene.handleOf("uncommitted"), 0)
        self.assertEqual(scene.referencesOf("a"), 4)
        scene.render(
            world.Element()
            .key("root")
            .children(world.Element().key(key).mesh(shared) for key in "dcba")
        )
        for key in "abcd":
            self.assertEqual(scene.handleOf(key), handles[key])
        self.assertEqual(scene.handleOf("e"), 0)

    def test_frame_executes_post_pass_and_reports_native_graph_errors(self):
        scene = world.Scene()
        base = (
            world.Frame(body())
            .camera(camera())
            .extent((96, 96))
            .pass_(world.geometryPass("main").writes(("color",)))
        )
        scene.render(base)
        original = scene.image((96, 96)).rgba()
        graded = (
            base.copy()
            .pass_(
                world.postPass("grade")
                .reads(["color"])
                .writes("graded")
                .levels(0.25, 0)
            )
            .present("graded")
        )
        scene.render(graded)
        result = scene.image((96, 96)).rgba()
        center = (48 * 96 + 48) * 4
        self.assertGreater(original[center], result[center])
        self.assertGreater(result[center], 0)
        self.assertEqual(scene.stats.passes, 2)
        broken = (
            world.Frame(body())
            .extent((96, 96))
            .pass_(world.postPass("a").reads("second").writes("first"))
            .pass_(world.postPass("b").reads("first").writes("second"))
        )
        with self.assertRaises(ValueError):
            scene.render(broken)
        no_extent = world.Frame(body()).pass_(world.geometryPass("main").writes("out"))
        with self.assertRaises(ValueError):
            scene.render(no_extent)
        scene.render(base)
        self.assertEqual(scene.error, "")
        self.assertEqual(scene.image((96, 96)).rgba(), original)

    def test_motion_is_sampled_and_keeps_native_output_alive(self):
        angle = Output(0)
        reference = weakref.ref(angle)
        scene = world.Scene()
        scene.render(world.Frame(body().rotateZ(angle)).camera(camera()))
        start = scene.image((96, 96)).rgba()
        angle.value = 90
        scene.advance(0)
        end = scene.image((96, 96)).rgba()
        self.assertNotEqual(start, end)
        del angle
        gc.collect()
        self.assertIsNone(reference())
        scene.advance(0.1)
        self.assertEqual(scene.image((96, 96)).rgba(), end)

    def test_lights_presets_and_getters_use_owned_native_values(self):
        material = surfaces.surface(surfaces.SurfaceParameters.gold())
        self.assertIsInstance(material, Material)
        setup = world.kit.Set(ground=0)
        setup.rig.extent = 90
        setup.table.period = 0
        setup.surface = material
        saved = setup.surface
        setup.surface = None
        self.assertIsInstance(saved, Material)
        scene = world.Scene()
        scene.render(world.kit.litSet(body(), setup, seconds=0))
        self.assertEqual(len(scene.lights), 3)
        self.assertIsNotNone(scene.camera)
        owned_camera, owned_lights, stats = scene.camera, scene.lights, scene.stats
        scene.render(world.Element())
        self.assertEqual(scene.lights, [])
        self.assertEqual(len(owned_lights), 3)
        self.assertGreater(stats.nodes, scene.stats.nodes)
        del scene
        self.assertIsNotNone(owned_camera)
        self.assertGreater(owned_lights[0].intensity, 0)

    def test_surface_colours_round_trip_through_the_space_they_are_typed_in(self):
        parameters = surfaces.SurfaceParameters()
        parameters.baseColor = "#804000"
        parameters.emissive = (0.5, 0.25, 0.125, 1.0)
        # The parameter holds light and a colour is the encoded number,
        # so a colour comes back out in the space it went in, close
        # enough that typing it into the next surface says the same
        # thing.
        typed = (0x80 / 255, 0x40 / 255, 0.0)
        for channel, expected in zip(parameters.baseColor, typed):
            self.assertAlmostEqual(channel, expected, places=4)
        for channel, expected in zip(parameters.emissive, (0.5, 0.25, 0.125)):
            self.assertAlmostEqual(channel, expected, places=4)
        # And a colour a builder was handed is the same colour on the
        # way back.
        metal = surfaces.SurfaceParameters.metal("#804000", 0.3)
        for written, built in zip(parameters.baseColor, metal.baseColor):
            self.assertAlmostEqual(written, built, places=4)

    def test_scene_rejects_wrong_thread_and_invalid_sizes_or_steps(self):
        scene = world.Scene()
        errors: list[Exception] = []

        def another_thread() -> None:
            try:
                scene.image((16, 16))
            except RuntimeError as error:
                errors.append(error)

        worker = threading.Thread(target=another_thread)
        worker.start()
        worker.join()
        self.assertEqual(len(errors), 1)
        self.assertIsInstance(errors[0], RuntimeError)
        for size in ((0, 10), (10, -1)):
            with self.assertRaises(ValueError):
                scene.image(size)
        for step in (-1, float("inf"), float("nan")):
            with self.assertRaises(ValueError):
                scene.advance(step)

    def test_world_draw_uses_checked_sketch_pen(self):
        with tempfile.TemporaryDirectory() as folder:
            source, output = Path(folder) / "world.py", Path(folder) / "world.png"
            source.write_text("""from sigil import world
from sigil.geometry import mesh
from sigil.material import kit
from sigil.sketch import sketch
@sketch(size=(96,96), capture_at=0, background="#000000")
class Study:
    def setup(self, ctx):
        self.scene = world.Scene()
        camera = mesh.camera.Camera()
        camera.eye = (0,0,320)
        body = world.Element().mesh(mesh.box((-65,-35,-20),(65,35,20)))
        body.fill(kit.unlit(kit.SurfaceParameters(baseColor="#e75a31")))
        self.scene.render(world.Frame(body).camera(camera))
    def draw(self, pen):
        self.scene.draw(pen)
""")
            render_file(source, output)
            pixels = image.load(output).rgba()
            self.assertEqual(pixels[:4], bytes([0, 0, 0, 255]))
            self.assertGreater(pixels[(48 * 96 + 48) * 4], 0)


if __name__ == "__main__":
    unittest.main()
