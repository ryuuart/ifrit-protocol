"""3D authoring uses each native library's own vocabulary and typed values."""

from sigil import material, media, world
from sigil.geometry import mesh
from sigil.geometry.mesh import pop
from sigil.material import Material
from sigil.material import surface as surfaces
from sigil.motion import animatable

parameters = surfaces.SurfaceParameters(baseColor="#a3c4bd", roughness=0.3)
finish: Material = surfaces.program(parameters)
spin = animatable(12)
part = (
    world.mesh(mesh.torus(50, 15))
    .key("part")
    .fill(finish)
    .at((0, 10, 0))
    .rotateY(spin)
    .collection("sculpture")
)
subject = world.group(part, part.copy().key("other").translateX(120))
listed = world.group([world.group().key("list")])
rig = world.kit.Rig(color=(1, 0.9, 0.8, 1), intensity=spin)
setup = world.kit.Set(rig=rig, table=world.kit.Turntable(period=16), surface=finish)
model = world.kit.litSet(subject, setup, now=1)
frame = (
    world.Frame(model)
    .extent((320, 240))
    .pass_(world.geometryPass("main").writes(("color",)).clear("#152334"))
    .pass_(world.postPass("grade").reads("color").writes("final").levels(1.1, 0))
    .pass_(world.postPass("glow").reads("final").only("sculpture"))
    .present("final")
)
scene = world.Scene()
scene.render(frame)
scene.advance(1 / 60)
sun = world.directionalLight((-1, -1, -1), color=(1, 1, 1, 1)).lightLink(
    world.Collection(include=["sculpture"], exclude=["/hidden"])
)
direction = sun.description.direction
studio = world.kit.environmentMap(
    zenith=(0.8, 0.8, 0.85),
    nadir=(0.2, 0.2, 0.25),
    softBoxes=[world.kit.SoftBox(centre=(0.2, 0.4), radiance=(4, 4, 3))],
)
lit = world.Frame(model).environment(studio, intensity=1.2, shown=0.5)

sheet: material.Texture = (
    material.Texture(media.fromRgba(bytes(64), 4, 4))
    .tile(material.Repeat.Pad)
    .sampling(material.Sampling.Nearest)
    .colorSpace(media.ColorSpace.Srgb)
    .mipmaps(False)
)
cell: material.PixelRect = material.PixelRect(x=0, y=0, width=2, height=2)
region = sheet.copy().region(cell).region()
played: Material = material.flipbook(
    sheet,
    material.FlipbookOptions(
        columns=2, rows=2, frame=animatable(1), frameAttribute="cell"
    ),
)
flowing: Material = material.flowmap(
    material.image(sheet), material.FlowmapOptions(flow=sheet, strength=0.2)
)
key: float = material.instanceAttributeKey("cell")
embers: mesh.Instances = (
    pop.on([(0, 0, 0), (10, 0, 0), (10, 10, 0)])
    .count(12)
    .jitter(1.0, pop.Attribute.P)
    .vary(0.2)
    .select("near", (0, 0, 0), 5.0)
    .instances(mesh.quad(2, 2), ["cell"])
)
sparkle = world.mesh(embers).key("embers").facing(world.Facing.Camera).fill(played)
