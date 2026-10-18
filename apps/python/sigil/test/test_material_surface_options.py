"""Every option a surface states reaches Python, under its native name.

A surface is stated as keywords on `Material.surface`, one per field of
the native surface options, and read back through the program it lowers
to; the program's own parameters are a record with a field for every
number the native one carries.
"""

import unittest

from sigil import material
from sigil.material import Material, ReliefOptions, surface
from sigil.material.surface import SurfaceParameters


def glass(**options):
    return Material((0.6, 0.7, 0.8, 1)).surface(
        transmission=1.0, thickness=0.3, **options
    )


class SurfaceOptions(unittest.TestCase):
    def test_a_flat_back_is_stated_and_read_back_from_the_parameters(self):
        self.assertEqual(surface.lower(glass(flatBack=True)).get("flatBack"), 1.0)
        self.assertEqual(surface.lower(glass()).get("flatBack"), 0.0)
        self.assertNotEqual(glass(flatBack=True), glass())

    def test_the_parameters_carry_every_number_the_native_record_does(self):
        parameters = SurfaceParameters(
            flatBack=1, thicknessAttribute=2, blend=3, depthWrite=0, depthFade=0.5
        )
        self.assertEqual(
            (
                parameters.flatBack,
                parameters.thicknessAttribute,
                parameters.blend,
                parameters.depthWrite,
                parameters.depthFade,
            ),
            (1.0, 2.0, 3.0, 0.0, 0.5),
        )
        self.assertEqual(SurfaceParameters().depthWrite, -1.0)
        self.assertEqual(surface.program(parameters).get("flatBack"), 1.0)
        self.assertEqual(surface.program(parameters).get("depthFade"), 0.5)

    def test_a_relief_and_a_brushing_along_the_unit_are_keywords_too(self):
        raised = Material((1, 1, 1, 1)).surface(
            relief=ReliefOptions(shoulder=3, depth=-1)
        )
        self.assertEqual(
            raised, Material((1, 1, 1, 1)).relief(shoulder=3, depth=-1)
        )
        brushed = Material((1, 1, 1, 1)).surface(
            anisotropy=1, anisotropyAlongUnit=True
        )
        self.assertEqual(brushed, Material((1, 1, 1, 1)).brushed(strength=1))
        self.assertNotEqual(
            brushed, Material((1, 1, 1, 1)).surface(anisotropy=1)
        )

    def test_a_field_the_program_lacks_is_refused_by_name(self):
        with self.assertRaisesRegex(KeyError, "noSuchField"):
            surface.lower(glass()).get("noSuchField")

    def test_a_colour_field_reads_back_as_a_colour(self):
        program = surface.program(SurfaceParameters(baseColor=(0.25, 0.5, 0.75, 1)))
        colour = program.get("baseColor")
        self.assertIsInstance(colour, material.Color)
        self.assertAlmostEqual(colour.g, 0.5)


if __name__ == "__main__":
    unittest.main()
