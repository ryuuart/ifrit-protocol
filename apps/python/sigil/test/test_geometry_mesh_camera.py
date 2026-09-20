"""Camera matrices, frustum extent and a readable Matrix.

A matrix is counted by columns everywhere it is read: through an index,
through `values`, and through the buffer. Every case below states which
of the four columns it is asking for, so a transposed reading fails
instead of passing quietly.
"""

import copy
import gc
import unittest

from sigil import skia
from sigil.geometry.mesh import camera

ASPECT = 640 / 480


class Matrix(unittest.TestCase):
    def test_indexing_counts_columns_then_rows(self):
        matrix = camera.Matrix.fromValues(list(range(16)))
        self.assertEqual(matrix[2, 3], 11)
        self.assertEqual(matrix[2], (8, 9, 10, 11))
        self.assertEqual(matrix[-1], (12, 13, 14, 15))
        self.assertEqual(matrix.column(0), (0, 1, 2, 3))
        self.assertEqual(matrix.values(), [float(value) for value in range(16)])
        self.assertEqual(len(matrix), 4)

    def test_columns_build_the_same_matrix_as_values(self):
        columns = camera.Matrix.fromColumns(
            (0, 1, 2, 3), (4, 5, 6, 7), (8, 9, 10, 11), (12, 13, 14, 15)
        )
        self.assertEqual(columns, camera.Matrix.fromValues(list(range(16))))

    def test_an_index_outside_the_matrix_is_refused(self):
        matrix = camera.Matrix()
        with self.assertRaises(IndexError):
            _ = matrix[4]
        with self.assertRaises(IndexError):
            _ = matrix[0, 4]
        with self.assertRaises(IndexError):
            matrix.column(-5)

    def test_writing_a_column_and_an_element(self):
        matrix = camera.Matrix()
        matrix.setColumn(0, (2, 0, 0, 0))
        matrix[3] = (7, 8, 9, 1)
        matrix[1, 1] = 3
        self.assertEqual(matrix.column(0), (2, 0, 0, 0))
        self.assertEqual(matrix[3], (7, 8, 9, 1))
        self.assertEqual(matrix[1, 1], 3)

    def test_multiplying_a_matrix_and_a_point(self):
        placed = camera.place((1, 2, 3))
        self.assertEqual(placed @ (0, 0, 0, 1), (1, 2, 3, 1))
        self.assertEqual(placed @ camera.Matrix(), placed)

    def test_the_buffer_is_sixteen_read_only_floats(self):
        view = memoryview(camera.Matrix())
        gc.collect()
        self.assertEqual(view.format, "f")
        self.assertEqual(len(view), 16)
        self.assertTrue(view.readonly)
        self.assertEqual([view[index] for index in (0, 5, 10, 15)], [1, 1, 1, 1])
        self.assertEqual(view[1], 0)

    def test_equality_hashing_and_copies_are_by_value(self):
        matrix = camera.place((1, 2, 3))
        self.assertEqual(matrix, camera.place((1, 2, 3)))
        self.assertNotEqual(matrix, camera.Matrix())
        self.assertEqual(hash(matrix), hash(camera.place((1, 2, 3))))
        duplicate = copy.deepcopy(matrix)
        self.assertEqual(duplicate, matrix)
        duplicate[0, 0] = 9
        self.assertNotEqual(duplicate, matrix)
        self.assertEqual(copy.copy(matrix), matrix)
        self.assertEqual(matrix.copy(), matrix)

    def test_the_reading_rebuilds_the_matrix(self):
        matrix = camera.place((1, 2, 3))
        rebuilt = eval(repr(matrix), {"Matrix": camera.Matrix})
        self.assertEqual(rebuilt, matrix)


class Lens(unittest.TestCase):
    def test_the_view_projection_lands_where_project_lands(self):
        lens = camera.Camera()
        clip = lens.viewProjection((640, 480)) @ (0, 0, 0, 1)
        pixel = lens.project((0, 0, 0), (640, 480))
        self.assertAlmostEqual(clip[0] / clip[3], pixel.x, delta=0.5)
        self.assertAlmostEqual(clip[1] / clip[3], pixel.y, delta=0.5)

    def test_a_viewport_is_a_size_or_two_numbers(self):
        lens = camera.Camera()
        self.assertEqual(
            lens.viewProjection(skia.Size(640, 480)),
            lens.viewProjection((640, 480)),
        )
        self.assertEqual(
            lens.viewProjection([640, 480]), lens.viewProjection((640, 480))
        )
        with self.assertRaises(TypeError):
            lens.viewProjection("640x480")

    def test_the_view_and_the_projection_compose_into_the_viewport(self):
        lens = camera.Camera()
        composed = lens.projection(ASPECT) @ lens.view()
        clip = composed @ (0, 0, 0, 1)
        self.assertAlmostEqual(clip[0] / clip[3], 0, places=5)
        self.assertAlmostEqual(clip[1] / clip[3], 0, places=5)

    def test_a_clip_projection_needs_whole_positive_pixels(self):
        lens = camera.Camera()
        with self.assertRaises(ValueError):
            lens.clipProjection((0, 480))
        with self.assertRaises(ValueError):
            lens.clipProjection((640, -1))
        with self.assertRaises(TypeError):
            lens.clipProjection((640.5, 480))
        clip = lens.clipProjection((640, 480)) @ (0, 0, 0, 1)
        self.assertAlmostEqual(clip[0] / clip[3], 0, places=5)
        self.assertAlmostEqual(clip[1] / clip[3], 0, places=5)

    def test_the_frustum_extent_fills_the_frame_exactly(self):
        lens = camera.Camera()
        extent = lens.extentAt(400, ASPECT)
        self.assertAlmostEqual(extent.width() / extent.height(), ASPECT, places=5)
        corner = lens.project(
            (extent.width() / 2, extent.height() / 2, lens.eye[2] - 400), (640, 480)
        )
        self.assertAlmostEqual(corner.x, 640, delta=0.05)
        self.assertAlmostEqual(corner.y, 0, delta=0.05)

    def test_nothing_behind_the_eye_projects(self):
        lens = camera.Camera()
        self.assertIsNone(lens.project((0, 0, 500), (640, 480)))


class Standpoint(unittest.TestCase):
    def test_an_orbit_and_a_camera_are_the_same_standpoint(self):
        lens = camera.Camera()
        self.assertEqual(camera.orbitOf(lens), camera.Orbit())
        self.assertEqual(camera.cameraAt(lens, camera.orbitOf(lens)), lens)

    def test_reading_a_turned_camera_back_gives_that_camera(self):
        lens = camera.Camera()
        lens.eye = (120, 90, 300)
        lens.target = (0, 10, 0)
        again = camera.cameraAt(lens, camera.orbitOf(lens))
        for index in range(3):
            self.assertAlmostEqual(again.eye[index], lens.eye[index], delta=0.01)
        self.assertEqual(again.target, lens.target)
        self.assertEqual(again.up, lens.up)
        self.assertEqual(again.fovYDeg, lens.fovYDeg)

    def test_a_camera_is_built_from_keywords_and_compares_by_value(self):
        lens = camera.Camera(eye=(0, 10, 300), fovYDeg=30)
        self.assertEqual(lens.eye, (0, 10, 300))
        self.assertEqual(lens.fovYDeg, 30)
        self.assertEqual(lens.target, camera.Camera().target)
        self.assertNotEqual(lens, camera.Camera())
        self.assertEqual(lens, camera.Camera(eye=(0, 10, 300), fovYDeg=30))
        with self.assertRaises(TypeError):
            camera.Camera(elevation=3)

    def test_a_camera_copy_moves_on_its_own(self):
        lens = camera.Camera()
        duplicate = copy.deepcopy(lens)
        duplicate.eye = (1, 2, 3)
        self.assertEqual(lens.eye, camera.Camera().eye)
        self.assertNotEqual(duplicate, lens)
        self.assertEqual(copy.copy(lens), lens)
        self.assertEqual(lens.copy(), lens)

    def test_an_orbit_compares_and_copies_by_value(self):
        orbit = camera.Orbit(yawDeg=30, pitchDeg=10, distance=200)
        self.assertEqual(orbit, camera.Orbit(yawDeg=30, pitchDeg=10, distance=200))
        self.assertNotEqual(orbit, camera.Orbit())
        duplicate = copy.deepcopy(orbit)
        duplicate.distance = 1
        self.assertEqual(orbit.distance, 200)
        self.assertEqual(copy.copy(orbit), orbit)
        self.assertEqual(orbit.copy(), orbit)

    def test_a_billboard_stands_where_it_is_put_and_faces_the_eye(self):
        # The eye a unit along +z, looking at the origin, asks for the
        # basis the quad is already written in, so the transform is the
        # one that changes nothing.
        self.assertEqual(camera.faceCamera((0, 0, 1), (0, 0, 0)), camera.Matrix())
        facing = camera.faceCamera((0, 0, 1), (5, 6, 7))
        self.assertEqual(facing[3], (5, 6, 7, 1))
        self.assertEqual(facing[2], (0, 0, 1, 0))


if __name__ == "__main__":
    unittest.main()
