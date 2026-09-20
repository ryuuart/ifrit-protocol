"""The instanced leaf: a pool, its lanes, its flights and the cell sheet.

A pool and a sheet are Python's own, held shared with the leaf that
stamps them, so most of what there is to say needs no canvas: what a lane
reads and writes, what publishes an edit, what a flight steps, how a
recipe sizes its variants, and which sprite a point picks. The cases that
need pixels draw a composer of their own inside a frame a session lends,
because a sheet bakes on its first stamp and a stamp happens in a draw.

A lane asks its pool again at every use, so one kept across a change of
length reads the pool as it then stands. As a buffer it is a read-only
copy; writing goes through the lane, by index or by slice.
"""

import array
import builtins
import copy
import gc
import inspect
import math
import tempfile
import textwrap
import unittest
import weakref
from pathlib import Path

from _sigil.compose import instancing as native
from sigil import compose, image, motion, skia
from sigil.compose import box, instancing
from sigil.sketch import render_file

LANES = (
    "PointLane",
    "NumberLane",
    "ColorLane",
    "FrameLane",
    "SizeLane",
    "RectLane",
    "FlightLane",
)


def filled(count):
    """A pool of `count` instances standing a step apart along x."""
    pool = instancing.Pool()
    for index in range(count):
        pool.add((float(index), 0.0))
    return pool


def rows(values, width):
    """The numbers of `values` as a float buffer with `width` to a row."""
    flat = array.array("f", values)
    return memoryview(flat).cast("B").cast("f", (len(flat) // width, width))


class Spellings(unittest.TestCase):
    def test_the_package_names_what_the_extension_registers(self):
        for name in ("Pool", "CellSheet", "Mode", "instances", "pick", *LANES):
            with self.subTest(name=name):
                self.assertIs(getattr(instancing, name), getattr(native, name))
        self.assertIs(compose.instancing, instancing)
        self.assertTrue(hasattr(instancing.Pool, "Flight"))

    def test_a_class_reports_the_module_an_author_imports(self):
        self.assertEqual(instancing.Pool.__module__, "sigil.compose.instancing")
        self.assertEqual(instancing.Pool.Flight.__module__, "sigil.compose.instancing")

    def test_the_mode_has_its_two_values(self):
        self.assertEqual(list(instancing.Mode.__members__), ["Data", "Live"])


class Pools(unittest.TestCase):
    def test_add_answers_increasing_indices_and_the_length_follows(self):
        pool = instancing.Pool()
        self.assertEqual(len(pool), 0)
        self.assertEqual([pool.add((index, 0)) for index in range(4)], [0, 1, 2, 3])
        self.assertEqual(len(pool), 4)
        self.assertEqual(pool.size(), 4)

    def test_add_takes_every_lane_by_keyword(self):
        pool = instancing.Pool()
        pool.add(
            position=skia.Point(3, 4),
            frame=2,
            rotateRadians=0.5,
            scale=2.0,
            tint=(0, 1, 0, 0.5),
        )
        where = pool.positions()[0]
        self.assertEqual((where.x, where.y), (3, 4))
        self.assertEqual(pool.frames()[0], 2)
        self.assertEqual(pool.rotations()[0], 0.5)
        self.assertEqual(pool.scales()[0], 2.0)
        self.assertEqual(tuple(pool.tints()[0]), (0, 1, 0, 0.5))

    def test_an_instance_added_with_a_position_alone_is_at_rest(self):
        pool = instancing.Pool()
        pool.add([7, 9])
        self.assertEqual(pool.frames()[0], 0)
        self.assertEqual(pool.rotations()[0], 0)
        self.assertEqual(pool.scales()[0], 1)
        self.assertEqual(tuple(pool.tints()[0]), (1, 1, 1, 1))

    def test_a_tint_is_any_colour_spelling(self):
        pool = instancing.Pool()
        pool.add((0, 0), tint="#ff0000")
        self.assertEqual(tuple(pool.tints()[0]), (1, 0, 0, 1))

    def test_a_position_that_is_no_point_is_a_type_error(self):
        pool = instancing.Pool()
        for junk in ("nowhere", None, (1, 2, 3)):
            with self.subTest(position=junk), self.assertRaises(TypeError):
                pool.add(junk)
        self.assertEqual(len(pool), 0)

    def test_a_change_of_length_publishes_itself_and_a_lane_write_does_not(self):
        pool = instancing.Pool()
        seen = [pool.revision()]
        for change in (
            lambda: pool.add((1, 1)),
            lambda: pool.resize(3),
            lambda: pool.clear(),
        ):
            change()
            self.assertNotIn(pool.revision(), seen)
            seen.append(pool.revision())
        pool.add((1, 1))
        staged = pool.revision()
        pool.positions()[0] = (5, 5)
        pool.scales()[:] = [2]
        self.assertEqual(pool.revision(), staged)
        pool.commit()
        committed = pool.revision()
        self.assertNotEqual(committed, staged)
        pool.commit()
        self.assertNotEqual(pool.revision(), committed)

    def test_resize_appends_instances_at_rest_and_truncates(self):
        pool = filled(1)
        pool.resize(count=3)
        self.assertEqual(len(pool), 3)
        where = pool.positions()[2]
        self.assertEqual((where.x, where.y), (0, 0))
        self.assertEqual(pool.scales()[2], 1)
        self.assertEqual(pool.frames()[2], 0)
        self.assertEqual(tuple(pool.tints()[2]), (1, 1, 1, 1))
        pool.resize(1)
        self.assertEqual(len(pool), 1)

    def test_clear_drops_the_opt_in_lanes_with_their_generation(self):
        pool = filled(2)
        pool.alphas()[0] = 0.25
        self.assertTrue(pool.hasAlphas())
        pool.clear()
        pool.add((0, 0))
        self.assertFalse(pool.hasAlphas())
        self.assertEqual(pool.alphas()[0], 1)


class Lanes(unittest.TestCase):
    def test_a_lane_is_read_and_written_by_index(self):
        pool = filled(3)
        lane = pool.rotations()
        self.assertIsInstance(lane, instancing.NumberLane)
        lane[1] = 0.25
        lane[-1] = 0.5
        self.assertEqual(lane[1], 0.25)
        self.assertEqual(pool.rotations()[2], 0.5)
        self.assertEqual(pool.rotations()[-3], 0)

    def test_an_index_outside_the_lane_is_an_index_error(self):
        lane = filled(2).frames()
        for index in (2, -3):
            with self.subTest(index=index):
                with self.assertRaises(IndexError):
                    lane[index]
                with self.assertRaises(IndexError):
                    lane[index] = 1

    def test_every_lane_takes_its_item_however_it_is_spelled(self):
        pool = filled(1)
        pool.positions()[0] = skia.Point(1, 2)
        pool.positions()[0] = [3, 4]
        pool.tints()[0] = "#00ff00"
        pool.frames()[0] = 5
        pool.sizes()[0] = (2, 0.5)
        pool.sizes()[0] = skia.Size(3, 0.25)
        pool.texWindows()[0] = (0.25, 0, 0.5, 1)
        self.assertEqual(pool.positions()[0].x, 3)
        self.assertEqual(tuple(pool.tints()[0]), (0, 1, 0, 1))
        self.assertEqual(pool.frames()[0], 5)
        self.assertEqual(pool.sizes()[0].width(), 3)
        window = pool.texWindows()[0]
        self.assertEqual((window.left(), window.right()), (0.25, 0.75))

    def test_an_item_that_does_not_read_is_a_type_error(self):
        pool = filled(1)
        for lane, junk in (
            (pool.positions(), "nowhere"),
            (pool.sizes(), None),
            (pool.texWindows(), (1, 2)),
            (pool.rotations(), "fast"),
            (pool.frames(), 1.5),
            (pool.flights(), (0, 0)),
        ):
            with self.subTest(lane=type(lane).__name__), self.assertRaises(TypeError):
                lane[0] = junk

    def test_an_item_read_is_a_copy(self):
        pool = filled(1)
        where = pool.positions()[0]
        where.x = 99
        self.assertEqual(pool.positions()[0].x, 0)

    def test_a_lane_kept_across_a_change_of_length_reads_the_pool_as_it_stands(self):
        pool = filled(2)
        lane = pool.positions()
        pool.resize(5)
        self.assertEqual(len(lane), 5)
        lane[4] = (8, 8)
        self.assertEqual(pool.positions()[4].x, 8)
        pool.clear()
        self.assertEqual(len(lane), 0)
        with self.assertRaises(IndexError):
            lane[0]
        pool.add((6, 6))
        self.assertEqual(lane[0].x, 6)

    def test_a_lane_keeps_its_pool(self):
        lane = filled(3).scales()
        gc.collect()
        self.assertEqual(list(lane), [1, 1, 1])

    def test_a_lane_iterates_over_copies_and_reads_by_slice(self):
        pool = filled(4)
        self.assertEqual([where.x for where in pool.positions()], [0, 1, 2, 3])
        pool.frames()[:] = [4, 5, 6, 7]
        self.assertEqual(pool.frames()[1:3], [5, 6])
        self.assertEqual(pool.frames()[::2], [4, 6])
        self.assertEqual(pool.frames()[::-1], [7, 6, 5, 4])
        self.assertEqual(pool.frames()[5:], [])

    def test_a_slice_is_assigned_from_as_many_items(self):
        pool = filled(4)
        pool.scales()[:] = (value for value in (2, 3, 4, 5))
        pool.scales()[::2] = [8, 9]
        self.assertEqual(list(pool.scales()), [8, 3, 9, 5])
        pool.positions()[1:3] = [(10, 11), skia.Point(12, 13)]
        self.assertEqual([where.y for where in pool.positions()], [0, 11, 13, 0])
        for values in ([1, 2, 3], [1, 2, 3, 4, 5]):
            with self.subTest(count=len(values)), self.assertRaises(ValueError):
                pool.scales()[:] = values
        self.assertEqual(len(pool), 4)

    def test_every_item_is_read_before_any_is_written(self):
        pool = filled(3)
        with self.assertRaises(TypeError):
            pool.scales()[:] = [5, "wide", 7]
        self.assertEqual(list(pool.scales()), [1, 1, 1])

    def test_an_iterable_that_changes_the_pool_is_measured_afterwards(self):
        pool = filled(2)

        def growing():
            yield (1, 1)
            pool.add((0, 0))
            yield (2, 2)

        with self.assertRaises(ValueError):
            pool.positions()[:] = growing()
        self.assertEqual(len(pool), 3)

    def test_a_slice_bound_that_changes_the_pool_meets_the_new_length(self):
        pool = filled(4)

        class Shrinking:
            def __index__(self):
                pool.resize(1)
                return 0

        self.assertEqual(pool.scales()[Shrinking() :], [1])
        pool.resize(4)
        with self.assertRaises(ValueError):
            pool.scales()[Shrinking() :] = [2, 3, 4, 5]
        self.assertEqual(list(pool.scales()), [1])

    def test_a_slice_is_assigned_from_a_buffer_of_numbers(self):
        pool = filled(2)
        pool.positions()[:] = array.array("f", [1, 2, 3, 4])
        self.assertEqual([(w.x, w.y) for w in pool.positions()], [(1, 2), (3, 4)])
        pool.positions()[:] = rows([5, 6, 7, 8], 2)
        self.assertEqual([(w.x, w.y) for w in pool.positions()], [(5, 6), (7, 8)])
        pool.positions()[:] = array.array("d", [9, 10, 11, 12])
        self.assertEqual(pool.positions()[1].y, 12)
        pool.rotations()[:] = memoryview(array.array("f", [0.5, 9, 0.25, 9]))[::2]
        self.assertEqual(list(pool.rotations()), [0.5, 0.25])
        pool.frames()[:] = array.array("i", [3, 4])
        self.assertEqual(list(pool.frames()), [3, 4])
        pool.frames()[:] = array.array("q", [5, 6])
        self.assertEqual(list(pool.frames()), [5, 6])
        pool.tints()[1:] = rows([0, 0, 1, 1], 4)
        self.assertEqual(tuple(pool.tints()[1]), (0, 0, 1, 1))

    def test_a_buffer_of_the_wrong_kind_or_shape_is_refused(self):
        pool = filled(2)
        for lane, values, error in (
            (pool.positions(), array.array("i", [1, 2, 3, 4]), TypeError),
            (pool.positions(), b"\0" * 16, TypeError),
            (pool.frames(), array.array("f", [1, 2]), TypeError),
            (pool.positions(), array.array("f", [1, 2, 3]), ValueError),
            (pool.positions(), rows([1, 2, 3, 4], 4), ValueError),
            (pool.rotations(), rows([1, 2, 3, 4], 2), ValueError),
        ):
            with (
                self.subTest(lane=type(lane).__name__, values=values),
                self.assertRaises(error),
            ):
                lane[:] = values
        self.assertEqual([where.x for where in pool.positions()], [0, 1])

    def test_a_lane_is_a_read_only_buffer_with_a_row_to_an_instance(self):
        pool = filled(3)
        pool.frames()[:] = [1, 2, 3]
        for lane, code, shape in (
            (pool.positions(), "f", (3, 2)),
            (pool.rotations(), "f", (3,)),
            (pool.tints(), "f", (3, 4)),
            (pool.frames(), "i", (3,)),
            (pool.sizes(), "f", (3, 2)),
            (pool.texWindows(), "f", (3, 4)),
            (pool.flights(), "f", (3, 12)),
        ):
            with self.subTest(lane=type(lane).__name__):
                view = memoryview(lane)
                self.assertTrue(view.readonly)
                self.assertEqual(view.format, code)
                self.assertEqual(view.itemsize, 4)
                self.assertEqual(view.shape, shape)
                self.assertTrue(view.c_contiguous)
                view.release()
        self.assertEqual(memoryview(pool.positions()).tolist()[2], [2, 0])
        self.assertEqual(memoryview(pool.frames()).tolist(), [1, 2, 3])
        self.assertEqual(memoryview(pool.tints()).tolist()[0], [1, 1, 1, 1])

    def test_a_rectangle_row_is_written_the_way_python_passes_one(self):
        pool = filled(1)
        pool.texWindows()[0] = (0.25, 0, 0.5, 1)
        self.assertEqual(memoryview(pool.texWindows()).tolist(), [[0.25, 0, 0.5, 1]])
        pool.texWindows()[:] = rows([0.5, 0.25, 0.25, 0.5], 4)
        window = pool.texWindows()[0]
        self.assertEqual(
            (window.left(), window.top(), window.right(), window.bottom()),
            (0.5, 0.25, 0.75, 0.75),
        )

    def test_a_lane_assigned_its_own_buffer_is_unchanged(self):
        pool = filled(2)
        pool.texWindows()[1] = (0.25, 0.5, 0.5, 0.25)
        pool.flights()[1] = instancing.Pool.Flight(to=(4, 5), start=0.5, duration=2)
        for lane in (pool.positions(), pool.texWindows(), pool.flights()):
            with self.subTest(lane=type(lane).__name__):
                before = memoryview(lane).tolist()
                lane[:] = lane
                self.assertEqual(memoryview(lane).tolist(), before)

    def test_a_buffer_is_the_lane_as_it_stood_and_outlives_the_pool(self):
        pool = filled(2)
        view = memoryview(pool.positions())
        pool.positions()[0] = (9, 9)
        pool.clear()
        del pool
        gc.collect()
        self.assertEqual(view.tolist(), [[0, 0], [1, 0]])

    def test_an_empty_lane_is_an_empty_buffer(self):
        view = memoryview(instancing.Pool().positions())
        self.assertEqual(view.nbytes, 0)
        self.assertEqual(view.shape, (0, 2))

    def test_a_writable_buffer_is_refused(self):
        lane = filled(1).positions()
        with self.assertRaises(BufferError):
            lane.__buffer__(inspect.BufferFlags.WRITABLE)
        view = lane.__buffer__(inspect.BufferFlags.FULL_RO)
        with self.assertRaises(TypeError):
            view[0, 0] = 1.0

    def test_an_opt_in_lane_exists_from_the_call_that_names_it(self):
        pool = filled(2)
        for has, lane in (
            (pool.hasSizes, pool.sizes),
            (pool.hasAlphas, pool.alphas),
            (pool.hasTexWindows, pool.texWindows),
            (pool.hasFlights, pool.flights),
        ):
            with self.subTest(lane=lane.__name__):
                self.assertFalse(has())
                lane()
                self.assertTrue(has())

    def test_an_opt_in_lane_starts_neutral_and_follows_the_pool(self):
        pool = filled(1)
        size, alpha, window = pool.sizes()[0], pool.alphas()[0], pool.texWindows()[0]
        self.assertEqual((size.width(), size.height()), (1, 1))
        self.assertEqual(alpha, 1)
        self.assertEqual(
            (window.left(), window.top(), window.width(), window.height()),
            (0, 0, 1, 1),
        )
        pool.alphas()[0] = 0.25
        pool.add((1, 1))
        self.assertTrue(pool.hasAlphas())
        self.assertEqual(list(pool.alphas()), [0.25, 1])


class Flights(unittest.TestCase):
    def test_a_flight_built_by_keywords_equals_one_built_field_by_field(self):
        Flight = instancing.Pool.Flight
        built = Flight(
            from_=(1, 2),
            to=skia.Point(3, 4),
            rotateFrom=0.5,
            rotateTo=1.5,
            scaleFrom=2,
            scaleTo=3,
            alphaFrom=0.25,
            alphaTo=0.75,
            start=0.5,
            duration=2,
        )
        assigned = Flight()
        assigned.from_ = (1, 2)
        assigned.to = (3, 4)
        assigned.rotateFrom, assigned.rotateTo = 0.5, 1.5
        assigned.scaleFrom, assigned.scaleTo = 2, 3
        assigned.alphaFrom, assigned.alphaTo = 0.25, 0.75
        assigned.start, assigned.duration = 0.5, 2
        self.assertEqual(built, assigned)
        assigned.duration = 3
        self.assertNotEqual(built, assigned)
        self.assertNotEqual(built, (1, 2))

    def test_the_native_field_name_reaches_the_same_point(self):
        Flight = instancing.Pool.Flight
        flight = Flight(**{"from": (6, 7)})
        self.assertEqual((flight.from_.x, flight.from_.y), (6, 7))
        self.assertEqual(getattr(flight, "from").x, 6)

    def test_a_flight_nobody_wrote_goes_nowhere_at_full_scale_and_opacity(self):
        flight = instancing.Pool.Flight()
        self.assertEqual((flight.from_.x, flight.to.y), (0, 0))
        self.assertEqual((flight.scaleFrom, flight.scaleTo), (1, 1))
        self.assertEqual((flight.alphaFrom, flight.alphaTo), (1, 1))
        self.assertEqual((flight.start, flight.duration), (0, 0))

    def test_an_unknown_field_is_named(self):
        with self.assertRaisesRegex(TypeError, "Unknown Flight field: detour"):
            instancing.Pool.Flight(detour=1)

    def test_a_flight_copies_and_does_not_hash(self):
        flight = instancing.Pool.Flight(to=(3, 4), duration=2)
        for duplicate in (flight.copy(), copy.copy(flight), copy.deepcopy(flight)):
            self.assertEqual(duplicate, flight)
            self.assertIsNot(duplicate, flight)
            duplicate.duration = 5
            self.assertEqual(flight.duration, 2)
        with self.assertRaises(TypeError):
            hash(flight)

    def test_a_point_read_from_a_flight_is_a_copy(self):
        flight = instancing.Pool.Flight(to=(3, 4))
        flight.to.x = 99
        self.assertEqual(flight.to.x, 3)

    def test_a_flight_point_that_is_no_point_is_a_type_error(self):
        flight = instancing.Pool.Flight(to=(3, 4))
        with self.assertRaises(TypeError):
            flight.to = "nowhere"
        with self.assertRaises(TypeError):
            flight.from_ = (1, 2, 3)
        with self.assertRaises(TypeError):
            instancing.Pool.Flight(to=None)
        self.assertEqual((flight.to.x, flight.from_.x), (3, 0))

    def test_a_flight_materialises_at_rest_where_its_instance_stands(self):
        pool = instancing.Pool()
        pool.add((3, 4), rotateRadians=0.5, scale=2)
        self.assertFalse(pool.hasFlights())
        flight = pool.flights()[0]
        self.assertTrue(pool.hasFlights())
        self.assertEqual((flight.from_.x, flight.from_.y), (3, 4))
        self.assertEqual((flight.to.x, flight.to.y), (3, 4))
        self.assertEqual((flight.rotateFrom, flight.rotateTo), (0.5, 0.5))
        self.assertEqual((flight.scaleFrom, flight.scaleTo), (2, 2))

    def flying(self):
        pool = filled(2)
        Flight = instancing.Pool.Flight
        pool.flights()[:] = [
            Flight(to=(10, 20), scaleTo=3, alphaFrom=0, duration=2),
            Flight(from_=(1, 0), to=(5, 0), start=1, duration=0),
        ]
        return pool

    def test_a_step_writes_the_lanes_the_stamp_reads_and_publishes(self):
        pool = self.flying()
        before = pool.revision()
        pool.fly(1.0)
        self.assertNotEqual(pool.revision(), before)
        where = pool.positions()[0]
        self.assertEqual((where.x, where.y), (5, 10))
        self.assertEqual(pool.scales()[0], 2)
        self.assertTrue(pool.hasAlphas())
        self.assertEqual(pool.alphas()[0], 0.5)
        # A zero duration arrives at its start and holds.
        self.assertEqual(pool.positions()[1].x, 5)
        pool.fly(seconds=0.5, ease=None)
        self.assertEqual(pool.positions()[0].x, 2.5)
        self.assertEqual(pool.positions()[1].x, 1)
        pool.fly(9)
        self.assertEqual(pool.positions()[0].x, 10)

    def test_a_python_ease_shapes_each_instance_once(self):
        pool = self.flying()
        asked = []

        def squared(unit):
            asked.append(unit)
            return unit * unit

        pool.fly(1.0, squared)
        self.assertEqual(asked, [0.5, 1])
        self.assertEqual(pool.positions()[0].x, 2.5)
        self.assertEqual(pool.alphas()[0], 0.25)

    def test_a_native_curve_shapes_the_step_the_same_way(self):
        pool = self.flying()
        pool.fly(1.0, ease=motion.ease.inQuad)
        self.assertEqual(pool.positions()[0].x, 2.5)

    def test_what_is_no_ease_is_refused(self):
        pool = self.flying()
        with self.assertRaises(TypeError):
            pool.fly(1.0, ease=3)

    def test_what_an_ease_raises_is_what_the_caller_sees(self):
        pool = self.flying()
        with self.assertRaises(ZeroDivisionError):
            pool.fly(1.0, lambda unit: unit / 0)
        with self.assertRaises(TypeError):
            pool.fly(1.0, lambda unit: "halfway")

    def test_an_ease_that_changes_the_pool_is_reported_not_followed(self):
        pool = self.flying()

        def growing(unit):
            pool.add((0, 0))
            return unit

        with self.assertRaisesRegex(RuntimeError, "changed the pool"):
            pool.fly(1.0, growing)
        self.assertEqual(len(pool), 4)

    def test_an_ease_is_let_go_when_the_step_returns(self):
        pool = self.flying()

        def ease(unit):
            return unit

        gone = weakref.ref(ease)
        pool.fly(1.0, ease)
        del ease
        gc.collect()
        self.assertIsNone(gone())

    def test_a_pool_without_flights_is_left_alone(self):
        pool = filled(2)
        before = pool.revision()
        asked = []
        pool.fly(1.0, asked.append)
        self.assertEqual(asked, [])
        self.assertEqual(pool.revision(), before)
        self.assertFalse(pool.hasFlights())

    def test_a_field_of_flights_is_seeded_from_one_buffer(self):
        pool = filled(2)
        first = [0, 0, 10, 20, 0, 0, 1, 1, 1, 1, 0, 2]
        second = [1, 0, 5, 0, 0, 0.5, 1, 2, 0, 1, 0.25, 4]
        pool.flights()[:] = rows(first + second, 12)
        flight = pool.flights()[1]
        self.assertEqual((flight.from_.x, flight.to.x), (1, 5))
        self.assertEqual((flight.rotateTo, flight.scaleTo), (0.5, 2))
        self.assertEqual(
            (flight.alphaFrom, flight.start, flight.duration), (0, 0.25, 4)
        )
        self.assertEqual(memoryview(pool.flights()).tolist(), [first, second])


class Sheets(unittest.TestCase):
    def test_a_sheet_is_built_with_or_without_its_oversample(self):
        self.assertEqual(instancing.CellSheet().oversample(), 2)
        self.assertEqual(instancing.CellSheet(3).oversample(), 3)
        self.assertEqual(instancing.CellSheet(oversample=1.5).oversample(), 1.5)
        self.assertEqual(instancing.CellSheet(0.25).oversample(), 0.5)

    def test_a_cell_answers_increasing_frames_and_the_count_follows(self):
        sheet = instancing.CellSheet()
        self.assertEqual(sheet.frameCount(), 0)
        self.assertEqual(sheet.cell(box().fill("#ffffff"), (8, 8)), 0)
        self.assertEqual(sheet.cell(tree=box(), logicalSize=skia.Size(20, 10)), 1)
        self.assertEqual(sheet.frameCount(), 2)
        size = sheet.frameSize(frame=1)
        self.assertEqual((size.width(), size.height()), (20, 10))
        for missing in (-1, 2):
            with self.subTest(frame=missing):
                size = sheet.frameSize(missing)
                self.assertEqual((size.width(), size.height()), (0, 0))

    def test_a_registration_makes_the_sheet_a_different_picture(self):
        sheet = instancing.CellSheet()
        before = sheet.revision()
        sheet.cell(box(), (8, 8))
        self.assertNotEqual(sheet.revision(), before)

    def test_a_logical_size_that_is_no_size_is_a_type_error(self):
        sheet = instancing.CellSheet()
        for junk in (None, "wide", (1, 2, 3)):
            with self.subTest(size=junk), self.assertRaises(TypeError):
                sheet.cell(box(), junk)
        self.assertEqual(sheet.frameCount(), 0)

    def test_the_filter_is_set_fluently_and_counted_only_when_it_changes(self):
        sheet = instancing.CellSheet()
        self.assertEqual(sheet.filter(), skia.FilterMode.Linear)
        before = sheet.revision()
        self.assertIs(sheet.filter(skia.FilterMode.Nearest), sheet)
        self.assertEqual(sheet.filter(), skia.FilterMode.Nearest)
        changed = sheet.revision()
        self.assertNotEqual(changed, before)
        sheet.filter(mode=skia.FilterMode.Nearest)
        self.assertEqual(sheet.revision(), changed)

    def test_nothing_is_baked_until_a_stamp(self):
        sheet = instancing.CellSheet()
        sheet.cell(box(), (8, 8))
        self.assertIsNone(sheet.image())
        self.assertEqual(sheet.frameTex(0).width(), 0)

    def test_a_recipe_that_answers_a_tree_is_baked_at_the_shared_size(self):
        sheet = instancing.CellSheet()
        sheet.cell(box(), (4, 4))
        seen = []

        def shade(index):
            seen.append(index)
            return box().fill((0.25 * index, 0, 0, 1))

        first = sheet.variants(3, (20, 10), shade)
        self.assertEqual(first, 1)
        self.assertEqual(seen, [0, 1, 2])
        self.assertEqual(sheet.frameCount(), 4)
        size = sheet.frameSize(first + 2)
        self.assertEqual((size.width(), size.height()), (20, 10))

    def test_a_recipe_that_reads_no_index_names_nothing(self):
        sheet = instancing.CellSheet()
        self.assertEqual(
            sheet.variants(count=3, logicalSize=(8, 8), make=lambda: box()), 0
        )
        self.assertEqual(sheet.frameCount(), 3)

    def test_a_recipe_that_answers_a_size_brings_its_own(self):
        sheet = instancing.CellSheet()

        def grown(index):
            side = 10 + 10 * index
            return box(), (side, side)

        self.assertEqual(sheet.variants(2, grown), 0)
        self.assertEqual(
            sheet.variants(count=1, make=lambda: [box(), skia.Size(5, 6)]), 2
        )
        # Beside a shared size, a variant that answers its own still keeps it.
        sheet.variants(1, (99, 99), grown)
        sides = [sheet.frameSize(frame).width() for frame in range(4)]
        self.assertEqual(sides, [10, 20, 5, 10])

    def test_a_recipe_with_no_shared_size_has_to_answer_one(self):
        sheet = instancing.CellSheet()
        with self.assertRaises(TypeError):
            sheet.variants(2, lambda index: box())

    def test_what_a_recipe_answers_is_checked(self):
        sheet = instancing.CellSheet()
        for answer in (None, "tree", (box(),), (box(), "wide"), ((8, 8), box())):
            with self.subTest(answer=answer), self.assertRaises(TypeError):
                sheet.variants(1, (8, 8), lambda index, answer=answer: answer)
        with self.assertRaises(TypeError):
            sheet.variants(1, (8, 8), "recipe")

    def test_what_a_recipe_raises_is_what_the_caller_sees(self):
        sheet = instancing.CellSheet()

        def failing(index):
            if index == 1:
                raise KeyError("palette")
            return box()

        with self.assertRaises(KeyError):
            sheet.variants(3, (8, 8), failing)
        self.assertEqual(sheet.frameCount(), 1)

    def test_a_recipe_is_let_go_when_the_call_returns(self):
        sheet = instancing.CellSheet()

        def recipe(index):
            return box()

        gone = weakref.ref(recipe)
        sheet.variants(2, (8, 8), recipe)
        del recipe
        gc.collect()
        self.assertIsNone(gone())
        self.assertEqual(sheet.frameCount(), 2)

    def test_a_recipe_that_names_more_than_the_index_is_refused(self):
        sheet = instancing.CellSheet()
        with self.assertRaises(TypeError):
            sheet.variants(1, (8, 8), lambda index, palette: box())
        self.assertEqual(sheet.frameCount(), 0)

    def test_no_variants_answers_no_frame(self):
        sheet = instancing.CellSheet()
        self.assertEqual(sheet.variants(0, (8, 8), lambda: box()), -1)
        self.assertEqual(sheet.frameCount(), 0)


class Picks(unittest.TestCase):
    def test_a_pick_inverts_the_stamp_topmost_first(self):
        sheet = instancing.CellSheet(1)
        sheet.cell(box().fill("#ff0000"), (40, 20))
        pool = instancing.Pool()
        pool.add((100, 100))
        pool.add((120, 100))
        pool.scales()[1] = 0.5
        pool.rotations()[0] = math.pi / 2
        pool.commit()
        # Inside both quads, the later instance is on top.
        self.assertEqual(instancing.pick(pool, sheet, (118, 100)), 1)
        # Inside the first quad only once it is turned a quarter.
        self.assertEqual(instancing.pick(pool, sheet, skia.Point(100, 117)), 0)
        self.assertIsNone(instancing.pick(pool, sheet, (84, 100)))
        self.assertIsNone(instancing.pick(pool, sheet, (135, 100)))
        self.assertEqual(instancing.pick(pool=pool, atlas=sheet, point=[100, 100]), 0)

    def test_a_pick_reads_the_size_and_window_lanes(self):
        sheet = instancing.CellSheet(1)
        sheet.cell(box(), (20, 20))
        pool = instancing.Pool()
        pool.add((50, 50))
        self.assertIsNone(instancing.pick(pool, sheet, (75, 50)))
        pool.sizes()[0] = (3, 1)
        self.assertEqual(instancing.pick(pool, sheet, (75, 50)), 0)
        pool.texWindows()[0] = (0, 0, 0.5, 1)
        self.assertIsNone(instancing.pick(pool, sheet, (75, 50)))

    def test_a_sheet_with_no_cells_picks_nothing(self):
        self.assertIsNone(instancing.pick(filled(1), instancing.CellSheet(), (0, 0)))

    def test_a_pick_needs_a_pool_a_sheet_and_a_point(self):
        sheet, pool = instancing.CellSheet(), filled(1)
        for arguments in (
            (pool, sheet, "nowhere"),
            (pool, sheet, None),
            (None, sheet, (0, 0)),
            (pool, None, (0, 0)),
            (sheet, pool, (0, 0)),
        ):
            with self.subTest(arguments=arguments), self.assertRaises(TypeError):
                instancing.pick(*arguments)


class Leaves(unittest.TestCase):
    def test_the_leaf_is_an_element_with_or_without_its_mode_and_blend(self):
        sheet, pool = instancing.CellSheet(), filled(1)
        sheet.cell(box(), (8, 8))
        for leaf in (
            instancing.instances(sheet, pool),
            instancing.instances(sheet, pool, instancing.Mode.Live),
            instancing.instances(
                atlas=sheet,
                pool=pool,
                mode=instancing.Mode.Data,
                blend=skia.BlendMode.Plus,
            ),
        ):
            self.assertIsInstance(leaf, compose.Element)

    def test_the_leaf_needs_a_sheet_and_a_pool(self):
        sheet, pool = instancing.CellSheet(), instancing.Pool()
        for atlas, field in ((None, pool), (sheet, None), (pool, sheet)):
            with self.subTest(atlas=atlas, pool=field), self.assertRaises(TypeError):
                instancing.instances(atlas, field)


class Session(unittest.TestCase):
    """A scene rendered through the native host, which lends the canvas."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        builtins._sigil_instancing_results = self.results
        self.addCleanup(delattr, builtins, "_sigil_instancing_results")

    def render(self, drawing):
        source = Path(self.directory.name) / "scene.py"
        output = Path(self.directory.name) / "scene.png"
        source.write_text(
            "import builtins\n"
            "import gc\n"
            "from sigil import skia\n"
            "from sigil.compose import Composer, box, instancing\n"
            "from sigil.sketch import sketch\n"
            "results = builtins._sigil_instancing_results\n"
            "def sheet_of(color, side=8):\n"
            "    sheet = instancing.CellSheet()\n"
            "    sheet.cell(box().fill(color), (side, side))\n"
            "    return sheet\n"
            "def field(leaf):\n"
            "    return box().width(64).height(48).children([leaf])\n"
            "def composer():\n"
            "    probe = Composer()\n"
            "    probe.setSize((64, 48))\n"
            "    return probe\n"
            "@sketch(size=(64, 48), capture_at=0)\n"
            "class Scene:\n"
            "    def setup(self, ctx):\n"
            "        pass\n"
            "    def update(self, elapsed, ctx):\n"
            "        pass\n"
            "    def draw(self, pen):\n"
            + textwrap.indent(textwrap.dedent(drawing).strip(), "        ")
            + "\n"
        )
        render_file(source, output, at=0)
        return image.decode(output.read_bytes())

    def pixel(self, picture, x, y):
        offset = 4 * (y * picture.width() + x)
        return tuple(picture.rgba()[offset : offset + 4])


class Frames(Session):
    """What needs a canvas, asked inside a frame that lends one."""

    WHITE = (255, 255, 255, 255)
    BLACK = (0, 0, 0, 255)

    def test_cells_are_stamped_at_the_pool_positions_with_their_tints(self):
        picture = self.render("""
            pen.background('#000000')
            sheet = sheet_of('#ffffff')
            pool = instancing.Pool()
            pool.add((16, 16))
            pool.add((40, 16), tint='#ff0000')
            results['before'] = sheet.image()
            probe = composer()
            probe.render(field(instancing.instances(sheet, pool)))
            probe.draw(pen.canvas())
            results['sheet'] = sheet
            results['pool'] = pool
        """)
        self.assertIsNone(self.results["before"])
        self.assertEqual(self.pixel(picture, 16, 16), self.WHITE)
        self.assertEqual(self.pixel(picture, 40, 16), (255, 0, 0, 255))
        self.assertEqual(self.pixel(picture, 28, 40), self.BLACK)
        # The sheet and the pool are Python's own, so they answer after the
        # session that drew them has closed, and the first stamp baked the
        # sheet at its oversample.
        sheet, pool = self.results["sheet"], self.results["pool"]
        self.assertEqual(len(pool), 2)
        self.assertIsInstance(sheet.image(), skia.Image)
        baked = sheet.frameTex(0)
        self.assertEqual((baked.width(), baked.height()), (16, 16))
        self.assertGreaterEqual(sheet.image().width(), 16)
        self.assertIsInstance(instancing.instances(sheet, pool), compose.Element)

    def data_mode(self, publish):
        return self.render(f"""
            sheet = sheet_of('#ffffff')
            pool = instancing.Pool()
            pool.add((16, 16))
            probe = composer()
            probe.render(field(instancing.instances(sheet, pool)))
            probe.draw(pen.canvas())
            pool.positions()[0] = (48, 32)
            {publish}
            probe.render(field(instancing.instances(sheet, pool)))
            results['dirty'] = probe.dirty()
            pen.background('#000000')
            probe.draw(pen.canvas())
        """)

    def test_a_lane_write_without_a_commit_replays_the_old_picture(self):
        picture = self.data_mode("pass")
        self.assertFalse(self.results["dirty"])
        self.assertEqual(self.pixel(picture, 16, 16), self.WHITE)
        self.assertEqual(self.pixel(picture, 48, 32), self.BLACK)

    def test_a_commit_repaints_the_leaf(self):
        picture = self.data_mode("pool.commit()")
        self.assertTrue(self.results["dirty"])
        self.assertEqual(self.pixel(picture, 48, 32), self.WHITE)
        self.assertEqual(self.pixel(picture, 16, 16), self.BLACK)

    def test_a_live_leaf_reads_the_pool_at_every_draw(self):
        picture = self.render("""
            sheet = sheet_of('#ffffff')
            pool = instancing.Pool()
            pool.add((16, 16))
            probe = composer()
            leaf = instancing.instances(sheet, pool, mode=instancing.Mode.Live)
            probe.render(field(leaf))
            probe.draw(pen.canvas())
            pool.positions()[0] = (48, 32)
            pen.background('#000000')
            probe.draw(pen.canvas())
        """)
        self.assertEqual(self.pixel(picture, 48, 32), self.WHITE)
        self.assertEqual(self.pixel(picture, 16, 16), self.BLACK)

    def test_a_flight_step_moves_what_is_drawn(self):
        picture = self.render("""
            pen.background('#000000')
            sheet = sheet_of('#ffffff')
            pool = instancing.Pool()
            pool.add((16, 16))
            pool.flights()[0] = instancing.Pool.Flight(
                from_=(16, 16), to=(48, 32), duration=2)
            pool.fly(2.0, lambda unit: unit)
            probe = composer()
            probe.render(field(instancing.instances(sheet, pool)))
            probe.draw(pen.canvas())
        """)
        self.assertEqual(self.pixel(picture, 48, 32), self.WHITE)
        self.assertEqual(self.pixel(picture, 16, 16), self.BLACK)

    def overlap(self, blend):
        picture = self.render(f"""
            pen.background('#000000')
            sheet = sheet_of('#666666', 16)
            pool = instancing.Pool()
            pool.add((24, 24))
            pool.add((32, 24))
            probe = composer()
            probe.render(field(instancing.instances(
                sheet, pool, blend=skia.BlendMode.{blend})))
            probe.draw(pen.canvas())
        """)
        return self.pixel(picture, 28, 24)[0]

    def test_the_blend_is_per_sprite_so_overlapping_sprites_accumulate(self):
        self.assertGreater(self.overlap("Plus"), self.overlap("SrcOver") + 50)

    def test_the_leaf_keeps_the_sheet_and_the_pool_it_names(self):
        picture = self.render("""
            pen.background('#000000')
            def built():
                pool = instancing.Pool()
                pool.add((16, 16))
                return instancing.instances(sheet_of('#ffffff'), pool)
            leaf = built()
            gc.collect()
            probe = composer()
            probe.render(field(leaf))
            probe.draw(pen.canvas())
        """)
        self.assertEqual(self.pixel(picture, 16, 16), self.WHITE)

    def test_a_variant_is_selected_by_its_frame(self):
        picture = self.render("""
            pen.background('#000000')
            sheet = instancing.CellSheet()
            shades = ('#ff0000', '#00ff00', '#0000ff')
            first = sheet.variants(
                3, (8, 8), lambda index: box().fill(shades[index]))
            pool = instancing.Pool()
            pool.add((16, 16), frame=first + 2)
            pool.add((40, 16), frame=first + 1)
            probe = composer()
            probe.render(field(instancing.instances(sheet, pool)))
            probe.draw(pen.canvas())
        """)
        self.assertEqual(self.pixel(picture, 16, 16), (0, 0, 255, 255))
        self.assertEqual(self.pixel(picture, 40, 16), (0, 255, 0, 255))


if __name__ == "__main__":
    unittest.main()
