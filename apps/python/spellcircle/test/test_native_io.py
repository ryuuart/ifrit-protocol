"""Optional native transport integration for the lightweight protocol package."""

import importlib.util
import tempfile
import unittest
from pathlib import Path

from SpellCircle import decode_scene, encode_scene
from SpellCircle.examples.native_io import example_scene, roundtrip


@unittest.skipUnless(
    importlib.util.find_spec("_sigil") is not None,
    "The optional native Sigil extension is not installed",
)
class NativeIO(unittest.TestCase):
    def test_typed_scene_crosses_udp_and_roundtrips_exact_mounted_bytes(self):
        from sigil import io

        expected = example_scene()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "nested" / "scene.bin"
            received = roundtrip(expected, output)
            self.assertEqual(received, expected)
            self.assertEqual(output.read_bytes(), encode_scene(expected))

            reader = io.Hub()
            reader.mount("saved://", Path(directory))
            stored = reader.blob("saved://nested/scene.bin")
            self.assertEqual(stored, encode_scene(expected))
            self.assertEqual(decode_scene(stored), expected)
            self.assertEqual(reader.resolve("saved://nested/scene.bin"), output)


if __name__ == "__main__":
    unittest.main()
