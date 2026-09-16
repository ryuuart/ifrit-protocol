"""Natural-media data, native execution, and Python callback ownership."""

import builtins
import gc
import math
import struct
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil.draw import brush
from sigil.sketch import render_file


class Brush(unittest.TestCase):
    def test_tools_keep_native_fields_and_nested_mutations(self):
        tool = brush.marker("#804020", 24)
        tool.pressure = brush.Pressure(0.2, 1.0, 0.1)
        tool.pressure.variation = None
        tool.pressure.middle = 0.75
        self.assertAlmostEqual(tool.pressure.at(0.5), 0.75)
        tool.shape = brush.Shape(spacing=0.2, scatter=0.1)
        shape = tool.shape
        tool.shape = None
        shape.spacing = 0.125
        tool.shape = shape
        self.assertAlmostEqual(brush.spacingOf(tool), 3.0)
        tool.dynamics.size = brush.Response(
            drive=brush.Drive.Pressure,
            curve=brush.Curve(minimum=0.25, maximum=1.0),
        )
        self.assertFalse(tool.dynamics.empty())
        copied = tool.copy()
        copied.width = 10
        self.assertAlmostEqual(tool.width, 24)
        self.assertAlmostEqual(copied.width, 10)
        with self.assertRaisesRegex(TypeError, "Unknown brush property"):
            brush.Tool(wdith=2)

    def test_pressure_and_response_curves_accept_python_callables(self):
        pressure = brush.Pressure(curve=lambda t: t * t, variation=None)
        self.assertAlmostEqual(pressure.at(0.5), 0.25)
        curve = brush.Curve(curve=lambda t: 1.0 - t)
        self.assertAlmostEqual(curve.at(0.25), 0.75)
        response = brush.Response(drive=brush.Drive.Pressure, curve=curve)
        self.assertAlmostEqual(response.at(brush.Dab(), 0.25, 100), 0.75)
        pressure.curve = None
        self.assertAlmostEqual(pressure.at(0.5), 1.0)
        with self.assertRaises(TypeError):
            brush.Curve(curve=3)

    def test_stored_and_live_paths_keep_pressure_and_progress(self):
        segment = brush.segment((0, 0), (10, 0), 2, 0.2, 0.8)
        self.assertAlmostEqual(segment[0].pressure, 0.2)
        self.assertAlmostEqual(segment[-1].pressure, 0.8)
        self.assertAlmostEqual(segment[-1].position.x, 10)
        controls = [(0, 0, 0.1), ((5, 4), 0.8), brush.Sample((10, 0), 0.3)]
        spline = brush.spline(controls, spacing=1, curvature=0.6)
        self.assertGreater(len(spline), len(controls))
        events = [brush.Input((0, 0), seconds=0), brush.Input((10, 0), seconds=1)]
        dabs = brush.dabs(events, 2)
        self.assertEqual(dabs[0].progress, 0)
        self.assertEqual(dabs[-1].progress, 1)
        self.assertAlmostEqual(dabs[-1].position.x, 10)
        sampler = brush.Sampler()
        self.assertEqual(sampler.begin(events[0]), [])
        self.assertTrue(sampler.active())
        self.assertGreater(len(sampler.move(events[1], 2)), 0)
        sampler.end(events[1], 2)
        self.assertFalse(sampler.active())

    def test_fields_polygons_and_plots_remain_native_geometry(self):
        a, b = brush.Curl(seed=42), brush.Curl(seed=42)
        self.assertEqual(a((20, 35), 0.2), b((20, 35), 0.2))
        field = brush.Wave(direction=0, amplitude=0)
        path = brush.trace((0, 0), 10, 2, 0, field)
        self.assertAlmostEqual(path[-1].position.x, 10)
        self.assertAlmostEqual(path[-1].position.y, 0)
        custom = brush.Direction(lambda position, seconds: math.pi / 2)
        vertical = brush.trace((0, 0), 10, 2, 0, custom)
        self.assertAlmostEqual(vertical[-1].position.y, 10, places=5)
        polygon = brush.Polygon([(0, 0), (10, 0), (10, 10), (0, 10)])
        crossings = polygon.intersect(brush.Line((-5, 5), (15, 5)))
        self.assertEqual(len(crossings), 2)
        self.assertAlmostEqual(crossings[0].x, 0)
        self.assertAlmostEqual(crossings[1].x, 10)
        plot = brush.Plot(brush.PlotType.Segments)
        plot.addSegment(0, 10, 0.5)
        plot.endPlot(0)
        self.assertAlmostEqual(plot.length(), 10)
        self.assertAlmostEqual(plot.path((5, 5))[-1].position.x, 15)
        self.assertIn("waves", brush.stockFields())

    def test_engine_selection_and_state_are_instance_owned(self):
        first, second = brush.Engine(), brush.Engine()
        self.assertIn("HB", first.names())
        first.set("HB", "#123456", 2)
        before = first.tool().width
        first.push()
        first.strokeWeight(4)
        self.assertAlmostEqual(first.tool().width, before * 2)
        first.pop()
        self.assertAlmostEqual(first.tool().width, before)
        self.assertFalse(second.hasStroke())
        self.assertIsNone(first.pick("missing"))
        self.assertTrue(first.addField("down", brush.Wave(direction=math.pi / 2)))
        self.assertTrue(first.field("down"))
        self.assertNotIn("down", second.listFields())
        cursor = first.position(0, 0)
        self.assertGreater(len(cursor.moveTo(0, 10)), 0)
        self.assertAlmostEqual(cursor.plotted(), 10)

    def test_native_description_round_trip_keeps_parameters(self):
        source = brush.Tool(
            width=13, opacity=0.7, pressure=brush.Pressure(0.2, 0.8, 0.4)
        )
        source.grain = brush.Grain(scale=1.5, depth=0.7)
        description = brush.format.encodeBrush(source)
        restored = brush.format.decodeBrush(description.encode())
        self.assertIsInstance(restored, brush.Tool)
        self.assertAlmostEqual(restored.width, source.width)
        self.assertAlmostEqual(restored.opacity, source.opacity)
        self.assertAlmostEqual(restored.pressure.middle, source.pressure.middle)
        self.assertIsNone(brush.format.decodeBrush(b"not a brush"))

    def test_render_uses_native_deposition_and_invalidates_custom_tip_pen(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_brush_pen", None))
        self.addCleanup(lambda: builtins.__dict__.pop("_brush_calls", None))
        source = """
            import builtins
            from sigil.draw import brush
            from sigil.sketch import sketch
            builtins._brush_calls = 0
            @sketch(size=(160, 120), background="#f5eedc", capture_at=0)
            class Study:
                def draw(self, pen):
                    pen.noLoop()
                    def tip(mark, dab):
                        builtins._brush_calls += 1
                        builtins._brush_pen = mark
                        mark.circle(0, 0, 1)
                    ink = brush.marker("#a84826", 8)
                    ink.tip = brush.Tip.Custom
                    ink.customTip = tip
                    brush.line(pen, ink, (10, 20), (145, 20))
                    wash = brush.Wash(color="#40785d", layers=3, texture=0.1)
                    polygon = [(15, 40), (145, 40), (135, 100), (20, 95)]
                    brush.wash(pen, wash, polygon)
                    engine = brush.Engine()
                    engine.set("HB", "#20382f", 0.8)
                    engine.hatch(brush.Hatch(spacing=8, angle=0.4))
                    engine.polygon(pen, polygon)
                    engine.beginInput(pen, brush.Input((10, 110), seconds=0))
                    engine.endInput(pen, brush.Input((145, 110), seconds=1))
        """
        with tempfile.TemporaryDirectory(prefix="sigil_brush_test_") as directory:
            entry, image = Path(directory) / "study.py", Path(directory) / "study.png"
            entry.write_text(textwrap.dedent(source))
            render_file(entry, image, at=0)
            content = image.read_bytes()
            self.assertEqual(content[:8], b"\x89PNG\r\n\x1a\n")
            self.assertEqual(struct.unpack(">II", content[16:24]), (160, 120))
        self.assertGreater(builtins._brush_calls, 0)
        with self.assertRaisesRegex(RuntimeError, "callback"):
            builtins._brush_pen.circle(0, 0, 1)

    def test_brush_bound_methods_do_not_keep_a_closed_sketch_alive(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_brush_owner", None))
        source = """
            import builtins
            import weakref
            from sigil.draw import brush
            from sigil.sketch import sketch
            @sketch(size=(32, 32), capture_at=0)
            class Study:
                def setup(self, ctx):
                    builtins._brush_owner = weakref.ref(self)
                    self.tool = brush.Tool(tip=brush.Tip.Custom)
                    self.tool.customTip = self.tip
                    self.tool.pressure.curve = self.pressure
                    self.engine = brush.Engine()
                    self.engine.addField("mine", self.field)
                def tip(self, pen):
                    pen.circle(0, 0, 1)
                def pressure(self, t):
                    return t
                def field(self, position, seconds):
                    return 0
        """
        with tempfile.TemporaryDirectory(prefix="sigil_brush_lifetime_") as directory:
            entry, image = Path(directory) / "study.py", Path(directory) / "study.png"
            entry.write_text(textwrap.dedent(source))
            render_file(entry, image, at=0)
        gc.collect()
        self.assertIsNone(builtins._brush_owner())


if __name__ == "__main__":
    unittest.main()
