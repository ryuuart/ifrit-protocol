"""Callback inspection is independent of live sketch loading."""

import subprocess
import sys
import unittest
from pathlib import Path

from sigil._callbacks import arity


class Callbacks(unittest.TestCase):
    def test_supported_prefix_respects_defaults_and_bound_methods(self):
        class Consumer:
            def update(self, elapsed, context=None):
                pass

        self.assertEqual(arity(Consumer().update, 2), 2)
        self.assertEqual(arity(Consumer().update, 1), 1)
        self.assertEqual(arity(lambda: None, 2), 0)
        self.assertEqual(arity(lambda *values: None, 2), 2)

    def test_unsatisfied_keyword_only_arguments_are_rejected(self):
        def callback(elapsed, *, required):
            pass

        with self.assertRaisesRegex(TypeError, "cannot accept"):
            arity(callback, 2)

    def test_importing_helpers_does_not_install_the_sketch_finder(self):
        package = Path(__file__).resolve().parents[1]
        program = """import sys
sys.path.insert(0, sys.argv[1])
before = tuple(sys.meta_path)
from sigil._callbacks import arity
from sigil import cli, environment
assert arity(lambda value: value, 2) == 1
assert 'sigil._loader' not in sys.modules
assert tuple(sys.meta_path) == before
"""
        result = subprocess.run(
            [sys.executable, "-I", "-c", program, str(package)],
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
