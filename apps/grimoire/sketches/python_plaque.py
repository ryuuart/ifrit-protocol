"""A Compose page as the material of a body: a copper title raised on a stone plaque, lit in a 3D scene."""

# TAGS: World/Scenes, Compose/Materials, Typography/Relief

import math

from sigil import compose, material, motion, world
from sigil.geometry import mesh
from sigil.motion import ease
from sigil.sketch import Scene3DContext, sketch

WIDTH, HEIGHT = 960, 540  # the canvas
PAGE_WIDTH, PAGE_HEIGHT = 768, 384  # the page the plaque wears, in page pixels
PLATE_WIDTH, PLATE_HEIGHT = 320, 160  # the plaque's size in the scene
GROUND = "#0f1216"
STONE = "#d9d0b8"
INK = "#2b3a33"
COPPER = "#b16d46"

# What the page is made of. Each fill states the surface it paints, and the
# page paints every map those surfaces name: the stone is rough and dull, the
# lettering polished metal on a rounded shoulder, the caption ink sits flat.
STONE_FACE = (
    material.from_(STONE)
    .layer(
        material.noise(0.02, octaves=3, seed=4, grain=True),
        blend=material.BlendMode.Multiply,
        opacity=0.16,
    )
    .surface(roughness=0.85)
)
COPPER_METAL = material.from_(COPPER).surface(metallic=1.0, roughness=0.32)
CAPTION_INK = material.from_(INK).surface(roughness=0.7)


# The rule under the title is copper that glows.
GLOWING_RULE = material.from_(COPPER).surface(
    metallic=1.0, roughness=0.3, emission=COPPER, emissionStrength=12.0
)


def plaque_face() -> compose.Element:
    """The plaque's face: ordinary Compose — a raised title, a glowing rule, a caption."""
    title = (
        compose.text("PLAQUE", size=150, color=COPPER)
        .fontWeight(700)
        .letterSpacing(14)
        .decorationOutline(compose.Boundary.Glyphs)
        .foreground(compose.relief(COPPER_METAL, shoulder=5, depth=1.2))
    )
    return (
        compose.box(
            title,
            compose.box().width(540).height(6).borderRadius(3).fill(GLOWING_RULE),
            compose.text(
                "compose paints the maps · world lights them", size=26, color=INK
            ).ink(CAPTION_INK),
        )
        .width(PAGE_WIDTH)
        .height(PAGE_HEIGHT)
        .fill(STONE_FACE)
        .column()
        .gap(22)
        .justifyContent("center")
        .alignItems("center")
    )


STUDIO = world.kit.environmentMap(
    zenith=(0.55, 0.6, 0.7),
    nadir=(0.08, 0.07, 0.06),
    softBoxes=[
        world.kit.SoftBox(centre=(0.25, 0.3), radiance=(6, 5.6, 5), size=(0.18, 0.1)),
        world.kit.SoftBox(centre=(0.7, 0.35), radiance=(3, 3.4, 4), size=(0.1, 0.12)),
    ],
)


@sketch(size=(WIDTH, HEIGHT), background=GROUND, captureAt=2.0)
class Plaque:
    def setup(self, ctx: Scene3DContext) -> None:
        # One page, asked for once. Every frame carries its pass over the
        # same face, which the page paints the first time and keeps while
        # the face is unchanged; the plaque wears what it painted.
        self.page = ctx.page((PAGE_WIDTH, PAGE_HEIGHT))
        self.face = plaque_face()
        self.plate = mesh.quad(PLATE_WIDTH, PLATE_HEIGHT)
        ctx.camera(
            mesh.camera.Camera(eye=(0, 150, 330), target=(0, 0, 0), fovYDegrees=36)
        )

    def describe(self, seconds: float) -> world.Frame:
        pulse = 0.5 + 0.5 * math.sin(seconds * 2 * math.pi / 3)  # a three-second breath
        key_angle = math.radians(
            -60 + 120 * (0.5 + 0.5 * math.sin(seconds * 2 * math.pi / 8))
        )
        model = world.group(
            world.directionalLight(
                (math.sin(key_angle), -0.7, -0.5),
                intensity=2.2 + 0.8 * pulse,
                castShadow=True,
            ).key("key"),
            world.hemisphereLight(
                color="#9fb3c8", groundColor="#2a241c", intensity=0.35
            ),
            world.mesh(self.plate)
            .key("plaque")
            .rotateX(-12)
            .rotateY(motion.animate(from_=-90, to=0, duration=1.6, ease=ease.outCubic))
            .fill(self.page.surface()),
        ).key("scene")
        return (
            world.Frame(model)
            .environment(STUDIO, intensity=1.2)
            .pass_(self.page.render(self.face))
            .pass_(world.bloom(threshold=1.8, intensity=0.6, radius=28))
        )
