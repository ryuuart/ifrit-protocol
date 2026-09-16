"""Checked native session views and exact headless capture clocks."""

import builtins
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil.sketch import render_file


class Runtime(unittest.TestCase):
    def setUp(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_runtime", None))

    def render(self, source, *, at=None, files=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entry = root / "scene.py"
            output = root / "frame.png"
            entry.write_text(textwrap.dedent(source))
            for name, value in (files or {}).items():
                (root / name).write_text(value)
            render_file(entry, output, at=at)
            return output.read_bytes()

    def test_declared_zero_and_explicit_zero_capture_the_same_instant(self):
        source = """
            import builtins
            from sigil.sketch import sketch
            @sketch(size=(20, 20), capture_at=0)
            class Clock:
                def update(self, elapsed, ctx):
                    builtins._sigil_runtime = elapsed
                def draw(self, pen, ctx):
                    pen.background("#ff0000" if ctx.elapsed == 0 else "#0000ff")
        """
        declared = self.render(source)
        self.assertEqual(builtins._sigil_runtime, 0)
        self.assertEqual(declared, self.render(source, at=0))
        self.assertEqual(builtins._sigil_runtime, 0)
        self.render(source, at=0.005)
        self.assertAlmostEqual(builtins._sigil_runtime, 0.005)

    def test_composer_queries_are_owned_and_slot_replacement_is_native(self):
        self.render(
            """import builtins
from sigil.compose import box, slot
from sigil.sketch import sketch


@sketch(size=(80, 48), capture_at=0)
class Queries:
    def setup(self, ctx):
        ctx.composer.render(
            (
                box()
                .key("root")
                .children(
                    [
                        slot("replace"),
                    ]
                )
            )
        )
        ctx.composer.renderSlot(
            "replace",
            (
                box()
                .width(32)
                .height(24)
                .fill("#abcdef")
                .key("replacement")
                .hitTestable(True)
            ),
        )

    def update(self, elapsed, ctx):
        if elapsed > 1 / 60:
            bounds = ctx.composer.bounds("replacement")
            builtins._sigil_runtime = (
                ctx.composer,
                bounds,
                ctx.composer.hitTest((8, 8)),
                ctx.composer.stats(),
                ctx.composer.settling("missing"),
            )
""",
            at=0.05,
        )
        composer, bounds, hit, stats, settling = builtins._sigil_runtime
        self.assertEqual((bounds.width(), bounds.height()), (32, 24))
        self.assertEqual(hit, "replacement")
        self.assertGreater(stats.instances, 0)
        self.assertFalse(settling.live)
        with self.assertRaisesRegex(RuntimeError, "closed session"):
            composer.stats()

    def test_assets_return_owned_values_and_checked_views(self):
        self.render(
            """import builtins
from sigil.compose import box
from sigil.sketch import sketch


@sketch(size=(20, 20), capture_at=0)
class Resources:
    def setup(self, ctx):
        hub = ctx.assets.hub()
        uri = ctx.local("hello.txt")
        text = hub.text(uri)
        blob = hub.blob(uri)
        info = hub.probe(uri)
        names = hub.select(ctx.local("*.txt"))
        missing = ctx.assets.image(ctx.local("missing.png"))
        builtins._sigil_runtime = (ctx.assets, hub, text, blob, info, names, missing)
        assert hub.text(ctx.local("absent.txt")) is None
        ctx.render(box())
""",
            files={"hello.txt": "resource value"},
        )
        assets, hub, text, blob, info, names, image = builtins._sigil_runtime
        self.assertEqual(text, "resource value")
        self.assertEqual(blob, b"resource value")
        self.assertEqual(info.byteSize, len(blob))
        self.assertEqual(len(names), 1)
        self.assertGreater(image.width(), 0)
        with self.assertRaisesRegex(RuntimeError, "closed session"):
            assets.hub()
        with self.assertRaisesRegex(RuntimeError, "closed session"):
            hub.text("anything")

    def test_context_measure_snapshot_and_deterministic_values(self):
        self.render("""import builtins
from sigil.compose import box, picture
from sigil.sketch import sketch


@sketch(size=(60, 40), capture_at=0)
class Measured:
    def setup(self, ctx):
        tree = box().children(
            [
                (box().width(24).height(18).fill("#ff3300")),
            ]
        )
        size = ctx.measure(tree)
        snapshot = ctx.snapshot(tree)
        builtins._sigil_runtime = (
            size,
            snapshot,
            ctx.measured(17),
            ctx.measured(17, 4),
        )
        ctx.render(picture(snapshot, 24, 18))
""")
        size, snapshot, pinned, custom = builtins._sigil_runtime
        self.assertEqual((size.width(), size.height()), (24, 18))
        self.assertIsNotNone(snapshot)
        self.assertEqual((pinned, custom), (0, 4))

    def test_context_and_views_reject_foreign_thread(self):
        self.render("""import builtins
from threading import Thread
from sigil.compose import box
from sigil.sketch import sketch


@sketch(size=(20, 20), capture_at=0)
class Threads:
    def setup(self, ctx):
        views = [ctx, ctx.composer, ctx.assets, ctx.assets.hub(), ctx.ticker]
        operations = [
            lambda: ctx.elapsed,
            views[1].stats,
            views[2].root,
            lambda: views[3].text("missing"),
            views[4].elapsed,
        ]
        errors = []

        def worker():
            for operation in operations:
                try:
                    operation()
                except RuntimeError as error:
                    errors.append(str(error))

        thread = Thread(target=worker)
        thread.start()
        thread.join()
        builtins._sigil_runtime = errors
        ctx.render(box())
""")
        self.assertEqual(len(builtins._sigil_runtime), 5)
        self.assertTrue(
            all("sketch thread" in error for error in builtins._sigil_runtime)
        )


if __name__ == "__main__":
    unittest.main()
