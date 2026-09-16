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


if __name__ == "__main__":
    unittest.main()
