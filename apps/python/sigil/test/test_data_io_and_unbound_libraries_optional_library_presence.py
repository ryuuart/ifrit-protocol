"""The optional SDK libraries this build carries, and who agrees about them."""

import importlib
import tempfile
import unittest
from pathlib import Path

import _sigil
from sigil import core
from sigil._requirements import missing_requirements

# The libraries whose SDK is a licensed download. A build that finds one
# registers a submodule under its name and a build that does not registers
# nothing, so this is the vocabulary the build's own answer is drawn from.
LICENSED = ("scry", "substance", "usd")


class OptionalLibraryPresence(unittest.TestCase):
    def setUp(self):
        self.carried = core.optionalLibraries()

    def test_the_answer_is_a_stable_tuple_of_module_names(self):
        self.assertIsInstance(self.carried, tuple)
        for name in self.carried:
            self.assertIsInstance(name, str)
            self.assertTrue(all(part.isidentifier() for part in name.split(".")))
        self.assertEqual(list(self.carried), sorted(set(self.carried)))
        self.assertEqual(core.optionalLibraries(), self.carried)

    def test_the_answer_takes_no_arguments(self):
        with self.assertRaises(TypeError):
            core.optionalLibraries("scry")

    def test_a_licensed_library_is_named_exactly_when_it_registered(self):
        for name in LICENSED:
            with self.subTest(library=name):
                self.assertEqual(name in self.carried, hasattr(_sigil, name))

    def test_a_carried_library_reaches_the_module_it_is_named_for(self):
        for name in self.carried:
            with self.subTest(library=name):
                module = importlib.import_module("sigil." + name)
                self.assertEqual(module.__name__, "sigil." + name)

    def test_a_library_this_build_lacks_carries_nothing(self):
        for name in LICENSED:
            if name in self.carried:
                continue
            with self.subTest(library=name):
                self.assertFalse(hasattr(_sigil, name))
                try:
                    module = importlib.import_module("sigil." + name)
                except ImportError:
                    continue
                self.assertEqual(
                    [member for member in dir(module) if not member.startswith("_")],
                    [],
                )

    def test_a_sketch_requirement_finds_what_this_build_carries(self):
        absent = "sigil.no_such_optional_library"
        declared = tuple("sigil." + name for name in self.carried) + (absent,)
        with tempfile.TemporaryDirectory(prefix="sigil optional libraries ") as folder:
            source = Path(folder) / "study.py"
            source.write_text(f"REQUIRES = {declared!r}\n", encoding="utf-8")
            self.assertEqual(missing_requirements(source), [absent])


if __name__ == "__main__":
    unittest.main()
