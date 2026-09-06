# Merge-readiness review — SigilGeometry mesh, point operators, kit, device (sub-pass)

Read-only pass over `src/common/geometry/{mesh,kit,device}` (delegated by
the geometry+material+world reviewer; delivered directly). Paths relative
to `apps/spell-circle-canvas/`. Category in brackets: 1 correctness,
2 API, 3 README drift, 4 comment rules, 5 leftovers, 6 duplication,
7 file size, 8 test gaps.

## Findings, most severe first

- blocker | `src/common/geometry/kit/Silhouettes.cpp:170` | `Parallelogram::path` with a negative `skewDeg` sets `l=|lean|, r=0`, so the top edge runs to `w+|lean|` — a trapezoid that escapes the box, against `Generators.h:241` | derive both shifts from one signed `lean` so the negative case mirrors the positive [1]
- blocker | `geometry/mesh/pop/device/Cook.cpp:317` | `readBack` walks every lane buffer `PopGpu` still holds, including lanes a previous chain created (`beginCook` clears only when the count changes), so `exportLanes` exports stale values as custom lanes | skip any lane whose `stamp != cooks` (also in the copy loop above) [1]
- blocker | `geometry/mesh/Generators.cpp:52` | the pole fallback tests `dot(n,n) < 0.5f` after `normalized()` already substituted `{0,0,1}`, so it never runs and every pole of `superellipsoid()` gets `+z` while `Solids.cpp:239` says the fallback covers them | test the raw cross product's squared length before normalising [1]
- blocker | `geometry/device/residency/Meshes.cpp:112` | `MeshResidency::upload` keys on `artefact` alone and ignores `primColorLane`, so the same artefact drawn with and without a prim lane gets whichever form crossed first | key on `{artefact, primColorLane}` or re-upload when the lane differs [1]
- blocker | `include/sigilgeometry/Geometry.h:5`, `README.md:1039` | "every public header in one include" omits `kit/*`, `mesh/pop/Kernel.h`, `Stamp.h`, `mesh/curve/Frame.h`, `mesh/render/Shading.h`, `device/Device.h`, `mesh/render/device/Painter.h` and ~25 `path/*` headers; the README repeats the claim | include every public header (device ones excepted, and say so), or state the tiers it covers [3]
- blocker | `README.md:1060` | "Four operators read points they do not own, each over `path::Neighbours`" — `Smooth` reads chain-order indices and `Cluster` is k-means over the set; only `Relax` and `Transfer` use the grid | say two, and name them [3]
- should-fix | `geometry/mesh/pop/Cook.cpp:219` | `seedLanes` guards a `MeshScatter` only on `indices.empty()`, but `points::onMesh` answers an empty cloud for zero total area; line 295 still returns `count`, so the chain cooks `count` points at the origin | return the seeds' size [1]
- should-fix | `geometry/mesh/render/Shading.cpp:90` | `pixelsOf` returns a reference into a static cache and releases the mutex; another thread can `cache.clear()` under the read | return a `shared_ptr<const Pixels>` or refcount entries [1]
- should-fix | `geometry/device/residency/Textures.cpp:221` | `endFrame` ages `m_wrapped` and `m_uploaded` but never `m_environments` or `m_irradiances` | age all four, as README:1350 states [1]
- should-fix | `geometry/mesh/render/Runtime.cpp:229` | `valid/screen/shaded[i0]` indexed by unchecked `mesh.indices` — an out-of-bounds read on a caller-built mesh | skip a triangle naming a vertex the mesh lacks [1]
- should-fix | `geometry/mesh/Mesh.cpp:153` | `computeNormals()` writes `normals[indices[i]]` unvalidated — an out-of-bounds write on every imported mesh | guard the three lookups [1]
- should-fix | `geometry/mesh/pop/Sinks.cpp:101` | `connectAdjacent` reads the piece lane without checking its length against the positions | null the lane when the size does not match, as `Stamp.cpp:186` and `Cook.cpp:144` do [1]
- should-fix | `geometry/mesh/pop/device/Cook.cpp:355` | when the device refuses the kernel the pop executor returns an empty cloud silently, while the stamp and sweep device executors fall back to the host | one failure posture: fall back to the host cook [2]
- should-fix | `include/sigilgeometry/mesh/pop/Pop.h:121` | `Vary`'s doc says `lane.x` but the kernel writes all four components (`kernels/Pop.slang:151`), so a `Vary` on `Color` overwrites alpha | agree the doc and the kernel [3]
- should-fix | `geometry/mesh/render/device/Painter.cpp:306` | `writeUniforms` carries no environment, metallic or roughness term, so the device painter drops IBL and the metal split the host applies, while `Painter.h:22` says the shading is exactly the host's | carry the terms or amend the header and README:809 [3]
- should-fix | `geometry/mesh/render/device/shaders/Painter.slang:128` | the device Blinn exponent truncates `shininess` to an integer; the host uses a float power | one exponent on both executors [1]
- should-fix | `geometry/mesh/pop/CMakeLists.txt:64`, README:1582,1566 | `SigilGeometryMeshPop` compiles `device/*.cpp` and links `SigilGeometryDevice` PUBLIC, pulling Diligent, Graphite and Vulkan into every consumer, while render splits the identical seam into its own target for that reason and the README says nothing above the device links it | split `SigilGeometryMeshPopDevice`, or fix the README [2]
- should-fix | `include/sigilgeometry/mesh/pop/Pop.h:1104`, `Sweep.h:235`, `Stamp.h:170` | "Defined only where this library was built with a device feature" — the device sources are unconditional | drop the sentence [3]
- should-fix | `include/sigilgeometry/kit/Divisions.h:336` | `chords()` applies `inset` only on the open branch though the doc says both ends, no exception | inset the closed rings or note the restriction [3]
- should-fix | `include/sigilgeometry/kit/Shapers.h:209` | a kit header reopens `path::profile` and adds `wave()` to it | put it in `shapers::` or move it into `path/Profile.h` [2]
- should-fix | `include/sigilgeometry/mesh/pop/Pop.h:48,72` | `Lane` numbers the builtins P,Dir,Color,Scale,T while `builtinIndex` numbers P,T,Dir,Scale,Color | one numbering [2]
- should-fix | `geometry/mesh/pop/Fields.cpp:227` | `opName`'s final `else` answers "PointSet" for any new alternative | name every alternative; fail to compile on a new one [1]
- should-fix | `include/sigilgeometry/mesh/pop/Pop.h:981` | `deformFrame` is public with a doc reason (the GPU deforms in the same frame) that is false — `Deform` has no kernel; the only caller is `Cook.cpp:650` | make it internal or restate [3]
- should-fix | `include/sigilgeometry/mesh/pop/Pop.h:952` | `seedCustomNames` is public with only a test behind it | remove, or name the consumer [5]
- should-fix | `geometry/kit/Solids.cpp:37` | a bound reference immediately `(void)`-discarded | delete both lines [5]
- should-fix | `geometry/mesh/pop/Sinks.cpp:24` | an empty anonymous namespace | delete [5]
- should-fix | `geometry/device/Resources.cpp:108` | `Resources::read` creates a fresh staging texture per call while `Resources.h:6` and README:1391 say it is made once | hold `m_staging` sized to the largest read, or fix both comments [3]
- should-fix | comment rules: `kit/Divisions.h:63` cites a non-existent `kit/Frame.h`; `mesh/Mesh.cpp:94` cites a test name; `mesh/Generators.cpp:31` and `mesh/render/Runtime.cpp:118` cite `Space.h` and `Materials.h`, which do not exist; `mesh/pop/Points.h:12`, `Pop.h:482`, `Mesh.h:100` name SigilWorld, a consumer, as the reason for a constraint | state each constraint without the file or the consumer [4]
- should-fix | `README.md:895` | attributes `instance()`/`quads()` to `Modifiers.cpp`; both live in `Stamp.cpp` | fix [3]
- should-fix | `README.md:1094` | the Builder verb list omits `relax()`, `cluster()`, `transfer()` | add [3]
- should-fix | `geometry/mesh/CMakeLists.txt:2` | "expose only Skia and glm types" — `Mesh.h` exposes `boost::container::map` and no Skia type | fix [4]
- should-fix | `include/sigilgeometry/mesh/curve/Frame.h:19` | `Frame3`'s default binormal `{1,0,0}` is not "tangent × normal" (`{-1,0,0}`) — a default frame is left-handed against what `frames()` builds | set `{-1,0,0}` [1]
- should-fix | tests missing: `parallelogram(-12)` in `SilhouettesTest.cpp:73`; a degenerate pop chain (fewer than three loop points, `count(0)`, empty `PointSet`, zero-area `MeshScatter`); two different chains through one device runtime (`DeviceCookTest.cpp:262`); `upload()` with and without a prim lane (`ResidencyTest.cpp:74`); host cases naming `Vary`, `Fill`, `LookAt` [8]
- should-fix | `include/sigilgeometry/mesh/pop/Pop.h` (1131), `mesh/pop/Cook.cpp` (792), `mesh/codec/Geo.cpp` (601) | one subject per file | `Ops.h`/`Runtime.h`/`Builder.h` under `Pop.h`; `Lanes.cpp` and `Neighbourhood.cpp` out of `Cook.cpp`; `GeoLanes.cpp` [7]
- nit | `geometry/device/Device.cpp:101,98,56` | `fprintf(stderr)` where the sibling uses `reportOnce`; an unsynchronised `static bool warned`; `setenv` from a library entry point | `reportOnce`; atomic; state the constraint [5][1]
- nit | `include/sigilgeometry/kit/Divisions.h:339,135` | `std::vector` without `<vector>`; a dead `n > 0` ternary | include; drop [5]
- nit | `geometry/mesh/pop/kernels/Pop.slang:177` | an unreachable `cells < 1` guard | delete [5]
- nit | `include/sigilgeometry/mesh/pop/Points.h:114` | `displaceNoise` takes `uint32_t seed`; the operator it mirrors takes `float` | one seed type [2]
- nit | `include/sigilgeometry/mesh/pop/Pop.h:576` | a roadmap note in a public header | drop [4]
- nit | `README.md:1673,869,1199` | history about the tiers; a broken clause; "all three" for four headers | rewrite [4][3]
- nit | `include/sigilgeometry/mesh/pop/Stamp.h:91` | `vertices()` is a `uint32_t` product that truncates past 2^32 | check in `describe` [1]
- nit | `include/sigilgeometry/kit/Shapers.h:63` | `Wave::bleed()` and `max()` identical bodies | one calls the other [6]
- nit | `geometry/mesh/Generators.cpp:51` | the pole-fallback loop is O(n²) when many normals are degenerate | find one valid normal once [1]

## Counts

- blocker: 7
- should-fix: 40
- nit: 15
