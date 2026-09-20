"""The mesh currency: its lanes, its faces and the sheet it is built by.

Splitting the currency out of the file that also bound arrangement and
the painter moves no name: every spelling an author had before is the
spelling they have now, on the same module, reading the same inputs.
These cases are that promise, and the readings the tier had no spelling
for at all — the primitive lanes, the faces a solid is made of, the
parametric sheet that answers its own normal, and the option records the
generators were always documented to take.

A primitive lane is one float4 per TRIANGLE, and a face is what a solid
is made of rather than what it is drawn from: a dodecahedron is twelve
pentagons and thirty-six triangles at the same time.
"""

import copy
import gc
import math
import unittest

from _sigil.geometry import mesh as native
from sigil import skia
from sigil.geometry import mesh

# The corner of a unit-circumradius cube: every face centre stands this
# far from the middle along its own outward normal.
CUBE_FACE_DISTANCE = 1 / math.sqrt(3)


def plane(u: float, v: float) -> tuple[float, float, float]:
    """A flat sheet in xy, whose differenced normal is the z axis."""
    return (u * 100, v * 100, 0)


def sphere(u: float, v: float) -> tuple[float, float, float]:
    """A sphere whose first row of stations all stand on the +y pole."""
    around = u * 2 * math.pi
    down = v * math.pi
    return (
        40 * math.sin(down) * math.cos(around),
        40 * math.cos(down),
        40 * math.sin(down) * math.sin(around),
    )


def sphereWithNormal(
    u: float, v: float
) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    """The same sphere, stating the normal a difference cannot read."""
    position = sphere(u, v)
    return position, tuple(value / 40 for value in position)


class MovedNames(unittest.TestCase):
    def test_every_name_the_module_carried_is_still_there(self):
        for name in (
            "BoxOptions",
            "Mesh",
            "Platonic",
            "box",
            "cylinderPanel",
            "extrude",
            "grid",
            "platonic",
            "quad",
            "revolve",
            "superellipsoid",
            "torus",
        ):
            self.assertTrue(hasattr(native, name), name)
            self.assertTrue(hasattr(mesh, name), name)

    def test_the_moved_classes_report_the_package_they_come_from(self):
        for value in (native.Mesh, native.Edge, native.ExtrudeOptions):
            self.assertEqual(value.__module__, "sigil.geometry.mesh")

    def test_the_generators_answer_what_they_answered(self):
        solid = native.platonic(native.Platonic.Dodecahedron, radius=63)
        self.assertEqual(solid.triangleCount(), 36)
        ring = native.torus(188, 1.2, nu=180, nv=8)
        self.assertEqual(ring.vertexCount(), 180 * 8)
        panel = native.quad(20, 10)
        self.assertEqual(panel.triangleCount(), 2)
        self.assertEqual(
            native.box((-2, -3, -4), (2, 3, 4)).bounds(),
            ((-2, -3, -4), (2, 3, 4)),
        )


class PrimitiveLanes(unittest.TestCase):
    def cube(self) -> native.Mesh:
        return native.box((-10, -10, -10), (10, 10, 10))

    def test_a_lane_is_created_on_touch_at_one_value_per_triangle(self):
        solid = self.cube()
        self.assertEqual(solid.primitiveNames(), [])
        lane = solid.primitive("Color")
        self.assertEqual(len(lane), solid.triangleCount())
        self.assertEqual(set(lane), {(1, 1, 1, 1)})
        self.assertEqual(solid.primitiveNames(), ["Color"])

    def test_a_lane_is_created_at_the_fill_it_was_asked_for(self):
        solid = self.cube()
        self.assertEqual(set(solid.primitive("Id", (2, 0, 0, 0))), {(2, 0, 0, 0)})

    def test_a_lane_that_is_not_there_answers_nothing(self):
        solid = self.cube()
        self.assertIsNone(solid.primitiveIf("Color"))
        solid.primitive("Color")
        self.assertEqual(len(solid.primitiveIf("Color")), solid.triangleCount())

    def test_a_lane_is_copied_on_read_so_a_list_cannot_write_it(self):
        solid = self.cube()
        lane = solid.primitive("Color")
        lane[0] = (0, 0, 0, 1)
        self.assertEqual(solid.primitive("Color")[0], (1, 1, 1, 1))

    def test_a_written_lane_reads_back(self):
        solid = self.cube()
        written = [(index / 16, 0.5, 0, 1) for index in range(solid.triangleCount())]
        solid.setPrimitive("Color", written)
        for wanted, found in zip(written, solid.primitive("Color")):
            self.assertAlmostEqual(found[0], wanted[0], places=6)
            self.assertEqual(found[1:], (0.5, 0, 1))

    def test_writing_a_lane_of_the_wrong_length_names_both_counts(self):
        solid = self.cube()
        with self.assertRaises(ValueError) as refused:
            solid.setPrimitive("Color", [(1, 0, 0, 1)])
        message = str(refused.exception)
        self.assertIn("Color", message)
        self.assertIn(f"{solid.triangleCount()} wanted", message)
        self.assertIn("1 given", message)

    def test_baking_a_lane_unshares_the_vertices(self):
        ring = native.torus(30, 8, nu=12, nv=6)
        ring.setPrimitive("Color", [(0.5, 0.25, 0.125, 1)] * ring.triangleCount())
        baked = native.bakePrimitiveColor(ring)
        self.assertEqual(baked.triangleCount(), ring.triangleCount())
        self.assertEqual(baked.vertexCount(), 3 * ring.triangleCount())
        self.assertEqual(set(baked.colors), {(0.5, 0.25, 0.125, 1)})
        self.assertEqual(baked.primitiveNames(), ["Color"])

    def test_baking_a_lane_that_is_not_there_changes_nothing(self):
        ring = native.torus(30, 8, nu=8, nv=6)
        self.assertEqual(native.bakePrimitiveColor(ring), ring)


class ParametricSheet(unittest.TestCase):
    def test_a_sheet_is_evaluated_on_a_grid_of_stations(self):
        sheet = native.grid(5, 7, plane)
        self.assertEqual(sheet.vertexCount(), 35)
        self.assertEqual(sheet.triangleCount(), 2 * 4 * 6)

    def test_a_position_only_formula_is_differenced(self):
        sheet = native.grid(4, 4, plane)
        for normal in sheet.normals:
            self.assertAlmostEqual(abs(normal[2]), 1, places=5)

    def test_a_stated_normal_survives_a_pole_a_difference_cannot_read(self):
        # Every station of the first row stands on the same point, where
        # the sheet's two parameter directions collapse and a difference
        # has no direction left to normalize.
        stated = native.grid(9, 5, sphereWithNormal, normals=True)
        differenced = native.grid(9, 5, sphere)
        self.assertAlmostEqual(stated.normals[0][1], 1, places=5)
        self.assertLess(abs(differenced.normals[0][1]), 0.99)

    def test_a_grid_too_small_or_too_large_is_refused(self):
        for nu, nv in ((1, 5), (5, 1), (3000, 3000)):
            with self.assertRaises(ValueError):
                native.grid(nu, nv, plane)

    def test_a_formula_is_not_held_after_the_grid_is_built(self):
        released = []

        class Sheet:
            def __call__(self, u: float, v: float):
                return plane(u, v)

            def __del__(self):
                released.append("released")

        sheet = native.grid(4, 4, Sheet())
        gc.collect()
        self.assertEqual(released, ["released"])
        self.assertEqual(sheet.vertexCount(), 16)


class Faces(unittest.TestCase):
    def test_a_dodecahedron_reads_as_twelve_pentagons(self):
        solid = native.platonic(native.Platonic.Dodecahedron)
        self.assertEqual(solid.triangleCount(), 36)
        self.assertEqual(native.faceCount(solid), 12)

    def test_the_shared_corner_form_answers_eulers_count(self):
        cage = native.platonic(
            native.Platonic.Dodecahedron,
            native.PlatonicOptions(sharedVertices=True),
        )
        corners = cage.vertexCount()
        borders = len(native.edges(cage))
        faces = native.faceCount(cage)
        self.assertEqual((corners, borders, faces), (20, 30, 12))
        self.assertEqual(corners - borders + faces, 2)

    def test_an_open_sheet_has_nothing_across_its_rim(self):
        sheet = native.quad(10, 10)
        edges = native.edges(sheet)
        rim = [edge for edge in edges if edge.opposite == native.kNoFace]
        self.assertEqual(len(edges), 5)
        self.assertEqual(len(rim), 4)

    def test_an_edge_is_a_value_with_a_hash_and_a_reading(self):
        cage = native.platonic(
            native.Platonic.Icosahedron,
            native.PlatonicOptions(sharedVertices=True),
        )
        edges = native.edges(cage)
        self.assertEqual(len(set(edges)), len(edges))
        first = edges[0]
        self.assertEqual(
            first,
            native.Edge(
                from_=first.from_,
                to=first.to,
                face=first.face,
                opposite=first.opposite,
            ),
        )
        self.assertLess(first.from_, first.to)
        self.assertIn("from_=", repr(first))

    def test_nothing_stands_across_an_odd_faced_solid(self):
        self.assertIsNone(
            native.opposedFace(native.platonic(native.Platonic.Tetrahedron), 0)
        )
        self.assertIsNotNone(
            native.opposedFace(native.platonic(native.Platonic.Cube), 0)
        )

    def test_a_face_centre_stands_along_its_own_normal(self):
        cube = native.platonic(native.Platonic.Cube)
        centre = native.faceCentroid(cube, 0)
        normal = native.faceNormal(cube, 0)
        distance = math.sqrt(sum(value * value for value in centre))
        self.assertAlmostEqual(distance, CUBE_FACE_DISTANCE, places=5)
        along = sum(a * b for a, b in zip(centre, normal)) / distance
        self.assertAlmostEqual(along, 1, places=5)

    def test_turning_a_face_up_lands_its_normal_on_the_direction(self):
        solid = native.platonic(native.Platonic.Dodecahedron)
        normal = native.faceNormal(solid, 3)
        turned = native.faceUp(solid, 3, (0, 1, 0)) @ (*normal, 0)
        self.assertAlmostEqual(turned[0], 0, places=5)
        self.assertAlmostEqual(turned[1], 1, places=5)
        self.assertAlmostEqual(turned[2], 0, places=5)


class VectorPolicies(unittest.TestCase):
    def test_a_vector_with_no_length_takes_the_fallback(self):
        self.assertEqual(native.normalized((0, 0, 0)), (0, 0, 1))
        self.assertEqual(native.normalized((0, 0, 0), (0, 1, 0)), (0, 1, 0))
        self.assertEqual(native.normalized((0, 5, 0)), (0, 1, 0))

    def test_the_stamp_basis_stands_its_third_axis_along_the_direction(self):
        across, up, along = native.basisFor((0, 0, 4), (0, 1, 0))
        self.assertEqual(across, (1, 0, 0))
        self.assertEqual(up, (0, 1, 0))
        self.assertEqual(along, (0, 0, 1))


class Options(unittest.TestCase):
    def rect(self) -> skia.Path:
        return skia.Path.Rect((0, 0, 40, 40))

    def test_each_generator_takes_its_options_as_a_record_or_as_fields(self):
        path = self.rect()
        self.assertEqual(
            native.extrude(path, native.ExtrudeOptions(depth=10)).bounds(),
            native.extrude(path, depth=10).bounds(),
        )
        profile = [(0, 0), (10, 0), (10, 20)]
        self.assertEqual(
            native.revolve(profile, native.RevolveOptions(segments=12)),
            native.revolve(profile, segments=12),
        )
        self.assertEqual(
            native.platonic(
                native.Platonic.Cube, native.PlatonicOptions(circumradius=3)
            ),
            native.platonic(native.Platonic.Cube, radius=3),
        )

    def test_an_option_record_is_held_edited_and_copied(self):
        options = native.ExtrudeOptions(depth=12, walls=False)
        self.assertEqual(options.depth, 12)
        self.assertFalse(options.walls)
        self.assertTrue(options.frontCap)
        options.tolerance = 0.5
        self.assertEqual(copy.deepcopy(options).tolerance, 0.5)
        with self.assertRaises(TypeError):
            native.ExtrudeOptions(thickness=3)

    def test_dropping_the_caps_leaves_an_open_shell(self):
        path = self.rect()
        walls = native.extrude(
            path, native.ExtrudeOptions(frontCap=False, backCap=False)
        )
        self.assertGreater(walls.triangleCount(), 0)
        self.assertLess(walls.triangleCount(), native.extrude(path).triangleCount())

    def test_a_partial_sweep_and_a_full_one_differ(self):
        profile = [(4, 0), (10, 0), (10, 20)]
        self.assertNotEqual(
            native.revolve(profile, native.RevolveOptions(sweepDeg=180)),
            native.revolve(profile),
        )


class ValueProtocol(unittest.TestCase):
    def test_a_mesh_is_built_from_the_fields_it_carries(self):
        built = native.Mesh(
            positions=[(0, 0, 0), (1, 0, 0), (0, 1, 0)], indices=[0, 1, 2]
        )
        self.assertEqual(built.vertexCount(), 3)
        self.assertEqual(built.triangleCount(), 1)
        with self.assertRaises(TypeError):
            native.Mesh(vertices=[])

    def test_two_meshes_compare_by_content(self):
        self.assertEqual(native.Mesh(), native.Mesh())
        one = native.box((-1, -1, -1), (1, 1, 1))
        other = native.box((-1, -1, -1), (1, 1, 1))
        self.assertEqual(one, other)
        moved = other.positions
        moved[0] = (9, 9, 9)
        other.positions = moved
        self.assertNotEqual(one, other)

    def test_a_lane_counts_towards_equality(self):
        one = native.box((-1, -1, -1), (1, 1, 1))
        other = one.copy()
        other.primitive("Color")
        self.assertNotEqual(one, other)

    def test_a_deep_copy_is_a_value_of_its_own(self):
        solid = native.box((-1, -1, -1), (1, 1, 1))
        solid.setPrimitive("Color", [(1, 0, 0, 1)] * solid.triangleCount())
        duplicate = copy.deepcopy(solid)
        self.assertEqual(duplicate, solid)
        self.assertEqual(duplicate.primitiveNames(), ["Color"])
        duplicate.indices = []
        self.assertNotEqual(duplicate, solid)
        self.assertEqual(len(solid.primitive("Color")), solid.triangleCount())


if __name__ == "__main__":
    unittest.main()
