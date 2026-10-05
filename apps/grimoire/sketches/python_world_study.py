"""A native World frame baked on the device: one vessel, three finishes, one moving rig."""

# TAGS: World/Scenes, Geometry/Meshes, Materials/Surfaces

from sigil import world
from sigil.draw import LEFT, TOP, Pen
from sigil.geometry import mesh
from sigil.material import hexColor
from sigil.material import surface as surfaces
from sigil.sketch import SketchContext, sketch

INK = "#ebeee7"
MUTED = "#93a4aa"
PAPER = "#141d25"


@sketch(size=(1200, 820), background=PAPER, capture_at=1.0)
class WorldStudy:
    def setup(self, ctx: SketchContext) -> None:
        self.vessel = mesh.revolve(
            [
                (0, -64),
                (40, -64),
                (45, -52),
                (52, -15),
                (42, 24),
                (24, 54),
                (25, 70),
                (19, 70),
                (18, 54),
            ],
            segments=64,
        )
        self.plinth = mesh.box((-80, -9, -68), (80, 9, 68))
        self.finishes = (
            surfaces.program(
                surfaces.SurfaceParameters(baseColor="#87b9ad", roughness=0.45)
            ),
            surfaces.program(
                surfaces.SurfaceParameters(baseColor="#dfa573", roughness=0.45)
            ),
            surfaces.program(
                surfaces.SurfaceParameters(baseColor="#bdcbd8", roughness=0.45)
            ),
        )
        self.base = surfaces.program(
            surfaces.SurfaceParameters(baseColor="#283642", roughness=0.9)
        )
        self.camera = mesh.camera.Camera()
        self.camera.eye = (0, 175, 510)
        self.camera.target = (0, -6, 0)
        self.camera.fovYDeg = 30

    def draw(self, pen: Pen, ctx: SketchContext) -> None:
        pen.background(PAPER)
        seconds = pen.millis() / 1000
        pieces = [
            world.group(
                world.mesh(self.vessel)
                .key(f"vessel-{index}")
                .fill(finish)
                .rotateY(seconds * 14),
                world.mesh(self.plinth)
                .key(f"plinth-{index}")
                .fill(self.base)
                .translateY(-74),
            )
            .key(f"station-{index}")
            .at((x, 0, 0))
            for index, (x, finish) in enumerate(zip((-205, 0, 205), self.finishes))
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
        # A set is drawn on a device and nowhere else: the bake answers
        # None where the host brought none up, and the sheet says so.
        still = ctx.bakeSet(
            world.Frame(model),
            (1104, 510),
            camera=self.camera,
            background=hexColor(0x141D25),
        )
        if still is not None:
            pen.image(still, 48, 150)
        else:
            pen.noStroke()
            pen.fill(MUTED)
            pen.textAlign(LEFT, TOP)
            pen.textSize(15)
            pen.text("This sheet bakes its set on a device, and none is here.", 48, 380)

        pen.noStroke()
        pen.textAlign(LEFT, TOP)
        pen.fill(MUTED)
        pen.textSize(12)
        pen.text("WORLD STUDY  /  01", 48, 36)
        pen.fill(INK)
        pen.textSize(38)
        pen.text("One form. Three surfaces.", 48, 68)
        pen.fill(MUTED)
        pen.textSize(15)
        pen.text(
            "One shared mesh and light rig; only the material’s base color changes.",
            50,
            123,
        )

        for x, number, title, detail, color in (
            (48, "01", "Celadon glaze", "base color · #87b9ad", "#87b9ad"),
            (424, "02", "Terracotta glaze", "base color · #dfa573", "#dfa573"),
            (800, "03", "Porcelain glaze", "base color · #bdcbd8", "#bdcbd8"),
        ):
            pen.stroke("#364550")
            pen.strokeWeight(1)
            pen.line(x, 662, x + 352, 662)
            pen.noStroke()
            pen.fill(color)
            pen.textSize(12)
            pen.text(number, x, 679)
            pen.fill(INK)
            pen.textSize(19)
            pen.text(title, x + 32, 675)
            pen.fill(MUTED)
            pen.textSize(13)
            pen.text(detail, x + 32, 705)

        pen.fill(MUTED)
        pen.textSize(12)
        pen.text("WORLD FRAME / DEVICE EXECUTOR", 48, 751)
        pen.text(
            "Dielectric surfaces share roughness 0.45. This light rig has no environment reflections.",
            48,
            776,
        )
