"""Catalogue registration distinguishes declared sketches from Python helpers."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class Registration(unittest.TestCase):
    def test_only_declared_sketch_classes_join_the_catalogue(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            examples = {
                "scene.py": "@sketch()\nclass Scene: pass\n",
                "qualified.py": "@api.sketch(size=(12, 12))\nclass Art: pass\n",
                "helper.py": "class Helper: pass\n",
                "__init__.py": "from .helper import Helper\n",
                "notes.py": '"""@sketch()\nclass Example: pass"""\n',
                "function.py": "@sketch()\ndef helper(): pass\n",
            }
            for name, source in examples.items():
                (root / name).write_text(source)
            output = root / "registry.cpp"
            generator = (
                Path(__file__).resolve().parents[1] / "cmake/register_sketches.py"
            )
            subprocess.run(
                [
                    sys.executable,
                    str(generator),
                    "--output",
                    str(output),
                    *(str(root / name) for name in examples),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            registered = output.read_text()
            self.assertIn('"scene", "scene", "Python"', registered)
            self.assertIn('"qualified", "qualified", "Python"', registered)
            self.assertEqual(registered.count("sigil::sketch::add("), 2)

    def test_a_file_of_hook_functions_joins_the_catalogue_under_its_stem(self):
        registered = self.generate(
            {
                "bounce.py": "def setup(): pass\ndef draw(pen): pass\n",
                "page.py": "def describe(seconds): return None\n",
                "helpers.py": "def update(elapsed): pass\ndef paint(pen): pass\n",
                "nested.py": "class Helper:\n    def draw(self, pen): pass\n",
            }
        )
        self.assertIn('"bounce", "bounce", "Python"', registered)
        self.assertIn('"page", "page", "Python"', registered)
        self.assertEqual(registered.count("sigil::sketch::add("), 2)

    def test_a_study_states_its_name_and_category_and_keeps_its_stem(self):
        # The module raises the moment it runs, so a registration that
        # reads its declarations by importing it fails here.
        study = (
            '"""A reader\'s view into the Library of Babel."""\n'
            "raise SystemExit('registration must not run the sketch')\n"
            "NAME = 'The Library of Babel'\n"
            "CATEGORY: str = 'Study \u00b7 Literature'\n"
            "REQUIRES = ('numpy',)\n"
            "@sketch()\n"
            "class Library: pass\n"
        )
        registered = self.generate({"borges_library.py": study})
        self.assertIn(
            '"borges_library", "The Library of Babel", '
            '"Study \u00b7 Literature", "A reader\'s view into the Library of Babel."',
            registered,
        )
        self.assertIn('{"numpy"}', registered)

    def test_an_undeclared_name_and_category_read_as_the_stem_and_python(self):
        registered = self.generate(
            {"plain_study.py": '"""A plain study."""\n@sketch()\nclass Plain: pass\n'}
        )
        self.assertIn('"plain_study", "plain_study", "Python"', registered)

    def test_a_name_or_category_that_is_not_a_literal_string_is_refused(self):
        for declaration in (
            "CATEGORY = 'Study' + ' \u00b7 Pattern'",
            "CATEGORY = 3",
            "NAME = ''",
            "NAME = title()",
        ):
            with self.subTest(declaration=declaration):
                with self.assertRaises(subprocess.CalledProcessError) as refused:
                    self.generate(
                        {"study.py": f"{declaration}\n@sketch()\nclass Study: pass\n"}
                    )
                self.assertIn("literal", refused.exception.stderr)

    def generate(self, sources):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name, source in sources.items():
                (root / name).write_text(source, encoding="utf-8")
            output = root / "registry.cpp"
            generator = (
                Path(__file__).resolve().parents[1] / "cmake/register_sketches.py"
            )
            subprocess.run(
                [
                    sys.executable,
                    str(generator),
                    "--output",
                    str(output),
                    *(str(root / name) for name in sources),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            return output.read_text(encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
