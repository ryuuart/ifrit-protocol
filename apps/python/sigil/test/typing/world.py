"""3D authoring uses each native library's own vocabulary and typed values."""

from sigil import world
from sigil.geometry import mesh
from sigil.material import Material
from sigil.material import kit as surfaces
from sigil.motion import Output
from sigil.skia import Image

parameters = surfaces.SurfaceParameters(baseColor="#a3c4bd", roughness=0.3)
finish: Material = surfaces.surface(parameters)
spin = Output(12)
part = (
    world.Element()
    .key("part")
    .mesh(mesh.torus(50, 15))
    .fill(finish)
    .at((0, 10, 0))
    .rotateY(spin)
    .tag("sculpture")
)
subject = world.Element().children(part, part.copy().key("other").translateX(120))
subject.children((world.Element().key("tuple"),))
subject.children([world.Element().key("list")])
rig = world.kit.Rig(at=(0, 10, 0), color=(1, 0.9, 0.8, 1))
setup = world.kit.Set(rig=rig, table=world.kit.Turntable(period=16), surface=finish)
model = world.kit.litSet(subject, setup, seconds=1)
frame = (
    world.Frame(model)
    .extent((320, 240))
    .pass_(world.geometryPass("main").writes(("color",)).clear("#152334"))
    .pass_(world.postPass("grade").reads("color").writes("final").levels(1.1, 0))
    .present("final")
)
scene = world.Scene()
scene.render(frame)
scene.advance(1 / 60)
plate: Image = scene.image((320, 240), background="#152334")
sun = world.light.sun(direction=(-1, -1, -1), color=(1, 1, 1, 1))
sun.color = [1, 0.9, 0.8, 1]
direction: tuple[float, float, float] = sun.direction
selection = world.selectors.tag("sculpture") & ~world.selectors.key("hidden")
