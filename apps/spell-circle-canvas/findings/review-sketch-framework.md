# Merge-readiness review — sketch framework (src/sketch minus sketches/)

Branch `sigil/library-campaigns` against merge base `aabd3fe1b224`. Read-only.
Paths are relative to `apps/spell-circle-canvas/src/sketch/`.

## Top ten, most severe first

1. **blocker** `set/Session.cpp:102-104`, `book/SketchCatalog.cpp:464-488`, `plate/Thumbnails.h:13-14`, `book/main.cpp:1169,1237,1323-1329` — `SetSession::frame` draws through the process-wide `processRuntime()`; in the app that is the Diligent device runtime installed by `useDevice()` (main.cpp:1169) before `SketchCatalog` exists (QML load at 1237), so the background thumbnail worker renders every set sketch through the same device and queue the render thread is drawing with. Thumbnails.h ("CPU only … never a device one") and README.md:820 ("never touching the device") are false for sets. At quit, `releaseDevice()` (main.cpp:1329) runs before the QML engine's destruction joins the worker (`~SketchCatalog`), so a set render in flight finishes its frame through a destroyed device. Should: a thumbnail is CPU-only whatever the process holds, and nothing renders after the device goes. Fix: make the runtime a per-session value (`Kind::open` takes a `world::Runtime`, `renderThumbnail` passes `Runtime::cpu()`); until then `wantsThumbnail` stands down `needsDevice()` kinds while `sketch::device()` is set, and main ends the fill and joins the worker before `releaseDevice()`.
2. **blocker** `live/Host.cpp:456-457,278-287` — the skew guard calls `hostBinaryTime()` on every compile, which stats the executable's path on disk. After the host is rebuilt while the window is still running, the file is newer than every header, the guard passes, and the old running process dlopens a dylib compiled against the new headers — the corruption the guard exists to refuse (its own message at 462-472 tells the user to rebuild and re-save). Should: compare against the image that is running. Fix: read the stamp once (function-local static taken at first call, before any rebuild can land) or record it in `Host::Options` at process start.
3. **should-fix** `book/SketchbookView.cpp:369,442,452-453` — `openSketch` and `publishMetrics` run on the render thread outside `synchronize()` and assign `m_view->m_orbitable`, `m_view->m_metrics` (an implicitly shared `QVariantMap`) and `m_view->m_orbit`, while the GUI thread copies them through the property getters at any time (Main.qml:454-460 reads `view.metrics` on every change). Should: cross-thread state moves only in `synchronize()` or on the GUI thread. Fix: carry the values in the already-queued lambdas (`invokeMethod(m_view, [view, metrics = std::move(metrics)] { view->m_metrics = …; emit …; })`).
4. **should-fix** `plate/Sweep.cpp:415,450` vs `plate/Sweep.h:98` — on the GPU lane a failed async readback does `continue`, writes no plate, and the sweep returns 0 (and prints no "wrote N plates" line under `--gpu`); the header promises 0 only when every selected sketch rendered. Fix: treat it as the raster path treats a failed write — close `timingJson`, print the sketch, `return 1`.
5. **should-fix** `README.md:623-627` vs `book/main.cpp:1037-1051` — README: "`--gpu` is REQUIRED for a selection that holds a set … a run that asks for the device and cannot have it fails"; code: a device is brought up only when `--gpu` is given AND the selection needs one, and without `--gpu` a set is encoded on the CPU mesh executor — the "picture no recipe ran in" the README says is refused. Fix: `if (!gpu && selectionNeedsDevice(chosen, kind)) { fprintf(...); return 2; }` in the `--video` branch, or rewrite the paragraph to the code.
6. **should-fix** `include/sigilsketch/live/Host.h:36-45`, `book/SketchCatalog.h:44`, `book/main.cpp:736` — three comments say the host binary's stamp is folded into the thumbnail key / a still is kept "until the sketch's source or the host changes" / "while its source and its host stay put"; `plate/Thumbnails.h:57-61`, `plate/Thumbnails.cpp:73-77` and README.md:806-811 state the opposite and the code hashes the source alone. Fix: delete the host clause from the three; `hostBinaryTime`'s doc becomes "what the skew guard compares headers against".
7. **should-fix** `live/Host.cpp:461-472`, `core/Crash.cpp:1018-1033` — shipped strings carry workflow prose and citations: the skew message instructs "ONE AGENT IN A SHARED SESSION … WAIT A MOMENT AND RE-RUN THIS EXACT COMMAND. Do not run cmake or ninja yourself"; the crash handler cites "Patterns.h" and "sketches/stock_materials.cpp" and states "A sketch dylib links its own libskia.a", which `book/CMakeLists.txt:126-130` and `Host.h:52-56` say is not the case (nothing is linked into the dylib; Skia resolves out of the host). Fix: skew text = "framework headers are newer than this host: rebuild Sketchbook and restart it"; crash text = the two constraints (a non-monolithic SkSL main / uniform-guarded loop faults on PAC across the host/dylib line; a steppable must not capture `SketchContext&`) with no file names and no libskia claim.
8. **should-fix** `live/Residency.cpp:35-36`, `live/Host.cpp:342`, `book/SketchbookView.cpp:344-348,657,760` — `Residency::present` evicts inside `hostMutex` on the render thread; `~Host` waits on its compile future; so evicting a host mid-build stalls the render thread for a clang build, and the GUI thread's poll (760) and pointer/key calls block behind the same lock. Fix: `present()` returns the evicted `unique_ptr<Host>` and `openSketch` destroys it after releasing `hostMutex` (or the compile moves off `std::async` so `~Host` need not join).
9. **should-fix** `include/sigilsketch/kit/Rows.h:30,46`, `Legend.h:67`, `Ticker.h:75`, `Heading.h:39`, `Legend.h:42,127`, `Console.h:41`, `Scrollbar.h:64` — one concept spelt two ways: `Reading::swatch` is a `Ground` (a paint) while `Readout::swatch` and `Legend::swatch` are `optional<float>` (a side); `Timeline::ink` is `optional<compose::Fill>` while `Line::ink`, `LegendEntry::ink`, `Chip::ink`, `Console::ink`, `Timeline::Mark::ink` are `optional<SkColor4f>`; `Scrollbar::horizontal` (false = down) is the opposite polarity of `Run::column`/`Legend::column` (false = along). Fix: `swatchSide` for the length; `Fill` for every ink (the shader case Ticker.cpp:36-49 handles is wanted everywhere); `column` on the scrollbar.
10. **should-fix** `include/sigilsketch/kit/Panel.h:71,73,77`, `Legend.h:128`, `Ticker.h:71`, `Cells.h:108-124` — look decisions live in props with hard defaults (`Frame::corners = 6`, `screenCorners = 2`, `bezel = 10`, `Chip::corners = 2`, `Timeline::tick = 5`) instead of `Theme::spacing`, so a sketch under another theme has to restate them at every call; and `columns(Columns)` is `panelGrid` with `columns = cells.size()` and no `rowGap` — a near-duplicate verb. Fix: `Spacing::panelCorners/screenCorners/bezel/chipCorners/tickReach` read where the (now optional) prop is unset; delete `columns` and let `PanelGrid::columns = 0` mean "one row".

## 1. Correctness

- **blocker** — top-ten #1 (worker draws sets on the device; device released under the worker).
- **blocker** — top-ten #2 (skew guard reads the rebuilt binary's stamp).
- **should-fix** — top-ten #3 (render-thread writes to `SketchbookView` members outside `synchronize`).
- **should-fix** — top-ten #4 (GPU readback failure exits 0).
- **should-fix** — top-ten #8 (eviction of a compiling host under `hostMutex`).
- **should-fix** `book/main.cpp:1029-1035`, `sketches/web_panel.cpp:198-221` — the `--thumbnails` warm command is the one lane that creates no `SharedWebEngineScope` (every other lane does: 1044, 1066, 1136, 1173), so `sharedEngine()` answers null and a web sketch's still is its `unavailable(why)` card, written under the sketch's key as if it were the picture. Fix: a `SharedWebEngineScope` around `runThumbnails`.
- **nit** `book/main.cpp:1178-1179`, `live/Host.cpp:323-324` — the comment says the owner's async sweep keeps the walk off the render thread; nothing orders the async's `g_swept.store(true)` before the first host's `g_swept.exchange(true)`, so when the render thread opens the first sketch first, the host walks the temp directory under `hostMutex` after all and the async walks again. Fix: flip the flag synchronously (`Host::claimSweep()`) before launching the async.
- **nit** `live/Residency.cpp:35-36` — the new host is opened before the oldest is evicted, so the "memory budget" the header states (Residency.h:28-30) is exceeded by one whole session on every switch. Fix: `pop_back` when `size() == m_capacity` before `open()`.
- **nit** `book/SketchCatalog.cpp:311-326` — the destructor joins the thumbnail worker but leaves `m_task` (a child `--video` encode) running; `QProcess` kills it in its own destructor with a warning. Fix: `m_task.kill(); m_task.waitForFinished();` or say the child is meant to outlive the window and `startDetached` it.
- **nit** `plate/Sweep.cpp:230` vs `live/Host.cpp:637` — the sweep wraps `session->frame` (update and draw) in `PhaseMark(Phase::Draw)`, the host wraps the same call in `Phase::Update`; a fault in a body's `update()` reads "Composer::draw()" in a sweep and "Sketch::update()" in the window. Fix: one phase name for the whole frame call in both, or the two marks inside `Session::frame`.
- **nit** `live/Host.cpp:548-557` — on an ABI/entry-point refusal the `dlopen` handle is neither recorded in `m_libraries` nor closed; nothing in that image is referenced, so it can go. Fix: `dlclose(handle)` before each early `return`.
- **nit** `book/main.cpp:347` — "Always exits 0" while lines 350 and 359 return 1 (README.md:694 repeats it). Fix: "exits 0 whenever it measured; a build or surface failure exits 1".
- **nit** `plate/Thumbnails.cpp:244-247` — the stem is recovered by string surgery on `run.out` (`substr` up to `__`) when every caller already has it. Fix: `ThumbnailRun::stem`.

## 2. Kit API consistency

- **should-fix** — top-ten #9 (swatch / ink / orientation spelt two ways).
- **should-fix** — top-ten #10 (look defaults in props; `columns` duplicates `panelGrid`).
- **should-fix** `kit/README.md:365-368`, `kit/CMakeLists.txt:12-13,19` — "It draws nothing … links no device and no runtime" / "links no device, no runtime and no sketch", while the target links `SigilSketch` PUBLIC — the archive that holds all three runtimes, the reload engine and the sweep — because `stage()` spells `SketchContext`. Fix: split `core/` (CanvasSpec, the contexts' seam) into its own target the kit can link, or state that the kit links the canvas runtime.
- **nit** `include/sigilsketch/kit/Page.h:12`, `Passage.h:8` — each includes `<sigilsketch/canvas/Sketch.h>` (the whole compose surface) to name `SketchContext&` in one signature. Fix: `struct SketchContext;` forward declaration; include in the .cpp.
- **nit** `include/sigilsketch/kit/Theme.h:4-5,20,155-156` — "the five colours", "THE FIVE COLOURS", "the registers the four lines are set in", "none of the four registers": `Palette` has six fields, `TypeScale` seven registers (kit/README.md:37 counts them right). Fix: drop the numbers.

## 3. README drift

- **should-fix** `README.md:820-821,849-852` — "on the CPU and never touching the device"; "Set sketches render through the same path the CPU tier uses for them" is true of `--thumbnails` only (top-ten #1). Fix with #1.
- **should-fix** `README.md:623-627` — `--video --gpu` (top-ten #5).
- **should-fix** `README.md:806-811` is right; `Host.h:36-45`, `SketchCatalog.h:44`, `main.cpp:736` contradict it (top-ten #6).
- **should-fix** `kit/README.md:365-368` — "links no device and no runtime" (see §2).
- **nit** `book/main.cpp:6` — usage says `--list [--kind canvas|set]`; README.md:528 and the code (`selection()` matches any `runtime()` string) take `draw` too. Fix: `canvas|set|draw`.
- **nit** `README.md:694` — "It always exits 0" (see §1, `runBench`).
- **nit** `test/support/Sessions.h:7` — "the five claims below" registers six (README.md:1279 says six). Fix: "six".
- Verified as matching: `--headless`, `--frame`, `--bench`, `--shot`, `--compare`, `--thumbnails`, `--promotion`/`--no-promotion`, `--catalog`, `--list`, `--window-bench`, `scripts/sigil.py bench --lane fps` (scripts/sigil/bench.py:7-15), the four targets (README.md:1162-1167), the thumbnail store/key/notes/refresh text, the header-parsing rule (README.md:867-886 = SketchCatalog.cpp:42-66), `SIGIL_SKETCH_ONLY`, and every kit API name in kit/README.md against the headers.

## 4. Comment-rule violations

- **should-fix** `live/Host.cpp:461-472`, `core/Crash.cpp:1018-1033` — shipped strings with workflow prose, citations, and a stale mechanism (top-ten #7).
- **nit** `scry/test/SettledPageTest.cpp:2-5` — "sketch_settled_test — … so this is a binary of its own rather than a case beside the shared-engine test"; there is one `sketch_test` binary (scry/CMakeLists.txt:9-16). Fix: "…a case ctest runs in a process of its own, because the engine allows one renderer per process".
- **nit** `scry/CMakeLists.txt:12` — "now that the library has one binary" (history). Fix: "since the library has one binary".
- **nit** `book/SketchbookView.cpp:221-223` — "Declared before everything Skia so reverse destruction releases any Graphite-backed images before the context goes": the renderer holds no images; the resident sessions (static `SketchbookView::sessions`) do, and `initialize()` clears them by hand. Fix: delete or say what it orders.
- **nit** `book/test/reserved_word_probe.cpp:17-21` — "(see live/Host.cpp — …)" and "The fix is in SketchbookView::render()…" (citation and history). Fix: state the constraint: a failed Graphite frame drops one session and keeps the context.
- **nit** `cmake/SketchFlags.py:7` — "Why the database is the right seam is scripts/README.md" (citation). Fix: state it: the database is the only place the build's own flags are written down after generate.
- **nit** `book/qml/PlateThumb.qml:1` — "the still the quick tier photographed it as": there is no quick tier; the still is the app's own store (README.md:799). Fix: "the still the app's store holds".
- **nit** `book/main.cpp:1226-1233`, `main.cpp:86-88` — hand-registers `SketchCatalog` and forces the generated `qml_register_types_Sigil_Sketchbook()` first to compensate for Qt's lazy module registration, with no `workaround:` marker; the class is already a module SOURCE, so `QML_ELEMENT` on `SketchCatalog` removes both the call and the extern declaration (the statics are set before the engine loads either way). Fix: `QML_ELEMENT` in SketchCatalog.h; delete main.cpp:86-88 and 1223-1233's hand registration.

## 5. Leftovers

- **should-fix** `book/main.cpp:211-217` = `book/SketchbookView.cpp:80-87` — two identical anonymous-namespace `fonts()` each leaking its own `FontContext`; the thumbnail worker, `--frame`, `--bench`, the sweep and the montage use main's, live sessions SketchbookView's. Nothing says the split is deliberate (one context per thread) and the shaping caches are paid twice. Fix: one owner (`SketchCatalog::thumbnailFonts` is already the seam main hands over; SketchbookView takes one the same way), with the per-thread rule stated if that is why there are two.
- **nit** `book/main.cpp:1217-1218` — repeats `setOrganizationDomain`/`setApplicationName` already done at 814-815. Fix: delete.
- **nit** `plate/Thumbnails.cpp:246` — stem surgery (see §1).
- No TODO/FIXME, debug prints, commented-out code or stale QML found; `.DS_Store`, `.qmlls.ini` and `__pycache__` under `book/test/` are ignored, not tracked.

## 6. Duplicated mechanisms

- **should-fix** — the two `fonts()` (§5).
- **nit** `book/SketchbookView.cpp:110-121` (`fitOf`) and `plate/Story.cpp:73-81` (`fit`) — the same letterbox arithmetic twice in one library. Fix: one `core/` helper (or the geometry kit's rect fit, if it has one) used by both.
- **nit** `plate/Thumbnails.cpp:55-59` — a hand-rolled hash combine; if `sigilcore` carries one, use it.

## 7. Files over ~600 lines

- **should-fix** `book/main.cpp` (1331) — split by lane: `Arguments.cpp` (the parse, 840-960), `FrameLane.cpp` (`runFrames`/`runBench`, 290-578), `WindowBench.cpp` (580-708), `ThumbnailWarm.cpp` (710-803), `main.cpp` (wiring).
- **should-fix** `book/SketchbookView.cpp` (896) — `SketchbookRenderer.cpp` (201-746), `Keys.cpp` (`keyAs`, 123-197), `SketchbookView.cpp` (the item).
- **should-fix** `kit/test/SketchKitTest.cpp` (1221) — one test file per kit header (Theme, Page/Stage, Cells, Heading, Rows, Legend, Meter, Scrollbar, Panel, Console, Ticker), the `Host`/`sameDrawing` fixture in `kit/test/Drawn.h`.
- **should-fix** `book/qml/Main.qml` (850) — the filter/sort/fold model (122-292, 304-392) is a browser model, not a window: `Browser.qml` or a `QSortFilterProxyModel` in C++ beside `SketchCatalog`.
- **nit** `live/Host.cpp` (696) — `BuildDir.cpp` (181-313: the pid-named directory and the sweep), `SkewGuard.cpp` (129-179, 278-287), `Host.cpp`.
- **nit** `book/SketchCatalog.cpp` (631) — `SketchHeader.cpp` (42-202: the header parser), `ThumbnailWorker.cpp` (369-546), `SketchCatalog.cpp`.

## 8. Test gaps

- **should-fix** `book/SketchCatalog.cpp:391-546` — the worker's queue and cancellation (front-of-queue on `requestThumbnail`, `cancelThumbnail` of a pending vs in-flight index, `endFill` abandoning the walk at its next frame, the destructor joining with a render in flight, `m_failed` never retried) has no case; only two script tests exercise the book, neither touches the worker. Fix: lift the queue/worker into a Qt-free class under `plate/` (`ThumbnailQueue`) and test it in `plate/test/`.
- **should-fix** `plate/test/ThumbnailsTest.cpp` — every fixture is a canvas kind; a set-kind case with `useRuntime(...)` installed would have caught top-ten #1 (assert the render never reaches the installed runtime, once the runtime is per-session).
- **should-fix** `live/test/HostTest.cpp:185-215` — the skew guard is tested only for a header newer than the binary on disk; the stamp is not injectable (`dladdr`), so the rebuilt-while-running case (top-ten #2) cannot be asserted. Fix: `Host::Options::hostStamp` (defaulting to the once-captured stamp) and a case that moves the binary's mtime forward after construction and expects the refusal to stand.
- **nit** `plate/test/CompareTest.cpp` — the `size <name> WxH WxH` line (Compare.cpp:150-155) has no case.
- **nit** `plate/test/ThumbnailsTest.cpp` — `thumbnailKey` on a directory sketch (Thumbnails.cpp:79-91: siblings hashed, order-independent) has no case.
- **nit** `book/SketchbookView.cpp:134-197` — `keyAs` (the p5 key table) is in an anonymous namespace and untested; move it to `core/` beside `Session::key` and pin the table.
- **nit** `plate/test/SweepTest.cpp:340` — `story()` is exercised on a canvas kind only; the `retainsPixels()` pre-roll path (Story.cpp:210-244) has no draw-kind case.

## Counts

- blocker: 2
- should-fix: 19
- nit: 29
