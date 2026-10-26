"""A native World frame lit on the device: three forms on three plinths
under one moving three-point rig, the words laid over the finished frame.

The forms are Python data handed to the native geometry library: a vessel
lathed from a profile (`mesh.revolve`), a knot evaluated from a function of
two parameters (`mesh.grid`) and a regular solid (`mesh.platonic`).
Tessellation, normals, projection and lighting are native; the words are a
Compose page painted once onto a screen laid over the frame.

EDIT THESE FIRST: PROFILE, knot(), STATIONS, the rig in describe().

TAGS: World/Scenes, Geometry/Meshes, Materials/Surfaces, Python
"""

from math import cos, pi, sin

from sigil import compose, material, world
from sigil.geometry import mesh
from sigil.material import surface as surfaces
from sigil.sketch import Scene3DContext, sketch

INK = "#ebeee7"
MUTED = "#93a4aa"
RULE = "#364550"
PAPER = "#141d25"
WIDTH, HEIGHT = 1200, 820

# The vessel's profile: radius and height pairs from the foot to the lip.
PROFILE = [
    (0, -64),
    (40, -64),
    (45, -52),
    (52, -15),
    (42, 24),
    (24, 54),
    (25, 70),
    (19, 70),
    (18, 54),
]


def knot(u: float, v: float) -> tuple[float, float, float]:
    """A tube wound three times round a ring: a point of its surface at a
    fraction along the knot and a fraction round its section."""
    along, section = u * 2 * pi, v * 2 * pi
    radius = 116 + 35 * cos(3 * along) + 13 * cos(section)
    return (
        radius * cos(2 * along),
        44 * sin(3 * along) - 13 * sin(section),
        radius * sin(2 * along),
    )


def finish(color: str, roughness: float = 0.45) -> material.Material:
    return surfaces.program(surfaces.SurfaceParameters(baseColor=color, roughness=roughness))


# One station each: its number, the form's name, the call that made it, and
# the glaze it wears.
STATIONS = (
    ("01", "Revolved profile", "mesh.revolve(PROFILE)", "#87b9ad"),
    ("02", "Parametric knot", "mesh.grid(180, 22, knot)", "#dfa573"),
    ("03", "Twelve faces", "mesh.platonic(Dodecahedron)", "#bdcbd8"),
)


def text(content: str, size: float, color: str) -> compose.Text:
    return compose.text(content, size=size, color=color)


def caption(number: str, title: str, call: str, detail: str, color: str) -> compose.Element:
    """A station's caption: a rule, its number and name, and how it was made."""
    return (
        compose.box(
            compose.box().height(1).fill(RULE),
            compose.box(
                text(number, 12, color),
                compose.box(text(title, 19, INK), text(call, 13, MUTED), text(detail, 12, MUTED))
                .column()
                .gap(6),
            )
            .row()
            .gap(20),
        )
        .column()
        .gap(12)
        .width(352)
    )


def words(details: list[str]) -> compose.Element:
    """The sheet's words: the heading above the set, a caption under each
    station, and the footer."""
    heading = compose.box(
        text("WORLD STUDY  /  01", 12, MUTED),
        text("One rig. Three forms.", 38, INK),
        text("Python describes each shape; the native world tessellates and lights it.", 15, MUTED),
    ).column().gap(10)
    captions = compose.box(
        *(caption(*station[:3], detail, station[3]) for station, detail in zip(STATIONS, details))
    ).row().gap(24)
    footer = compose.box(
        text("WORLD FRAME / DEVICE EXECUTOR", 12, MUTED),
        text("Dielectric glazes share roughness 0.45 under a three-point rig with no environment reflections.", 12, MUTED),
    ).column().gap(10)
    return (
        compose.box(heading, compose.box().flexGrow(1), captions, footer)
        .width(WIDTH)
        .height(HEIGHT)
        .padding(36, 48)
        .column()
        .gap(30)
    )


@sketch(size=(WIDTH, HEIGHT), background=PAPER, captureAt=1.0)
class WorldStudy:
    def setup(self, ctx: Scene3DContext) -> None:
        self.forms = (
            mesh.revolve(PROFILE, segments=64),
            mesh.grid(180, 22, knot),
            mesh.platonic(mesh.Platonic.Dodecahedron, radius=52),
        )
        # How each form stands on its plinth: its scale and the height of its
        # centre, so all three sit on the plinth's top.
        self.stands = ((1.0, 0.0), (0.42, -36.0), (1.0, -24.0))
        self.finishes = tuple(finish(station[3]) for station in STATIONS)
        self.plinth = mesh.box((-80, -9, -68), (80, 9, 68))
        self.base = finish("#283642", roughness=0.9)
        # The lens is wide enough that the set fills the band between the
        # heading and the captions, its centre a few pixels above the
        # canvas's.
        ctx.camera(
            mesh.camera.Camera(
                eye=(0, 175, 510),
                target=(0, -9, 0),
                verticalFieldOfViewDegrees=46.6,
            )
        )
        # The words stand on a screen laid over the finished frame, painted
        # once and kept while the tree is unchanged.
        self.words = ctx.textureScene((WIDTH, HEIGHT))
        self.sheet = words(
            [f"{form.vertexCount():,} vertices · {form.triangleCount():,} triangles" for form in self.forms]
        )

    def describe(self, seconds: float) -> world.Frame:
        pieces = [
            world.group(
                world.mesh(form)
                .key(f"form-{index}")
                .fill(glaze)
                .backface(material.Backface.Visible)
                .scale(scale)
                .rotateY(seconds * 14)
                .translateY(height),
                world.mesh(self.plinth)
                .key(f"plinth-{index}")
                .fill(self.base)
                .translateY(-74),
            )
            .key(f"station-{index}")
            .at((x, 0, 0))
            for index, (x, form, glaze, (scale, height)) in enumerate(
                zip((-205, 0, 205), self.forms, self.finishes, self.stands)
            )
        ]
        model = world.group(
            world.group(pieces).key("forms"),
            world.kit.threePoint(
                world.kit.Rig(
                    extent=380,
                    bearing=-35 + seconds * 8,
                    elevation=38,
                    intensity=3.0,
                    fillIntensity=1.5,
                    backIntensity=1.6,
                    ambientIntensity=0.06,
                )
            ),
        ).key("study")
        self.words.render(self.sheet, seconds)
        return world.Frame(model).overlay(self.words.texture())
