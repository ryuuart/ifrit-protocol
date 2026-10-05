"""3D authoring uses each native library's own vocabulary and typed values."""

from sigil import world
from sigil.geometry import mesh
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
