"""The shell and the search index the whole site is read through."""

import json
import unittest

from sigil.reference import model, site


def entity(**asked) -> model.Entity:
    fields = {
        "library": "SigilPaint",
        "kind": model.VERB,
        "name": "tint",
        "qualified": "sigil::paint::Brush::tint",
    }
    fields.update(asked)
    return model.Entity(**fields)


class AnIndexHoldsOneRowPerPage(unittest.TestCase):
    def test_a_row_carries_the_fields_the_search_asks_by(self) -> None:
        index = site.Index()
        index.add(
            entity(group="Colour", python="sigil.paint.Brush.tint"),
            "reference/SigilPaint/verbs/tint.html",
            ["Ink"],
            "Brush &",
        )
        row = index.rows[0]
        self.assertEqual(row["kind"], model.VERB)
        self.assertEqual(row["library"], "SigilPaint")
        self.assertEqual(row["group"], "Colour")
        self.assertEqual(row["accepts"], ["Ink"])
        self.assertEqual(row["returns"], "Brush &")
        self.assertEqual(row["state"], model.CPP_ONLY)

    def test_a_value_is_one_row_and_carries_its_three_counts(self) -> None:
        index = site.Index()
        url = site.value_page("sigil::paint::Ink")
        index.add(entity(kind=model.TYPE, name="Ink"), url, [], "")
        index.counts(url, 3, 4, 1)
        self.assertEqual(len(index.rows), 1)
        self.assertEqual(index.rows[0]["kind"], model.TYPE)
        self.assertEqual(index.rows[0]["made"], 3)
        self.assertEqual(index.rows[0]["taken"], 4)
        self.assertEqual(index.rows[0]["given"], 1)

    def test_counts_for_a_page_that_is_not_there_change_nothing(self) -> None:
        index = site.Index()
        index.counts("values/nothing.html", 1, 1, 1)
        self.assertEqual(index.rows, [])


class AValueHasOnePageWhereverItIsDeclared(unittest.TestCase):
    def test(self) -> None:
        self.assertEqual(
            site.value_page("sigil::compose::Fill"), "values/sigil.compose.Fill.html"
        )


class TheShellPointsEveryLinkAtItsOwnDepth(unittest.TestCase):
    def setUp(self) -> None:
        import tempfile
        from pathlib import Path

        held = tempfile.TemporaryDirectory()
        self.addCleanup(held.cleanup)
        self.root = Path(held.name)

    def test_a_deep_page_climbs_back_to_the_root(self) -> None:
        shell = site.Shell(self.root, {})
        shell.write(
            "reference/SigilPaint/verbs/tint.html",
            "tint",
            "<p>body</p>",
            badge="differs",
        )
        page = (self.root / "reference/SigilPaint/verbs/tint.html").read_text()
        self.assertIn('href="../../../index.html"', page)
        self.assertIn('href="../../../reference.css"', page)
        self.assertIn('window.referenceRoot = "../../../"', page)
        self.assertIn('<span class="badge">differs</span>', page)

    def test_an_unchanged_page_is_left_where_it_is(self) -> None:
        path = self.root / "page.html"
        site.write_if_changed(path, "one")
        stamp = path.stat().st_mtime_ns
        site.write_if_changed(path, "one")
        self.assertEqual(path.stat().st_mtime_ns, stamp)
        site.write_if_changed(path, "two")
        self.assertEqual(path.read_text(), "two")

    def test_the_index_is_written_as_data_and_as_a_script(self) -> None:
        index = site.Index()
        index.add(entity(), "reference/SigilPaint/verbs/tint.html", [], "")
        index.write(self.root)
        rows = json.loads((self.root / site.SEARCH).read_text())
        self.assertEqual(len(rows), 1)
        # A page opened off the disk cannot fetch the JSON beside it.
        script = (self.root / site.SEARCH_SCRIPT).read_text()
        self.assertTrue(script.startswith("window.referenceIndex = ["))


if __name__ == "__main__":
    unittest.main()
