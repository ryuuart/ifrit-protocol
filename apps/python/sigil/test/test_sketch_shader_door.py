"""A shader file beside a sketch, read through the host's hub by
`material.shader`: it paints, and a file that is not there paints nothing
rather than raising."""

import tempfile
import unittest
from pathlib import Path

from sigil import media
from sigil.sketch import render_file

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

    def test_a_missing_file_paints_nothing(self):
        # The background shows through where the shader would stand.
        self.assertEqual(
            self.render("absent.sksl", {}, at=(20, 4)), bytes([0, 0, 0, 255])
        )


if __name__ == "__main__":
    unittest.main()
