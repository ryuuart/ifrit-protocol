"""A retained native World scene: one vessel, three finishes, one moving rig."""

# TAGS: World/Scenes, Geometry/Meshes, Materials/Surfaces

from sigil import world
from sigil.draw import LEFT, TOP, Pen
from sigil.geometry import mesh
from sigil.material import kit as surfaces
from sigil.sketch import SketchContext, sketch

INK = "#ebeee7"
MUTED = "#93a4aa"
PAPER = "#141d25"


@sketch(size=(1200, 820), background=PAPER, capture_at=1.0)
class WorldStudy:
    def setup(self, ctx: SketchContext) -> None:
        self.scene = world.Scene()
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
            surfaces.surface(surfaces.SurfaceParameters.dielectric("#87b9ad", 0.45)),
            surfaces.surface(surfaces.SurfaceParameters.dielectric("#dfa573", 0.45)),
            surfaces.surface(surfaces.SurfaceParameters.dielectric("#bdcbd8", 0.45)),
        )
        self.base = surfaces.surface(
            surfaces.SurfaceParameters.dielectric("#283642", 0.9)
        )
        self.camera = mesh.camera.Camera()
        self.camera.eye = (0, 175, 510)
        self.camera.target = (0, -6, 0)
        self.camera.fovYDeg = 30

    def draw(self, pen: Pen) -> None:
        pen.background(PAPER)
        seconds = pen.millis() / 1000
        pieces = [
            world.Element()
            .key(f"station-{index}")
            .at((x, 0, 0))
            .children(
                world.Element()
                .key(f"vessel-{index}")
                .mesh(self.vessel)
                .fill(finish)
                .rotateY(seconds * 14),
                world.Element()
                .key(f"plinth-{index}")
                .mesh(self.plinth)
                .fill(self.base)
                .translateY(-74),
            )
            for index, (x, finish) in enumerate(zip((-205, 0, 205), self.finishes))
        ]
        model = (
            world.Element()
            .key("study")
            .children(
                world.Element().key("forms").children(pieces),
                world.kit.threePoint(
                    world.kit.Rig(
                        extent=380,
                        bearing=-35 + seconds * 8,
                        elevation=38,
                        intensity=1.2,
                        fill=0.5,
                    )
                ),
            )
        )
        self.scene.render(world.Frame(model).camera(self.camera))
        pen.image(self.scene.image((1104, 510), background=PAPER), 48, 150)

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
        pen.text("RETAINED WORLD / NATIVE CPU EXECUTOR", 48, 751)
        pen.text(
            "Dielectric surfaces share roughness 0.45. This light rig has no environment reflections.",
            48,
            776,
        )
