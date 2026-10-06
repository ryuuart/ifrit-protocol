"""Native retained World scenes, frame declarations and Python value ownership.

A scene3d sketch is drawn on a device and nowhere else, and a Python
process brings none up, so what is asked here is what the retained scene holds: the
nodes, their placements, the lights and the camera, and the refusals a
frame meets before anything is drawn.
"""

import gc
import threading
import unittest
import weakref

from sigil import world
from sigil.geometry import mesh
from sigil.geometry.mesh import pop
from sigil.material import Color, Material
from sigil.material import surface as surfaces
from sigil.motion import animatable


def camera() -> mesh.camera.Camera:
    result = mesh.camera.Camera()
    result.eye = (0, 0, 320)
    result.target = (0, 0, 0)
    return result


def body() -> world.Mesh:
    return (
        world.mesh(mesh.box((-65, -35, -20), (65, 35, 20)))
        .key("body")
        .fill(surfaces.unlit(surfaces.SurfaceParameters(baseColor="#e75a31")))
    )


class World(unittest.TestCase):
    def test_headless_scene_retains_native_geometry(self):
        scene = world.Scene()
        scene.render(world.Frame(body()).defaultCamera(camera()))
        self.assertEqual(scene.stats.nodes, 1)
        self.assertNotEqual(scene.handleOf("body"), 0)
        self.assertEqual(scene.referencesOf("body"), 1)

    def test_group_forms_and_reordering_preserve_native_identity(self):
        shared = mesh.box((-10, -10, -10), (10, 10, 10))
        children = [world.mesh(shared).key(key) for key in "abcd"]
        # A group takes its children as its argument: nodes, or one
        # iterable of them.
        root = world.group(children + [world.group().key("e"), world.group().key("f")])
        self.assertIsInstance(root.key("root"), world.Group)
        self.assertIsInstance(world.group(*children), world.Group)
        self.assertIsInstance(world.group(node for node in children), world.Group)
        with self.assertRaises(TypeError):
            world.group(world.group().key("uncommitted"), "wrong type")
        scene = world.Scene()
        scene.render(root)
        handles = {key: scene.handleOf(key) for key in "abcdef"}
        self.assertTrue(all(handles.values()))
        self.assertEqual(scene.handleOf("uncommitted"), 0)
        self.assertEqual(scene.referencesOf("a"), 4)
        scene.render(
            world.group(world.mesh(shared).key(key) for key in "dcba").key("root")
        )
        for key in "abcd":
            self.assertEqual(scene.handleOf(key), handles[key])
        self.assertEqual(scene.handleOf("e"), 0)

    def test_every_kind_places_itself_and_a_light_links_to_a_collection(self):
        lamp = (
            world.pointLight(color=(2, 1, 0.5, 1), intensity=0.5, range=300)
            .key("lamp")
            .at((10, 20, 30))
            .lightLink("props")
        )
        self.assertIsInstance(lamp, world.Light)
        self.assertEqual(lamp.description.kind, world.LightKind.Point)
        self.assertEqual(lamp.description.lightLink.include, ["props"])
        spot = world.spotLight((0, -1, 0), innerAngle=10, outerAngle=30)
        self.assertEqual(spot.description.outerAngle, 30)
        eye = world.camera(camera(), exposure=1.5).key("eye")
        scene = world.Scene()
        scene.render(world.group(lamp, eye, body().collection("props")))
        self.assertEqual(len(scene.lights), 1)
        self.assertEqual(tuple(scene.lights[0].position), (10, 20, 30))
        self.assertEqual(scene.lights[0].key, "lamp")
        self.assertIsNotNone(scene.camera)
        # Several cameras with none named is refused, naming them.
        with self.assertRaisesRegex(ValueError, "eye"):
            scene.render(world.group(eye, world.camera(camera()).key("other")))

    def test_a_light_casts_when_asked_and_a_body_can_decline(self):
        sun = world.directionalLight(
            (0, -1, 0), castShadow=True, shadow=world.LightShadow(radius=2)
        ).shadowLink(world.Collection(exclude=["glass"]))
        self.assertTrue(sun.description.castShadow)
        self.assertEqual(sun.description.shadow.radius, 2)
        self.assertEqual(sun.description.shadow.mapSize, 2048)
        self.assertEqual(sun.description.shadow.cascades, 1)
        self.assertEqual(sun.description.shadowLink.exclude, ["glass"])
        self.assertFalse(world.pointLight().description.castShadow)
        self.assertIsInstance(body().castShadow(False).receiveShadow(False), world.Mesh)

    def test_lights_with_a_size_carry_it_and_the_kit_shows_them(self):
        panel = world.rectLight((0, -1, 0), width=4, height=2, intensity=3)
        self.assertEqual(panel.description.kind, world.LightKind.Rect)
        self.assertEqual(panel.description.width, 4)
        self.assertEqual(panel.description.height, 2)
        ball = world.sphereLight(radius=0.5, intensity=world.intensityAt(1, 3))
        self.assertEqual(ball.description.kind, world.LightKind.Sphere)
        self.assertEqual(ball.description.radius, 0.5)
        disk = world.diskLight((0, 0, -1), radius=0.25)
        self.assertEqual(disk.description.kind, world.LightKind.Disk)
        self.assertIsInstance(world.kit.visibleLight(panel.key("panel")), world.Group)

    def test_instanced_geometry_is_a_mesh_and_a_null_is_looked_at(self):
        sparks = mesh.Instances()
        sparks.stamp = mesh.quad(4, 4)
        sparks.positions = [(0, 0, 0), (10, 0, 0)]
        sparks.orientations = [(0, 0, 0, 1), (0, 0, 0, 1)]
        sparks.setAttribute("age", [0.25, 0.75])
        self.assertEqual(len(sparks), 2)
        self.assertEqual(sparks.attribute("age"), [0.25, 0.75])
        self.assertEqual(sparks.attributeNames(), ["age"])
        with self.assertRaises(ValueError):
            sparks.setAttribute("phase", [0.5])
        eye = world.camera(camera()).key("eye").lookAt("pivot")
        scene = world.Scene()
        scene.render(
            world.group(
                world.mesh(sparks).key("sparks"),
                world.null().key("pivot").at((5, 6, 7)),
                eye,
            )
        )
        self.assertNotEqual(scene.handleOf("sparks"), 0)
        self.assertEqual(tuple(scene.camera.target), (5, 6, 7))
        with self.assertRaisesRegex(ValueError, "nowhere"):
            scene.render(world.group(world.camera(camera()).lookAt("nowhere")))

    def test_a_point_chain_ends_in_instances_a_body_wears(self):
        loop = [(0, 0, 0), (100, 0, 0), (100, 100, 0), (0, 100, 0)]
        chain = (
            pop.on(loop)
            .count(24)
            .spread(4)
            .jitter(1.5)
            .vary(0.3)
            .fade((1, 0.5, 0, 1), (1, 1, 1, 1))
            .select("top", pop.Select.Shape.Box, (50, 90, 0), (200, 40, 200))
            .keep("top")
        )
        sparks = chain.instances(mesh.quad(4, 4))
        self.assertGreater(len(sparks), 0)
        self.assertLess(len(sparks), 24)
        self.assertEqual(len(sparks.colors), len(sparks))
        every = pop.on(loop).count(24).instances(mesh.quad(4, 4))
        self.assertEqual(len(every), 24)
        self.assertEqual(
            len(pop.on(pop.on(loop).count(24)).count(5).instances(mesh.quad(1, 1))), 5
        )
        surface = pop.on(mesh.box((-50, -50, -50), (50, 50, 50)), 40).fill(
            "heat", (1, 0, 0, 0)
        )
        self.assertEqual(len(surface.instances(mesh.quad(2, 2))), 40)
        scene = world.Scene()
        scene.render(world.group(world.mesh(sparks).key("sparks")))
        self.assertNotEqual(scene.handleOf("sparks"), 0)

    def test_frame_passes_need_a_device_and_report_native_graph_errors(self):
        scene = world.Scene()
        base = (
            world.Frame(body())
            .defaultCamera(camera())
            .extent((96, 96))
            .pass_(world.geometryPass("main").writes(("color",)))
        )
        # A pass is drawn on a device, and a frame that names no runtime
        # is refused with the reason rather than drawn as nothing.
        with self.assertRaisesRegex(ValueError, "device"):
            scene.render(base)
        self.assertEqual(scene.stats.passes, 0)
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
        scene.render(world.Frame(body()).defaultCamera(camera()))
        self.assertEqual(scene.error, "")

    def test_motion_is_sampled_and_keeps_a_live_value_alive(self):
        angle = animatable(0)
        reference = weakref.ref(angle)
        scene = world.Scene()
        scene.render(world.Frame(body().rotateZ(angle)).defaultCamera(camera()))
        start = scene.transformOf("body")
        angle.value = 90
        scene.advance(0)
        end = scene.transformOf("body")
        self.assertNotEqual(start, end)
        del angle
        gc.collect()
        self.assertIsNone(reference())
        scene.advance(0.1)
        self.assertEqual(scene.transformOf("body"), end)

    def test_lights_presets_and_getters_use_owned_native_values(self):
        material = surfaces.program(
            surfaces.SurfaceParameters(
                baseColor=(1.0, 0.766, 0.336), metallic=1, roughness=0.18
            )
        )
        self.assertIsInstance(material, Material)
        setup = world.kit.Set(ground=0)
        setup.rig.extent = 90
        setup.table.period = 0
        setup.surface = material
        saved = setup.surface
        setup.surface = None
        self.assertIsInstance(saved, Material)
        scene = world.Scene()
        scene.render(world.kit.litSet(body(), setup, now=0))
        self.assertEqual(len(scene.lights), 3)
        self.assertIsNotNone(scene.camera)
        owned_camera, owned_lights, stats = scene.camera, scene.lights, scene.stats
        scene.render(world.group())
        self.assertEqual(scene.lights, [])
        self.assertEqual(len(owned_lights), 3)
        self.assertGreater(stats.nodes, scene.stats.nodes)
        del scene
        self.assertIsNotNone(owned_camera)
        self.assertGreater(owned_lights[0].light.intensity, 0)

    def test_surface_colours_round_trip_through_the_space_they_are_typed_in(self):
        parameters = surfaces.SurfaceParameters()
        parameters.baseColor = "#804000"
        parameters.emissive = (0.5, 0.25, 0.125, 1.0)
        # The parameter is a factor on the map in its slot, so nothing
        # converts a colour on the way in or on the way out and it
        # comes back the number it was typed as, close enough to write
        # into the next surface.
        typed = (0x80 / 255, 0x40 / 255, 0.0)
        for channel, expected in zip(parameters.baseColor, typed):
            self.assertAlmostEqual(channel, expected, places=4)
        for channel, expected in zip(parameters.emissive, (0.5, 0.25, 0.125)):
            self.assertAlmostEqual(channel, expected, places=4)
        # And a colour a builder was handed is the same colour on the
        # way back.
        metal = surfaces.SurfaceParameters(
            baseColor="#804000", metallic=1, roughness=0.3
        )
        for written, built in zip(parameters.baseColor, metal.baseColor):
            self.assertAlmostEqual(written, built, places=4)
        # The attenuation colour is a colour like the others: what is
        # written is what is read back.
        parameters.attenuationColor = Color(0.25, 0.5, 0.75)
        for channel, expected in zip(parameters.attenuationColor, (0.25, 0.5, 0.75)):
            self.assertAlmostEqual(channel, expected, places=4)
        parameters.attenuationDistance = 2.0
        self.assertAlmostEqual(parameters.attenuationDistance, 2.0, places=4)

    def test_scene_rejects_wrong_thread_and_invalid_sizes_or_steps(self):
        scene = world.Scene()
        errors: list[Exception] = []

        def another_thread() -> None:
            try:
                scene.handleOf("body")
            except RuntimeError as error:
                errors.append(error)

        worker = threading.Thread(target=another_thread)
        worker.start()
        worker.join()
        self.assertEqual(len(errors), 1)
        self.assertIsInstance(errors[0], RuntimeError)
        for size in ((0, 10), (10, -1)):
            with self.assertRaises(ValueError):
                world.Frame(body()).extent(size)
        for step in (-1, float("inf"), float("nan")):
            with self.assertRaises(ValueError):
                scene.advance(step)


if __name__ == "__main__":
    unittest.main()
