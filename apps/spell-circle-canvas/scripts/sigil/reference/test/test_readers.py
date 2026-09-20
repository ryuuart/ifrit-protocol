"""The three readers, and the catalogue they are joined into."""

import unittest

from sigil.reference import bindings, model
from sigil.reference.test.support import Tree


class ADoxygenInventoryFindsTheLibrarysNode(Tree):
    def test(self) -> None:
        from sigil.reference import doxygen_xml

        inventories = doxygen_xml.read(self.root / "work", ["SigilPaint"])
        paint = inventories["SigilPaint"]
        self.assertEqual(paint.namespace, "sigil::paint")
        # Two self-returning members is under the floor, so nothing in
        # this fixture is a node until the floor is lowered for it.
        doxygen_xml.NODE_FLOOR, floor = 2, doxygen_xml.NODE_FLOOR
        try:
            again = doxygen_xml.read(self.root / "work", ["SigilPaint"])
        finally:
            doxygen_xml.NODE_FLOOR = floor
        self.assertEqual(again["SigilPaint"].node, "sigil::paint::Brush")


class AVerbInheritedFromAMixinIsTheNodesOwn(Tree):
    def setUp(self) -> None:
        super().setUp()
        self.inherit_the_edges()

    def test_the_member_is_read_under_the_inheritors_name(self) -> None:
        from sigil.reference import doxygen_xml

        paint = doxygen_xml.read(self.root / "work", ["SigilPaint"])["SigilPaint"]
        soften = paint.declarations["classsigil_1_1paint_1_1_edge_verbs_1aff"]
        self.assertEqual(soften.qualified, "sigil::paint::Brush::soften")
        self.assertEqual(soften.owner, "sigil::paint::Brush")
        self.assertEqual(soften.returns, "Brush &")
        self.assertIn("classsigil_1_1paint_1_1_brush", soften.return_refs)
        # Where it is declared, and the page Doxygen wrote it on, stay
        # the mixin's.
        self.assertEqual(soften.header, "sigilpaint/verbs/Edge.h")
        self.assertEqual(soften.compound_refid, "classsigil_1_1paint_1_1_edge_verbs")
        self.assertEqual(soften.anchor(), "aff")

    def test_it_counts_toward_the_node_and_is_catalogued_as_a_verb(self) -> None:
        catalogue = self.catalogue(node=True)
        held = {entity.qualified: entity for entity in catalogue.entities}
        self.assertEqual(catalogue.nodes["SigilPaint"], "sigil::paint::Brush")
        self.assertEqual(held["sigil::paint::Brush::soften"].kind, model.VERB)
        self.assertEqual(
            held["sigil::paint::Brush::soften"].doxygen,
            "SigilPaint/html/classsigil_1_1paint_1_1_edge_verbs.html#aff",
        )
        # Its private members stay private, and the mixin is no type of
        # the library's: nothing takes one and nothing hands one back.
        self.assertNotIn("sigil::paint::Brush::self", held)
        self.assertNotIn("sigil::paint::EdgeVerbs", held)
        self.assertNotIn("sigil::paint::EdgeVerbs::soften", catalogue.declared)


class ADeclarationCarriesItsTypesAndItsProse(Tree):
    def test(self) -> None:
        from sigil.reference import doxygen_xml

        paint = doxygen_xml.read(self.root / "work", ["SigilPaint"])["SigilPaint"]
        tint = paint.declarations["classsigil_1_1paint_1_1_brush_1aaa"]
        self.assertEqual(tint.name, "tint")
        self.assertEqual(tint.returns, "Brush &")
        self.assertEqual([one.type_text for one in tint.parameters], ["Ink"])
        self.assertEqual(tint.brief, "The colour the brush lays down.")
        self.assertEqual(tint.detail, "A tint is resolved where the mark lands.")
        self.assertEqual(tint.header, "sigilpaint/Brush.h")
        self.assertEqual(tint.line, 20)
        self.assertEqual(tint.anchor(), "aaa")


class APublicSpellingWinsOverTheNativeOne(Tree):
    def test(self) -> None:
        from sigil.reference import python_stubs

        surface, _ = python_stubs.read(self.package, self.declarations)
        self.assertEqual(surface.public_of("_sigil.paint.Brush"), "sigil.paint.Brush")
        self.assertEqual(
            surface.public_of("_sigil.paint.Brush.tint"), "sigil.paint.Brush.tint"
        )
        # The public package renames this one, and the page has to show
        # what an author types rather than what the extension exports.
        self.assertEqual(
            surface.public_of("_sigil.paint.hexInk"), "sigil.paint.hex_ink"
        )
        declaration = surface.declaration("_sigil.paint.Brush.tint")
        self.assertEqual(
            declaration.signatures[0].spelling("tint"),
            "def tint(self, colour: _t.InkLike) -> Brush: ...",
        )


class ARoleUnionExpandsThroughItsNesting(Tree):
    def test(self) -> None:
        from sigil.reference import python_stubs

        _, roles = python_stubs.read(self.package, self.declarations)
        self.assertEqual(
            roles.expand("InkLike"),
            ["ColorLike", "str", "tuple[float, float, float]", "sigil.paint.Ink"],
        )
        self.assertEqual(roles.path_of("sigil.paint.Ink"), "sigil.paint.Ink")
        self.assertEqual(roles.containing("sigil.paint.Ink"), ["InkLike"])


class ABindingScanTellsTheThreeShapesApart(Tree):
    def test(self) -> None:
        found = bindings.read(self.sources)
        by_name = {(one.owner, one.name): one for one in found}
        self.assertEqual(by_name[("paint::Brush", "width")].style, model.DIRECT)
        self.assertEqual(
            by_name[("paint::Brush", "width")].target, "paint::Brush::width"
        )
        self.assertEqual(by_name[("paint::Brush", "tint")].style, model.WRAPPED)
        self.assertEqual(by_name[("", "brush")].module, "paint")
        self.assertEqual(by_name[("", "brush")].target, "paint::brush")
        self.assertEqual(by_name[("", "hexInk")].style, model.WRAPPED)
        # A type's own name is a binding too, and the only place a
        # type's Python spelling is written down.
        self.assertEqual(by_name[("", "Brush")].flavour, "class")
        self.assertEqual(by_name[("", "Brush")].target, "paint::Brush")


class ALambdaBodyDoesNotCutTheChain(unittest.TestCase):
    def test(self) -> None:
        source = """
        namespace sigil::python {
        void bind(py::module_& module) {
          py::class_<Brush> brush(module, "Brush");
          brush.def("one", [](Brush& self) { int x = 1; return self; })
              .def("two", &Brush::two);
        }
        }
        """
        held = bindings.statements(source)
        chains = [body for _, body in held if ".def(" in body]
        self.assertEqual(len(chains), 1)
        self.assertIn('"one"', chains[0])
        self.assertIn('"two"', chains[0])


class ACatalogueSortsEveryDeclarationIntoAKind(Tree):
    def test(self) -> None:
        catalogue = self.catalogue(node=True)
        kinds = {entity.qualified: entity.kind for entity in catalogue.entities}
        self.assertEqual(kinds["sigil::paint::Brush::tint"], model.VERB)
        # Handed back by value, the chain ends there; it is still a verb.
        self.assertEqual(kinds["sigil::paint::Brush::dry"], model.VERB)
        self.assertEqual(kinds["sigil::paint::brush"], model.ELEMENT)
        self.assertEqual(kinds["sigil::paint::kit::hairline"], model.KIT)
        self.assertEqual(kinds["sigil::paint::hexInk"], model.FUNCTION)
        self.assertEqual(kinds["sigil::paint::Ink"], model.TYPE)
        self.assertEqual(kinds["sigil::paint::Cap"], model.ENUM)

    def test_states_and_spellings(self) -> None:
        catalogue = self.catalogue(node=True)
        held = {entity.qualified: entity for entity in catalogue.entities}
        self.assertEqual(held["sigil::paint::Brush::width"].binding_state, model.DIRECT)
        self.assertEqual(held["sigil::paint::Brush::tint"].binding_state, model.WRAPPED)
        self.assertEqual(
            held["sigil::paint::Brush::tint"].python, "sigil.paint.Brush.tint"
        )
        self.assertEqual(held["sigil::paint::hexInk"].python, "sigil.paint.hex_ink")
        self.assertEqual(
            held["sigil::paint::kit::hairline"].binding_state, model.CPP_ONLY
        )

    def test_names_read_without_the_namespace(self) -> None:
        catalogue = self.catalogue()
        self.assertEqual(
            catalogue.display("sigil::paint::kit::hairline", "SigilPaint"),
            "kit::hairline",
        )


class ANameResolvesOnItsTail(Tree):
    def test(self) -> None:
        catalogue = self.catalogue()
        self.assertEqual(catalogue.resolve("Ink"), "sigil::paint::Ink")
        self.assertEqual(catalogue.resolve("paint::Ink"), "sigil::paint::Ink")
        self.assertEqual(catalogue.resolve("Nothing"), "")

    def test_a_tail_two_types_answer_to_resolves_to_neither(self) -> None:
        catalogue = self.catalogue()
        self.assertEqual(catalogue.resolve("Wash"), "")

    def test_a_refid_settles_what_the_tail_cannot(self) -> None:
        catalogue = self.catalogue()
        self.assertEqual(
            catalogue.resolve("Wash", refs=("structsigil_1_1paint_1_1stock_1_1_wash",)),
            "sigil::paint::stock::Wash",
        )


class APageIsWrittenUnderANameThatIsItsOwn(Tree):
    def test(self) -> None:
        catalogue = self.catalogue()
        slugs = {entity.qualified: entity.slug() for entity in catalogue.entities}
        # Two namespaces spell `hexInk`, and two pages cannot be one
        # file: the one that has to say more says it.
        self.assertEqual(slugs["sigil::paint::hexInk"], "hexInk")
        self.assertEqual(slugs["sigil::paint::mixers::hexInk"], "mixers.hexInk")
        paths = [entity.path() for entity in catalogue.entities]
        self.assertEqual(len(paths), len(set(paths)))


class ABoundEnumeratorIsNotAContradiction(Tree):
    def test(self) -> None:
        catalogue = self.catalogue(node=True)
        # Doxygen writes an enumerator inside its enum rather than as a
        # declaration of its own, and the binding names it under the
        # enum, so the two have to be joined by hand.
        self.assertIn("sigil::paint::Cap::Butt", catalogue.declared)
        self.assertEqual(
            [binding.name for binding in catalogue.unmatched],
            [],
        )


class AConvenienceOnlyPythonHasIsStillAnEntity(Tree):
    def test(self) -> None:
        catalogue = self.catalogue(node=True)
        held = {entity.qualified: entity for entity in catalogue.entities}
        thicken = held["sigil.paint.Brush.thicken"]
        self.assertEqual(thicken.binding_state, model.PYTHON_ONLY)
        self.assertEqual(thicken.kind, model.VERB)
        self.assertEqual(thicken.library, "SigilPaint")
        self.assertEqual(thicken.python, "sigil.paint.Brush.thicken")
        self.assertEqual(thicken.header, "")
        self.assertTrue(thicken.python_signatures)


class WhatPybindWritesOnEveryClassIsNotAGap(Tree):
    def test(self) -> None:
        catalogue = self.catalogue(node=True)
        unbound = [spelling for spelling, _ in catalogue.unbound_python()]
        # The stubs declare a protocol method and the two properties
        # every bound enumeration carries; only `thin` is a name with
        # nothing behind it.
        self.assertEqual(unbound, ["sigil.paint.Brush.thin"])


if __name__ == "__main__":
    unittest.main()
