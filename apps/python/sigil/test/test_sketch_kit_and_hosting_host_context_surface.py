"""Everything a canvas sketch is handed: the canvas it declares, the files
beside it, the set it bakes, the art it requires and the publication it wears.
"""

import builtins
import tempfile
import textwrap
import unittest
from pathlib import Path

from _sigil.sketch import CanvasSpecification, Guest, requireCached
from sigil.sketch import render_file


def png_extent(encoded: bytes) -> tuple[int, int]:
    """The width and height a PNG's header states."""
    return (
        int.from_bytes(encoded[16:20], "big"),
        int.from_bytes(encoded[20:24], "big"),
    )


class HostContextSurface(unittest.TestCase):
    def setUp(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_host_context", None))

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

    def test_canvas_specification_carries_every_field_the_host_reads_back(self):
        encoded = self.render("""
            import builtins
            from _sigil.sketch import CanvasSpecification
            from sigil.compose import box
            from sigil.sketch import sketch


            @sketch(size=(20, 20), capture_at=1.0)
            class Declared:
                def setup(self, ctx):
                    wanted = CanvasSpecification(
                        size=(64, 48),
                        background="#204080",
                        captureSeconds=0,
                        oversample=1,
                        plateOnly=True,
                        nonlinearPicture=True,
                    )
                    ctx.canvas(wanted)
                    builtins._sigil_host_context = (
                        wanted,
                        wanted.copy(),
                        ctx.size,
                        ctx.width,
                        ctx.height,
                    )
                    ctx.render(box().width(40).height(30).fill("#c04030"))
        """)
        wanted, copied, size, width, height = builtins._sigil_host_context
        self.assertEqual((width, height), (64, 48))
        self.assertEqual(size, (64, 48))
        self.assertEqual(png_extent(encoded), (64, 48))
        # The record outlives the session that read it, and its copy is
        # independent of the original.
        self.assertEqual((wanted.size.width(), wanted.size.height()), (64, 48))
        self.assertEqual(wanted.captureSeconds, 0)
        self.assertEqual(wanted.oversample, 1)
        self.assertTrue(wanted.plateOnly)
        self.assertTrue(wanted.nonlinearPicture)
        self.assertEqual(copied.background, wanted.background)
        copied.oversample = 4
        self.assertEqual(wanted.oversample, 1)

    def test_a_declaration_states_the_whole_value_and_refuses_an_empty_canvas(self):
        self.render("""
            import builtins
            from _sigil.sketch import CanvasSpecification
            from sigil.compose import box
            from sigil.sketch import sketch


            @sketch(size=(30, 30), capture_at=0)
            class Refused:
                def setup(self, ctx):
                    refusals = []
                    for bad in ((0, 10), (10, -4), (float("inf"), 10)):
                        try:
                            ctx.canvas(CanvasSpecification(size=bad))
                        except ValueError as error:
                            refusals.append(str(error))
                    try:
                        CanvasSpecification(canvasSize=(10, 10))
                    except TypeError as error:
                        refusals.append(str(error))
                    builtins._sigil_host_context = (refusals, ctx.size)
                    ctx.render(box())
        """)
        refusals, size = builtins._sigil_host_context
        self.assertEqual(len(refusals), 4)
        for refusal in refusals[:3]:
            self.assertIn("finite and positive", refusal)
        self.assertIn("Unknown canvas property: canvasSize", refusals[3])
        # A refused declaration leaves the canvas the decorator asked for.
        self.assertEqual(size, (30, 30))

    def test_the_key_names_the_directory_the_local_files_stand_in(self):
        self.render(
            """
            import builtins
            from sigil.compose import box
            from sigil.sketch import sketch


            @sketch(size=(20, 20), capture_at=0)
            class Keyed:
                def setup(self, ctx):
                    builtins._sigil_host_context = (
                        ctx.key,
                        ctx.local("beside.txt"),
                        ctx.assets.hub().text(ctx.local("beside.txt")),
                    )
                    ctx.render(box())
        """,
            files={"beside.txt": "a file beside the sketch"},
        )
        key, uri, text = builtins._sigil_host_context
        self.assertTrue(key)
        self.assertEqual(uri, f"sketch://{key}/beside.txt")
        self.assertEqual(text, "a file beside the sketch")

    def test_a_baked_set_is_an_owned_image_of_the_extent_that_was_asked_for(self):
        self.render("""
            import builtins
            from sigil import world
            from sigil.compose import image
            from sigil.geometry import mesh
            from sigil.material import kit as surfaces
            from sigil.sketch import sketch


            @sketch(size=(80, 80), capture_at=0)
            class Baked:
                def setup(self, ctx):
                    lens = mesh.camera.Camera()
                    lens.eye = (0, 0, 320)
                    lens.target = (0, 0, 0)
                    body = (
                        world.Element()
                        .key("body")
                        .mesh(mesh.box((-65, -35, -20), (65, 35, 20)))
                        .fill(
                            surfaces.unlit(
                                surfaces.SurfaceParameters(baseColor="#e75a31")
                            )
                        )
                    )
                    frame = world.Frame(body)
                    still = ctx.bakeSet(frame, lens, (64, 64), background="#101820")
                    later = ctx.bakeSet(
                        frame=frame,
                        camera=lens,
                        size=(64, 64),
                        background="#101820",
                        seconds=0.5,
                    )
                    wider = ctx.bakeSet(frame, lens, (48, 32))
                    refusals = []
                    for bad in ((0, 8), (8, -1)):
                        try:
                            ctx.bakeSet(frame, lens, bad)
                        except ValueError as error:
                            refusals.append(str(error))
                    try:
                        ctx.bakeSet(frame, lens, (8, 8), seconds=-1)
                    except ValueError as error:
                        refusals.append(str(error))
                    builtins._sigil_host_context = (still, later, wider, refusals)
                    ctx.render(image(still))
        """)
        still, later, wider, refusals = builtins._sigil_host_context
        self.assertEqual((still.width(), still.height()), (64, 64))
        self.assertEqual((wider.width(), wider.height()), (48, 32))
        # A set with no motion in it is the same picture at every moment.
        self.assertEqual(still.rgba(), later.rgba())
        self.assertNotEqual(still.rgba(), wider.rgba())
        # The background reaches the bake, so the corner is the colour
        # that was asked for rather than nothing at all.
        self.assertEqual(still.rgba()[3], 255)
        self.assertEqual(len(refusals), 3)
        for refusal in refusals[:2]:
            self.assertIn("positive", refusal)
        self.assertIn("finite and nonnegative", refusals[2])
        # The image belongs to whoever took it, not to the closed session.
        self.assertEqual(len(still.rgba()), 64 * 64 * 4)

    def test_required_art_stands_a_sketch_down_with_the_first_missing_url(self):
        with tempfile.TemporaryDirectory() as directory:
            empty = Path(directory) / "nothing-fetched"
            self.assertEqual(requireCached([], empty), (True, ""))
            present, why = requireCached(
                [
                    "https://sigil.invalid/first.png",
                    "https://sigil.invalid/second.png",
                ],
                empty,
            )
            self.assertFalse(present)
            self.assertIn("first.png", why)
            self.assertNotIn("second.png", why)
            # A string path and a keyword call read the same directory.
            self.assertEqual(
                requireCached(
                    urls=["https://sigil.invalid/first.png"],
                    cacheDirectory=str(empty),
                ),
                (False, why),
            )
            # A URI no fetch ever lands under is not in any cache.
            self.assertFalse(requireCached(["res://ground.png"], empty)[0])
        # Without a directory the probe reads the hub's own cache, which
        # has never held this.
        self.assertFalse(requireCached(["https://sigil.invalid/unfetched.png"])[0])

    def test_a_guest_with_nothing_publishing_wears_no_picture(self):
        self.render("""
            import builtins
            from _sigil.sketch import Guest
            from sigil.sketch import sketch


            @sketch(size=(24, 24), capture_at=0)
            class Wearing:
                def setup(self, ctx):
                    self.guest = Guest(
                        ctx, "sigil.test.publication", application="Sigil Test"
                    )
                    builtins._sigil_host_context = {
                        "guest": self.guest,
                        "frames": [],
                        "publishing": self.guest.publishing(),
                    }

                def draw(self, pen, ctx):
                    pen.background("#101820")
                    builtins._sigil_host_context["frames"].append(
                        self.guest.frame(pen)
                    )
        """)
        read = builtins._sigil_host_context
        guest = read["guest"]
        self.assertFalse(read["publishing"])
        self.assertTrue(read["frames"])
        self.assertTrue(all(frame is None for frame in read["frames"]))
        # The guest is the author's, so it answers after the session it was
        # made in has closed.
        self.assertFalse(guest.publishing())
        self.assertEqual(guest.name(), "sigil.test.publication")
        self.assertEqual(guest.application(), "")

    def test_a_guest_is_made_from_a_context_a_session_still_stands_behind(self):
        self.render("""
            import builtins
            from _sigil.sketch import Guest
            from sigil.compose import box
            from sigil.sketch import sketch


            @sketch(size=(20, 20), capture_at=0)
            class Escaped:
                def setup(self, ctx):
                    builtins._sigil_host_context = ctx
                    ctx.render(box())
        """)
        escaped = builtins._sigil_host_context
        with self.assertRaisesRegex(RuntimeError, "closed session"):
            Guest(escaped, "sigil.test.publication")
        with self.assertRaises(TypeError):
            Guest(None, "sigil.test.publication")


if __name__ == "__main__":
    unittest.main()
