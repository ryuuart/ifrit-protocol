"""Cross-library authoring contracts: owned values, native results and typed inputs."""

import concurrent.futures
import gc
import inspect
import json
import tempfile
import unittest
from pathlib import Path

from sigil import data, image, io, material, weave
from sigil.compose import Annotation, text
from sigil.sketch import render_file


class LibraryCoverage(unittest.TestCase):
    def test_database_creation_writes_missing_cells_and_detached_results(self):
        database = data.Database.memory()
        rows = data.Table()
        rows.add("name", ("fern", "moss"))
        rows.add("height", (42, None))
        database.insert("plants", rows)
        database.execute("UPDATE plants SET height = 4 WHERE name = 'moss'")
        result = database.query("SELECT * FROM plants ORDER BY name")
        self.assertEqual(result.column("height").values(), [42, 4])
        self.assertEqual(rows.column("height").values(), [42, None])
        with self.assertRaises(RuntimeError):
            database.execute("not a SQL statement")
        del database
        gc.collect()
        self.assertEqual(result.column("name").values(), ["fern", "moss"])

    def test_codecs_use_python_values_and_native_wire_formats(self):
        packet = data.encodeOsc(address="/level", arguments=(0.5, True, "hello"))
        message = data.decodeOsc(packet).to_python()
        self.assertEqual(message["address"], "/level")
        self.assertEqual(message["arguments"], [0.5, True, "hello"])
        self.assertEqual(data.encodeOsc(message=message), packet)
        midi = data.encodeMidi(
            {"kind": "NoteOn", "channel": 1, "note": 60, "velocity": 0}
        )
        self.assertEqual(midi, b"\x90\x3c\x00")
        self.assertEqual(data.decodeMidi(midi)["kind"].text(), "NoteOff")
        dmx = data.decodeArtNet(
            data.encodeArtNet({"kind": "Dmx", "channels": [255, 64, 8]})
        )
        self.assertEqual(dmx["channels"].to_python(), [255, 64, 8, 0])
        for decoder in (data.decodeOsc, data.decodeMidi, data.decodeArtNet):
            self.assertIsNone(decoder(b""))

    def test_schema_owns_its_bytes_and_verifies_both_directions(self):
        fixture = Path(__file__).with_name("assets") / "coverage.bfbs"
        schema = data.Schema.fromBinarySchema(fixture.read_bytes())
        self.assertTrue(schema)
        self.assertEqual(schema.rootName(), "coverage.Reading")
        binary = schema.binary('{"label":"humidity","value":42.5}')
        self.assertEqual(
            json.loads(schema.text(binary)), {"label": "humidity", "value": 42.5}
        )
        with self.assertRaises(ValueError):
            schema.text(binary[:4])
        with self.assertRaises(ValueError):
            schema.binary('{"unknown":42}')
        with self.assertRaises(ValueError):
            data.Schema.fromBinarySchema(b"broken")

    def test_owned_hub_loads_data_and_context_manages_residency(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sample.csv").write_text("name,value\na,2\nb,3\n")
            hub = io.Hub()
            data.registerDecoders(hub)
            hub.mount("res://", root)
            self.assertEqual(hub.preload(("res://sample.csv",)), 1)
            with hub.retain(("res://*.csv",)) as lease:
                table = hub.load(data.Table, "res://sample.csv")
                self.assertEqual(lease.uris(), ["res://sample.csv"])
                self.assertEqual(hub.discardUnretained(), 0)
                self.assertEqual(table.column("value").values(), [2, 3])
                with (
                    concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool,
                    self.assertRaisesRegex(RuntimeError, "thread"),
                ):
                    pool.submit(lease.close).result()
            with self.assertRaisesRegex(RuntimeError, "closed"):
                lease.refresh()
            self.assertGreaterEqual(hub.discardUnretained(), 1)
            self.assertIsNone(hub.load(data.Table, "res://missing.csv"))
            with self.assertRaises(TypeError):
                hub.load(str, "res://sample.csv")
        self.assertEqual(table.size(), 2)

    def test_palette_color_spaces_and_python_iteration(self):
        self.assertEqual(material.Oklch(L=0.5, chroma=0.1, hueDegrees=20).alpha, 1)
        color = material.Color("#6e99bb")
        restored = material.fromOklab(material.toOklab(color))
        for left, right in zip(color, restored):
            self.assertAlmostEqual(left, right, places=5)
        palette = material.harmony(color, material.Scheme.Triad)
        self.assertEqual(len(palette), 3)
        self.assertEqual(palette[-1], list(palette)[-1])
        with self.assertRaises(IndexError):
            _ = palette[len(palette)]
        self.assertEqual(palette.at(500), palette[-1])
        palette.entries = (c for c in ("#ff0000", "#0000ff"))
        ramp = material.Ramp(
            stops=(material.RampStop(0, palette[0]), material.RampStop(1, palette[1])),
            space=material.RampSpace.Linear,
        )
        middle = ramp(0.5)
        self.assertGreater(middle.r, 0.7)
        self.assertGreater(middle.b, 0.7)
        self.assertEqual(material.palette(ramp, 5).size(), 5)
        self.assertEqual(material.closestEntry(palette, "#ff0000"), 0)
        self.assertEqual(
            material.palette(
                ("#ff0000", "#0000ff") * 10, material.PaletteOptions(entries=2)
            ).size(),
            2,
        )
        with self.assertRaises(ValueError):
            material.rampBracket((), 0.5)
        self.assertIsNotNone(text("native color", color=middle))

    def test_paragraph_editing_measurements_and_font_thread_ownership(self):
        fonts = weave.FontContext()
        paragraph = weave.Paragraph(
            "A paragraph with several words. Another sentence.", weave.Type(size=20)
        )
        flow = weave.BlockFlow((0, 0, 180, 400))
        layout = weave.layoutParagraph(fonts, paragraph, flow)
        self.assertGreater(layout.lineCount, 1)
        self.assertFalse(layout.overflowed())
        metrics = layout.lineMetrics(paragraph)
        self.assertTrue(all(m.right <= 181 for m in metrics))
        self.assertFalse(layout.glyphOutline().isEmpty())
        revision = paragraph.revision()
        paragraph.replaceText(start=0, end=1, text="The")
        self.assertEqual(paragraph.editsSince(revision)[0].inserted, 3)
        with self.assertRaisesRegex(ValueError, "Rebuild"):
            layout.lineMetrics(paragraph)
        self.assertFalse(layout.glyphOutline().isEmpty())
        with (
            concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool,
            self.assertRaisesRegex(RuntimeError, "thread"),
        ):
            pool.submit(fonts.stats).result()
        del fonts, paragraph, layout
        gc.collect()
        self.assertGreater(metrics[0].right, 0)

    def test_queries_use_native_utf16_ranges_and_markers_follow_edits(self):
        paragraph = weave.Paragraph("😀 fern fern", weave.Type(size=18))
        ranges = weave.findAllOccurrences(paragraph, "fern")
        self.assertEqual([(r.start, r.end) for r in ranges], [(3, 7), (8, 12)])
        markers = weave.MarkerSet(paragraph)
        markers.setRanges("species", ranges)
        paragraph.replaceText(0, 0, "a ")
        self.assertTrue(markers.synchronize(paragraph))
        self.assertEqual(markers.rangesFor("species")[0].start, 5)
        with self.assertRaises(ValueError):
            weave.findRegexMatches(paragraph, "[")

    def test_exclusion_flows_and_readings_are_native_geometry(self):
        fonts = weave.FontContext()
        paragraph = weave.Paragraph(
            "One two three four five six seven eight nine. " * 4, weave.Type(size=18)
        )
        flow = weave.ExclusionFlow((0, 0, 280, 400))
        flow.exclusions = (
            weave.Exclusion(
                shape=weave.flowshape.rectangle((0, 0, 100, 100)), margin=8
            ),
        )
        layout = weave.layoutParagraph(fonts, paragraph, flow)
        self.assertGreater(layout.lineMetrics(paragraph)[0].left, 100)
        reading = weave.Paragraph("reading", weave.Type(size=10))
        note = weave.layoutBeside(
            fonts, reading, weave.Beside(base=(0, 60, 80, 24), gap=3)
        )
        self.assertLess(note.lineMetrics(reading)[0].rect().bottom(), 61)
        annotation = Annotation(
            readings=("かん", "じ"), style=weave.Type(size=weave.em(0.5))
        )
        self.assertEqual(annotation.readings, ["かん", "じ"])
        self.assertIsNotNone(text("漢字").textAnnotation(annotation))

    def test_bound_values_name_the_module_an_author_imports_them_from(self):
        from sigil import compose, draw, geometry, motion, skia, world
        from sigil.compose import kit as compose_kit
        from sigil.sketch import kit as sketch_kit

        for module in (
            compose,
            compose_kit,
            data,
            draw,
            geometry,
            image,
            io,
            material,
            motion,
            sketch_kit,
            skia,
            weave,
            world,
        ):
            for name in module.__all__:
                value = getattr(module, name)
                # A submodule and a type alias have no module of their own:
                # one is named where it is published, the other reports the
                # typing machinery that built it.
                if not isinstance(value, type) and not inspect.isroutine(value):
                    continue
                owner = value.__module__
                self.assertTrue(
                    owner == module.__name__ or owner.startswith("sigil."),
                    f"{module.__name__}.{name} reports {owner}",
                )
        # The Skia backend of the material catalogue stands in the
        # catalogue, and a colour is one class wherever it is read.
        self.assertEqual(material.Paint.__module__, "sigil.material")
        self.assertEqual(material.Color.__module__, "sigil.material")
        self.assertFalse(hasattr(material, "skia"))
        self.assertEqual(compose.Element.__module__, "sigil.compose")
        self.assertEqual(compose_kit.Well.__module__, "sigil.compose.kit")
        self.assertEqual(sketch_kit.Theme.__module__, "sigil.sketch.kit")

    def test_weave_layout_renders_headlessly_through_checked_pen(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "paragraph.py"
            output = Path(directory) / "paragraph.png"
            source.write_text("""from sigil import weave, material
from sigil.sketch import sketch

@sketch(size=(240, 100))
class ParagraphSketch:
    def setup(self):
        self.fonts = weave.FontContext()
        style = weave.textStyle(weave.Type(size=28, color="#ffffff"))
        style.paint.addUnderlay(weave.kit.glow("#00ffff", 2))
        self.paragraph = weave.Paragraph("Native Weave", style)
        self.layout = weave.layoutSingleLine(self.fonts, self.paragraph, (12, 50))
    def draw(self, pen):
        pen.background("#000000")
        self.layout.drawBatched(pen, self.paragraph)
""")
            render_file(source, output, at=0)
            pixels = image.load(output).rgba()
            self.assertGreater(sum(pixels[0::4]), 1000)


if __name__ == "__main__":
    unittest.main()
