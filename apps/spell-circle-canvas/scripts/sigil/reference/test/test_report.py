"""The coverage report: what it names, and what the ledger keeps."""

import json
import unittest

from sigil.reference import report
from sigil.reference.test.support import Tree


class TheReportNamesEveryGapByKind(Tree):
    def setUp(self) -> None:
        super().setUp()
        self.run = self.built()
        self.found = report.collect(self.run)

    def kinds(self, library: str) -> set:
        return {finding.kind for finding in self.found.get(library, [])}

    def where(self, library: str, kind: str) -> list:
        return sorted(
            finding.where
            for finding in self.found.get(library, [])
            if finding.kind == kind
        )

    def test_an_entity_with_no_prose_is_named_with_its_origin(self) -> None:
        self.assertIn("verbs/width", self.where("SigilPaint", "no page"))
        detail = next(
            finding.detail
            for finding in self.found["SigilPaint"]
            if finding.kind == "no page" and finding.where == "verbs/width"
        )
        self.assertIn("sigilpaint/Brush.h:24", detail)
        self.assertIn("bound as sigil.paint.Brush.width", detail)

    def test_a_page_naming_no_entity_is_an_orphan(self) -> None:
        self.assertEqual(len(self.where("SigilPaint", "orphan page")), 1)
        self.assertIn("nothingAtAll.md", self.where("SigilPaint", "orphan page")[0])

    def test_a_page_spelling_a_generated_key_is_stale(self) -> None:
        self.assertIn("verbs/tint", self.where("SigilPaint", "stale front matter"))

    def test_a_page_that_names_its_example_is_not_missing_one(self) -> None:
        self.assertNotIn("verbs/tint", self.where("SigilPaint", "no example"))

    def test_a_python_path_the_stubs_do_not_carry_is_unresolved(self) -> None:
        detail = next(
            finding.detail
            for finding in self.found["SigilPaint"]
            if finding.kind == "unresolved link"
        )
        self.assertIn("sigil.paint.nothing", detail)

    def test_a_stub_declaration_that_reaches_no_page_is_named(self) -> None:
        self.assertEqual(
            self.where("Bindings", "unbound python"), ["sigil.paint.Brush.thin"]
        )

    def test_every_finding_kind_is_one_the_report_can_print(self) -> None:
        for held in self.found.values():
            for finding in held:
                self.assertIn(finding.kind, report.FINDINGS)


class TheLedgerKeepsOneRowPerLibrary(Tree):
    def setUp(self) -> None:
        super().setUp()
        self.run = self.built()
        self.path = self.run.templates / f"{report.LEDGER}.json"

    def standing(self) -> dict:
        return json.loads(self.path.read_text())["coverage"]

    def test_a_narrowed_run_raises_its_own_row_and_keeps_the_rest(self) -> None:
        self.path.write_text(
            json.dumps(
                {
                    "coverage": {
                        "SigilPaint": {"entities": 1, "pages": 0},
                        "SigilOther": {"entities": 9, "pages": 4},
                    }
                }
            )
        )
        self.assertEqual(report.judge(self.run, report.counts(self.run)), 0)
        held = self.standing()
        # The run never looked at SigilOther, so it cannot have an
        # opinion about it.
        self.assertEqual(held["SigilOther"], {"entities": 9, "pages": 4})
        self.assertEqual(held["SigilPaint"]["pages"], 1)

    def test_a_drop_fails_strict_and_is_not_written(self) -> None:
        before = {"coverage": {"SigilPaint": {"entities": 12, "pages": 11}}}
        self.path.write_text(json.dumps(before))
        self.run.options.strict = True
        self.assertEqual(report.judge(self.run, report.counts(self.run)), 1)
        self.assertEqual(json.loads(self.path.read_text()), before)

    def test_a_drop_without_strict_is_written_and_said(self) -> None:
        self.path.write_text(
            json.dumps({"coverage": {"SigilPaint": {"entities": 12, "pages": 11}}})
        )
        self.assertEqual(report.judge(self.run, report.counts(self.run)), 0)
        self.assertEqual(self.standing()["SigilPaint"]["pages"], 1)


if __name__ == "__main__":
    unittest.main()
