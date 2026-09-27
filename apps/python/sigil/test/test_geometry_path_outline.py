"""The outline as a value, the general shapes that answer one, and the
points and bodies made from it.

An outline measured, cut and combined answers the same numbers Python
reads as C++ does; a stock shape is its general form with the options
fixed; and a compose node takes any of them as its shape.
"""

import math
import unittest

from sigil import compose
from sigil.geometry import arrange, mesh, path, shapes

SQUARE = path.Outline.rectangle(path.Rect.of((0, 0), (100, 100)))


class Outline(unittest.TestCase):
    def test_is_measured_by_distance(self):
        self.assertAlmostEqual(SQUARE.length(), 400.0, places=3)
        x, y = SQUARE.pointAt(150)
        self.assertAlmostEqual(x, 100.0, places=3)
        self.assertAlmostEqual(y, 50.0, places=3)
        nearest = SQUARE.nearest((130, 40))
        self.assertAlmostEqual(nearest.gap, 30.0, places=1)

    def test_combines_and_transforms(self):
        shifted = SQUARE.transformed(path.Transform.translate((50, 0)))
        self.assertAlmostEqual(SQUARE.united(shifted).area(), 15000.0, delta=1.0)
        self.assertAlmostEqual(SQUARE.intersected(shifted).area(), 5000.0, delta=1.0)
        self.assertTrue(SQUARE.contains((50, 50)))
        self.assertEqual(SQUARE, path.Outline.rectangle(path.Rect.of((0, 0), (100, 100))))

    def test_through_offset_and_points(self):
        line = path.through([(0, 0), (100, 0)])
        self.assertAlmostEqual(line.length(), 100.0, places=3)
        tapered = path.band(line, [(0, 20), (1, 0)])
        self.assertGreater(tapered.area(), 0)
        self.assertEqual(len(path.points(SQUARE, path.random(25))), 25)
        seeds = path.points(
            path.Rect.of((0, 0), (100, 100)),
            path.radial(50, path.RadialOptions(stepDegrees=137.508,
                                               growth=path.Growth.SquareRoot)))
        self.assertEqual(len(seeds), 50)
        self.assertAlmostEqual(arrange.heading((0, 1)), 90.0, places=4)


class Arrangements(unittest.TestCase):
    def test_a_ring_places_and_faces_each_item(self):
        ring = arrange.Ring(center=(100, 50), radii=(80, 40))
        top = arrange.onRing(0, 4, ring)
        self.assertAlmostEqual(top[0], 100.0, places=4)
        self.assertAlmostEqual(top[1], 10.0, places=4)
        right = arrange.placeOnRing(1, 4, ring)
        self.assertAlmostEqual(right.position[0], 180.0, places=4)
        self.assertAlmostEqual(right.headingDegrees, 90.0, places=4)

    def test_a_cell_block_swallows_the_gaps_it_crosses(self):
        module = arrange.moduleSize((100, 50), 4, 2, (4, 2))
        rect = arrange.cellRect(
            arrange.Cell(1, 1), module,
            arrange.CellBlock(gap=(4, 2), origin=(10, 10), columnSpan=2))
        self.assertAlmostEqual(rect.min[0], 36.0, places=4)
        self.assertAlmostEqual(rect.width(), 48.0, places=4)


class Shapes(unittest.TestCase):
    def test_a_stock_value_is_its_general_form(self):
        self.assertEqual(shapes.polygon(6, 15),
                         shapes.radial(6, path.RadialOptions(fromDegrees=15)))
        self.assertEqual(shapes.circle(), shapes.ellipse())

    def test_shapes_answer_outlines_and_shape_a_node(self):
        star = shapes.star(5, 0.42)
        self.assertFalse(star.outline((100, 100)).empty())
        placed = shapes.circle().at((300, 40), 20).bounds()
        self.assertAlmostEqual(placed.width(), 40.0, places=3)
        compose.box().width(80).height(80).shape(star)
        compose.box().width(80).height(80).shape(shapes.blob(3))

    def test_an_outline_lifts_into_a_body(self):
        body = mesh.extrude(shapes.circle().outline((100, 100)), 10)
        self.assertGreater(len(body.indices), 0)
        flat = mesh.fill(shapes.annulus(0.5).outline((100, 100)))
        self.assertGreater(len(flat.indices), 0)


if __name__ == "__main__":
    unittest.main()
