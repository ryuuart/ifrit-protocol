"""The native file host and Python API photograph the same scene state."""

import json
import os
import struct
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil.sketch import render_file


@unittest.skipUnless(
    os.environ.get("SIGIL_TEST_SKETCHBOOK"), "native Sketchbook not supplied"
)
class CaptureParity(unittest.TestCase):
    def test_declared_zero_explicit_zero_and_fractional_moments_match(self):
        with tempfile.TemporaryDirectory(prefix="sigil capture ") as directory:
            root = Path(directory)
            source = root / "scene.py"
            source.write_text(
                textwrap.dedent("""
                import json
                from pathlib import Path
                from sigil.sketch import sketch

                @sketch(size=(64.25, 48.25), capture_at=0)
                class Scene:
                    def setup(self):
                        self.times = []

                    def update(self, elapsed, ctx):
                        self.times.append(elapsed)
                        ctx.canvas(80.25, 60.25)
                        Path(__file__).with_suffix(".json").write_text(
                            json.dumps(self.times))

                    def draw(self, pen):
                        pen.noStroke()
                        pen.fill(100 + len(self.times), 200, 50)
                        pen.rect(len(self.times) * 4, 10, 3, 20)
                        pen.fill(255, 50, 100)
                        pen.circle(40 + pen.millis() / 10, 40, 8)
                """)
            )
            for at in (None, 0.0, 0.025):
                with self.subTest(at=at):
                    api = root / "api.png"
                    render_file(source, api, at=at)
                    api_times = json.loads(source.with_suffix(".json").read_text())
                    native = root / "native.png"
                    arguments = [
                        sys.executable,
                        "-m",
                        "sigil",
                        "open",
                        str(source),
                        "--sketchbook",
                        os.environ["SIGIL_TEST_SKETCHBOOK"],
                        "--",
                        "--frame",
                        str(native),
                    ]
                    if at is not None:
                        arguments.extend(["--at", str(at)])
                    result = subprocess.run(
                        arguments,
                        capture_output=True,
                        text=True,
                        timeout=60,
                        check=False,
                    )
                    self.assertEqual(
                        result.returncode, 0, result.stdout + result.stderr
                    )
                    native_times = json.loads(source.with_suffix(".json").read_text())
                    self.assertEqual(api_times, native_times)
                    self.assertAlmostEqual(api_times[-1], at or 0)
                    self.assertEqual(len(api_times), 2 if at == 0.025 else 1)
                    self.assertEqual(api.read_bytes(), native.read_bytes())
                    self.assertEqual(
                        struct.unpack(">II", api.read_bytes()[16:24]), (81, 61)
                    )
