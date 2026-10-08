"""An open spine ridden by a point chain, and a solid turned to face in.

A chain entered on a spline reads it by its own closure, so a window at
the end of an open trace stays on the trace instead of crossing the chord
back to its start. A lathe or a loft asked for `inward` faces into the
solid it encloses, the inside of a bowl or a tunnel seen from within.
"""

import math
import unittest

from sigil.geometry import mesh
from sigil.geometry.mesh import curve, pop


class OpenSpine(unittest.TestCase):
    def test_the_last_window_of_an_open_spine_lands_on_no_chord(self):
        # Three sides of a square, left open: the chord back to the start
        # would be the fourth side, along x = 0.
        spine = curve.Spline3(
            [(0, 0, 0), (100, 0, 0), (100, 100, 0), (0, 100, 0)],
            curve.Spline3.Type.Linear,
        )
        self.assertFalse(spine.closed)
        stamp = mesh.quad(0.01, 0.01)
        stamps = pop.on(spine).count(200).window(1.0, 0.1).stamps(stamp)
        self.assertGreater(len(stamps.positions), 0)
        for position in stamps.positions:
            self.assertAlmostEqual(position[1], 100, delta=0.05)
            self.assertGreater(position[0], -0.05)


class InwardSolids(unittest.TestCase):
    def facing(self, solid, centre):
        """How many normals point away from @p centre, and how many toward."""
        away = toward = 0
        for position, normal in zip(solid.positions, solid.normals):
            outward = [position[axis] - centre[axis] for axis in range(3)]
            reach = sum(outward[axis] * normal[axis] for axis in range(3))
            if reach > 1e-4:
                away += 1
            elif reach < -1e-4:
                toward += 1
        return away, toward

    def test_an_inward_lathe_faces_its_axis(self):
        # A cylinder wall of radius 5 from height 0 to 10.
        profile = [(5, 0), (5, 10)]
        outward = mesh.revolve(profile, segments=24)
        inward = mesh.revolve(profile, segments=24, inward=True)
        self.assertEqual(
            inward, mesh.revolve(profile, mesh.RevolveOptions(segments=24, inward=True))
        )
        away, toward = self.facing(outward, (0, 5, 0))
        self.assertEqual(toward, 0)
        self.assertGreater(away, 0)
        # Each normal of the wall is measured from the axis at its own height.
        for position, normal in zip(inward.positions, inward.normals):
            radial = math.hypot(position[0], position[2])
            reach = (position[0] * normal[0] + position[2] * normal[2]) / radial
            self.assertLess(reach, -0.99)

    def test_an_inward_loft_faces_into_its_solid(self):
        def square(z):
            return [(-10, -10, z), (10, -10, z), (10, 10, z), (-10, 10, z)]

        sections = [square(0), square(10)]
        away, toward = self.facing(mesh.loft(sections), (0, 0, 5))
        self.assertEqual(toward, 0)
        away, toward = self.facing(mesh.loft(sections, inward=True), (0, 0, 5))
        self.assertEqual(away, 0)
        self.assertGreater(toward, 0)


if __name__ == "__main__":
    unittest.main()
