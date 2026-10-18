"""The compose texture scene belongs to the compose bindings.

A Python scene3d sketch is handed one by its context, and a consumer
without the sketch adapter names the same class from `sigil.compose`.
"""

import subprocess
import sys
import textwrap
import unittest


class TextureSceneHome(unittest.TestCase):
    def test_it_imports_with_the_compose_module_alone(self):
        script = textwrap.dedent("""
            import sys
            from sigil.compose import TextureScene
            assert TextureScene.__module__ == "sigil.compose", TextureScene.__module__
            assert "sigil.sketch" not in sys.modules
            for name in ("render", "texture", "revision", "size"):
                assert hasattr(TextureScene, name), name
        """)
        completed = subprocess.run(
            [sys.executable, "-c", script], capture_output=True, text=True
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)


if __name__ == "__main__":
    unittest.main()
