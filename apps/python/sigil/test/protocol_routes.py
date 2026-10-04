"""Both routes to a sketch host: in this process through sigil.testing, and
over a socket to a headless Grimoire through sigil.protocol.launch.

The same generated domain classes speak through either, so a case written
for one reads as a script for the other. Run under the protocol's label as
python_protocol_routes, with the Grimoire to launch named by
SIGIL_TEST_GRIMOIRE; the socket route is the served lane's own case —
its address written before its first frame and taken back as it ends —
and, end to end, a registry sketch stepped a second and photographed over
the socket is the plate the sweep takes of it at that moment, and so is
the still a written `--frame` of its file takes."""

import os
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import media
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
        pixels = media.load(still.path).frameAt(0).image.rgba()
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


@unittest.skipUnless(os.environ.get("SIGIL_TEST_GRIMOIRE"), "no Grimoire to launch")
class SocketRoute(unittest.TestCase):
    SCENE = "cascade"
    """A registry sketch the plate sample carries."""

    def setUp(self):
        folder = tempfile.TemporaryDirectory(prefix="sigil_launch_")
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)
        self.grimoire = os.environ["SIGIL_TEST_GRIMOIRE"]

    def plate(self, at):
        """The sweep's plate of the scene at @p at seconds."""
        plates = self.root / "plates"
        subprocess.run(
            [
                self.grimoire,
                "--headless",
                str(plates),
                "--ledger",
                "--sketch",
                self.SCENE,
                "--at",
                str(at),
            ],
            check=True,
            capture_output=True,
        )
        return plates / f"plate_{self.SCENE}.png"

    def test_a_launched_grimoire_is_driven_over_its_socket_and_ends_with_it(self):
        state = self.root / "state"
        host = launch(executable=self.grimoire, state=state)
        with host:
            # The address was written before the first frame: nothing is
            # open and the clock has not moved.
            self.assertTrue((state / "protocol-address").is_file())
            self.assertTrue(host.definition)
            described = Host(host).describe()
            self.assertEqual(described.version.program, "Grimoire")
            self.assertEqual(
                described.domains, ("clock", "host", "registry", "session")
            )
            self.assertEqual(described.sessions, ())
            self.assertEqual(Clock(host).current().frame, 0)
            entries = Registry(host).list(kind="canvas").sketches
            self.assertIn(self.SCENE, [entry.name for entry in entries])
            # A second client finds the same host through the state
            # directory it keeps its address under.
            with connect(state) as second:
                attached = Host(second).describe().attached
                self.assertEqual(len(attached), 2)
        self.assertFalse((state / "protocol-address").exists())

    def test_a_registry_sketch_stepped_a_second_is_the_sweeps_plate_of_it(self):
        plate = self.plate(1.0)
        expected = media.load(plate).frameAt(0).image
        state = self.root / "state"
        with launch(executable=self.grimoire, state=state) as host:
            Clock(host).set_policy(policy=Policy.Advance)
            # Baked on the plate's grid from the first frame, as the sweep
            # bakes: zero pins the density a plate is photographed at.
            Session(host).pin_density(density=0)
            opened = Session(host).open(sketch=self.SCENE)
            self.assertEqual(opened.kind, "canvas")
            self.assertEqual(Clock(host).step(seconds=1).frame, 60)
            density = expected.width() / opened.width
            still = Session(host).still(density=density, path="stepped.png")
            self.assertEqual(Path(still.path).parent, state.resolve())
            self.assertEqual(
                (still.width, still.height), (expected.width(), expected.height())
            )
            self.assertEqual(
                media.load(still.path).frameAt(0).image.rgba(), expected.rgba()
            )

    def test_a_written_still_is_the_sweeps_plate_and_the_sockets_still(self):
        plate = self.plate(1.0)
        expected = media.load(plate).frameAt(0).image
        source = (
            Path(__file__).resolve().parents[3]
            / "grimoire/sketches"
            / f"{self.SCENE}.cpp"
        )
        written = self.root / "written.png"
        subprocess.run(
            [
                self.grimoire,
                str(source),
                "--frame",
                str(written),
                "--at",
                "1",
                "--scale",
                "2",
            ],
            check=True,
            capture_output=True,
            timeout=600,
        )
        state = self.root / "state"
        with launch(executable=self.grimoire, state=state) as host:
            Clock(host).set_policy(policy=Policy.Advance)
            Session(host).pin_density(density=2)
            Session(host).open(sketch=self.SCENE)
            Clock(host).step(seconds=1)
            still = Session(host).still(density=2, path="stepped.png")
        pictures = (
            media.load(written).frameAt(0).image,
            media.load(still.path).frameAt(0).image,
        )
        for picture in pictures:
            self.assertEqual(
                (picture.width(), picture.height()),
                (expected.width(), expected.height()),
            )
            self.assertEqual(picture.rgba(), expected.rgba())
