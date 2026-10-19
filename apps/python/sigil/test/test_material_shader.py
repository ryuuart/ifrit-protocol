"""A shader as a material from Python: the source and its parameters as a
dict or a NamedTuple, one definition per source, fields written through the
Material by name, a colour bound in any spelling, the body painting what it
returns, and a program file with no text that compiled answering the
placeholder and standing on the hub's problems. Beside it, the stock
programs written over options records: glass refraction, and normals made
from a height or reoriented into a base; and textures: an image and how it
is sampled, played as a flipbook showing the cell its frame names, or moved
along a flow map."""

import tempfile
import textwrap
import unittest
from pathlib import Path
from typing import NamedTuple

from sigil import io, material, media, motion
from sigil.sketch import renderFile

FLAT = "half4 main(float2 p) { return half4(ink.rgb * level, 1); }"

SCENE = """from sigil import material
from sigil.compose import box
from sigil.sketch import sketch


@sketch(size=(8, 8), background="#000000", captureAt=0)
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

    def test_color_strings_declare_colors_and_numeric_sequences_declare_vectors(self):
        explicit = material.shader(
            FLAT, {"ink": material.Color(0, 0, 1, 1), "level": 1.0}
        )
        self.assertEqual(
            explicit, material.shader(FLAT, {"ink": "#0000ff", "level": 1.0})
        )
        self.assertNotEqual(
            explicit, material.shader(FLAT, {"ink": (0, 0, 1, 1), "level": 1.0})
        )

    def test_a_field_is_written_through_the_material_by_name(self):
        tone = {"ink": material.Color(1, 0, 0, 1), "level": 0.5}
        moved = material.shader(FLAT, tone)
        moved.set("level", 0.8)
        self.assertNotEqual(moved, material.shader(FLAT, tone))

    def test_color_bindings_accept_typed_constants_and_motion_values(self):
        factories = (
            lambda: material.shader(
                FLAT, {"ink": material.Color(1, 0, 0, 1), "level": 1.0}
            ),
            lambda: material.Paint.sksl(
                "uniform float4 ink; uniform float level; " + FLAT,
                {"ink": (1, 0, 0, 1), "level": 1.0},
            ),
        )
        for make in factories:
            for value in (
                material.Color(0, 0, 1, 1),
                motion.ColorTween(to="#0000ff"),
                motion.animatable("#0000ff"),
            ):
                with self.subTest(factory=make, value=value):
                    paint = make().bind("ink", value).bind("level", 0.5)
                    copied = paint.copy()
                    self.assertEqual(paint, copied)
                    self.assertEqual(
                        paint.isRunning(), isinstance(value, motion.ColorAnimatable)
                    )
                    self.assertEqual(copied.isRunning(), paint.isRunning())
        for value in (motion.ColorTween(to="#0000ff"), motion.animatable("#0000ff")):
            with self.subTest(initializer=value):
                paint = material.shader(
                    FLAT, {"ink": value, "level": motion.Tween(to=0.5)}
                )
                self.assertEqual(
                    paint.isRunning(), isinstance(value, motion.ColorAnimatable)
                )

    def test_a_bound_color_is_read_in_every_color_spelling(self):
        factories = (
            lambda: material.shader(
                FLAT, {"ink": material.Color(1, 0, 0, 1), "level": 1.0}
            ),
            lambda: material.Paint.sksl(
                "uniform float4 ink; uniform float level; " + FLAT,
                {"ink": (1, 0, 0, 1), "level": 1.0},
            ),
        )
        for make in factories:
            # Copies of one value share its program, so what differs
            # between them is only what was bound.
            base = make()
            expected = base.copy().bind("ink", material.Color(0, 0, 1, 1))
            for value in ("#0000ff", (0, 0, 1), [0, 0, 1, 1]):
                with self.subTest(factory=make, value=value):
                    bound = base.copy().bind("ink", value)
                    self.assertEqual(bound, expected)
                    self.assertFalse(bound.isRunning())
            # A number is still a number, whichever form it is bound in.
            with self.subTest(factory=make, value="number"):
                self.assertEqual(
                    base.copy().bind("level", 0.5), base.copy().bind("level", 0.5)
                )
                self.assertNotEqual(
                    base.copy().bind("level", 0.5), base.copy().bind("level", 0.25)
                )

    def test_a_copied_color_binding_tracks_its_cell_without_redescribing(self):
        code = """
            from sigil import compose, material, motion
            from sigil.sketch import sketch

            @sketch(size=(8, 8), background="#000000", captureAt=0)
            class Scene:
                def setup(self, ctx):
                    self.tint = motion.animatable("#ff0000")
                    self.level = motion.animatable(1.0)
                    mode = MODE
                    body = "half4 main(float2 p) { return half4(ink.rgb * level, 1); }"
                    if mode == "initializer":
                        paint = material.shader(body, {"ink": self.tint,
                                                      "level": self.level})
                    elif mode == "material":
                        paint = material.shader(body, {"ink": material.Color(1, 0, 0, 1),
                                                      "level": 1.0})
                        paint.bind("ink", self.tint)
                    else:
                        source = "uniform float4 ink; uniform float level; " + body
                        uniforms = {"ink": (1, 0, 0, 1), "level": 1.0}
                        if mode == "dictionary":
                            uniforms["ink"] = self.tint
                        paint = material.Paint.sksl(source, uniforms)
                        if mode == "bind":
                            paint.bind("ink", self.tint)
                        elif mode == "set":
                            paint.set("ink", self.tint)
                    if mode != "initializer":
                        paint.bind("level", self.level)
                    ctx.render(compose.box().width(8).height(8)
                               .cache(compose.Cache.Texture).fill(paint.copy()))

                def update(self, elapsed):
                    self.tint.set("#ff0000" if elapsed < .5 else "#0000ff")
                    self.level.set(1.0 if elapsed < .5 else .5)
        """
        with tempfile.TemporaryDirectory(prefix="sigil_uniform_color_") as folder:
            source, output = Path(folder) / "scene.py", Path(folder) / "scene.png"
            reference = {}
            for mode in ("material", "initializer", "bind", "set", "dictionary"):
                source.write_text(textwrap.dedent(code).replace("MODE", repr(mode)))
                for seconds, expected in (
                    (0.25, (255, 0, 0, 255)),
                    (1, (0, 0, 128, 255)),
                ):
                    with self.subTest(mode=mode, seconds=seconds):
                        renderFile(source, output, at=seconds)
                        pixels = media.load(output).frameAt(0).image.rgba()
                        if mode == "material":
                            reference[seconds] = bytes(pixels)
                        else:
                            self.assertEqual(bytes(pixels), reference[seconds])
                        offset = (4 * 8 + 4) * 4
                        for actual, channel in zip(
                            pixels[offset : offset + 4], expected
                        ):
                            self.assertAlmostEqual(actual, channel, delta=1)

    def test_it_paints_what_its_body_returns(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "scene.py"
            output = Path(folder) / "scene.png"
            source.write_text(SCENE.format(source=FLAT))
            renderFile(source, output, at=0)
            pixels = media.load(output).frameAt(0).image.rgba()
            offset = (4 * 8 + 4) * 4
            self.assertEqual(
                bytes(pixels[offset : offset + 4]), bytes([0, 0, 255, 255])
            )

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


LAYER = """from sigil import material
from sigil.compose import box
from sigil.sketch import sketch

SHIFT = material.shader(
    "half4 main(float2 p) { return content.eval(p + float2(4, 0)); }",
    textures={"content": None},
)


@sketch(size=(32, 8), background="#000000", captureAt=0)
class Scene:
    def setup(self, ctx):
        mark = box().width(8).height(8).left(12).fill("#ff0000")
        ctx.render(box().width(32).height(8).filter(FILTER).children(mark))
"""


class LayerPrograms(unittest.TestCase):
    """A filter's program over the layer the executor supplies."""

    def render(self, filter_spelling):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "scene.py"
            output = Path(folder) / "scene.png"
            source.write_text(LAYER.replace("FILTER", filter_spelling))
            renderFile(source, output, at=0)
            return media.load(output).frameAt(0).image.rgba()

    def red(self, pixels, x):
        return pixels[(4 * 32 + x) * 4]

    def test_an_empty_slot_is_the_layer_and_a_reach_is_stated(self):
        plain = self.render("material.Filter.of(SHIFT)")
        reaching = self.render("material.Filter.of(SHIFT, sampleRadius=4)")
        # The mark stands at 12 to 20; read four pixels to the right, it
        # lands at 8 to 16.
        for pixels in (plain, reaching):
            self.assertGreater(self.red(pixels, 10), 200)
            self.assertLess(self.red(pixels, 18), 50)
        self.assertEqual(bytes(plain), bytes(reaching))

    def test_a_raw_program_states_a_reach_and_captures_the_same_twice(self):
        raw = (
            'material.Filter.program("uniform shader content; '
            'half4 main(float2 p) { return content.eval(p + float2(4, 0)); }", '
            "sampleRadius=4)"
        )
        first = self.render(raw)
        # The mark stands at 12 to 20; read four pixels to the right, it
        # lands at 8 to 16, and the pixels past the layer's right edge are
        # handed over defined, so a second capture is byte for byte the
        # first.
        self.assertGreater(self.red(first, 10), 200)
        self.assertLess(self.red(first, 18), 50)
        self.assertEqual(bytes(first), bytes(self.render(raw)))
        self.assertEqual(bytes(first), bytes(self.render("material.Filter.of(SHIFT, sampleRadius=4)")))
        with self.assertRaises(ValueError):
            material.Filter.program(
                "uniform shader content; half4 main(float2 p) "
                "{ return content.eval(p); }",
                sampleRadius=-1,
            )

    def test_a_reach_is_a_distance_and_a_texture_is_a_picture(self):
        program = material.shader(
            "half4 main(float2 p) { return content.eval(p); }",
            textures={"content": None},
        )
        self.assertIsInstance(
            material.Filter.of(program, sampleRadius=0), material.Filter
        )
        with self.assertRaises(ValueError):
            material.Filter.of(program, sampleRadius=-1)
        with self.assertRaises(TypeError):
            material.shader(
                "half4 main(float2 p) { return content.eval(p); }",
                textures={"content": 3},
            )


class StockPrograms(unittest.TestCase):
    def test_glass_refracts_over_the_options_it_is_given(self):
        options = material.GlassOptions(
            ior=1.33,
            thickness=24,
            sampleRadius=16,
            normal=material.Material("#8080ff"),
            normalDirectX=True,
        )
        self.assertAlmostEqual(options.ior, 1.33, places=5)
        self.assertEqual(options.thickness, 24)
        self.assertEqual(options.sampleRadius, 16)
        self.assertEqual(options.normal, material.Material("#8080ff"))
        self.assertTrue(options.normalDirectX)
        self.assertIsNone(material.GlassOptions().normal)
        copied = options.copy()
        copied.ior = 2
        self.assertAlmostEqual(options.ior, 1.33, places=5)
        with self.assertRaises(TypeError):
            material.GlassOptions(refraction=1.5)
        glass = material.Filter.glass(options)
        self.assertIsInstance(glass, material.Filter)
        self.assertIsInstance(material.Filter.glass(), material.Filter)
        # The scalar names the filter states take a set and a bind.
        self.assertIs(glass.set("ior", 1.5), glass)
        self.assertIs(glass.bind("thickness", motion.animatable(12.0)), glass)
        self.assertTrue(glass.isRunning())
        # A negative sampling ceiling is refused, and no filter stands.
        self.assertTrue(
            material.Filter.glass(material.GlassOptions(sampleRadius=-1)).isNone()
        )

    def test_normals_are_made_from_a_height_and_reoriented_into_a_base(self):
        height = material.Material("#808080")
        steps = material.surface.HeightNormalOptions(depth=-2, step=0.5, directX=True)
        self.assertEqual(steps.depth, -2)
        self.assertEqual(steps.step, 0.5)
        self.assertTrue(steps.directX)
        self.assertEqual(steps, steps.copy())
        self.assertNotEqual(steps, material.surface.HeightNormalOptions())
        flat = material.surface.normalFromHeight(height)
        dented = material.surface.normalFromHeight(height, steps)
        self.assertIsInstance(flat, material.Material)
        self.assertNotEqual(dented, flat)
        conventions = material.surface.NormalBlendOptions(detailDirectX=True)
        self.assertFalse(conventions.baseDirectX)
        self.assertTrue(conventions.detailDirectX)
        self.assertFalse(conventions.outputDirectX)
        self.assertEqual(conventions, conventions.copy())
        with self.assertRaises(TypeError):
            material.surface.NormalBlendOptions(directX=True)
        blended = material.surface.blendNormals(flat, dented, conventions)
        self.assertIsInstance(blended, material.Material)
        self.assertIsInstance(
            material.surface.blendNormals(base=flat, detail=dented), material.Material
        )


SHEET = """from sigil import material, media
from sigil.compose import box
from sigil.sketch import sketch

# Two by two cells of four pixels: red, green, blue and white.
CELLS = ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255))


def sheet():
    pixels = bytearray()
    for y in range(8):
        for x in range(8):
            pixels += bytes(CELLS[(y // 4) * 2 + x // 4] + (255,))
    return material.Texture(media.fromRgba(bytes(pixels), 8, 8)).sampling(
        material.Sampling.Nearest
    )


@sketch(size=(8, 8), background="#000000", captureAt=0)
class Scene:
    def setup(self, ctx):
        options = material.FlipbookOptions(columns=2, rows=2, framesPerSecond=0, frame=FRAME)
        ctx.render(box().width(8).height(8).fill(material.flipbook(sheet(), options)))
"""


def blank_texture(width=4, height=4):
    return material.Texture(media.fromRgba(bytes(width * height * 4), width, height))


class Texture(unittest.TestCase):
    def test_a_texture_states_its_sampling_and_reads_it_back(self):
        plain = blank_texture()
        self.assertTrue(plain.valid())
        self.assertEqual(tuple(plain.size()), (4, 4))
        self.assertEqual(plain.tileX(), material.Repeat.Pad)
        self.assertIsNone(plain.region())
        self.assertIsNone(plain.colorSpace())
        self.assertIsNone(plain.mipmaps())
        self.assertEqual(plain.sampling(), material.Sampling.Linear)
        cut = (
            plain.copy()
            .tile(material.Repeat.Repeat)
            .region(material.PixelRect(x=1, y=1, width=2, height=2))
            .sampling(material.Sampling.Nearest)
            .colorSpace(media.ColorSpace.Data)
            .mipmaps(False)
        )
        self.assertEqual(cut.tileY(), material.Repeat.Repeat)
        self.assertEqual(cut.region(), material.PixelRect(x=1, y=1, width=2, height=2))
        self.assertEqual(tuple(cut.size()), (2, 2))
        self.assertEqual(cut.colorSpace(), media.ColorSpace.Data)
        self.assertFalse(cut.mipmaps())
        self.assertEqual(cut, cut.copy())
        self.assertNotEqual(cut, plain)
        self.assertFalse(material.Texture().valid())

    def test_a_flipbook_records_every_field_and_binds_a_frame(self):
        vectors = blank_texture()
        options = material.FlipbookOptions(
            columns=4,
            rows=2,
            frames=6,
            framesPerSecond=12,
            loop=False,
            blendFrames=True,
            startFrame=3,
            frame=motion.animatable(2.5),
            frameAttribute="cell",
            motionVectors=vectors,
            motionStrength=0.5,
        )
        self.assertEqual((options.columns, options.rows, options.frames), (4, 2, 6))
        self.assertFalse(options.loop)
        self.assertTrue(options.blendFrames)
        self.assertEqual(options.frameAttribute, "cell")
        self.assertEqual(options.motionVectors, vectors)
        self.assertIsNotNone(options.frame)
        self.assertIsNone(options.progress)
        options.progress = 0.5
        self.assertIsNotNone(options.progress)
        options.frame = None
        self.assertIsNone(options.frame)
        with self.assertRaises(TypeError):
            material.FlipbookOptions(cells=4)
        sheet = blank_texture()
        self.assertEqual(
            material.flipbook(sheet, material.FlipbookOptions(columns=2)),
            material.flipbook(sheet, material.FlipbookOptions(columns=2)),
        )
        self.assertNotEqual(material.instanceAttributeKey("cell"), 0)
        self.assertEqual(
            material.instanceAttributeKey("cell"), material.instanceAttributeKey("cell")
        )

    def test_an_image_and_a_flow_map_are_materials(self):
        flow = blank_texture().colorSpace(media.ColorSpace.Data)
        source = material.image(blank_texture())
        self.assertIsInstance(source, material.Material)
        tiled = material.image(
            media.fromRgba(bytes(16), 2, 2),
            material.ImageOptions(repeat=material.Repeat.Repeat),
        )
        self.assertIsInstance(tiled, material.Material)
        options = material.FlowmapOptions(flow=flow, strength=0.2, speed=0.25)
        self.assertEqual(options.flow, flow)
        self.assertIsInstance(material.flowmap(source, options), material.Material)

    def test_a_flipbook_shows_the_cell_its_frame_names(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "scene.py"
            output = Path(folder) / "scene.png"
            for frame, expected in (
                (0, (255, 0, 0)),
                (2, (0, 0, 255)),
                (3, (255, 255, 255)),
            ):
                with self.subTest(frame=frame):
                    source.write_text(SHEET.replace("FRAME", str(frame)))
                    renderFile(source, output, at=0)
                    pixels = media.load(output).frameAt(0).image.rgba()
                    offset = (4 * 8 + 4) * 4
                    for actual, channel in zip(pixels[offset : offset + 3], expected):
                        self.assertAlmostEqual(actual, channel, delta=1)

    def test_a_flipbook_plays_its_cells_with_the_clock(self):
        # One frame a second and no frame stated: the clock chooses, so
        # half a second in shows the first cell and two and a half the
        # third.
        playing = SHEET.replace("framesPerSecond=0, frame=FRAME", "framesPerSecond=1")
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "scene.py"
            output = Path(folder) / "scene.png"
            source.write_text(playing)
            for seconds, expected in ((0.5, (255, 0, 0)), (2.5, (0, 0, 255))):
                with self.subTest(seconds=seconds):
                    renderFile(source, output, at=seconds)
                    pixels = media.load(output).frameAt(0).image.rgba()
                    offset = (4 * 8 + 4) * 4
                    for actual, channel in zip(pixels[offset : offset + 3], expected):
                        self.assertAlmostEqual(actual, channel, delta=1)


if __name__ == "__main__":
    unittest.main()
