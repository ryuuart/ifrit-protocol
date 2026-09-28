"""A shader file beside a sketch, read through the host's hub by
`material.shader`: a missing file paints the placeholder, a valid edit
replaces it, and a broken edit keeps the last compiled program."""

import tempfile
import unittest
from pathlib import Path

from sigil import media
from sigil.protocol import Clock, Session
from sigil.protocol.clock import Policy
from sigil.sketch import render_file
from sigil.testing import InProcess

SCENE = """from sigil import material
from sigil.compose import box
from sigil.sketch import sketch


@sketch(size=(32, 32), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        program = material.shader(ctx.assets.hub(), ctx.local({name!r}))
        ctx.render(box().width(32).height(32).fill(program))
"""


class ShaderFile(unittest.TestCase):
    def render(self, name, files, at=(16, 16)):
        with tempfile.TemporaryDirectory() as folder:
            for file, body in files.items():
                (Path(folder) / file).write_text(body)
            source = Path(folder) / "scene.py"
            output = Path(folder) / "scene.png"
            source.write_text(SCENE.format(name=name))
            render_file(source, output, at=0)
            pixels = media.load(output).frameAt(0).image.rgba()
            offset = (at[1] * 32 + at[0]) * 4
            return bytes(pixels[offset : offset + 4])

    def test_a_file_beside_the_sketch_paints(self):
        red = "half4 main(float2 xy) { return half4(1.0, 0.0, 0.0, 1.0); }\n"
        self.assertEqual(
            self.render("fill.sksl", {"fill.sksl": red}), bytes([255, 0, 0, 255])
        )

    def test_a_missing_file_paints_the_checker(self):
        self.assertEqual(
            self.render("absent.sksl", {}, at=(20, 4)), bytes([255, 0, 255, 255])
        )
        self.assertEqual(
            self.render("absent.sksl", {}, at=(4, 4)), bytes([0, 0, 0, 255])
        )

    def test_file_edits_replace_the_placeholder_and_keep_the_last_good_program(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "scene.py"
            source.write_text(SCENE.format(name="fill.sksl"))
            host = InProcess(root / "state")
            clock, session = Clock(host), Session(host)
            clock.set_policy(policy=Policy.Advance)
            session.open(sketch=str(source))

            def pixel():
                still = session.still(density=1)
                pixels = media.load(still.path).frameAt(0).image.rgba()
                offset = (4 * 32 + 20) * 4
                return bytes(pixels[offset : offset + 4])

            self.assertEqual(pixel(), bytes([255, 0, 255, 255]))
            program = root / "fill.sksl"
            program.write_text("half4 main(float2 xy) { return half4(1, 0, 0, 1); }")
            clock.step(seconds=0.6)
            host.frame()
            self.assertEqual(pixel(), bytes([255, 0, 0, 255]))
            program.write_text("half4 main(float2 xy) { return missingColour; }")
            clock.step(seconds=0.6)
            host.frame()
            self.assertEqual(pixel(), bytes([255, 0, 0, 255]))


if __name__ == "__main__":
    unittest.main()
