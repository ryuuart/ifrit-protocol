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

from sigil.sketch import renderFile


@unittest.skipUnless(
    os.environ.get("SIGIL_TEST_GRIMOIRE"), "native Grimoire not supplied"
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

                @sketch(size=(64.25, 48.25), captureAt=0)
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
                    renderFile(source, api, at=at)
                    api_times = json.loads(source.with_suffix(".json").read_text())
                    native = root / "native.png"
                    arguments = [
                        sys.executable,
                        "-m",
                        "sigil",
                        "open",
                        str(source),
                        "--grimoire",
                        os.environ["SIGIL_TEST_GRIMOIRE"],
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
                    # Both photograph as the sweep does: the scene2d runtime
                    # draws one frame of no length more to take its still,
                    # at the moment asked for.
                    self.assertAlmostEqual(api_times[-1], at or 0)
                    self.assertEqual(len(api_times), 3 if at == 0.025 else 2)
                    self.assertEqual(api.read_bytes(), native.read_bytes())
                    # The canvas's quarter pixel is dropped, as a plate
                    # drops it.
                    self.assertEqual(
                        struct.unpack(">II", api.read_bytes()[16:24]), (80, 60)
                    )

    def test_a_file_sequence_shows_each_requested_time_on_either_backend(self):
        # One scene with a moving edge and the times its updates read: a
        # file sequence asked for at a moment shows that moment and the
        # moments one step of the rate after it, says so line by line and
        # names the backend it drew on, and asked again draws the same
        # pictures, because a still moves no clock.
        from sigil import media

        with tempfile.TemporaryDirectory(prefix="sigil sequence ") as directory:
            root = Path(directory)
            source = root / "edge.py"
            source.write_text(
                textwrap.dedent("""
                import json
                from pathlib import Path
                from sigil.sketch import sketch

                @sketch(size=(64, 8), background="#000000", captureAt=0)
                class Edge:
                    def setup(self):
                        self.times = []

                    def update(self, elapsed, ctx):
                        self.times.append(elapsed)
                        Path(__file__).with_suffix(".json").write_text(
                            json.dumps(self.times))

                    def draw(self, pen, ctx):
                        pen.noStroke()
                        pen.fill(255, 0, 0)
                        pen.rect(ctx.elapsed * 100, 0, 4, 8)
                """)
            )
            for backend, flags in (("CPU raster", []), ("Graphite GPU", ["--gpu"])):
                pictures = []
                for attempt in range(2):
                    with self.subTest(backend=backend, attempt=attempt):
                        output = root / f"{backend.split()[0]}{attempt}.png"
                        result = subprocess.run(
                            [
                                sys.executable,
                                "-m",
                                "sigil",
                                "open",
                                str(source),
                                "--grimoire",
                                os.environ["SIGIL_TEST_GRIMOIRE"],
                                "--",
                                "--frame",
                                str(output),
                                "--at",
                                "0.25",
                                "--frames",
                                "3",
                                "--fps",
                                "20",
                                *flags,
                            ],
                            capture_output=True,
                            text=True,
                            timeout=120,
                            check=False,
                        )
                        said = result.stdout + result.stderr
                        if flags and result.returncode != 0 and "device" in said:
                            self.skipTest("no device to draw on: " + said)
                        self.assertEqual(result.returncode, 0, said)
                        self.assertIn(f"backend: {backend}", said)
                        for index, seconds in enumerate(("0.25", "0.3", "0.35")):
                            self.assertIn(f"frame {index + 1}: t={seconds}s", said)
                        times = json.loads(source.with_suffix(".json").read_text())
                        self.assertAlmostEqual(times[-1], 0.35)
                        frames = [
                            output.with_name(f"{output.stem}_{index:04d}.png")
                            for index in (1, 2, 3)
                        ]
                        for frame, left in zip(frames, (25, 30, 35)):
                            image = (
                                media.decode(media.Image, frame.read_bytes())
                                .frameAt(0)
                                .image
                            )
                            rgba = image.rgba()

                            def red(x, rgba=rgba, width=image.width()):
                                return rgba[4 * (4 * width + x)]

                            self.assertEqual(red(left - 1), 0, frame.name)
                            self.assertEqual(red(left + 1), 255, frame.name)
                            self.assertEqual(red(left + 5), 0, frame.name)
                        pictures.append([frame.read_bytes() for frame in frames])
                if len(pictures) == 2:
                    self.assertEqual(pictures[0], pictures[1])
