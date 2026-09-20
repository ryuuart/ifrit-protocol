"""One whole run: the pages written, the index, and the example."""

import contextlib
import io
import json
import unittest

from sigil.reference import model, site
from sigil.reference.test.support import Manifest, Tree, lowered_floor


class ARunWritesOnePageForEveryEntity(Tree):
    def setUp(self) -> None:
        super().setUp()
        self.run = self.built()
        self.run.write()

    def written(self, path: str) -> str:
        return (self.run.root / path).read_text()

    def test_every_entity_has_a_file_of_its_own(self) -> None:
        for entity in self.run.catalogue.entities:
            self.assertTrue(
                (self.run.root / f"{entity.path()}.html").exists(),
                f"{entity.qualified} has no page",
            )

    def test_a_name_two_namespaces_share_is_two_files(self) -> None:
        self.assertTrue(
            (self.run.root / "reference/SigilPaint/functions/hexInk.html").exists()
        )
        other = self.written("reference/SigilPaint/functions/mixers.hexInk.html")
        self.assertIn("sigilpaint/Mixers.h", other)

    def test_a_value_has_one_page_and_it_is_tree_wide(self) -> None:
        page = self.written("values/sigil.paint.Ink.html")
        self.assertIn("Make one", page)
        self.assertIn("Pass it to", page)
        self.assertIn("Brush::tint", page)
        # The two libraries a value crosses meet here and nowhere else,
        # so the library's own types index links to this page rather
        # than writing a second one beside it.
        self.assertFalse(
            (self.run.root / "reference/SigilPaint/types/Ink.html").exists()
        )
        self.assertIn(
            "values/sigil.paint.Ink.html",
            self.written("reference/SigilPaint/types/index.html"),
        )

    def test_the_prose_and_the_generated_sections_meet(self) -> None:
        page = self.written("reference/SigilPaint/verbs/tint.html")
        self.assertIn("The colour every mark after it is laid down in.", page)
        self.assertIn("A tint is resolved where the mark lands", page)
        self.assertIn("Syntax", page)
        self.assertIn("def tint", page)

    def test_an_example_is_named_without_being_rendered(self) -> None:
        page = self.written("reference/SigilPaint/verbs/tint.html")
        self.assertIn("Built without --example-images", page)
        self.assertIn("paint.brush().tint", page)
        self.assertEqual(self.run.examples.failures, [])

    def test_the_search_index_carries_one_row_per_page(self) -> None:
        rows = json.loads((self.run.root / site.SEARCH).read_text())
        urls = [row["url"] for row in rows]
        self.assertEqual(len(urls), len(set(urls)))
        self.assertEqual(len(rows), len(self.run.catalogue.entities))
        ink = next(row for row in rows if row["url"] == "values/sigil.paint.Ink.html")
        self.assertEqual(ink["kind"], model.TYPE)
        self.assertEqual(ink["made"], len(self.run.graph.make_one("sigil::paint::Ink")))

    def test_a_row_says_what_its_page_accepts(self) -> None:
        rows = json.loads((self.run.root / site.SEARCH).read_text())
        tint = next(
            row for row in rows if row["url"].endswith("SigilPaint/verbs/tint.html")
        )
        self.assertEqual(tint["accepts"], ["Ink"])
        self.assertEqual(tint["group"], "Colour")
        self.assertEqual(tint["python"], "sigil.paint.Brush.tint")

    def test_the_indexes_stand_over_the_pages(self) -> None:
        for path in (
            "reference/index.html",
            "reference/SigilPaint/index.html",
            "reference/SigilPaint/verbs/index.html",
            "values/index.html",
        ):
            self.assertTrue((self.run.root / path).exists(), path)
        self.assertIn(
            "Marks, and what they are made of",
            self.written("reference/SigilPaint/index.html"),
        )


class TwoEntitiesNeverShareAPage(Tree):
    def test(self) -> None:
        run = self.built()
        # The disambiguation is what keeps this from happening; undoing
        # it has to stop the run rather than write one page over the
        # other and leave both in the index.
        for entity in run.catalogue.entities:
            entity.page_name = ""
        with self.assertRaises(SystemExit) as raised:
            run.write()
        self.assertIn("claimed by two entities", str(raised.exception))


class AMissingDeclarationTreeIsANamedSkip(Tree):
    def test(self) -> None:
        from sigil.reference.build import Build, Options

        # The declarations are written when the extension links, so a
        # tree built without the generator has none to read. The layer
        # names the reader it is missing and is skipped, the way it is
        # for an inventory that has not been written.
        (self.declarations / "_types.pyi").unlink()
        run = Build(
            Manifest(self.root, self.source),
            Options(
                package=self.package,
                declarations=self.declarations,
                binding_sources=self.sources,
            ),
        )
        said = io.StringIO()
        with lowered_floor(True), contextlib.redirect_stderr(said):
            self.assertFalse(run.read())
        self.assertIn("no Python declarations to read", said.getvalue())


if __name__ == "__main__":
    unittest.main()
