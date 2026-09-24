"""A catalog of sketch files reads as the rows a browser shows."""

import tempfile
import unittest
from pathlib import Path

from sigil.sketch import catalog, registryRows


class CatalogRows(unittest.TestCase):
    def test_files_opened_by_path_are_rows_after_the_registry(self):
        with tempfile.TemporaryDirectory(prefix="sigil catalog ") as directory:
            root = Path(directory)
            (root / "studies").mkdir()
            drawing = root / "studies" / "lamp.py"
            drawing.write_text('"""A lamp on a table."""\n\nLIGHT = 1\n')
            draft = root / "draft.cpp"
            draft.write_text("// a draft nothing has built\n")

            rows = catalog([drawing, draft], workspace=root)
            files = [row for row in rows if row.external]
            self.assertEqual([row.key for row in files], ["lamp", "draft"])
            lamp, unbuilt = files
            self.assertEqual(lamp.kind, "canvas")
            self.assertEqual(unbuilt.kind, "")
            self.assertEqual(lamp.source.subject, "A lamp on a table.")
            self.assertEqual(lamp.entryPath, Path("studies/lamp.py"))
            self.assertTrue(lamp.category.endswith("studies"))
            self.assertFalse(lamp.videoExportable)
            self.assertEqual(rows.index(unbuilt), unbuilt.index)

    def test_a_plain_interpreter_registers_no_sketch(self):
        # The registry is the process's: Sketchbook's holds every sketch it
        # was built with, and the extension alone registers none.
        self.assertEqual(registryRows(), [])
        self.assertEqual(registryRows("canvas"), [])


if __name__ == "__main__":
    unittest.main()
