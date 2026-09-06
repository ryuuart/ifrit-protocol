# Merge-readiness review: core, data, measure, motion, io, video, ui, spellcircle, build, tooling

Branch `sigil/library-campaigns` against merge base `aabd3fe1b224`, read-only.
Paths are relative to `apps/spell-circle-canvas/` unless they start with `/`.
Each entry: severity · `path:line` · what the code does · what it should do · fix.
`src/common/loader` named in the brief does not exist in the tree; nothing to review there.

## Top ten, most severe first

1. **blocker** · `src/common/core/include/sigilcore/compute/Chance.h:219-224` · `normal()` rejects (u,v) pairs until `0 < s < 1`, but a `Stream::stratified(0|1)` source (documented valid at line 124-127) answers `fixed(0)` on every draw (line 174-184), so `signedUnit()` is always −1, `s` is always 2 and the loop never exits; `Gaussian::draw` inherits the hang. · Should terminate on every documented source. · Cap the rejection loop and fall back to Box–Muller on the capped path, or refuse `stratified(<2)`.
2. **should-fix** · `src/common/core/include/sigilcore/reconcile/Reconciler.h:146-152` · two siblings with the same match key: `keyed.emplace` does not insert the second, `inst.children.clear()` at 152 destroys it, so it never reaches `m_host.destroy()` and `m_stats.retired` under-counts. · Every previous child must retire through the host. · On a failed `emplace`, push the child onto `unkeyed`.
3. **should-fix** · `src/common/core/hardware/GpuDevice.cpp:148` · `frameIndex()` reads `m_impl->frame` with no lock while `beginFrame()` (136-139) writes it under `m_impl->mutex`; `GpuDevice.h:135` promises every call is safe from any thread. · Read under the same lock. · Lock in `frameIndex()` or make `frame` `std::atomic<uint64_t>`.
4. **should-fix** · `src/common/io/hub/Network.cpp:95-101` · every fetch of one URL writes the same `<cacheDir>/<key>.part` then renames it; two threads fetching the same URL (the README at `io/README.md:185-187` says concurrent cold asks may do the same source work, and `preload()` fans out) truncate each other's partial and one rename can commit a file the other writer has just truncated. · The "whole or nothing" promise (README:235-238) must hold under concurrency. · Write to a per-writer temporary (`.part.<pid>.<tid>` or `mkstemp`) and rename that.
5. **should-fix** · `src/common/io/hub/Mounts.cpp:18-27` · `readFile` casts `tellg()` to `size_t` and resizes; `tellg()` answers −1 on failure, which becomes `resize(SIZE_MAX)` and a thrown `length_error`. `fetchResource` (43-52) only checks `last_write_time`, which succeeds on a directory, so `hub.blob("res://some/dir")` reaches this path. · A directory or unreadable path must answer null. · Check `is_regular_file` in `fetchResource` and guard `size < 0` in `readFile`.
6. **should-fix** · `src/common/data/decode/Csv.cpp:351-352` with `include/sigildata/table/Table.h:189-192` · a header with two equal names builds two columns and `Table::add` replaces the first in place, so the earlier column's data is silently lost. · A file's columns must all survive decoding. · De-duplicate header names (`name`, `name_2`) before `columnOf`, or refuse the file.
7. **should-fix** · `src/common/io/hub/Selection.cpp:257-266` · a relative glob written `./shaders/*.sksl` walks `./shaders` but builds each candidate as `shaders/a.sksl` (relative to cwd, no `./`), and `matches(selector, candidate)` at 266 compares the pattern verbatim, so nothing ever matches. · A `./`-prefixed selector must select what its un-prefixed spelling selects. · Lexically normalise the selector before matching (or match against `path.lexically_relative(root)` joined onto the selector's literal prefix, as the mounted branch does at 306-309).
8. **should-fix** · `src/common/core/cache/Volatility.cpp:27,41` vs `include/sigilcore/cache/Volatility.h:72` · the header says `samplesDestination` implies `readsBackdrop`; the fold reads `self.readsBackdrop` alone, so a destination-sampling node with `readsBackdrop == false` is `memoSafe`. · Enforce the implication. · `const bool backdrop = self.readsBackdrop || self.samplesDestination;` in both lines.
9. **should-fix** · `src/common/measure/include/sigilmeasure/time/Laps.h:32-37,62` · `mark(std::string_view)` stores the view; a name built from a temporary (`"phase " + std::to_string(n)`) dangles by `each()`. · A stored name must own or refuse a temporary. · `double mark(std::string&&) = delete;` or store `std::string`.
10. **should-fix** · `src/common/io/hub/Cache.cpp:266-288` with `include/sigilio/hub/Hub.h:115-124` · `Hub::probe()` calls `sigil::image::probeImage` and answers `ResourceInfo::Kind::Image` with an `ImageProbe` member — the hub deciding what bytes mean, which `CLAUDE.md` and `io/README.md:262-270` assign to SigilImage. · IO answers where and how many bytes; meaning is a registered prober. · Make `probe()` answer `byteSize`/`path` only and add `probe<T>()` over a registered probe function, so the image case lives in SigilImage's registration.

## 1. Correctness

Besides top-ten items 1-9:

- **should-fix** · `src/common/core/include/sigilcore/compute/Field.h:441-452` · `dimension < 2/3` zeroes y/z before the warp, and the warp then adds to y and z, so a one-dimensional warped field reads a three-dimensional lattice, contradicting 405-409. · Re-zero after warping.
- **should-fix** · `src/common/core/include/sigilcore/compute/Noise.h:117-120` · `pcgUnitNext` divides by `(float)0xFFFFFFFFu`, which rounds to 2^32, and the largest numerator rounds to the same float, so it answers exactly `1.0f` while the doc says `[0, 1)` (`core/README.md:395-400` describes the same defect). · State the closed range or divide through 24 mantissa bits as `Mix64Stream::unit` does.
- **should-fix** · `src/common/core/include/sigilcore/compute/Hash.h:63` · `combine(size_t, uint32_t)` narrows a 64-bit member silently, dropping the high half of a key component. · Take `uint64_t` and mix both halves.
- **should-fix** · `src/common/measure/include/sigilmeasure/stats/Fit.h:83-88` · the degenerate (vertical / n<2) path fills `maxResidual` and leaves `rmsResidual` at 0. · Accumulate `ssRes` on that path too.
- **should-fix** · `src/common/measure/include/sigilmeasure/stats/Fit.h:82` · `den = nn*sxx - sx*sx` is the subtractive form `measure/README.md:105-113` and `Moments.h:25-33` forbid; in `float` over large close x it is rounding, and only exact zero is guarded. · Accumulate `Sxx += (x - mx)²` against running means.
- **should-fix** · `src/common/core/include/sigilcore/compute/Chance.h:358-380` · `Weighted` holds a non-owning `std::span<const float>` inside what the file calls a value, and is the only shape with no `operator==`. · Document the borrow at the member and say why it cannot compare.
- **should-fix** · `src/common/core/include/sigilcore/compute/Chance.h:98,108,152,161` · `pcg`/`xorshift` take a `uint64_t` seed and `bits()` keeps `(uint32_t)m_state`; `Chance::stream()` (461-463) folds a name to 64 bits and then loses half. · Mix the high half down before narrowing, or say so.
- **should-fix** · `src/common/core/include/sigilcore/hardware/Handle.h:24-27,33` · `operator==` is a hidden friend on the base, so `TypedHandle<A> == TypedHandle<B>` compiles and answers true on equal bits. · Declare the comparison on `TypedHandle<Tag>`.
- **should-fix** · `src/common/core/include/sigilcore/compute/Curve.h:187-212` · `inElastic`/`outElastic` divide by the period; `p == 0` answers inf/NaN from a function whose contract (25-29) is a determinate float. · State the precondition or fall back to the default period.
- **should-fix** · `src/common/core/include/sigilcore/reconcile/Reconciler.h:102-105` with `Stats.h:19` · `described` is an out-parameter set at 220/237 and never read; `describedNodes` increments before resolution and so counts visits (memo hits included) while `Stats.h` says descriptions. · Drop the parameter or use it to count.
- **nit** · `src/common/data/decode/Csv.cpp:59-61,88-92` · `grouped()` strips a leading `+` but `std::from_chars` rejects one, so `+5` and `+1,000` become text. · Skip a leading `+` before `from_chars`.
- **nit** · `src/common/data/scale/Scale.cpp:116-120,128-134` · `Symlog` with `threshold == 0` divides by zero; `Pow` with `exponent == 0` inverts through `1.0 / 0`. · Guard both as `Log` guards its base, or document.
- **nit** · `src/common/io/include/sigilio/hub/TextCatalog.h:33-41` · `text(name)` is `m_prefix + name`; a prefix without a trailing `/` (`"shader://glow"`) asks for `shader://glowGlow.sksl`. · Append `/` in the constructor when missing.
- **nit** · `src/common/io/hub/Mounts.cpp:86-94` vs `hub/Selection.cpp:178-187,213-216` · `resolve()` joins a URI's remainder onto the mount root with no `..` check, while the directory walk refuses `..` through `safeRelative`; `blob("res://../x")` and `select("res://../x")` (exact, 291-295) both escape the mount, the glob walk does not. · One rule: refuse `..` in `resolve()` too, or drop `safeRelative`.
- **nit** · `src/spellcircle/shared/net/UdpReceiver.cpp:55-63` · a persistent receive error that is not `operation_aborted` while the socket stays open re-arms immediately with no backoff, spinning the I/O thread. · Stop the loop on a non-transient error and report it.
- **nit** · `src/common/motion/physics/Constraints.cpp:43` and `Forces.cpp:43-46` · `stiffness` is documented in [0, 1] (`Constraints.h:50-55`) and not clamped, so 2.0 overshoots; `Attract` falls off as `1/distance` with nothing bounding it near the centre (a point at 1e-6 receives 1e6·strength), contradicting "falling off with distance" (`Forces.h:35-38`). · Clamp stiffness; floor the attract distance at a documented epsilon or the radius fraction.
- **nit** · `src/common/motion/include/sigilmotion/physics/Points.h:166-174` · `movable()`/`inverseMass()` index the lanes unchecked while `Constraint::project` (`Constraints.cpp:12,20`) checks bounds before calling them; the two conventions differ in one header pair. · State the precondition on `Points` or check once in `project`.
- **nit** · `src/common/core/include/sigilcore/compute/Noise.h:91-95` · `(z & 0xffffff)/0x7fffff - 1.0f` peaks at ≈1.00000024, outside the `[-1, 1]` the doc states.
- **nit** · `src/common/core/include/sigilcore/comparable/Erased.h:69` · `operator*` dereferences `m_state->ops` unchecked while `operator->` (66-68) and `get()` (71) check.
- **nit** · `src/common/core/include/sigilcore/reconcile/Reconciler.h:266` · `indexKeys` recurses into `*child` unchecked; `patchChildren:143` guards `if (child)`.
- **nit** · `src/common/core/include/sigilcore/compute/Intervals.h:148-165` · `firstOverlap` is O(n·m) over inputs the file says are sorted and disjoint; `intersectIntervals` above it is the one sweep.

## 2. Public API

- **should-fix** · `src/common/core/include/sigilcore/reconcile/Erased.h:1-8`, `include/sigilcore/cache/Bake.h:8`, `cache/CMakeLists.txt:11-14` · `reconcile/Erased.h` is a pure re-export of SigilCoreComparable's type; `Bake.h` includes that spelling, which is why `SigilCoreCache` links `SigilCoreReconcile` PUBLIC (Boost.Unordered, ContainerHash, SigilMeasure) for a header-only type. · `Bake.h` includes `<sigilcore/comparable/Erased.h>`, cache links Comparable, delete the re-export.
- **should-fix** · `src/common/core/include/sigilcore/reconcile/Reconcile.h:12-19` · the umbrella omits `Reads.h` while `core/README.md` lists it under the umbrella's directory. · Add the include.
- **should-fix** · `src/common/core/include/sigilcore/compute/Hash.h:41,51` with `include/sigilcore/reconcile/Env.h:117-125` · `fnv1a` is not `constexpr`, so `envTypeTag()` re-implements FNV-1a with basis `14695981039346656037` while `hash::kFnvOffset` is `1469598103934665603` — one fold, two bodies, two constants. · Make `fnv1a`/`combine` `constexpr` and call them from `Env.h`.
- **nit** · `src/common/io/include/sigilio/hub/Hub.h:100,260` · `ResourceLease::uris()` answers `span<const std::string>` and `Hub::preload` takes `span<const std::string_view>`, so a lease's URIs cannot be handed to the hub's own preload without a copy loop (`Retention.cpp:98-101` does exactly that). · Add a `preload(std::span<const std::string>)` overload or answer views.
- **nit** · `src/common/data/include/sigildata/decode/Json.h:53-54` · `Json(bool)` and `Json(double)` make `Json(1)` ambiguous (int→bool and int→double have equal rank); only `const char*` got its disambiguating constructor. · Add `Json(int)`/`Json(long)` forwarding to `double`.
- **nit** · `src/common/core/include/sigilcore/schedule/Parallel.h:67-68` · `parallelFor(count, 1, body)` fails to deduce `Index` from a `size_t` count and an `int` grain; `parallelForEach` (81) takes the grain as `size_t`. · Take `size_t grain`.
- **nit** · `src/common/core/include/sigilcore/compute/Settle.h:53,84` · `moved(Values now)` by value, `observe(…, const Values& now, …)` by reference.
- **nit** · `src/common/core/include/sigilcore/hardware/GpuDevice.h:232` · `Backend_` — a trailing underscore invented to dodge the `Backend` enum at 39; the file is already `DeviceBackend.h`. · Name it `DeviceBackend`.
- **nit** · `src/common/measure/include/sigilmeasure/stats/Samples.h:26-30,47-50,21,39,57,77-116` · `quantile` and `quantiles` duplicate the interpolation the README says is one; `quantile` lacks `[[nodiscard]]` while its siblings carry it; 77-116 mark nothing. · Extract `detail::interpolate`, mark the pure readers.
- **nit** · `src/common/core/include/sigilcore/compute/Noise.h:46,98,103,109,167` · `lattice` is `constexpr`, `mix64`/`pcgAdvance`/`pcgMix`/`pcgHash` plain `inline` — one family, two spellings.
- **nit** · `src/common/core/include/sigilcore/reconcile/Env.h:185-188` · `Provide::~Provide` writes to `stderr` unconditionally with no way to silence it in a headless sweep.

## 3. Documentation drift

- **should-fix** · `scripts/README.md` ("Formatting and linting", the paragraph beginning "Two paths no checker touches") · claims the FlatBuffers-generated sources and `vcpkg_installed/` are named in `.clang-format-ignore`; `/.clang-format-ignore` carries no patterns and says so. · Say the verb's `EXCLUDED_FRAGMENTS` (`scripts/sigil/check.py:37-40`) is the one list, or add the patterns to the file.
- **should-fix** · `src/common/core/include/sigilcore/hardware/GpuDevice.h:93-94` vs `hardware/GpuDevice.cpp:18` · doc says one more level "each time both sides can still be halved"; code loops `while (w > 1 || h > 1)` — 1024×16 is 5 by the doc, 11 by the code. · Reword to "either".
- **should-fix** · `src/common/core/include/sigilcore/compute/Field.h:20,385` · "seven numbers"; the struct has ten members and `core/README.md` says ten. · Say ten.
- **nit** · `src/common/measure/README.md:24-32` · the header table omits public `Moments::clear/empty`, `Histogram::bins/low/high/clear`, `LineFit::samples`, `Laps::size/totalMs/reset`, `FrameTimer::addFrame/addWork/addPresent/reset/resetPresentation`.
- **nit** · `/CLAUDE.md` ("Build and test") · "Ports for `choreograph`, `skia` and `diligent-engine` come from sigil-vcpkg-registry"; `vcpkg-configuration.json:12-18` also routes `shader-slang` and `syphon` there. · List all five.
- **nit** · `/CLAUDE.md` ("Every workflow is also a mise task") vs `/mise.toml` · the `flags` verb has no task; `scripts/sigil.py:9` makes only the converse claim. · Add `[tasks.flags]` or narrow the sentence.
- **nit** · `/.clang-format-ignore:8` · names `scripts/check.py`; the module is `scripts/sigil/check.py`.
- **nit** · `ThreadSanitizerSuppressions.txt:2` · names `scripts/sanitize.py`; the module is `scripts/sigil/sanitize.py`.
- **nit** · `src/common/data/include/sigildata/decode/Csv.h:38` · "nothing when it holds no row at all"; a header-only file answers a table of zero-length columns (`Csv.cpp:335-353`). · Say which.
- **nit** · `src/common/measure/include/sigilmeasure/check/Check.h:220` vs `check/Check.cpp:20-21` · doc promises `, <k> findings`; the code prints `, 1 finding`.
- **nit** · `src/common/measure/include/sigilmeasure/stats/Histogram.h:123-124` · "the total weight that landed in a bin" is the weight in any bin.
- **nit** · `src/common/ui/README.md` · matches the QML and `WindowChrome.h` (verified: `applyVibrancy`, `setSubtitle`, every listed type); no drift.

## 4. Comment-rule violations

- **nit** · `src/spellcircle/mac/bridge/SCKEngine.mm:300` · `// Rendering happens through the paced governor (see drainSocket).` — a citation, to a method that does not exist in the file.
- **nit** · `src/spellcircle/mac/bridge/SCKEngine.mm:411-412` · "Graphite does not auto-upload raster-backed shader images the way Ganesh did" — history, and a dependency compensation without the `workaround:` marker.
- **nit** · `src/spellcircle/mac/CMakeLists.txt:14-23` and `:100-107` · two compensations for swift-driver (rejects sanitizer link flags; `cannotResolveTempPath` on incremental builds) with no `workaround:` line, so `grep -r 'workaround:'` misses them.
- **nit** · `/.clang-format:7-9` · "The whole-tree reformat onto this style is a single commit listed in .git-blame-ignore-revs … see .clang-format-ignore" — history and two citations; `scripts/README.md` ("the one whole-tree reformat that adopted the style") repeats the history.
- **nit** · `scripts/sigil/setup.py:10-12`, `scripts/sigil/check.py:17-18`, `src/sketch/cmake/SketchFlags.py` (the `--help` text printed by `sigil.py flags --help`: "Why the database is the right seam is scripts/README.md") · module docstrings and a shipping help string that cite the README instead of stating the constraint.
- **nit** · `src/common/core/include/sigilcore/schedule/Parallel.h:61` · "see the file comment for how a caller chooses it".

## 5. Leftovers

- **nit** · `src/spellcircle/qt/CMakeLists.txt:217` · `# WIN32_EXECUTABLE TRUE` — commented-out code.
- **nit** · `CMakePresets.json:13-19` and `scripts/sigil/setup.py:254-266` · the `xcode`/`main-xcode` presets: no verb or task consumes them, and `tree.build_dir("main-xcode")` answers `build-main-xcode` while the preset's `binaryDir` is `build-xcode`. · Delete both or route `tree.build_dir` through the preset file.
- **nit** · `/.gitignore:20-21` · `--help/` and `.skilltree-export.json` match nothing in the tree or the scripts (`*_demo_out/` is still live: `src/sigilweave/examples/demo/weave_demo.cpp:29`). · Delete the two lines.
- **nit** · `src/common/io/source/CMakeLists.txt:9` · `find_package(boost_container)` in a leaf whose library links no Boost; only the test at 16-18 does. · Move the find next to the test, or drop the test's flat_map.
- **nit** · unused includes: `<utility>` in `core/include/sigilcore/schedule/Parallel.h:35` and `ConcurrentIo.h:34`; `<cassert>` in `reconcile/Env.h:11`; `<cstdio>` in `core/hardware/GpuDevice.cpp:7`; `<string>` in `reconcile/Host.h:14`.
- **nit** · `src/common/core/include/sigilcore/hardware/Handle.h`, `Fence.h` · the only two public headers in core/measure without a `/** @file */` block.
- **nit** · `src/common/core/include/sigilcore/reconcile/Env.h:222` · `std::move(fallback)` on a `const T&` — a no-op that reads as an optimisation.
- No TODO/FIXME/XXX/HACK, no debug prints outside tests, in any in-scope library (the four `std::fprintf(stderr, …)` in `motion/schedule/Cascade.cpp:52`, `Order.cpp:79`, `clock/Ticker.cpp:68` and `NSLog` in `SCKEngine.mm:253` are refusal reports, not leftovers).

## 6. CMake

- **should-fix** · `src/common/video/decode/CMakeLists.txt:5-8,25-28` with `src/common/ui/CMakeLists.txt:31-34`, `src/spellcircle/mac/CMakeLists.txt:33-36`, `src/spellcircle/qt/CMakeLists.txt:127-135,161-168` · ARC on a library `.mm` is hand-rolled with `set_source_files_properties` (the helper offers `ARC` only to `sigil_test`/`sigil_bench`); frameworks are linked two ways in one file (`find_library(METAL_FRAMEWORK)` and the string `"-framework CoreVideo"`), and `find_library(FOUNDATION/APPKIT/METAL …)` is repeated per directory. · Give `sigil_library` an `ARC` flag and a `FRAMEWORKS <name>...` keyword resolved once in `cmake/Sigil.cmake`; product targets use the same keyword.
- **nit** · `docs/Docs.cmake` · a second tree-wide helper outside `cmake/` that `sigil_library_root()` (`cmake/Sigil.cmake:58-63`) calls into, so `Sigil.cmake` works only when `Docs.cmake` is included alongside it (`CMakeLists.txt:36-37`). `CLAUDE.md` says `cmake/` holds what the whole tree shares. · Move it to `cmake/Docs.cmake` and include it from `Sigil.cmake`, or fold it in.
- **nit** · `cmake/Sigil.cmake:313-316` · the helper names one library (`SigilSkia`) for `sigil_bench(GPU)`, the only place the helper knows a target by name. · Take the GPU arm's link as an argument (`GPU_LIBRARIES`).
- **nit** · `src/common/measure/CMakeLists.txt:13-29` · one target over three subjects (`time/`, `stats/`, `check/`) with `test/` and `bench/` at the root — the one library here not shaped one-feature-per-target with `test/` beside each.
- `sigil_qt_target`: every directory declaring `Q_OBJECT`/`QML_ELEMENT` (`common/ui`, `spellcircle/qt`, `sketch/book`, `sigilweave/examples/gallery`, `skia/qt`) calls it before `qt_add_qml_module`; nothing is missing. Every `vcpkg.json` dependency is consumed by a `find_package` or an include (`cgltf` by `geometry/mesh/codec`). `SPELLCIRCLE_DOCS_WARN_UNDOCUMENTED` is documented in `docs/README.md:90` and read by `docs.py`; not dead.

## 7. Files over ~600 lines

- `src/common/motion/bind/test/BindTest.cpp` (875) · one test file per header: `BindTest` (chain builder), `BoundFloatTest` (evaluation), `WiggleNoiseTest`, `CurveComparatorTest`.
- `src/common/io/hub/test/HubTest.cpp` (834) · split to mirror the sources: `MountsTest`, `SelectionTest`, `RetentionTest`, `CacheTest` (views, poll, write), `NetworkTest`, `OiioTest`.
- `src/common/video/decode/Decode.cpp` (695) · `Container.cpp` (open/probe/seek), `Frames.cpp` (decode loop and cache), `Raster.cpp` (CPU executor), `Video.cpp` (facade and `draw`).
- `src/spellcircle/mac/bridge/SCKEngine.mm` (667) · `SCKEngine+Network.mm` (receiver, feed, rate), `SCKEngine+Render.mm` (scene texture, Syphon), `SCKEngine+Blit.mm` (window blit and top-edge chrome), setters in the base file.
- `src/common/core/README.md` (843), `src/common/motion/README.md` (662) · a chapter file per feature beside the README, the way `compose/TYPOGRAPHY.md` sits beside `compose/README.md`.

## 8. Test gaps

- `io_test`: no case loads from several threads while a lease is retained/dropped or `discardUnretained()` runs; `ConcurrentColdLoadsPublishOneCachedView` (`HubTest.cpp:153-159`) is the only multi-threaded case and touches no lease. No case for two concurrent fetches of one network URL (top-ten 4), a `./`-prefixed relative glob (7), a directory passed to `blob()` (5), a selector containing `..`, a `TextCatalog` prefix without a slash, or `poll()` racing a `write()` on the same URI.
- `data_test` decoders (`DecodeTest.cpp`): none of — empty file, BOM-only file, BOM before the header, CRLF and lone-CR endings, an unterminated quote at EOF, a header-only file, duplicate header names (6), `+5`, `NaN`/`inf` cells, a JSON top-level scalar, duplicate JSON keys, deep nesting, invalid UTF-8, a number past double range.
- `data_test` scales: `Symlog` at `threshold == 0`, `Pow` at `exponent == 0`, `Log` with a domain touching zero (documented NaN, unasserted), `Threshold` with unsorted thresholds.
- `motion_test` physics (`PhysicsTest.cpp`): no `dt == 0` (the early return at `Verlet.cpp:15`), no large-`dt` stability claim, no `iterations == 0`, no `stiffness > 1`, no attract at near-zero distance, no `Body` force with a null `body`.
- `core_test`: `Reconciler` with two same-keyed siblings (2); `foldSubtree` with `samplesDestination && !readsBackdrop` (8); `Stream::normal()` over `Stratified`/`Halton`/`Sobol`/`Golden` (1); `Field` `dimension == 1 && warp != 0` and `frequency == 0`; `outElastic`/`inElastic` at `p == 0`; cross-tag `TypedHandle` comparison; `hash::combine` above 2^32; `Settle` with `hold <= 0`.
- `measure_test`: `lineFit` `rmsResidual` on the degenerate path; `Laps::mark` with a temporary name.
- `video_test`: no `Encoder::append` after `finish()` (guarded at `Encode.cpp:174`, unasserted), no `finish()` twice, no zero-frame `finish()`, no seek past duration on a non-looping `draw`, no `frameAt` on a stream with `AV_NOPTS_VALUE` timestamps.
- `spellcircle_test`: `RejectsTruncatedPayload` is the only malformed-packet case; no oversized datagram, no verifier depth/table-count limit, no scene with a circle index out of range for a point, no empty scene.

## FINDINGS.md triage

- "Ring and grid placement is respelled where geometry already has it" — **stale in its list**: of the 51 named sketch files, 4 still match the spellings the entry quotes (`(float)i / (float)n`, `/ (n - 1)`); rewrite the entry to the remaining sites or delete it.
- "Automatic texture promotion moves 84 of 161 plates" — open; unverifiable without a plates run. Not a merge blocker (a compose defect, recorded as such).
- "Two causes … fixed, and a third of it is open" — **partially stale**: `blends()` now exists on `compose/include/sigilcompose/brush/LayerStyles.h:123`, `Layered.h:48`, `PixelStyles.h:184`, `Decorations.h:374` and `core/Shape.h:421,455,513` requires it through a concept, so "only decorations and `custom()` declare it today" is no longer what the code says. Re-verify which of Brush / Silhouette / Material program still lack it and rewrite or delete.
- "chaucer_astrolabe cannot finish a plate under the sweep's ceiling" — open; unverifiable here.
- "slang_portable's plate draws a counter over the process's history" — **still open**: `material/slang/SlangCompiler.cpp:185-186` still names modules from a function-local `static uint64_t serial`.
- Must be resolved before merge: none of the five is in this review's scope; the two stale ones should be rewritten so the file states what the tree contains at merge.

## Stray files

- `default/` — an empty, untracked directory at the app root (`--headless default` residue). Delete.
- `src/spellcircle/shared/schema/build/` — a stale CMake tree (`CMakeCache.txt` and siblings) inside the source directory, ignored by the `build/` pattern but present. Delete.
- `.DS_Store` at `/`, `/apps`, `/apps/python`, `apps/spell-circle-canvas`, `src/sketch`, `/touchdesigner`, `/touchdesigner/scripts` — ignored, delete.
- Ignored build products outside `build/`: `/apps/python/build/`, `/apps/python/ifrit_protocol_apps.egg-info/`, `/apps/python/.venv/`, `/touchdesigner/.venv/`, `/.ruff_cache/`, `__pycache__` under `scripts/`, `scripts/sigil/`, `src/common/compose/test/docs/`, `src/sketch/book/test/`. All ignored; nothing tracked that should not be (`git ls-files` finds no `.DS_Store`, `*_demo_out`, logs or CMake caches).
- `bench/app_fps_Release.json`, `bench/baseline_Release.json`, `ThreadSanitizerSuppressions.txt`, `CMakePresets.json` are tracked on purpose.

## Counts

- blocker: 1
- should-fix: 26
- nit: 43
