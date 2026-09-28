"""A shader as a material from Python: the source and its parameters as a
dict or a NamedTuple, one definition per source, fields written through the
Material by name, the body painting what it returns, and a program file
with no text that compiled answering the placeholder and standing on the
hub's problems."""

import tempfile
import unittest
from pathlib import Path
from typing import NamedTuple

from sigil import io, material, media
from sigil.sketch import render_file

FLAT = "half4 main(float2 p) { return half4(ink.rgb * level, 1); }"

SCENE = """from sigil import material
from sigil.compose import box
from sigil.sketch import sketch


@sketch(size=(8, 8), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        flat = material.shader({source!r}, {{"ink": material.Color(0, 0, 1, 1), "level": 1.0}})
        ctx.render(box().width(8).height(8).fill(flat))
"""


class Tone(NamedTuple):
    ink: material.Color
    level: float


class Shader(unittest.TestCase):
    def test_one_source_is_one_definition(self):
        red = {"ink": material.Color(1, 0, 0, 1), "level": 0.5}
        self.assertEqual(material.shader(FLAT, red), material.shader(FLAT, red))
        self.assertNotEqual(
            material.shader(FLAT, red), material.shader(FLAT + "\n", red)
        )

    def test_a_named_tuple_is_the_same_parameters_as_a_dict(self):
        self.assertEqual(
            material.shader(FLAT, Tone(material.Color(1, 0, 0, 1), 0.5)),
            material.shader(FLAT, {"ink": material.Color(1, 0, 0, 1), "level": 0.5}),
        )

    def test_a_field_is_written_through_the_material_by_name(self):
        tone = {"ink": material.Color(1, 0, 0, 1), "level": 0.5}
        moved = material.shader(FLAT, tone)
        moved.set("level", 0.8)
        self.assertNotEqual(moved, material.shader(FLAT, tone))

    def test_it_paints_what_its_body_returns(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "scene.py"
            output = Path(folder) / "scene.png"
            source.write_text(SCENE.format(source=FLAT))
            render_file(source, output, at=0)
            pixels = media.load(output).frameAt(0).image.rgba()
            offset = (4 * 8 + 4) * 4
            self.assertEqual(bytes(pixels[offset : offset + 4]), bytes([0, 0, 255, 255]))

    def test_a_file_with_no_program_is_the_placeholder_and_a_problem(self):
        with tempfile.TemporaryDirectory() as folder:
            hub = io.Hub()
            hub.mount("res://", folder)
            self.assertEqual(
                material.shader(hub, "res://absent.sksl"), material.placeholder()
            )
            self.assertEqual(
                [problem.uri for problem in hub.problems()], ["res://absent.sksl"]
            )
            (Path(folder) / "absent.sksl").write_text(FLAT)
            self.assertTrue(hub.poll())
            tone = {"ink": material.Color(1, 0, 0, 1), "level": 0.5}
            good = material.shader(hub, "res://absent.sksl", tone)
            self.assertNotEqual(good, material.placeholder())
            self.assertFalse(hub.problems())
            (Path(folder) / "absent.sksl").write_text(
                "half4 main(float2 xy) { return missingColour; }"
            )
            self.assertTrue(hub.poll())
            self.assertEqual(material.shader(hub, "res://absent.sksl", tone), good)
            problems = hub.problems()
            self.assertEqual(len(problems), 1)
            self.assertEqual(problems[0].uri, "res://absent.sksl")
            self.assertIn("missingColour", problems[0].message)


if __name__ == "__main__":
    unittest.main()
