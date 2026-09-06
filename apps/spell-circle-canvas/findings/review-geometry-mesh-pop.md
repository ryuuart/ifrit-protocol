# Review — SigilGeometry outside `path`

Scope: `src/common/geometry/{mesh,device,kit,test}`, their `CMakeLists.txt`, the
matching public headers under `include/sigilgeometry/{mesh,device,kit}` and
`Geometry.h`, and the non-path chapters of `src/common/geometry/README.md`.
Read-only merge-readiness review against HEAD on `sigil/library-campaigns`
(merge base `aabd3fe1b224`). All paths absolute; every finding verified against
the code at HEAD.

Format: `severity | path:line | what the code does | what it should do | one-line fix` `[category]`

---

## blocker

blocker | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/kit/Silhouettes.cpp:170 | `Parallelogram::path` with a NEGATIVE `skewDeg` sets `l=|lean|, r=0`, so the top edge runs to `w+|lean|` while the bottom runs 0..`w-|lean|` — a trapezoid that escapes the box | for either sign the figure must be a parallelogram of width `w-|lean|` inscribed in the box, as `Parallelogram`'s own doc (Generators.h:241) promises | swap which end each of `l`/`r` shifts so the negative case mirrors the positive one, derived from one shared `offset = lean` [1]

blocker | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/device/Cook.cpp:317 | `readBack` walks every lane buffer `PopGpu` still holds, including lanes a PREVIOUS chain created that this cook never touched (`beginCook` only clears when the point count changes), and pours them into `lanes` so `exportLanes` exports stale values as custom lanes | only the lanes this cook seeded or dispatched may be read back, so the device cloud matches the CPU one lane for lane | skip any lane whose `stamp != cooks` in the readBack loop (and in the copy loop above it) [1]

blocker | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/Generators.cpp:52 | the pole fallback tests `dot(n,n) < 0.5f`, but `normalized()` already substituted the unit fallback `{0,0,1}` for a degenerate cross product, so `dot` is always 1 and the loop never runs — every pole of `superellipsoid()` gets `+z` instead of a borrowed neighbour normal, and `Solids.cpp:239` says the fallback covers them | detect the degeneracy before normalizing (keep the raw `cross(du,dv)` and test its length) so a pole really does borrow | compute `cross(du,dv)` into a local, test its squared length, and mark degenerate entries for the borrow pass [1]

blocker | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/device/residency/Meshes.cpp:112 | `MeshResidency::upload` keys the cache on `artefact` alone and returns the held buffers on the second call, ignoring `primColorLane` — the same artefact drawn once with a prim lane and once without gets whichever form crossed first (shared vs unwelded vertices, wrong prim values) | the lane name is part of what the buffers ARE, so it belongs in the key or must invalidate the entry | make the key `{artefact, primColorLane}`, or store the lane on `MeshBuffers` and re-upload when it differs [1]

blocker | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/Geometry.h:5 | the file comment says "Every public header of SigilGeometry in one include" but the list omits `kit/*` (9 headers), `mesh/pop/Kernel.h`, `mesh/pop/Stamp.h`, `mesh/curve/Frame.h`, `mesh/render/Shading.h`, `device/Device.h`, `mesh/render/device/Painter.h` and ~25 `path/*` headers | either include every public header or say which tiers it covers | add the missing includes (device ones excepted, and say so) [3]

blocker | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/README.md:1039 | "`Geometry.h` at the root of the include tree includes every public header" — false for the same ~40 headers | state what it actually aggregates | reword to name the tiers it covers, or fix the header [3]

blocker | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/README.md:1060 | "Four operators read points they do not own, each over `path::Neighbours`" — `Smooth` reads chain-order indices `i-1`/`i+1` (Cook.cpp:388) and `Cluster` is k-means over the whole set (Cook.cpp:427); neither touches `Neighbours` | only `Relax` and `Transfer` go through the grid | say "two of them over `path::Neighbours`" and name which [3]

---

## should-fix

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/Cook.cpp:219 | `seedLanes` guards a `MeshScatter` only on `indices.empty()`, but `points::onMesh` returns an EMPTY cloud when the total triangle area is zero (Generators.cpp:122); `copied` is then 0 while line 295 still returns `count`, so the chain cooks `count` points at the origin | a generator that made no points must return 0 | return `seeds.size()` (or 0 when it is 0) instead of the requested `count` [1]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/render/Shading.cpp:90 | `pixelsOf` returns a reference into a static cache and releases the mutex on return; another thread entering `pixelsOf` can hit `cache.clear()` (line 73) and free the node `samplePanorama` is still reading through | the reference must outlive the read, or the read must hold the lock | return a `shared_ptr<const Pixels>`, or keep entries alive with a refcount instead of clearing wholesale [1]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/device/residency/Textures.cpp:221 | `TextureResidency::endFrame` ages `m_wrapped` and `m_uploaded` but never `m_environments` or `m_irradiances`, so every panorama ever sampled is held for the residency's life | all four maps age on the same beat, as README:1350 states ("`endFrame()` on either lets go of what no draw has named lately") | add the same erase loop for the two environment maps [1]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/render/Runtime.cpp:229 | `valid[i0]`, `screen[i0]` and `shaded[i0]` are indexed with `mesh.indices` values that were never checked against `mesh.vertexCount()` — an out-of-range index on a caller-built `Mesh` is an out-of-bounds read | reject or skip a triangle naming a vertex the mesh does not have, the way `PlyDecode.cpp:397` does at import | test `i0|i1|i2 < n` before the cull and `continue` otherwise [1]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/Mesh.cpp:153 | `computeNormals()` does `normals[indices[i]] += n` with unvalidated indices — an out-of-range index is an out-of-bounds WRITE, and `decode::detail::finishPart` calls this on every imported mesh | skip a triangle whose indices exceed `positions.size()` | guard the three lookups per triangle [1]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/Sinks.cpp:101 | `connectAdjacent` reads `(*pieces)[other]` and `(*pieces)[i]` without checking the piece lane's length against `cloud.positions.size()`; a hand-built or short lane is an out-of-bounds read | check `pieces->size() == cloud.positions.size()` before using it, as every other lane consumer here does (`Stamp.cpp:186`, `Cook.cpp:144`) | null out `pieces` when the size does not match [1]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/device/Cook.cpp:355 | when the device refuses the kernel the pop executor returns an EMPTY cloud, silently; the stamp and sweep device executors in the same directory fall back to `kernel::run` on the host instead (`device/Stamp.cpp:289`, `device/Sweep.cpp:265`) | one seam, one failure posture — fall back to the host cook or throw the way `pop::cook` throws for an unsupported operator | call the CPU runtime's `cook(chain)` when `ready()` fails [2]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Pop.h:121 | `Vary`'s doc says "lane.x = base * (1 + spread * …)" but the kernel writes the value into all four components (`kernels/Pop.slang:151`), so a `Vary` on `Color` overwrites alpha too | the doc and the kernel must agree | fix the doc to "every component of `lane`", or make the kernel write `.x` only [3]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/render/device/Painter.cpp:306 | `writeUniforms` writes no environment, metallic or roughness term, so the device painter drops IBL, the metal/dielectric split and the metal-tinted highlight the host executor applies (`render/Runtime.cpp:155-190`), while `render/device/Painter.h:22` says "the shading is per vertex in view space exactly as the host executor's is" and names only sort/antialias as the difference | either carry the terms or say the environment is host-only | add the missing terms, or amend the header and README:809 [3]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/render/device/shaders/Painter.slang:128 | the device Blinn exponent is `powiP(…, int(uShading.y))`, truncating `MeshStyle::shininess` to an integer, where the host uses `std::pow` with the float (`render/Runtime.cpp:177`) | one style field must mean one exponent on both executors | use a float power, or document that shininess is integral [1]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/CMakeLists.txt:64 | `SigilGeometryMeshPop` compiles `device/*.cpp` into its own archive and links `SigilGeometryDevice` PUBLIC, so every consumer of the point operators pulls Diligent, SigilSkia Graphite and the Vulkan loader — while `mesh/render/device/CMakeLists.txt:5` splits the identical seam into its own target precisely so "the feature above must stay free of a device … that is the shape every other seam here has" | one shape for both seams | split `SigilGeometryMeshPopDevice` the way render does, or amend render's comment and README:1582 [2]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/README.md:1582 | "Nothing above it links it unless it wants a device" — `SigilGeometryMeshPop` links `SigilGeometryDevice` PUBLICLY (mesh/pop/CMakeLists.txt:64), and README:1566 says the public link set is "Skia, glm, Boost … and the two SigilCore leaves … and nothing else" | state that pop carries the device | fix the link or fix both sentences [3]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Pop.h:1104 | "Defined only where this library was built with a device feature" — `device/Cook.cpp` is an unconditional source of `SigilGeometryMeshPop`, so `deviceRuntime` is always defined | say it is always present, or make the source conditional | drop the sentence (same in `Sweep.h:235` and `Stamp.h:170`) [3]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/kit/Divisions.h:336 | `chords()` applies `Chords::inset` only on the open branch (line 357); the `closed` branch walks the star traversal and ignores it, though the doc at line 306 says "shortens each chord by that many px at BOTH ends" with no exception | either trim the closed rings too or say the inset is open-only | note the restriction in the doc, or inset the vertices before the traversal [3]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/kit/Shapers.h:209 | a kit header reopens `sigil::geometry::path::profile` and adds `wave()` to it, so `path::profile`'s contents depend on whether a consumer happened to include a kit header | a feature's namespace is that feature's; kit composes over a seam, it does not grow the seam's scope | put it in `shapers::` (or move it into `path/Profile.h` if it belongs to the seam) [2]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Pop.h:48 | `Lane` numbers the builtins P=0, Dir=1, Color=2, Scale=3, T=4 while `builtinIndex` (line 72) numbers the same names P=0, T=1, Dir=2, Scale=3, Color=4 — two disagreeing numberings for one set, in one header | one numbering, or the enum carries no numeric meaning | make `Lane`'s values match `builtinIndex`, or drop the explicit enumerator values [2]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/Fields.cpp:227 | `opName`'s final `else` returns "PointSet" as a catch-all, so a new alternative appended to `Op` (which Pop.h:483 says is the growth rule) is silently named "PointSet" in the message an unsupported-operator throw produces | every alternative names itself, and a new one must fail to compile rather than mis-name | make the last arm an explicit `is_same_v<T, PointSet>` test with a static-assert fallthrough [1]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Pop.h:981 | `deformFrame` is public and its doc says "so the CPU cook and the GPU executor's parameter upload deform in the identical frame", but `Deform` has no kernel and every device executor declines it (Kernel.cpp:82, README:1148) — the only caller is `Cook.cpp:650` | a public function's stated reason must be true | make it internal to `Cook.cpp`, or restate the reason [3]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Pop.h:952 | `seedCustomNames` is public API with no consumer anywhere in `src/` other than `PopLanesTest.cpp:287` | a public function with only a test behind it is either dead or under-documented as to who needs it | remove it, or name the consumer it exists for [5]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/kit/Solids.cpp:37 | `orientTriangle` binds `const glm::vec3& p1 = positions[tri[1]]` and immediately discards it with `(void)p1;` — the value is re-read inline on line 36 | delete both lines | remove the unused local and the void cast [5]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/Sinks.cpp:24 | an empty anonymous namespace `namespace {}  // namespace` with nothing in it | remove it | delete the line [5]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/device/Resources.cpp:108 | `Resources::read` creates a fresh USAGE_STAGING texture on every call, while `Resources.h:6` lists "the staging copy that brings a texture's pixels home" among the four things that "belong to the device" and are "made once", and README:1391 repeats it | hold the staging texture as a member sized to the largest read, or fix both comments | add `m_staging` + a size, mirroring `PopGpu::staging` [3]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/kit/Divisions.h:63 | "That is the whole reason `Frame` exists; see `kit/Frame.h`" — a citation, and of a file that does not exist (the header is `path/Frame.h`) | comments state the constraint, not a document reference | delete the citation [4]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/Mesh.cpp:94 | "Verified by Mesh.AppendRepairsShortIncomingLanes." — a citation of a test name in a comment | state the invariant, not where it is checked | delete the sentence [4]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/Generators.cpp:31 | "Both renderers (Space.h texs, SigilWorld) assume this" — cites `Space.h`, which exists nowhere under `src/` | state the convention without naming a file | drop the parenthetical [4]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/render/Runtime.cpp:118 | "Materials.h G-buffer convention: DEVICE-space normals, +y down" — cites `Materials.h`, which exists nowhere under `src/` | state the convention itself | drop the file name [4]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Points.h:12 | a SigilGeometry public header names `world::World` as a destination; `Pop.h:482` makes the variant ORDER an ABI constraint "SigilWorld maps each op's variant index to a compute PSO", and `Mesh.h:100` names "SigilWorld's Diligent pipelines" | the dependency runs one way — SigilGeometry must not know SigilWorld exists (README:1611) | state the constraint without the consumer's name ("a renderer that maps the variant index to a pipeline") [4]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/README.md:895 | the `mesh/pop/Points.h` bullet attributes "the consumers `instance()` and `quads()` … (`Modifiers.cpp`)" — both are defined in `Stamp.cpp:205` and `:256`; `Modifiers.cpp` holds `jitter`, `displaceNoise`, `stampOptions` and `promoteToPrims` | name the file each lives in | move `instance()`/`quads()` into the `Stamp.cpp` sentence [3]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/README.md:1094 | the Builder verb list omits `relax()`, `cluster()` and `transfer()`, which exist at `Pop.h:697`, `:703` and `:711` | list every chained verb, since the paragraph presents itself as the set | add the three [3]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/CMakeLists.txt:2 | "The public headers expose only Skia and glm types" — `Mesh.h` and `Vec.h` name no Skia type at all, and `Mesh.h:20` exposes `boost::container::map` | say glm and Boost.Container | fix the comment [4]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/curve/Frame.h:19 | `Frame3`'s defaults are tangent `{0,0,1}`, normal `{0,1,0}`, binormal `{1,0,0}`, but the comment on the same line says "tangent x normal", which is `{-1,0,0}` — a default-constructed frame is left-handed against the convention `frames()` builds (Curve.cpp:183, 206) | the default must satisfy the stated identity | set the default binormal to `{-1,0,0}` [1]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/kit/test/SilhouettesTest.cpp:73 | `SilhouetteGenerator.StaysInsideTheBoxItIsGiven` instantiates only `parallelogram(12)`; the negative-skew case, which is the one that escapes the box, is untested | the row should cover both signs | add `parallelogram(-12)` to the value list [8]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/test/PopChainsTest.cpp:41 | no pop case cooks a DEGENERATE chain — a loop of fewer than three points, `count(0)`, an empty `PointSet`, or a `MeshScatter` over a zero-area mesh (the case that returns `count` origin points) | each generator's empty answer is a promise worth pinning | add a case asserting each of those cooks an empty cloud [8]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/test/DeviceCookTest.cpp:262 | no case cooks two DIFFERENT chains through one device runtime, which is exactly what surfaces the stale-lane export | a runtime reused across chains must answer each one as the CPU does | add a case cooking a chain with a `Select` then one without, on one runtime, compared with the host [8]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/device/residency/test/ResidencyTest.cpp:74 | `upload()` is tested for the same artefact twice with the same mesh, never with a different `primColorLane` | the cache must answer the lane it was asked for | add a case uploading artefact N once with a lane and once without, asserting the vertex counts differ [8]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Pop.h:1 | `Pop.h` is 1131 lines carrying five subjects: the attribute vocabulary, the 25 operator descriptions, the executor/Runtime seam, the `Builder`, and the sinks plus shared helpers | one subject per file | split into `Ops.h` (AttrRef + operator structs + variant), `Runtime.h` (Executor/Runtime/cook), `Builder.h`, leaving `Pop.h` as the umbrella [7]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/Cook.cpp:1 | `Cook.cpp` is 792 lines holding the lane table, the seed, the export, the deform frame, the noise field, and eight operator bodies inside one `std::visit` | one subject per file | lift `laneFill`/`attrFor`/`seedLanes`/`seedAttrs`/`exportLanes` into `Lanes.cpp` and the neighbourhood operators into `Neighbourhood.cpp` [7]

should-fix | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/codec/Geo.cpp:1 | 601 lines, over the ~600 line guide | split by subject | lift the attribute-lane decoding out of the topology decoding into `GeoLanes.cpp` [7]

---

## nit

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/device/Device.cpp:101 | library code writes a diagnostic with `fprintf(stderr, …)`, where the sibling device executor uses `material::reportOnce` (`render/device/Painter.cpp:69`) | one reporting mechanism per library | route through `reportOnce` [5]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/device/Device.cpp:98 | `static bool warned` is read and written with no synchronization; two threads bringing devices up race on it | make it atomic or use `std::call_once` | `static std::atomic<bool>` with `exchange` [1]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/device/Device.cpp:56 | `Device::create` calls `setenv` — a process-wide, non-thread-safe mutation from a library entry point | note the constraint, or move the pin to the host that owns process setup | say in the comment that it must run before any other thread reads the environment [1]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/kit/Divisions.h:339 | `std::vector<bool> seen` with no `#include <vector>` in the header (only `<algorithm>` and `<functional>`) — it compiles on a transitive include | each header includes what it needs, as README:355 states | add `#include <vector>` [5]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/kit/Divisions.h:135 | `const float step = n > 0 ? t.sweep / (float)n : 0.0f;` — `n == 0` already returned on line 133, so the condition is dead | drop the ternary | `const float step = t.sweep / (float)n;` [5]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/kernels/Pop.slang:177 | `if (cells < 1) cells = 1;` is unreachable: `describe` clamps both `cols` and `rows` to at least 1 (Kernel.cpp:142) and the comment above it says so | drop the dead guard, or move the clamp here | delete the two lines [5]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Points.h:114 | `points::displaceNoise` takes `uint32_t seed` while the operator it is documented to mirror, `pop::Noise::seed` (Pop.h:109), is a `float` — the modifier cannot reach a fractional seed the chain can | one verb, one seed type | make the modifier's seed `float` [2]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Pop.h:576 | "(Cooked on the CPU reference at build time; a GPU-resident chain-to-chain feed is the queued next step.)" — a roadmap note in a public header | state what the code does | drop the second clause [4]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/README.md:1673 | "The tiers were once separate binaries so that a test reaching past its tier failed to link; that proof is deliberately gone" — history | state the arrangement as it stands | keep only "a suite's file sits in the feature it covers" [4]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/README.md:869 | "the two rails — `curve::frames()` …, and `curve::hangFrames()` …; the and `project()` to draw the curve" — a broken clause left by an edit | repair the sentence | drop the stray "the" [3]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/README.md:1199 | "`kit/Silhouettes.h` — the 2D shelf, including all three" — it includes four headers (Corners, Curves, Generators, Hatches) | say four, or name them | fix the count [3]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/mesh/pop/Stamp.h:91 | `StampDispatch::vertices()` returns `args.code.w`, a `uint32_t` filled with `verts * points` (Stamp.cpp:159) — a stamping past 2^32 vertices truncates and forms the wrong count silently | carry the total as `size_t`, or refuse a dispatch that would overflow | check the product in `describe` and return false [1]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/include/sigilgeometry/kit/Shapers.h:63 | `Wave::bleed()` and `Wave::max()` have identical bodies (`std::abs(amplitude)`) for two different seams | one of them should call the other | `float bleed() const { return max(); }` [6]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/Generators.cpp:51 | the pole-fallback loop is O(n²) when many normals are degenerate (a constant `fn` makes every one of them so) | bound the search, or collect the degenerate indices once | scan for one valid normal up front and reuse it [1]

nit | /Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/src/common/geometry/mesh/pop/test/PopFiltersTest.cpp:1 | `Vary`, `Fill` and `LookAt` are exercised only inside composite chains and on the device conformance rows; no host case names any of the three or pins its formula | each operator with a kernel should have a case naming it | add three short cases asserting each operator's stated formula [8]

---

## Counts

- blocker: 7
- should-fix: 40
- nit: 15
- total: 62

## Verified clean (checked, nothing to report)

- No heavy SDK header (Diligent, pxr, Alembic, OpenImageIO, Slang, Vulkan) appears
  in any public header under `include/sigilgeometry/{mesh,device,kit}`; `device/Device.h`
  forward-declares its two Diligent interfaces as documented.
- No SigilWorld *type* is used anywhere in SigilGeometry (only comment references —
  reported above).
- No `TODO`/`FIXME`/`XXX`/`HACK` markers, no performance numbers in comments,
  no commented-out code in the slice.
- The kernel seam is consistent: `kernel::has()`'s operator list, `Pop.slang`'s
  index constants and `pop::Op`'s variant order agree exactly (25 alternatives).
- `Transfer`'s `break` at the radius is valid — `path::Neighbours::nearest(p, k)`
  is documented and implemented nearest-first.
- `path::Relaxation`'s aggregate order matches the braced init at `Cook.cpp:415`.
- Every API name the README spells for these headers exists (33 names grep-checked).
- PLY and Alembic decoders both validate file-supplied indices; glTF is covered by
  `cgltf_validate` and OBJ by tinyobjloader's own bounds check.
