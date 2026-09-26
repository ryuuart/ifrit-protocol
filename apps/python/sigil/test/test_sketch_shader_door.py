"""The shader door: an `.sksl` file beside a sketch, compiled into the
runtime effect a paint takes, and a checker standing in for a file that is
not there rather than an exception."""

import tempfile
import unittest
from pathlib import Path

from sigil import media
from sigil.sketch import render_file

SCENE = """from sigil import skia
from sigil.compose import box
from sigil.material import Paint
from sigil.sketch import sketch


@sketch(size=(32, 32), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        program = ctx.assets.shader(ctx.local({name!r}))
        assert isinstance(program, skia.RuntimeEffect)
        ctx.render(box().width(32).height(32).fill(Paint.sksl(program)))
"""


class ShaderDoor(unittest.TestCase):
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
        # Sixteen-unit cells, black where the checker starts.
        self.assertEqual(
            self.render("absent.sksl", {}, at=(4, 4)), bytes([0, 0, 0, 255])
        )
        self.assertEqual(
            self.render("absent.sksl", {}, at=(20, 4)), bytes([255, 0, 255, 255])
        )


if __name__ == "__main__":
    unittest.main()
