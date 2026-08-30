# SigilUsd

SigilUsd moves the world's data in and out of USD. Out: a `Writer` builds
a stage from the values a scene is made of — meshes with their placements
and material slots, stamps as point instancers, lights, a camera — and
saves it as binary crate (`.usdc`, the default), ASCII (`.usda`), or a
`.usdz` package. In: `readModel()` pours a USD stage's meshes, point
instancers and materials into `geometry::decode::Model`, the same currency
every other format lands in.

Materials travel as `UsdPreviewSurface` with `UsdUVTexture` inputs — the
shading model this renderer shades with, slot for slot — and their images
are written as PNG files beside the stage.

Namespace `sigil::usd`, target `SigilUsd`, header `sigilusd/Usd.h`.

## Using it

```cpp
#include <sigilusd/Usd.h>

usd::Writer writer("shots/lab.usdc");
writer.mesh("floor", floorMesh, glm::mat4(1.0f), plate);
writer.mesh("torus", torus, place({420, 20, 0}), {plate, rubber});  // slots
writer.stamps("sparks", cloud, geometry::mesh::quad(4, 4), glm::mat4(1.0f), glow);
writer.sun("sun", lighting);
writer.camera("camera", camera);
std::string error;
if (!writer.save(&error)) std::fprintf(stderr, "%s\n", error.c_str());

std::optional<geometry::decode::Model> back = usd::readModel("shots/lab.usdc");
```

## The mental model

**Values in, a stage out.** The writer never looks inside a `World` — the
GPU has the meshes, not the CPU — it takes the same values you placed:
`geometry::Mesh`, `glm::mat4`, `world::Material` (or a slot list),
`geometry::Cloud`, `world::LightComponent`, `world::Lighting`,
`geometry::space::Camera`. Keep those around (as `world_demo`'s material lab
does) and writing the scene is one call per prop.

**What goes where.**

| ours | USD |
| --- | --- |
| a mesh's positions / normals / uvs / colours | `UsdGeomMesh` points, normals (vertex), `st` primvar (v flipped: USD's runs up the image), `displayColor` + `displayOpacity` |
| a mesh's prim lanes | uniform `float4[]` primvars under the lane's name |
| the `"Material"` lane + slots | `UsdGeomSubset`s (family `materialBind`) bound to one material each; one slot binds the whole mesh |
| a `Material` | `UsdPreviewSurface` under `/World/Materials`, textures as `UsdUVTexture` reading `st`, wrap by `tile`, sRGB/raw by role, normal maps remapped through the node's `scale`/`bias` (the DirectX flag flips green); channels of packed maps select the output; `alphaCutoff` → `opacityThreshold`; `ior` |
| what UsdPreviewSurface has no word for | custom data on the prim: `sigil:transmission`, `sigil:layers` (count), `sigil:unlit`, `sigil:baseColorFactor` |
| stamps | `UsdGeomPointInstancer` with the stamp as its one prototype: positions, `size` → scales, `dir`/`normal` → orientations (the stamp's +z along it), `tint` → `displayColor`/`displayOpacity` |
| a point light / the sun | `UsdLuxSphereLight` (translated, `sigil:range`) / `UsdLuxDistantLight` (oriented, -Z along the direction) |
| the camera | `UsdGeomCamera`, camera-to-world from the view's inverse, a 24 mm vertical aperture and the focal length that gives the vertical fov |

**A layered material exports its base.** Layers are this renderer's live
composition; `UsdPreviewSurface` cannot hold them, and this library does
not bake. The layer count rides as `sigil:layers` so a consumer knows
something is missing.

**Reading unwelds.** Every face-vertex becomes a mesh vertex (so
face-varying `st` and normals survive), faces fan-triangulate, xforms are
baked into positions, `st`'s v is flipped back, subsets become the
`"Material"` lane with `materialIndex` set from the mesh's whole binding,
and a bound `UsdPreviewSurface` fills the part's factors and texture
references (bytes read from the stage's neighbours). Point instancers
come back as faceless parts with `size` from scales.

## Conventions that will bite you

**Matrices are transposed on the way through, elementwise.** glm's
`m[column][row]` copied element for element into `GfMatrix4d[row][column]`
*is* the transpose USD's row-vector convention wants; there is no
explicit transpose call in either direction.

**Identifiers are sanitized and made unique.** A name with spaces or
punctuation becomes underscores; a second prop with the same name gets
`_2`. Read the returned path back rather than assuming.

**Same material, one prim.** Materials compare by value (images by
pointer), so the same `Material` placed on ten props binds one
`/World/Materials/...` prim.

**Metres per unit is metadata, not a scale.** Meshes are written in the
units they were authored in; `WriteOptions::metersPerUnit` (0.01 by
default — centimetres, the DCC default) tells a consumer how to read
them.

## Boundary

Public: SigilWorld, SigilGeometry, Skia. Private: OpenUSD core (`usd`,
`usdGeom`, `usdShade`, `usdLux`, `sdf`, `tf`, `gf`, `vt`) — no imaging,
no MaterialX. Neither SigilWorld nor SigilGeometry links or includes this
library; it is a leaf. It does not bake materials or generate maps of any
kind: what it writes are the images and values it was handed.

## Build and test

OpenUSD comes from vcpkg (`usd`, default features off: `tbb` and `zlib`
only). When the package is not found the top-level configure warns and
leaves `SigilUsd`, `usd_test` and `world_demo`'s USD export out.

```sh
ctest --test-dir build -C Debug -R usd_test --output-on-failure
```

The test writes a crate and an ASCII stage to a temporary directory,
checks the crate's magic bytes, and reads both back.
