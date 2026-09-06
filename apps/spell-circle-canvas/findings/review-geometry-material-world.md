# Merge-readiness review: SigilWorld, SigilUsd, SigilSubstance, SigilImage

Branch `sigil/library-campaigns` vs merge base `aabd3fe1b224`, read-only, every
finding verified against the code at HEAD (paths relative to
`apps/spell-circle-canvas`).

## Coverage

Covered in full: `src/common/world`, `src/common/usd`, `src/common/substance`,
`src/common/image` (sources, tests, CMake, READMEs).

NOT covered: `src/common/geometry` (path ops, point operators, kit, device,
codecs) and `src/common/material` (Ramp, colormaps, palette, harmony, Dither,
phosphorBloom, masks, stock, Slang workflow). Their delegated reviews were lost
with the session; the only geometry item below (`mesh/codec/Model.cpp`) was
found incidentally. Those two libraries (44k of the 49k inserted lines) still
need a pass before merge.

Mechanical checks that DID run over all six libraries: no Diligent, pxr/USD,
Substance, OpenImageIO, OpenColorIO, Alembic or Slang header is included by any
public header under `include/`; no `sigilworld` include appears in geometry,
material or image; the comment-rule greps (dates, "renamed", "used to be",
"legacy", "phase", "campaign", "see the", TODO/FIXME, ms/fps) over world, usd,
substance and image found only domain uses of "phase"/"renamed" and no
violation; every backticked API name in the world, usd, substance and image
READMEs resolves to a symbol in the tree.

## Top ten

1. blocker | src/common/world/graph/Realise.cpp:67 | two masked post passes behind one geometry pass: each iteration writes `into[producer].coverageOut = into[step].coverageIn`, so the second overwrites the first; the first pass's `coverageIn` names a resource nothing writes, `Targets::image()` answers null, and CpuPost.cpp:89 applies the op unmasked with no error | every masked pass reads coverage its producer actually paints | make `PassWork::coverageOut/coverageOf` a vector of (name, selector) pairs, or share one coverage per (producer, selector); add a two-masked-passes test [1][8]
2. should-fix | src/common/world/graph/Order.cpp:99,109 | a reader declared BEFORE the first writer is edged after `writers.front()` (line 99) but the WAR loop's `reader > replaced` test (line 109) gives it no edge to the second writer; with any other dependency it can be scheduled after the second write and read version 2 (declared order [R reads a,x; W1 writes a; W2 writes a; X writes x] runs W1, W2, X, R) | a read runs before the write that replaces the version it reads | when `reader < writers.front() && writers.size() > 1`, `edge(reader, writers[1])`; add the test [1][8]
3. should-fix | src/common/image/decode/Ktx.cpp:292-295,336-337,221 | `rowBytes * height` and `faceBytes * faces` are unchecked size_t products of header fields; dims such as 2^30 x 2^30 at 16 B/texel wrap `faceBytes` to 0, pass the `imageSize`/`byteLength` checks and `view(0)`, and `channelsOf` then `resize`s 2^62 floats — an uncaught `std::bad_alloc` from a ~100-byte file instead of `nullopt` | a malformed header is refused | cap width/height (1<<15) and check each product with `__builtin_mul_overflow` before `view()`; add a crafted-header test [1][8]
4. should-fix | src/common/usd/read/Mesh.cpp:97, src/common/usd/read/Primvar.h:42 | `points[(size_t)indices[fv]]` and `indices[fv]` are unchecked; a stage whose faceVertexIndices exceed `points.size()` or whose sum(counts) != indices.size() reads out of bounds, and Primvar.h indexes `counts[face]` past its end | a malformed mesh is skipped like one with no points | validate sum(counts) == indices.size() and max(index) < points.size() before the fan; add a test [1][8]
5. should-fix | src/common/world/scene/Phases.cpp:356 | `fprintf(stderr, ...)` for a second environment map sits in `foldVolatility`, which the extract phase walks every frame — the warning repeats at frame rate | say it once per change | once-per-key reporting (as `material::reportOnce` does) or surface it through `Scene::error()`; drop `<cstdio>` [5]
6. should-fix | src/common/world/frame/CpuGeometry.cpp:117, src/common/world/diligent/Geometry.cpp:459 | `targets.points(name)` on a non-const `Targets` resolves to the inserting overload (`&m_points[name]`), so every IMAGE name a stamped geometry pass reads gains an empty `Cloud` that is never erased | read without inserting | `std::as_const(targets).points(name)` [1]
7. should-fix | src/common/substance/graph/Inputs.cpp:63 | `heldImages.push_back(held)` on every `setImage`; nothing (not `reset()`) ever releases an entry, so a graph fed a new input image per cook grows without bound | hold one image per image input | keep a map keyed by identifier and replace [1]
8. should-fix | src/common/substance/graph/Render.cpp:63,78 | `render()` ignores what `renderer->run()` reports and returns `!byIdentifier.empty()`; Graph.h promises "false when the engine reports a failure", and a graph with only numeric outputs answers false after a successful cook | header and body agree | check the run result; return true on success regardless of image outputs [2][3]
9. should-fix | src/common/usd/write/Stamps.cpp:63, src/common/usd/write/Lights.cpp:48,64 | `glm::normalize` on a zero-length "dir" lane or light direction yields NaN, which goes into `GfRotation` and is authored as the prim's orientation | a zero direction keeps the default orientation | length-guard as `world/light/Light.cpp:19 unit()` already does [1]
10. should-fix | src/common/world/scene/Draw.cpp:25,37 vs src/common/world/frame/CpuGeometry.cpp:22,77 | `painterLight()` and `dress()` are byte-identical anonymous-namespace copies in two files | one definition | move both to View.cpp beside `surfaceTermsOf` and declare them in View.h [6]

## By category

### 1. Correctness

- blocker | src/common/world/graph/Realise.cpp:67 | (top ten #1)
- should-fix | src/common/world/graph/Order.cpp:99,109 | (top ten #2)
- should-fix | src/common/image/decode/Ktx.cpp:292-295,336-337,221 | (top ten #3)
- should-fix | src/common/usd/read/Mesh.cpp:97, src/common/usd/read/Primvar.h:42 | (top ten #4)
- should-fix | src/common/world/frame/CpuGeometry.cpp:117, src/common/world/diligent/Geometry.cpp:459 | (top ten #6)
- should-fix | src/common/substance/graph/Inputs.cpp:63 | (top ten #7)
- should-fix | src/common/usd/write/Stamps.cpp:63, src/common/usd/write/Lights.cpp:48,64 | (top ten #9)
- nit | src/common/image/decode/Svg.cpp:74 | `pathHint.extension() == ".svg"` is case-sensitive, so `.SVG` reaches only the content sniff | match the writer's lower-casing (usd `Writer.cpp extensionOf`) | lower-case before comparing
- nit | src/common/usd/write/Camera.cpp:35 | `aperture / tan(fovYDeg*pi/360)` with `fovYDeg == 0` writes an infinite focal length | clamp | `std::max(camera.fovYDeg, 1e-3f)`
- nit | src/common/world/scene/Phases.cpp:66 | `placeLight` carries a direction by `glm::mat3(world)`; under a non-uniform scale a direction wants the inverse transpose | normal-transform it | `glm::inverseTranspose(glm::mat3(world)) * light.direction`

### 2. Public API

- should-fix | src/common/substance/graph/Render.cpp:63,78 | (top ten #8)
- should-fix | src/common/world/frame/Targets.cpp:107 vs src/common/world/scene/Resources.h:44 | `Targets::stamped` decides membership by the 64-bit `stampKey` fold alone, while the resource store beside it documents that a hash is "a BUCKET and never an answer" and confirms with `==` | one policy for the two artefact caches | keep cloud/stamp counts beside the fold and confirm, or state in the comment why a collision is accepted here
- nit | src/common/image/include/sigilimage/decode/ChannelData.h:42, src/common/image/decode/ChannelData.cpp:48 | `at()` and `makeImage(int,int,int,int)` take unchecked channel indices | check or document | assert/clamp against `names.size()`

### 3. README drift

- nit | src/common/world/README.md | `SceneStats`, `Sampling`/`samplingOf`, `SurfaceTerms`, `paintedEnvironment`, `subjectOf`, `lanesOf`, `standingValue`, `localMatrix`, `GeneratorOps`, `PassBodyOps` are public names the README never spells | document or mark `@private` | one "for an executor author" paragraph
- nit | src/common/usd/README.md | `ReadEnvironment` and `Writer::environmentMap()` absent from the API table | document
- nit | src/common/substance/README.md | `Package::engineVersion()` absent | document
- (No README in the four libraries names an API that does not exist.)

### 4. Comment rules

- None found in world, usd, substance, image. `FindSubstance.cmake:1` carries the one permitted `workaround:` marker correctly.

### 5. Leftovers

- should-fix | src/common/world/scene/Phases.cpp:356 | (top ten #5) — the only library-code print in the four libraries.
- No TODO/FIXME, commented-out code or dead public functions found in world, usd, substance, image.

### 6. Duplicated mechanisms

- should-fix | src/common/world/scene/Draw.cpp:25,37 vs src/common/world/frame/CpuGeometry.cpp:22,77 | (top ten #10)
- nit | src/common/usd/read/Material.cpp:33-37, src/common/substance/package/Package.cpp:73, src/common/geometry/mesh/codec/Model.cpp:184 | three libraries hand-roll `std::ifstream` byte reads (Material.cpp one `get(c)` per byte) while SigilIO owns resource access and the write side already goes through `io::writeBytes` | one read door | add `io::readBytes(path)` beside `writeBytes` (SigilIO today has only `Source::fetch(uri)`) and use it
- nit | src/common/image/include/sigilimage/field/DistanceField.h | the file comment states the algorithm but not what Skia offers (`SkDistanceFieldGen` is private to `src/core` and produces an 8-bit signed glyph field) and why it did not serve | state it | one sentence

### 7. Files over ~600 lines

- None in world, usd, substance, image (largest: world/diligent/test/RuntimeTest.cpp 582, world/README.md 1279 — a document, not code).

### 8. Test gaps

- should-fix | src/common/world/graph/test/GraphTest.cpp | no case for a reader declared before two writers (finding #2) nor for two masked post passes behind one producer (finding #1) | name both | add `AReaderDeclaredFirstStillRunsBeforeTheSecondWrite` and `TwoMaskedPassesEachReadTheirOwnCoverage`
- should-fix | src/common/image/field/test/DistanceFieldTest.cpp | every `distanceField` case is square (9x9, 64x64, 8x8); a non-square raster is where a row/column stride swap in the two-pass transform would show; 1x1 and a fully covered mask are untested | name the edge cases | add 5x2 and 2x5, 1x1, all-covered
- should-fix | src/common/image/decode/test/DecodeTest.cpp:133 | `ATruncatedFileIsRefused` covers truncation only; no case for an overflowing header (finding #3) | add one
- should-fix | src/common/usd/read/test/ReadTest.cpp | no case for a mesh with out-of-range indices or mismatched counts (finding #4), nor for a zero-length stamp direction on write | add both
- nit | src/common/usd/write/test/WriteTest.cpp | no case writing an empty mesh or an empty cloud

## Counts

- blocker: 1
- should-fix: 15 (11 code findings + 4 test-gap entries; the test gaps for #1–#4 are folded into those findings' fixes)
- nit: 10

Total distinct findings: 26 (world 8, usd 6, substance 3, image 7, cross-library 2).
