"""Everything a scene2d sketch is handed: the canvas it declares, the files
beside it, the scene3d sketch it bakes and the page that bake lights, the art
it requires and the publication it wears.
"""

import builtins
import tempfile
import textwrap
import unittest
from pathlib import Path

from _sigil.sketch import CanvasSpecification, Guest, requireCached
from sigil.sketch import renderFile


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
            renderFile(entry, output, at=at)
            return output.read_bytes()

    def test_canvas_specification_carries_every_field_the_host_reads_back(self):
        encoded = self.render("""
            import builtins
            from _sigil.sketch import CanvasSpecification
            from sigil.compose import box
            from sigil.sketch import sketch


            @sketch(size=(20, 20), captureAt=1.0)
            class Declared:
                def setup(self, ctx):
                    wanted = CanvasSpecification(
                        size=(64, 48),
                        background="#204080",
                        captureAt=0,
                        oversample=1,
                        paintDiscardedFrames=False,
                        plateOnly=True,
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
        self.assertEqual(wanted.captureAt, 0)
        self.assertEqual(wanted.oversample, 1)
        self.assertFalse(wanted.paintDiscardedFrames)
        self.assertTrue(CanvasSpecification().paintDiscardedFrames)
        self.assertTrue(wanted.plateOnly)
        self.assertEqual(copied.background, wanted.background)
        copied.oversample = 4
        self.assertEqual(wanted.oversample, 1)

    def test_a_declaration_states_the_whole_value_and_refuses_an_empty_canvas(self):
        self.render("""
            import builtins
            from _sigil.sketch import CanvasSpecification
            from sigil.compose import box
            from sigil.sketch import sketch


            @sketch(size=(30, 30), captureAt=0)
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


            @sketch(size=(20, 20), captureAt=0)
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

    def test_a_scene3d_frame_baked_with_no_device_is_none_and_bad_requests_are_refused(
        self,
    ):
        # A scene3d sketch is drawn on a device and nowhere else, and this process
        # brings none up: a bake answers None rather than a picture no
        # device drew, and a request that could never be drawn is refused
        # before that.
        self.render("""
            import builtins
            from sigil import world
            from sigil.compose import box
            from sigil.geometry import mesh
            from sigil.material import surface as surfaces
            from sigil.sketch import sketch


            @sketch(size=(80, 80), captureAt=0)
            class Baked:
                def setup(self, ctx):
                    lens = mesh.camera.Camera()
                    lens.eye = (0, 0, 320)
                    lens.target = (0, 0, 0)
                    body = (
                        world.mesh(mesh.box((-65, -35, -20), (65, 35, 20)))
                        .key("body")
                        .fill(
                            surfaces.unlit(
                                surfaces.SurfaceParameters(baseColor="#e75a31")
                            )
                        )
                    )
                    frame = world.Frame(body)
                    still = ctx.bakeScene3D(
                        frame, (64, 64), camera=lens, background="#101820"
                    )
                    later = ctx.bakeScene3D(
                        frame=frame,
                        size=(64, 64),
                        camera=lens,
                        background="#101820",
                        at=0.5,
                    )
                    wider = ctx.bakeScene3D(frame, (48, 32), camera=lens)
                    refusals = []
                    for bad in ((0, 8), (8, -1)):
                        try:
                            ctx.bakeScene3D(frame, bad, camera=lens)
                        except ValueError as error:
                            refusals.append(str(error))
                    try:
                        ctx.bakeScene3D(frame, (8, 8), camera=lens, at=-1)
                    except ValueError as error:
                        refusals.append(str(error))
                    builtins._sigil_host_context = (still, later, wider, refusals)
                    ctx.render(box())
        """)
        still, later, wider, refusals = builtins._sigil_host_context
        self.assertIsNone(still)
        self.assertIsNone(later)
        self.assertIsNone(wider)
        self.assertEqual(len(refusals), 3)
        for refusal in refusals[:2]:
            self.assertIn("positive", refusal)
        self.assertIn("finite and nonnegative", refusals[2])

    def test_a_page_paints_a_tree_for_a_baked_frame_and_is_worn_as_a_surface(
        self,
    ):
        # A page is the pass that paints a tree as a surface's maps and the
        # material a body wears to show it. This process brings no device up,
        # so the bake that carries it answers None; the page, its pass and
        # its surface are made all the same, and a size that could never be
        # painted is refused.
        self.render("""
            import builtins
            from sigil import compose, material, world
            from sigil.geometry import mesh
            from sigil.sketch import Page, sketch


            @sketch(size=(80, 80), captureAt=0)
            class Paged:
                def setup(self, ctx):
                    first = ctx.page((32, 32))
                    second = ctx.page((16, 8))
                    tree = compose.box().width(32).height(32).fill("#00ff00")
                    painting = first.render(tree)
                    worn = first.surface()
                    card = world.mesh(mesh.quad(100, 60)).key("card").fill(worn)
                    frame = world.Frame(world.group(card).key("set")).pass_(painting)
                    still = ctx.bakeScene3D(frame, (64, 64))
                    refusals = []
                    for bad in ((0, 8), (8, -1)):
                        try:
                            ctx.page(bad)
                        except ValueError as error:
                            refusals.append(str(error))
                    builtins._sigil_host_context = (
                        isinstance(first, Page),
                        first.name,
                        second.name,
                        Page().name,
                        isinstance(painting, world.Pass),
                        isinstance(worn, material.Material),
                        still,
                        refusals,
                    )
                    ctx.render(compose.box())
        """)
        (
            is_page,
            first,
            second,
            none,
            is_pass,
            is_material,
            still,
            refusals,
        ) = builtins._sigil_host_context
        self.assertTrue(is_page)
        self.assertEqual((first, second, none), ("page", "page 2", ""))
        self.assertTrue(is_pass)
        self.assertTrue(is_material)
        self.assertIsNone(still)
        self.assertEqual(len(refusals), 2)
        for refusal in refusals:
            self.assertIn("positive", refusal)

    def test_a_page_states_its_range_and_dresses_a_body_with_one_region(self):
        # A page's range says what its maps stand for at their white, and
        # one page dresses several bodies with a region each. This process
        # brings no device up, so nothing is baked; the values are made
        # all the same.
        self.render("""
            import builtins
            from sigil import compose, material
            from sigil.sketch import sketch


            @sketch(size=(80, 80), captureAt=0)
            class Paged:
                def setup(self, ctx):
                    stated = compose.SurfaceRange(emission=8, thickness=2)
                    page = ctx.page((64, 32), range=stated)
                    learned = ctx.page((64, 32))
                    left = page.surface(
                        material.PixelRect(x=0, y=0, width=32, height=32)
                    )
                    right = page.surface(region=material.PixelRect(x=32, width=32, height=32))
                    refused = None
                    try:
                        compose.SurfaceRange(brightness=2)
                    except TypeError as error:
                        refused = str(error)
                    builtins._sigil_host_context = (
                        (stated.emission, stated.thickness, stated.distortion),
                        stated == compose.SurfaceRange(emission=8, thickness=2),
                        isinstance(left, material.Material),
                        left == right,
                        learned.name,
                        refused,
                    )
                    ctx.render(compose.box())
        """)
        (
            readings,
            equal,
            is_material,
            same_region,
            learned,
            refused,
        ) = builtins._sigil_host_context
        self.assertEqual(readings, (8, 2, 0))
        self.assertTrue(equal)
        self.assertTrue(is_material)
        self.assertFalse(same_region)
        self.assertEqual(learned, "page 2")
        self.assertIn("brightness", refused)

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


            @sketch(size=(24, 24), captureAt=0)
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


            @sketch(size=(20, 20), captureAt=0)
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
