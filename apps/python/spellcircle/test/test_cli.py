"""Saved scenes can be inspected and delivered without a native host."""

import io
import json
import socket
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

from SpellCircle import CircleDefinition, SceneDefinition, encode_scene
from SpellCircle.cli import main
from SpellCircle.examples.send_spell_circles import main as example


class CLI(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.path = self.root / "scene.bin"
        self.payload = encode_scene(
            SceneDefinition(
                circles=(CircleDefinition("ring", 50, 50, 30),),
                width=100,
                height=100,
            )
        )
        self.path.write_bytes(self.payload)

    def test_inspect_prints_machine_readable_metadata_without_sending(self):
        output = io.StringIO()
        with redirect_stdout(output), patch("SpellCircle.cli.send_once") as send:
            self.assertEqual(main(["inspect", str(self.path)]), 0)
        send.assert_not_called()
        metadata = json.loads(output.getvalue())
        self.assertEqual(
            metadata,
            {
                "width": 100,
                "height": 100,
                "circles": 1,
                "edges": 0,
                "boxes": 0,
                "bytes": len(self.payload),
            },
        )

    def test_send_preserves_exact_saved_bytes(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
            receiver.bind(("127.0.0.1", 0))
            receiver.settimeout(2)
            with redirect_stdout(io.StringIO()):
                self.assertEqual(
                    main(
                        [
                            "send",
                            str(self.path),
                            "--port",
                            str(receiver.getsockname()[1]),
                        ]
                    ),
                    0,
                )
            self.assertEqual(receiver.recv(65536), self.payload)

    def test_malformed_and_missing_files_report_failure(self):
        self.path.write_bytes(b"broken")
        for path in (self.path, self.root / "missing"):
            with self.subTest(path=path), redirect_stderr(io.StringIO()) as output:
                self.assertEqual(main(["inspect", str(path)]), 1)
                self.assertIn("spellcircle:", output.getvalue())

    def test_offline_example_does_not_open_a_sender(self):
        destination = self.root / "nested" / "scene.bin"
        with (
            patch(
                "sys.argv",
                ["send_spell_circles", "--seed", "2", "--output", str(destination)],
            ),
            patch("SpellCircle.examples.send_spell_circles.send_once") as send,
            redirect_stdout(io.StringIO()),
        ):
            example()
        send.assert_not_called()
        self.assertTrue(destination.is_file())
        with redirect_stdout(io.StringIO()):
            self.assertEqual(main(["inspect", str(destination)]), 0)


if __name__ == "__main__":
    unittest.main()
