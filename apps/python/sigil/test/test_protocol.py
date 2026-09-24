"""Both routes to a sketch host: in this process through sigil.testing, and
over a socket to a headless Sketchbook through sigil.protocol.launch.

The same generated domain classes speak through either, so a case written
for one reads as a script for the other."""

import os
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import image
from sigil.protocol import (
    Clock,
    ErrorCode,
    Host,
    ProtocolError,
    Registry,
    Session,
    connect,
    launch,
)
from sigil.protocol.clock import Policy
from sigil.testing import InProcess

SCENE = """
    from sigil.compose import box
    from sigil.sketch import sketch


    @sketch(size=(32, 16), background="#000000", capture_at=0.5)
    class Scene:
        def setup(self, ctx):
            self.node = box().width(8).height(8).fill("#ff0000")
            ctx.render(self.node)

        def update(self, elapsed, ctx):
            ctx.render(
                box().width(8).height(8).fill("#ff0000").translateX(elapsed * 16)
            )
"""


class InProcessRoute(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory(prefix="sigil_protocol_")
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)
        self.source = self.root / "scene.py"
        self.source.write_text(textwrap.dedent(SCENE))
        self.host = InProcess(self.root / "state")

    def test_describe_names_the_domains_and_the_state_root(self):
        described = Host(self.host).describe()
        self.assertEqual(described.domains, ("clock", "host", "registry", "session"))
        self.assertEqual(described.version.program, "sigil.testing")
        self.assertEqual(Path(described.state_root), self.root / "state")

    def test_a_file_stepped_under_advance_is_photographed_where_it_was_stepped(self):
        Clock(self.host).set_policy(policy=Policy.Advance)
        opened = Session(self.host).open(sketch=str(self.source))
        self.assertEqual((opened.width, opened.height), (32.0, 16.0))
        self.assertEqual(opened.moment, 0.5)
        stepped = Clock(self.host).step(seconds=0.5)
        self.assertEqual(stepped.frame, 30)
        still = Session(self.host).still(density=1.0, path="after.png")
        self.assertEqual(Path(still.path), self.root / "state" / "after.png")
        pixels = image.load(still.path).rgba()
        # The box has moved eight units right of the origin by half a
        # second: the origin is ground again, and the box stands at x = 8.
        self.assertEqual(pixels[:4], bytes((0, 0, 0, 255)))
        self.assertEqual(
            pixels[(4 * 32 + 12) * 4 : (4 * 32 + 12) * 4 + 4], bytes((255, 0, 0, 255))
        )

    def test_a_step_under_the_wall_is_refused_by_the_agent(self):
        Session(self.host).open(sketch=str(self.source))
        with self.assertRaises(ProtocolError) as refused:
            Clock(self.host).step(seconds=1)
        self.assertEqual(refused.exception.error.code, ErrorCode.failed)
        self.assertIn("Advance", refused.exception.error.message)

    def test_a_budget_tells_a_client_that_enabled_the_clock(self):
        heard = []
        clock = Clock(self.host)
        clock.on_budget_expired(lambda event: heard.append(event.seconds))
        clock.enable()
        clock.set_policy(policy=Policy.Advance, budget_seconds=0.25)
        Session(self.host).open(sketch=str(self.source))
        clock.step(seconds=0.5)
        self.assertEqual(len(heard), 1)
        self.assertAlmostEqual(heard[0], 0.25)

    def test_the_readout_names_the_last_still(self):
        Clock(self.host).set_policy(policy=Policy.Advance)
        Session(self.host).open(sketch=str(self.source))
        still = Session(self.host).still()
        self.assertIn(still.path, self.host.readout())


@unittest.skipUnless(os.environ.get("SIGIL_TEST_SKETCHBOOK"), "no Sketchbook to launch")
class SocketRoute(unittest.TestCase):
    def test_a_launched_sketchbook_is_driven_over_its_socket_and_ends_with_it(self):
        with tempfile.TemporaryDirectory(prefix="sigil_launch_") as folder:
            state = Path(folder) / "state"
            host = launch(executable=os.environ["SIGIL_TEST_SKETCHBOOK"], state=state)
            with host:
                self.assertTrue(host.definition)
                described = Host(host).describe()
                self.assertEqual(described.version.program, "Sketchbook")
                self.assertIn("session", described.domains)
                entries = Registry(host).list(kind="canvas").sketches
                self.assertTrue(entries)
                name = next(entry.name for entry in entries if entry.available)
                Clock(host).set_policy(policy=Policy.Advance)
                opened = Session(host).open(sketch=name)
                self.assertEqual(opened.kind, "canvas")
                self.assertEqual(Clock(host).step(seconds=1).frame, 60)
                still = Session(host).still(density=1.0, path="launched.png")
                self.assertTrue(Path(still.path).is_file())
                self.assertEqual(Path(still.path).parent, state.resolve())
                # A second client finds the same host through the state
                # directory it keeps its address under.
                with connect(state) as second:
                    attached = Host(second).describe().attached
                    self.assertEqual(len(attached), 2)
            self.assertFalse((state / "protocol-address").exists())
