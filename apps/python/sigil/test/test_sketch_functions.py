"""A file of functions is a sketch: no class and no decorator, the canvas
declared by calls, the readings written into the module before each hook,
and the input hooks called from the changes between two frames."""

import sys
import tempfile
import textwrap
import unittest
import warnings
from pathlib import Path

from sigil import _loader, media
from sigil.sketch import renderFile


class Context:
    """What a hook's context answers, recording what is declared on it."""

    elapsed = 0.0

    def __init__(self, width=960, height=640):
        self.width, self.height = width, height
        self.calls = []

    def canvas(self, width, height):
        self.width, self.height = width, height
        self.calls.append(("canvas", (width, height)))

    def background(self, color):
        self.calls.append(("background", color))

    def captureAt(self, seconds):
        self.calls.append(("capture", seconds))

    def render(self, element):
        self.calls.append(("render", element))


class Pen:
    """The readings a pen hands a draw."""

    def __init__(self, **readings):
        self.width, self.height = 320, 200
        self.frameCount = 1
        self.deltaTime = 16.0
        self.mouseX = self.mouseY = 0.0
        self.mouseIsPressed = False
        self.key, self.keyCode, self.keyIsPressed = "", 0, False
        self.at = 0.0
        self.__dict__.update(readings)

    def millis(self):
        return self.at * 1000


class SketchFunctions(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def write(self, text, name="entry.py"):
        path = self.root / name
        path.write_text(textwrap.dedent(text))
        return path

    def load(self, text):
        body, prefix = _loader.load(self.write(text))
        self.addCleanup(_loader.release, prefix)
        return body, sys.modules[body.__module__]

    def test_a_file_of_functions_loads_as_a_sketch_class(self):
        body, module = self.load("""
            from sigil.sketch.globals import *

            def draw(pen):
                pen.background("#000000")
        """)
        self.assertEqual(body.__sigil_kind__, "scene2d")
        self.assertIs(module.__sigil_sketch__, body)
        self.assertEqual(body.__module__, module.__name__)
        self.assertTrue(callable(body.draw))

    def test_size_declares_the_canvas_at_the_top_of_the_file(self):
        source = self.write("""
            from sigil.compose import box
            from sigil.sketch.globals import *

            size(48, 36)
            background("#000000")
            captureAt(0)

            def setup():
                return box().width(width).height(height).fill("#00ff00")
        """)
        output = self.root / "entry.png"
        renderFile(source, output)
        image = media.load(output).frameAt(0).image
        self.assertEqual((image.width(), image.height()), (48, 36))
        pixels = image.rgba()
        offset = (18 * 48 + 24) * 4
        self.assertEqual(bytes(pixels[offset : offset + 4]), bytes([0, 255, 0, 255]))

    def test_size_called_in_setup_declares_the_canvas_after_it(self):
        body, module = self.load("""
            from sigil.sketch.globals import *

            def setup():
                size(200, 100)
                background("#102030")

            def draw(pen):
                pass
        """)
        ctx = Context()
        body().setup(ctx)
        self.assertEqual(
            ctx.calls[-2:], [("canvas", (200, 100)), ("background", "#102030")]
        )
        self.assertEqual((module.width, module.height), (200, 100))

    def test_a_capture_moment_may_be_a_module_constant(self):
        body, _ = self.load("""
            from sigil.sketch.globals import *

            captureAt = 2.5

            def draw(pen):
                pass
        """)
        ctx = Context()
        body().setup(ctx)
        self.assertIn(("capture", 2.5), ctx.calls)

    def test_a_declaration_inside_a_running_hook_is_refused(self):
        body, _ = self.load("""
            from sigil.sketch.globals import *

            def draw(pen):
                background("#ffffff")
        """)
        instance, ctx = body(), Context()
        instance.setup(ctx)
        with self.assertRaisesRegex(RuntimeError, "pen.background"):
            instance.draw(Pen(), ctx)

    def test_the_readings_are_written_into_the_module_before_each_draw(self):
        body, module = self.load("""
            from sigil.sketch.globals import *

            seen = []

            def draw(pen):
                seen.append((width, height, frameCount, seconds, deltaTime,
                             mouseX, mouseY, mouseIsPressed, key, keyCode,
                             keyIsPressed))
        """)
        instance, ctx = body(), Context()
        instance.setup(ctx)
        instance.draw(Pen(), ctx)
        instance.draw(
            Pen(
                frameCount=2,
                at=0.5,
                mouseX=12.0,
                mouseY=34.0,
                mouseIsPressed=True,
                key="a",
                keyCode=65,
                keyIsPressed=True,
            ),
            ctx,
        )
        self.assertEqual(
            module.seen[0], (320, 200, 1, 0.0, 16.0, 0.0, 0.0, False, "", 0, False)
        )
        self.assertEqual(
            module.seen[1], (320, 200, 2, 0.5, 16.0, 12.0, 34.0, True, "a", 65, True)
        )

    def test_a_describe_reads_the_time_and_counts_its_steps(self):
        body, module = self.load("""
            from sigil.compose import box
            from sigil.sketch.globals import *

            size(80, 60)
            seen = []

            def describe(seconds):
                seen.append((width, height, frameCount, deltaTime))
                return box()
        """)
        instance = body()
        instance.describe(0.25)
        instance.describe(0.5)
        self.assertEqual(module.seen, [(80, 60, 1, 250.0), (80, 60, 2, 250.0)])

    def test_the_input_hooks_fire_from_the_changes_between_frames(self):
        body, module = self.load("""
            from sigil.sketch.globals import *

            heard = []

            def draw(pen):
                heard.append("draw")

            def mousePressed():
                heard.append(("pressed", mouseX, mouseY))

            def mouseReleased():
                heard.append("released")

            def mouseMoved():
                heard.append("moved")

            def mouseDragged():
                heard.append("dragged")

            def keyPressed():
                heard.append(("key", key))

            def keyReleased():
                heard.append("key up")
        """)
        instance, ctx = body(), Context()
        instance.setup(ctx)
        for pen in (
            Pen(mouseX=5, mouseY=5),
            Pen(mouseX=6, mouseY=5),
            Pen(mouseX=6, mouseY=5, mouseIsPressed=True),
            Pen(mouseX=9, mouseY=7, mouseIsPressed=True),
            Pen(mouseX=9, mouseY=7),
            Pen(mouseX=9, mouseY=7, key="b", keyCode=66, keyIsPressed=True),
            Pen(mouseX=9, mouseY=7, key="b", keyCode=66),
        ):
            instance.draw(pen, ctx)
        self.assertEqual(
            module.heard,
            [
                "draw",
                "moved",
                "draw",
                ("pressed", 6, 5),
                "draw",
                "dragged",
                "draw",
                "released",
                "draw",
                ("key", "b"),
                "draw",
                "key up",
                "draw",
            ],
        )

    def test_a_function_redefined_while_the_sketch_runs_is_the_one_called(self):
        body, module = self.load("""
            from sigil.sketch.globals import *

            heard = []

            def draw(pen):
                heard.append("first")
        """)
        instance, ctx = body(), Context()
        instance.setup(ctx)
        instance.draw(Pen(), ctx)

        def draw(pen):
            module.heard.append("second")

        module.draw = draw
        instance.draw(Pen(), ctx)
        self.assertEqual(module.heard, ["first", "second"])

    def test_text_run_inside_asks_again_only_for_a_tree_setup_returned(self):
        stateful, _ = self.load("""
            balls = []

            def setup():
                balls.append(1)

            def draw(pen):
                pass
        """)
        stateful().setup(Context())
        self.assertFalse(stateful.__sigil_redescribes__)
        declarative, _ = self.load("""
            from sigil.compose import box

            def setup():
                return box()
        """)
        declarative().setup(Context())
        self.assertTrue(declarative.__sigil_redescribes__)

    def test_the_kind_is_read_from_the_describe_annotation(self):
        body, _ = self.load("""
            from sigil import world
            from sigil.sketch.globals import *

            def describe(seconds) -> world.Frame:
                return world.Frame(world.group())
        """)
        self.assertEqual(body.__sigil_kind__, "scene3d")

    def test_a_scene3d_file_has_no_draw(self):
        with self.assertRaisesRegex(TypeError, "no update or draw"):
            self.load("""
                from sigil import world

                def describe(seconds) -> world.Frame:
                    return world.Frame(world.group())

                def draw(pen):
                    pass
            """)

    def test_a_file_with_a_class_and_functions_runs_the_class_and_says_so(self):
        with warnings.catch_warnings(record=True) as caught:
            warnings.simplefilter("always")
            body, _ = self.load("""
                from sigil.sketch import sketch

                @sketch()
                class Study:
                    def draw(self, pen):
                        pass

                def draw(pen):
                    pass
            """)
        self.assertEqual(body.__name__, "Study")
        self.assertTrue(
            any("the class is the sketch" in str(warning.message) for warning in caught)
        )

    def test_a_file_with_neither_is_refused_by_name(self):
        with self.assertRaisesRegex(TypeError, "draw\\(pen\\)"):
            self.load("""
                def paint(pen):
                    pass
            """)

    def test_a_describe_with_another_signature_is_refused_by_name(self):
        with self.assertRaisesRegex(TypeError, "describe\\(seconds\\)"):
            self.load("""
                def describe():
                    return None
            """)


if __name__ == "__main__":
    unittest.main()
