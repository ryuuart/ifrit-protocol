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
        self.held = self.catalogue(node=True)
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

    def test_a_constructor_over_a_primitive_is_not_reported_as_one(self) -> None:
        # `Ink(unsigned int)` converts from nothing a reader could go
        # and make, so naming it a conversion would send them nowhere.
        made = self.graph.make_one("sigil::paint::Ink")
        conversion = next(site for site in made if site.label == "Ink(unsigned int)")
        self.assertEqual(conversion.note, "")

    def test_a_constructor_over_a_named_type_is_a_conversion(self) -> None:
        made = self.graph.make_one("sigil::paint::Wash")
        conversion = next(site for site in made if site.label == "Wash(Ink)")
        self.assertEqual(conversion.note, "implicit conversion from `Ink`")

    def test_what_converts_is_taken_wherever_the_target_is(self) -> None:
        # An `Ink` IS a `Wash` at every call that asks for one, and the
        # Ink page is the only place that answer can be read.
        taken = self.graph.pass_it_to("sigil::paint::Ink")
        derived = next(site for site in taken if site.label == "Wash")
        self.assertEqual(derived.kind, "type")
        self.assertEqual(derived.target, "sigil::paint::Wash")
        self.assertIn("anywhere a `Wash` is taken", derived.note)

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
