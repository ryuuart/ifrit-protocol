"""A window in this interpreter: a tree, a sketch class and an attribute
assigned at the prompt each reach the next still; the prompt keeps the
window drawing while it waits; and importing the package loads no window
system until a window opens.

Every window here is either headless or on Qt's offscreen platform, in a
process of its own, so the suite puts nothing on screen and a Qt
application made by one case never outlives it.
"""

import importlib.util
import os
import struct
import subprocess
import sys
import tempfile
import textwrap
import time
import unittest
import zlib
from pathlib import Path

from sigil import compose, live


def first_pixel(path):
    """The colour of a PNG's top left pixel as hex, read with the standard
    library: every filter leaves the first pixel of the first row as
    written."""
    data = Path(path).read_bytes()
    offset, compressed = 8, b""
    while offset < len(data):
        (length,) = struct.unpack(">I", data[offset : offset + 4])
        kind = data[offset + 4 : offset + 8]
        if kind == b"IDAT":
            compressed += data[offset + 8 : offset + 8 + length]
        offset += 12 + length
    return zlib.decompress(compressed)[1:4].hex()


def declared(name, source):
    """The ``@sketch`` class ``source`` declares, in a module of its own: a
    module exports one sketch class, and these cases need several."""
    directory = Path(tempfile.mkdtemp(prefix="sigil_live_case_"))
    path = directory / f"live_case_{name}.py"
    path.write_text(textwrap.dedent(source))
    specification = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(specification)
    sys.modules[path.stem] = module
    specification.loader.exec_module(module)
    return module.__sigil_sketch__


SWATCH = """
    from sigil.sketch import sketch

    @sketch(size=(120, 80), background="#000000")
    class Swatch:
        accent = "#00ff00"

        def draw(self, pen):
            pen.background(self.accent)
"""

COUNTER = """
    from sigil.sketch import sketch

    @sketch(size=(120, 80))
    class Counter:
        frames = 0

        def update(self, elapsed):
            self.frames += 1

        def draw(self, pen):
            pen.background("#204060")
"""

BROKEN = """
    from sigil.sketch import sketch

    @sketch(size=(10, 10))
    class Broken:
        def setup(self, ctx):
            raise ValueError("no canvas today")
"""

Swatch = declared("swatch", SWATCH)
Counter = declared("counter", COUNTER)


def run_python(source, *, environment=None, stdin=None, interactive=False, timeout=120):
    """Runs ``source`` in an interpreter of its own over this package."""
    arguments = [sys.executable] + (["-i"] if interactive else []) + ["-c", source]
    return subprocess.run(
        arguments,
        input=stdin,
        capture_output=True,
        text=True,
        timeout=timeout,
        env={**os.environ, **(environment or {})},
    )


LOADED_IMAGES = textwrap.dedent(
    """
    import ctypes
    def loaded():
        dyld = ctypes.CDLL(None)
        dyld._dyld_image_count.restype = ctypes.c_uint32
        dyld._dyld_get_image_name.restype = ctypes.c_char_p
        dyld._dyld_get_image_name.argtypes = [ctypes.c_uint32]
        return [dyld._dyld_get_image_name(index).decode()
                for index in range(dyld._dyld_image_count())]
    """
)


class HeadlessWindow(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.stills = Path(self.directory.name)
        self.window = live.open(size=(240, 160), headless=True)

    def tearDown(self):
        self.window.close()
        self.directory.cleanup()

    def turn(self, frames=3):
        for _ in range(frames):
            self.assertTrue(self.window.pump())

    def test_a_tree_is_drawn_on_the_windows_canvas(self):
        tree = compose.box().width(60).height(40).fill("#3366ff")
        self.assertIs(self.window.show(tree), tree)
        self.turn()
        still = self.window.still(self.stills / "tree.png", density=2)
        self.assertEqual(first_pixel(still), "3366ff")

    def test_an_attribute_assigned_to_the_running_sketch_is_in_the_next_still(self):
        running = self.window.show(Swatch)
        self.assertIsInstance(running, Swatch)
        self.assertIs(self.window.sketch, running)
        self.turn()
        self.assertEqual(
            first_pixel(self.window.still(self.stills / "green.png")), "00ff00"
        )
        self.window.sketch.accent = "#ff0000"
        self.turn()
        self.assertEqual(
            first_pixel(self.window.still(self.stills / "red.png")), "ff0000"
        )

    def test_an_instance_is_shown_as_it_stands_and_kept_through_a_restart(self):
        swatch = Swatch()
        swatch.accent = "#0000ff"
        self.assertIs(self.window.show(swatch), swatch)
        self.window.restart()
        self.assertIs(self.window.sketch, swatch)
        self.turn()
        self.assertEqual(
            first_pixel(self.window.still(self.stills / "blue.png")), "0000ff"
        )

    def test_the_session_clock_runs_as_the_window_turns(self):
        counter = self.window.show(Counter)
        self.turn(5)
        self.assertGreaterEqual(counter.frames, 5)

    def test_a_setup_that_raises_raises_at_show(self):
        with self.assertRaisesRegex(RuntimeError, "no canvas today"):
            self.window.show(declared("broken", BROKEN))

    def test_a_closed_window_turns_no_more(self):
        self.window.show(Swatch)
        self.window.close()
        self.assertFalse(self.window.isOpen)
        self.assertFalse(self.window.pump())
        with self.assertRaisesRegex(RuntimeError, "closed"):
            self.window.still(self.stills / "closed.png")


class WindowInItsOwnProcess(unittest.TestCase):
    @unittest.skipUnless(sys.platform == "darwin", "reads the images dyld loaded")
    def test_importing_the_package_loads_no_window_system_until_a_window_opens(self):
        source = LOADED_IMAGES + textwrap.dedent(
            """
            import sigil, sigil.live, sigil.compose, sigil.sketch, sigil.testing
            before = [image for image in loaded() if "QtCore" in image or "_sigil_window" in image]
            window = sigil.live.open(size=(64, 64))
            after = [image for image in loaded() if "QtCore" in image or "_sigil_window" in image]
            window.close()
            print("BEFORE", len(before), "AFTER", len(after) > 0)
            """
        )
        result = run_python(source, environment={"QT_QPA_PLATFORM": "offscreen"})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("BEFORE 0 AFTER True", result.stdout)

    def test_a_window_shows_a_tree_then_a_sketch_and_a_changed_attribute(self):
        with tempfile.TemporaryDirectory() as directory:
            source = textwrap.dedent(
                f"""
                import sys
                sys.path.insert(0, {str(Path(__file__).parent)!r})
                from test_live import Swatch, first_pixel
                from sigil import compose, live
                window = live.open(size=(240, 160), title="test_live")
                window.show(compose.box().width(60).height(40).fill("#3366ff"))
                for _ in range(3): window.pump(0.01)
                print("TREE", first_pixel(window.still({directory!r} + "/tree.png")))
                window.show(Swatch)
                for _ in range(3): window.pump(0.01)
                print("SKETCH", first_pixel(window.still({directory!r} + "/green.png")))
                window.sketch.accent = "#ff0000"
                for _ in range(3): window.pump(0.01)
                print("CHANGED", first_pixel(window.still({directory!r} + "/red.png")))
                window.close()
                print("OPEN", window.isOpen)
                """
            )
            result = run_python(source, environment={"QT_QPA_PLATFORM": "offscreen"})
        self.assertEqual(result.returncode, 0, result.stderr)
        for line in ("TREE 3366ff", "SKETCH 00ff00", "CHANGED ff0000", "OPEN False"):
            self.assertIn(line, result.stdout)

    def test_the_prompt_keeps_the_window_drawing_while_it_waits_for_a_line(self):
        source = textwrap.dedent(
            f"""
            import sys
            sys.path.insert(0, {str(Path(__file__).parent)!r})
            from test_live import Counter
            from sigil import live
            window = live.open(size=(120, 80), headless=True)
            counter = window.show(Counter)
            """
        )
        process = subprocess.Popen(
            [sys.executable, "-i", "-c", source],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            time.sleep(1.0)
            process.stdin.write("before = counter.frames\n")
            process.stdin.flush()
            time.sleep(1.0)
            output, errors = process.communicate(
                "print('ADVANCED', counter.frames - before > 10)\nwindow.close()\n",
                timeout=60,
            )
        finally:
            if process.poll() is None:
                process.kill()
        self.assertIn("ADVANCED True", output, errors)

    def test_under_ipython_the_window_is_ipythons_input_hook(self):
        source = textwrap.dedent(
            f"""
            import sys, types
            sys.path.insert(0, {str(Path(__file__).parent)!r})
            registered = {{}}
            enabled = []
            class Shell:
                def enable_gui(self, name):
                    enabled.append(name)
            shell = Shell()
            ipython = types.ModuleType("IPython")
            ipython.get_ipython = lambda: shell
            terminal = types.ModuleType("IPython.terminal")
            hooks = types.ModuleType("IPython.terminal.pt_inputhooks")
            hooks.register = lambda name, hook: registered.__setitem__(name, hook)
            sys.modules.update({{"IPython": ipython, "IPython.terminal": terminal,
                                 "IPython.terminal.pt_inputhooks": hooks}})
            from test_live import Counter
            from sigil import live
            window = live.open(size=(120, 80), headless=True)
            counter = window.show(Counter)
            class Context:
                waited = 0
                def input_is_ready(self):
                    self.waited += 1
                    return self.waited > 20
            registered["sigil"](Context())
            print("ENABLED", enabled, "FRAMES", counter.frames >= 20)
            window.close()
            """
        )
        result = run_python(source)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ENABLED ['sigil'] FRAMES True", result.stdout)


if __name__ == "__main__":
    unittest.main()
