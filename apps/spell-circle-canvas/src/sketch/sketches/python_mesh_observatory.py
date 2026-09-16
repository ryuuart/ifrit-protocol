"""A parametric sculpture, lathed vessel and solid displayed by the native mesh painter.

The forms are Python data and functions. Tessellation, camera projection,
normals, lighting and triangle drawing are the native geometry library's.
"""

# TAGS: Geometry/Meshes, Drawing/Generative

from math import cos, pi, sin

from sigil.draw import LEFT, TOP, Pen
from sigil.geometry import mesh
from sigil.material import pattern
from sigil.sketch import SketchContext, sketch

camera = mesh.camera
render = mesh.render


def knot(u, v):
    t, section = u * 2 * pi, v * 2 * pi
    radius = 116 + 35 * cos(3 * t) + 13 * cos(section)
    return radius * cos(2 * t), 44 * sin(3 * t) - 13 * sin(section), radius * sin(2 * t)


def surface(color, *, gloss=0.8):
    style = render.MeshStyle()
    style.baseColor = color
    style.ambient = "#152334"
    style.lights = [
        render.Light((-0.5, -0.8, -0.5), "#fff0d5", 1.1),
        render.Light((0.8, -0.1, -0.4), "#7bbdff", 0.85),
        render.Light((0.0, 0.8, 0.8), "#e39b80", 0.55),
    ]
    style.specular, style.shininess, style.rim = gloss, 72, 0.32
    return style


@sketch(size=(1400, 900), background="#101a25", capture_at=0.5)
class MeshObservatory:
    def setup(self, ctx: SketchContext) -> None:
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
        self.styles = [surface("#65c9c3"), surface("#d9a66e"), surface("#a8bbd6")]
        self.dark = surface("#293a4e", gloss=0.2)
        self.rule = surface("#456073", gloss=0.1)
        self.grid = pattern.gridLines(40, 0.6, "#21313d").paint()
        self.view = camera.Camera()
        self.view.eye, self.view.target, self.view.fovYDeg = (
            (320, 280, 680),
            (0, 0, 0),
            39,
        )

    def draw(self, pen: Pen) -> None:
        t = pen.millis() / 1000
        pen.background("#101a25")
        pen.push()
        pen.translate(280, 70)
        pen.fill(self.grid)
        pen.noStroke()
        pen.rect(0, 0, 1080, 720)
        pen.pop()

        render.drawMesh(
            pen, self.plinth, camera.place((0, -90, 0)), self.view, self.dark
        )
        render.drawMesh(pen, self.ring, camera.place((0, -73, 0)), self.view, self.rule)
        render.drawMesh(
            pen,
            self.knot,
            camera.place((0, 20, 0), yawDeg=20 + t * 12),
            self.view,
            self.styles[0],
        )
        render.drawMesh(
            pen,
            self.vase,
            camera.place((-240, -63, 0), scale=0.75),
            self.view,
            self.styles[1],
        )
        render.drawMesh(
            pen,
            self.solid,
            camera.place((245, -35, 0), yawDeg=-t * 20),
            self.view,
            self.styles[2],
        )

        pen.noStroke()
        pen.textAlign(LEFT, TOP)
        pen.fill("#7d9bae")
        pen.textSize(13)
        pen.text("FORM LABORATORY     /     003", 52, 42)
        pen.fill("#edf0e9")
        pen.textSize(42)
        pen.text("A surface\nfrom a function.", 52, 100)
        pen.fill("#9bb0bb")
        pen.textSize(15)
        pen.text(
            "Python describes the shape.\nThe native painter gives it light.", 55, 218
        )
        pen.fill("#65c9c3")
        pen.textSize(13)
        pen.text("PARAMETRIC KNOT", 55, 315)
        pen.fill("#a8bac5")
        pen.textSize(14)
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
            p = self.view.project(position, (pen.width, pen.height))
            if p:
                pen.fill(color)
                pen.textSize(12)
                pen.text(label, p.x - 70, p.y + 30)

        pen.stroke("#334855")
        pen.strokeWeight(1)
        pen.line(52, 810, 1348, 810)
        pen.noStroke()
        pen.fill("#afc1cc")
        pen.textSize(14)
        pen.text(
            "mesh.grid(180, 22, knot)   ·   mesh.revolve(profile)   ·   mesh.platonic(solid)",
            52,
            835,
        )
        pen.fill("#688591")
        pen.textSize(12)
        pen.text("NATIVE GEOMETRY / CPU MESH RENDERER / HEADLESS", 958, 840)
