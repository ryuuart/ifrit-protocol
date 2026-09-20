"""The type graph: what makes a value, what takes one, what returns one."""

import unittest

from sigil.reference import catalogue as catalogue_module
from sigil.reference.graph import Graph
from sigil.reference.test.support import Tree


class ASignatureIsReadForEveryTypeItMentions(unittest.TestCase):
    def test(self) -> None:
        self.assertEqual(
            catalogue_module.mentions("motion::Animatable< Fill >"),
            [("motion::Animatable", ""), ("Fill", "motion::Animatable")],
        )
        # The standard library and Skia have references of their own.
        self.assertEqual(
            catalogue_module.mentions("std::optional< Fill >"),
            [("Fill", "std::optional")],
        )
        self.assertEqual(catalogue_module.mentions("const Ink &"), [("Ink", "")])


class AValueKnowsWhatMakesItAndWhatTakesIt(Tree):
    def setUp(self) -> None:
        super().setUp()
        from sigil.reference import doxygen_xml

        doxygen_xml.NODE_FLOOR, floor = 2, doxygen_xml.NODE_FLOOR
        try:
            self.held = self.catalogue()
        finally:
            doxygen_xml.NODE_FLOOR = floor
        self.graph = Graph(self.held)

    def labels(self, sites) -> list:
        return [site.label for site in sites]

    def test_make_one(self) -> None:
        made = self.labels(self.graph.make_one("sigil::paint::Ink"))
        self.assertIn("Ink(unsigned int)", made)
        self.assertIn("hexInk", made)

    def test_the_python_spellings_are_on_the_make_one_table(self) -> None:
        made = self.graph.make_one("sigil::paint::Ink")
        python = [site.label for site in made if site.language == "python"]
        self.assertIn("str", python)
        self.assertIn("tuple[float, float, float]", python)
        self.assertTrue(
            all("InkLike" in site.note for site in made if site.language == "python")
        )

    def test_pass_it_to(self) -> None:
        taken = self.labels(self.graph.pass_it_to("sigil::paint::Ink"))
        self.assertIn("Brush::tint", taken)

    def test_a_single_argument_constructor_is_a_conversion(self) -> None:
        made = self.graph.make_one("sigil::paint::Ink")
        conversion = next(site for site in made if site.label == "Ink(unsigned int)")
        self.assertEqual(conversion.note, "")

    def test_a_field_is_a_way_in_and_a_way_out(self) -> None:
        # `Ink::alpha` is a float, so the float side of the graph sees
        # the field from both directions and Ink itself does not.
        taken = self.labels(self.graph.pass_it_to("sigil::paint::Ink"))
        self.assertNotIn("Ink::alpha", taken)

    def test_counts_answer_the_values_index(self) -> None:
        made, taken, given = self.graph.counts("sigil::paint::Brush")
        self.assertGreaterEqual(made, 2)
        self.assertGreaterEqual(taken, 0)
        self.assertGreaterEqual(given, 0)


if __name__ == "__main__":
    unittest.main()
