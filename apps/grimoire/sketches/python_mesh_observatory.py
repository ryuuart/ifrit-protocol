"""A parametric sculpture, lathed vessel and solid lit in a native world frame.

The forms are Python data and functions. Tessellation, camera projection,
normals and lighting are the native geometry and world libraries'; the
ruled ground is a pen program laid under the bodies and the sheet's words
one laid over the finished frame.
"""

# TAGS: Geometry/Meshes, Drawing/Generative

from math import cos, pi, sin

from sigil import compose, material, world
from sigil.draw import LEFT, TOP, Pen
from sigil.geometry import mesh
from sigil.material import pattern
from sigil.material import surface as surfaces
from sigil.sketch import Scene3DContext, kit, sketch

WIDTH, HEIGHT = 1400, 900

camera = mesh.camera


def knot(u, v):
    t, section = u * 2 * pi, v * 2 * pi
    radius = 116 + 35 * cos(3 * t) + 13 * cos(section)
    return radius * cos(2 * t), 44 * sin(3 * t) - 13 * sin(section), radius * sin(2 * t)


def finish(color, *, roughness=0.3):
    """A dielectric in @p color: low roughness reads as the glaze the forms
    wear, high as the matte stone they stand on."""
    return surfaces.program(
        surfaces.SurfaceParameters(baseColor=color, roughness=roughness)
    )


LENS = camera.Camera(eye=(320, 280, 680), target=(0, 0, 0), fovYDegrees=39)


@sketch(size=(WIDTH, HEIGHT), background="#0e1116", captureAt=0.5)
class MeshObservatory:
    def setup(self, ctx: Scene3DContext) -> None:
        self.look = kit.featureTheme(kit.Density.Spacious)
        self.knot = mesh.grid(180, 22, knot)
        self.vase = mesh.revolve(
            [
                (0, -70),
                (40, -70),
                (46, -60),
                (43, -35),
                (65, 0),
                (50, 35),
                (22, 60),
                (24, 85),
            ],
            segments=72,
        )
        self.solid = mesh.platonic(mesh.Platonic.Dodecahedron, radius=63)
        self.ring = mesh.torus(188, 1.2, nu=180, nv=8)
        self.plinth = mesh.superellipsoid((210, 10, 180), 6)
        self.finishes = [finish("#65c9c3"), finish("#d9a66e"), finish("#a8bbd6")]
        self.dark = finish("#293a4e", roughness=0.8)
        self.rule = finish("#456073", roughness=0.9)
        ctx.camera(LENS)
        # The ruled ground stands on a screen laid under the bodies, painted
        # once and kept while its tree is unchanged.
        self.grid = pattern.gridLines(40, 0.6, "#21313d").paint()
        self.ground = ctx.textureScene((WIDTH, HEIGHT))
        self.groundSheet = (
            compose.graphics("observatory.ground", self.rules)
            .width(WIDTH)
            .height(HEIGHT)
        )
        # The words stand on a screen laid over the finished frame, painted
        # once and kept while the tree is unchanged.
        self.words = ctx.textureScene((WIDTH, HEIGHT))
        self.sheet = (
            compose.graphics("observatory.words", self.labels)
            .width(WIDTH)
            .height(HEIGHT)
        )

    def describe(self, seconds: float) -> world.Frame:
        model = world.group(
            world.directionalLight((-0.5, -0.8, -0.5), "#fff0d5", 2.4),
            world.directionalLight((0.8, -0.1, -0.4), "#7bbdff", 1.5),
            world.directionalLight((0.0, 0.8, 0.8), "#e39b80", 1.1),
            world.ambientLight("#8fb0e0", 0.25),
            world.camera(LENS, toneCurve=world.ToneCurve.Neutral),
            world.mesh(self.plinth)
            .key("plinth")
            .transform(camera.place((0, -90, 0)))
            .fill(self.dark),
            world.mesh(self.ring)
            .key("ring")
            .transform(camera.place((0, -73, 0)))
            .fill(self.rule),
            world.mesh(self.knot)
            .key("knot")
            .transform(camera.place((0, 20, 0), yawDegrees=20 + seconds * 12))
            .backface(material.Backface.Visible)
            .fill(self.finishes[0]),
            world.mesh(self.vase)
            .key("vase")
            .transform(camera.place((-240, -63, 0), scale=0.75))
            .fill(self.finishes[1]),
            world.mesh(self.solid)
            .key("solid")
            .transform(camera.place((245, -35, 0), yawDegrees=-seconds * 20))
            .fill(self.finishes[2]),
        ).key("observatory")
        self.ground.render(self.groundSheet, seconds)
        self.words.render(self.sheet, seconds)
        return (
            world.Frame(model)
            .backdrop(self.ground.texture())
            .overlay(self.words.texture())
        )

    def rules(self, pen: Pen) -> None:
        """The ground the forms stand in front of: the sheet's colour and a
        ruled field behind the plinth."""
        pen.background(self.look.palette.ground)
        pen.noStroke()
        pen.fill(self.grid)
        pen.rect(280, 70, 1080, 720)

    def labels(self, pen: Pen) -> None:
        """The sheet's words: the heading on the left, a caption under each
        form where the lens puts it, and the footer."""
        look = self.look
        pen.noStroke()
        pen.textAlign(LEFT, TOP)
        pen.fill(look.palette.ash)
        pen.textFont(look.font(look.type.eyebrow))
        pen.text("FORM LABORATORY     /     003", 52, 42)
        pen.fill(look.palette.ink)
        pen.textFont(look.font(look.type.title))
        pen.text("A surface\nfrom a function.", 52, 100)
        pen.fill(look.palette.ash)
        pen.textFont(look.font(look.type.subtitle))
        pen.text(
            "Python describes the shape.\nThe native world gives it light.", 55, 218
        )
        pen.fill(look.palette.figure)
        pen.textFont(look.font(look.type.captionLabel))
        pen.text("PARAMETRIC KNOT", 55, 315)
        pen.fill(look.palette.ash)
        pen.textFont(look.font(look.type.control))
        pen.text(
            f"{self.knot.vertexCount():,} vertices\n{self.knot.triangleCount():,} triangles\n3 directional lights\n1 native mesh type",
            55,
            344,
        )

        for label, position, color in [
            ("01 / REVOLVED PROFILE", (-240, -125, 0), "#d9a66e"),
            ("02 / PARAMETRIC SHEET", (0, -130, 0), "#65c9c3"),
            ("03 / TWELVE FACES", (245, -115, 0), "#a8bbd6"),
        ]:
            p = LENS.project(position, (WIDTH, HEIGHT))
            if p:
                pen.fill(color)
                pen.textFont(look.font(look.type.captionLabel))
                pen.text(label, p[0] - 70, p[1] + 30)

        pen.stroke(look.palette.rule)
        pen.strokeWeight(1)
        pen.line(52, 810, 1348, 810)
        pen.noStroke()
        pen.fill(look.palette.ash)
        pen.textFont(look.font(look.type.footer))
        pen.text(
            "mesh.grid(180, 22, knot)   ·   mesh.revolve(profile)   ·   mesh.platonic(solid)",
            52,
            835,
        )
        pen.fill(look.palette.ash)
        pen.textFont(look.font(look.type.control))
        pen.text("NATIVE GEOMETRY / WORLD FRAME / DEVICE", 958, 840)
