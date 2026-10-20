"""The parity ledger's list of Python studies: a study is a file of its
own beside the catalogue's sketches or a directory holding a file of its
own name, and the ledger lists both."""

import importlib
import sys
import tempfile
import unittest
from pathlib import Path

TYPING = Path(__file__).resolve().parents[1] / "typing"


def parity():
    """typing/parity.py, imported the way its own command line runs it:
    beside the modules it imports by bare name."""
    sys.path.insert(0, str(TYPING))
    try:
        return importlib.import_module("parity")
    finally:
        sys.path.remove(str(TYPING))


class ParityStudies(unittest.TestCase):
    def test_a_directory_study_is_listed_beside_a_file_study(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "python_alone.py").write_text("")
            (root / "python_bundled").mkdir()
            (root / "python_bundled" / "python_bundled.py").write_text("")
            # A helper inside a study's directory is not a study of its own,
            # and neither is a directory that is not a Python study.
            (root / "python_bundled" / "python_helper.py").write_text("")
            (root / "native_sketch").mkdir()
            (root / "native_sketch" / "native_sketch.py").write_text("")
            self.assertEqual(
                parity().studies(root),
                {
                    "python_alone": "python_alone.py",
                    "python_bundled": "python_bundled/python_bundled.py",
                },
            )

    def test_the_catalogues_directory_studies_are_in_the_ledger(self):
        module = parity()
        found = module.studies(module.SKETCHES)
        self.assertEqual(
            found["python_market_tape"], "python_market_tape/python_market_tape.py"
        )
        self.assertEqual(
            found["python_bundled_type"], "python_bundled_type/python_bundled_type.py"
        )
        ledger = (TYPING.parent / "PARITY.md").read_text()
        self.assertIn("`python_market_tape`", ledger)
        self.assertIn("`python_bundled_type`", ledger)


if __name__ == "__main__":
    unittest.main()
