# SigilUsd

SigilUsd moves the world's data in and out of USD. Out: a `Writer` builds
a stage from the values a scene is made of — meshes with their placements
and material slots, stamps as point instancers, lights, a camera — and
saves it as binary crate (`.usdc`, the default), ASCII (`.usda`), or a
`.usdz` package. In: `readModel()` pours a USD stage's meshes, point
instancers and materials into `geometry::mesh::codec::decode::Model`,
the same currency every other format lands in, and `readLights()` and
`readCameras()` hand back its emitters and cameras as the same values
the writer took — so a stage this library authors round-trips whole.

Materials travel as `UsdPreviewSurface` with `UsdUVTexture` inputs — the
metallic-roughness surface SigilMaterial's kit defines, slot for slot —
and their images are written as PNG files beside the stage.

Namespace `sigil::usd`. One feature library per directory, linked by what
a consumer uses; every public header lives under
`include/sigilusd/<feature>/` and is spelled `<sigilusd/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilUsdRuntime` | `runtime/Runtime.h` | `runtime::available()` — whether the USD file-format plugins are present in this process |
| `SigilUsdWrite`   | `write/Writer.h`    | `WriteOptions` and `Writer` — a stage built from values and saved |
| `SigilUsdRead`    | `read/Reader.h`     | `ReadInfo` and `readModel()` — a stage read into a `Model`; `readLights()` and `readCameras()` — its emitters and cameras as values |

`SigilUsd` is the umbrella target over all three, and
`<sigilusd/Usd.h>` the umbrella header. Write and read are independent
of each other; neither links the other.

## Using it

```cpp
#include <sigilusd/write/Writer.h>
#include <sigilusd/read/Reader.h>

usd::Writer writer("shots/lab.usdc");
writer.mesh("floor", floorMesh, glm::mat4(1.0f), plate);
writer.mesh("torus", torus, place({420, 20, 0}), {plate, rubber});  // slots
writer.stamps("sparks", cloud, geometry::mesh::quad(4, 4), glm::mat4(1.0f), glow);
writer.light("sun", world::light::sun({-0.45f, -0.75f, -0.5f}));
writer.camera("camera", camera);
std::string error;
if (!writer.save(&error)) std::fprintf(stderr, "%s\n", error.c_str());

auto back = usd::readModel("shots/lab.usdc");      // decode::Model
auto lamps = usd::readLights("shots/lab.usdc");    // ReadLight: path + Light
auto lenses = usd::readCameras("shots/lab.usdc");  // ReadCamera: path + Camera
```

## The mental model

**Values in, a stage out.** The writer never looks inside a renderer — the
GPU has the meshes, not the CPU — it takes the same values you placed:
`geometry::mesh::Mesh`, `glm::mat4`, `material::Material` (or a slot
list), `geometry::mesh::Cloud`, `world::light::Light`,
`geometry::mesh::camera::Camera`. Keep those around and writing the
scene is one call per prop.

**What goes where.**

| ours | USD |
| --- | --- |
| a mesh's positions / normals / uvs / colours | `UsdGeomMesh` points, normals (vertex), `st` primvar (v flipped: USD's runs up the image), `displayColor` + `displayOpacity` |
| a mesh's prim lanes | uniform `float4[]` primvars under the lane's name |
| the `"Material"` lane + slots | `UsdGeomSubset`s (family `materialBind`) bound to one material each; one slot binds the whole mesh |
| a `Material` | `UsdPreviewSurface` under `/World/Materials`, the surface recipe's params read by name and its map slots as `UsdUVTexture` nodes reading `st`, wrap by each texture's own tiling, sRGB/raw by role, normal maps remapped through the node's `scale`/`bias` (the DirectX param flips green); the channel params select the output of a packed map; `alphaCutoff` → `opacityThreshold`; `ior` |
| what UsdPreviewSurface has no word for | custom data on the prim: `sigil:transmission`, `sigil:layers` (the stack depth), `sigil:unlit`, `sigil:baseColorFactor` |
| stamps | `UsdGeomPointInstancer` with the stamp as its one prototype: positions, `size` → scales, `dir`/`normal` → orientations (the stamp's +z along it), `tint` → `displayColor`/`displayOpacity` |
| a point light or a spot / a sun | `UsdLuxSphereLight` (translated, `sigil:range`; a spot oriented -Z along its direction, cone as `shaping:cone:angle` with the inner edge as `shaping:cone:softness`) / `UsdLuxDistantLight` (oriented, -Z along the direction) |
| an environment map | `UsdLuxDomeLight`: the panorama beside the stage as a sixteen-bit PNG on `inputs:texture:file`, `textureFormat` `latlong`, the strength and the tint on `intensity` and `color`, the orientation as the prim's transform, and the dials UsdLux has no word for as `sigil:diffuse`, `sigil:specular`, `sigil:roughnessBias`, `sigil:backdrop`, `sigil:backdropBlur`, `sigil:groundRadius` |
| the camera | `UsdGeomCamera`, camera-to-world from the view's inverse, a 24 mm vertical aperture and the focal length that gives the vertical fov, the clipping range, and the distance to the target as `focusDistance` |

**A panorama is written scaled and the scale rides the intensity.** A
sky holds values above one — that is what makes a sun a sun rather than
a white disc the same brightness as the sky beside it — and no encoder
in this tree writes a floating-point image. So the panorama is divided
by its peak, written as a sixteen-bit PNG, and the peak multiplied into
the dome light's `intensity`: the ratios survive at sixteen bits a
channel, the total radiance is right, and it is right through the
standard attribute rather than through a custom one only this library
reads. A stage written and read again lights a set as it was described.

**A stacked material exports the material at the bottom.** Stacking is a
live composition; `UsdPreviewSurface` cannot hold it, and this library
does not bake. The depth rides as `sigil:layers` so a consumer knows
something is missing.

**Emitters and cameras read back as values, not parts.** A
`UsdLuxDistantLight` is a sun aimed along the prim's -Z, a
`UsdLuxSphereLight` a point light where the prim stands — a spot when
the prim carries an *authored* shaping cone, whose angle is the outer
half-angle and whose softness gives the inner edge back as `outer × (1 -
softness)`. `sigil:range` is the range when the stage has it and the
`Light` default when it does not, so a light another tool authored still
reads. A camera comes back from the prim's local-to-world, with its
field of view from the focal length against the vertical aperture; its
target rides the view direction at the focus distance, one unit ahead
when the stage names none — a camera sees the same thing wherever along
that ray the target sits, which is why the distance has to be written
down to come back. A `UsdLuxDomeLight` comes back as an environment's
dials and the NAME of its panorama file, not its pixels: this library
opens no image, the way it opens no texture for a material either, so a
caller decodes the file and builds the map. Other UsdLux shapes are
skipped.

**Reading unwelds.** Every face-vertex becomes a mesh vertex (so
face-varying `st` and normals survive), faces fan-triangulate, xforms are
baked into positions, `st`'s v is flipped back, subsets become the
`"Material"` lane with `materialIndex` set from the mesh's whole binding,
and a bound `UsdPreviewSurface` fills the part's factors and texture
references (bytes read from the stage's neighbours). When the mesh as a
whole binds nothing, the first subset's material fills the factors.
Point instancers come back as faceless parts with `size` from scales.

**The runtime is a plugin registry.** USD's file formats are discovered
on disk when the process first touches USD; a build whose libraries are
present but whose `plugInfo.json` registry beside them is not will link,
start, and open nothing. `runtime::available()` asks for the crate,
ASCII and package formats by extension and creates one in-memory stage,
and names what is missing. Every test and benchmark in this library
skips through it rather than failing.

## Conventions that will bite you

**Matrices are transposed on the way through, elementwise.** glm's
`m[column][row]` copied element for element into `GfMatrix4d[row][column]`
*is* the transpose USD's row-vector convention wants; there is no
explicit transpose call in either direction.

**Identifiers are sanitized and made unique.** A name with spaces or
punctuation becomes underscores, one that is empty or starts with a
digit gains a leading underscore, and a second prop with the same name
gets `_2`. Read the returned path back rather than assuming.

**Same material, one prim.** Materials compare by value (images by
pointer), so the same `Material` placed on ten props binds one
`/World/Materials/...` prim, and the same image is written once however
many materials share it.

**Metres per unit is metadata, not a scale.** Meshes are written in the
units they were authored in; `WriteOptions::metersPerUnit` (0.01 by
default — centimetres, the DCC default) tells a consumer how to read
them.

**`sigil:` custom data nests.** USD reads a colon in a custom-data key
as a path into nested dictionaries, so `sigil:transmission` is the
`transmission` entry of a `sigil` dictionary — which is how a stage
authored by hand must spell it.

## Boundary

Public: SigilMaterialKit for the surface a preview surface is written
from, SigilWorldLight for the emitters, and the geometry features each
door takes values from (mesh, pop and camera for the writer; codec and
mesh for the reader), plus Skia. Private: OpenUSD core (`usd`, `usdGeom`, `usdShade`, `usdLux`,
`sdf`, `tf`, `gf`, `vt`) — no imaging, no MaterialX, and no public
header names a `pxr` type; USD is included only by the sources and the
internal headers beside them. Nothing beneath it links or includes this
library; it is a leaf. It does not bake materials or
generate maps of any kind: what it writes are the images and values it
was handed.

## Build and test

OpenUSD comes from vcpkg (`usd`, default features off: `tbb` and `zlib`
only). When the package is not found the top-level configure warns and
leaves every target here out.

Targets: `SigilUsdRuntime`, `SigilUsdWrite`, `SigilUsdRead`, the
`SigilUsd` umbrella; `usd_runtime_test`, `usd_write_test` and
`usd_read_test` (ctest); `usd_runtime_bench`, `usd_write_bench` and
`usd_read_bench` (Google Benchmark, through the `benches` target and
`scripts/bench_ledger.py`).

```sh
ctest --test-dir build -C Debug -R usd_ --output-on-failure
```

The write test authors stages into a temporary directory and inspects
them through USD's own API — prim paths and their uniqueness, the mesh's
attributes and subsets, shared material prims and the single texture
file behind them, the instancer, the file formats an extension selects.
The read test reads the hand-authored stages committed under
`read/test/assets/` (an ASCII stage with a parent xform, a mixed
triangle-and-quad mesh with per-vertex `st` and `displayColor`, two
subsets bound to two materials, a texture file beside it, and a point
instancer, and a stage as another tool would author it: a sphere light
with a shaping cone and no `sigil:` data, aimed by its own rotation
under a translated parent, beside a camera with no focus distance) and
round-trips a stage the writer produced — its meshes, its three kinds of
emitter and its camera. Every test skips,
with the reason, when the runtime probe says the plugins are absent, and
every benchmark then registers nothing.
