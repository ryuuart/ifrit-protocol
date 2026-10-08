"""A material's relief and brushing, and a bevel's shaded face, from Python.

A relief and a brushing are read from the unit a painter paints the
material into, so a material holds only what was stated: these cases say
that the statement is kept, compared by value, and answered by
`readsUnit`. A bevel's face options are a record whose fields read back
what they were given, with the stock horizon as the picture a chrome
reflects.
"""

import copy
import unittest

from sigil import material

GREY = "#808080"


class Relief(unittest.TestCase):
    def test_a_stated_relief_reads_the_unit_and_keeps_its_options(self):
        cut = material.Material(GREY).relief(shoulder=3, depth=-1)
        self.assertTrue(material.readsUnit(cut))
        self.assertEqual(cut, material.Material(GREY).relief(shoulder=3, depth=-1))
        self.assertNotEqual(cut, material.Material(GREY).relief(shoulder=3, depth=1))
        self.assertNotEqual(cut, material.Material(GREY).relief(shoulder=2, depth=-1))

    def test_the_relief_defaults_are_the_native_ones(self):
        self.assertEqual(
            material.Material(GREY).relief(),
            material.Material(GREY).relief(shoulder=2, depth=1),
        )

    def test_a_material_with_no_relief_or_brushing_reads_no_unit(self):
        self.assertFalse(material.readsUnit(material.Material(GREY)))
        self.assertFalse(
            material.readsUnit(material.Material(GREY).surface(anisotropy=1))
        )

    def test_an_unlit_surface_reads_no_unit_even_with_a_relief(self):
        unlit = material.Material(GREY).surface(unlit=True).relief(shoulder=3)
        self.assertFalse(material.readsUnit(unlit))


class Brushing(unittest.TestCase):
    def test_a_stated_brushing_reads_the_unit_and_keeps_its_options(self):
        brushed = material.Material(GREY).brushed(strength=0.5, rotation=90)
        self.assertTrue(material.readsUnit(brushed))
        self.assertEqual(
            brushed, material.Material(GREY).brushed(strength=0.5, rotation=90)
        )
        self.assertNotEqual(
            brushed, material.Material(GREY).brushed(strength=0.5, rotation=0)
        )
        self.assertNotEqual(
            brushed, material.Material(GREY).brushed(strength=1, rotation=90)
        )

    def test_a_relief_and_a_brushing_stack_on_one_surface(self):
        both = material.Material(GREY).relief(shoulder=4).brushed(strength=0.8)
        self.assertTrue(material.readsUnit(both))
        self.assertNotEqual(both, material.Material(GREY).brushed(strength=0.8))
        self.assertNotEqual(both, material.Material(GREY).relief(shoulder=4))


class BevelFace(unittest.TestCase):
    def test_the_face_options_default_to_the_native_ones(self):
        options = material.BevelOptions()
        self.assertAlmostEqual(options.altitudeDegrees, 30)
        self.assertEqual(options.gloss, [])
        self.assertIsNone(options.reflection)
        self.assertAlmostEqual(options.bend, 0.35)
        self.assertIsNone(options.normal)

    def test_a_chrome_bevel_keeps_its_face_options(self):
        horizon = material.horizonReflection()
        options = material.BevelOptions(
            altitudeDegrees=55,
            gloss=[0.0, 1.0, 0.25, 1.0],
            reflection=horizon,
            bend=0,
            normal=material.Material("#8080ff"),
        )
        self.assertAlmostEqual(options.altitudeDegrees, 55)
        self.assertEqual(options.gloss, [0.0, 1.0, 0.25, 1.0])
        self.assertEqual(options.reflection, horizon)
        self.assertAlmostEqual(options.bend, 0)
        self.assertEqual(options.normal, material.Material("#8080ff"))
        kept = copy.deepcopy(options)
        self.assertEqual(kept.reflection, horizon)
        self.assertEqual(kept.gloss, [0.0, 1.0, 0.25, 1.0])
        chrome = material.from_(GREY).effects(material.Filter.bevel(options))
        self.assertIsInstance(chrome, material.Material)

    def test_the_horizon_stands_where_it_is_asked(self):
        self.assertEqual(material.horizonReflection(), material.horizonReflection(0.5))
        self.assertNotEqual(
            material.horizonReflection(0.3), material.horizonReflection(0.7)
        )


if __name__ == "__main__":
    unittest.main()
