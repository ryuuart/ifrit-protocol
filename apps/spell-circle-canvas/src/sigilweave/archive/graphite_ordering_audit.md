# Graphite recording-order audit

Companion to `src/sigilweave/ROADMAP.md`. Same contract: a documented
limit is a claim until measured, and a correctness claim is a claim until
the code is read. Nothing here is a measurement — this file exists so the
one-line flag flip in ROADMAP §1 can ride a number instead of a hope.

---

## 2026-07-26 — §1 `fRequireOrderedRecordings`: every site, every guarantee

Skia in this tree is **m151** (`SK_MILESTONE 151`, confirmed at
`build/vcpkg_installed/arm64-osx/include/skia/include/core/SkMilestone.h:8`;
the m148 tree also present under vcpkg buildtrees is the pre-bump one).
All Skia line numbers below are from the m151 checkout at
`/Users/long/.local/share/vcpkg/buildtrees/skia/src/9afeb3a4fe-5a546058eb.clean/`.

### What the flag actually does (source, not release note)

`Recorder::snap()` — `src/gpu/graphite/Recorder.cpp:265-267`:

```cpp
    if (!fRequireOrderedRecordings) {
        fAtlasProvider->invalidateAtlases();
    }
```

and `AtlasProvider::invalidateAtlases()` — `src/gpu/graphite/AtlasProvider.cpp:137-149`
— calls `fTextAtlasManager->evictAtlases()` (plus raster-path and clip
atlases). So with the flag at its default `false`, **every `snap()` evicts
the text atlas**, and the next Recording re-uploads every glyph it touches.
That is precisely ROADMAP §1's claim, now grounded: the cost is per snap,
i.e. per frame on every host in this repo, and it lands hardest on dense
static text replayed through a picture (ROADMAP's "measurement gap").

Two spellings exist, and the *Recorder* one is the better seam:

- `ContextOptions::fRequireOrderedRecordings` (`bool`, default `false`) —
  `include/gpu/graphite/ContextOptions.h:121`. Context-wide default.
- `RecorderOptions::fRequireOrderedRecordings` (`std::optional<bool>`) —
  `include/gpu/graphite/Recorder.h:87`. Per-Recorder override; wins when
  engaged (`Recorder.cpp:131-133`).

This repo already owns exactly one funnel for the second one:
`SkiaGraphiteContext::makeRecorderOptions()`
(`src/common/skia/SkiaGraphiteContextCommon.cpp:69-73`), which **both**
backend factories are required to pass to `makeRecorder()`
(`SkiaGraphiteContextMetal.mm:34-35`, `SkiaGraphiteContextVulkan.cpp:89-90`).
One line there covers Metal and Vulkan at once — better than the two
`ContextFactory::Make*(backendContext, {})` sites ROADMAP §1 names
(`SkiaGraphiteContextMetal.mm:30`, `SkiaGraphiteContextVulkan.cpp:85`),
because it cannot drift apart per backend.

### How the requirement is enforced

`QueueManager::addRecording()` — `src/gpu/graphite/QueueManager.cpp:128-137`:

```cpp
    uint32_t recorderID = info.fRecording->priv().recorderID();
    if (recorderID != SK_InvalidGenID) {
        uint32_t* recordingID = fLastAddedRecordingIDs.find(recorderID);
        RETURN_FAIL_IF(recordingID && info.fRecording->priv().uniqueID() != *recordingID + 1,
                       InsertStatus::kOutOfOrderRecording,
                       "Recordings are expected to be replayed in order");
        fLastAddedRecordingIDs.set(recorderID, info.fRecording->priv().uniqueID());
    }
```

Four consequences that decide the verdict:

1. **`recorderID` is `SK_InvalidGenID` unless the flag is on**
   (`Recorder.cpp:216-218`) — today the check is dead code for us. Flipping
   the flag is what arms it.
2. The requirement is **strict successor**, not merely non-decreasing:
   `uniqueID == last + 1`. Skipping an ID is a failure.
3. `fNextRecordingID++` is consumed **inside `snap()`** (`Recorder.cpp:216`),
   *before* validity is known. When `prepareResources()` fails, `snap()`
   returns `nullptr` (`Recorder.cpp:252-255`) — but the ID was already burnt.
4. There is **no reset path**. `fLastAddedRecordingIDs` is written as soon
   as the order check passes — before the later failure paths, so an insert
   that fails downstream still advances the chain (`QueueManager.cpp:136`;
   `QueueManager.h:103` is its sole other mention); once
   the chain skips, every later insert from that Recorder fails forever.
   Recovery means a new Recorder, i.e. rebuilding the `SkiaGraphiteContext`.

The failure is *not* memory corruption, contrary to ROADMAP §1's phrasing —
m151 returns `InsertStatus::kOutOfOrderRecording` and makes **no** command-
buffer changes (`GraphiteTypes.h:57-62`). It is a silent permanent
no-render, which for a receiver app is arguably worse than a crash: the
frame loop keeps ticking and publishing an unchanged texture.
**No call site in this repo checks `insertRecording()`'s return value.**

### The sites

Every `insertRecording` and every Recorder/Recording creation in the tree.
"Chain-safe" = snap and insert are adjacent, same thread, same Context, no
snap discarded and no reordering.

| # | Site | Recorder | Pattern | Ordering guarantee |
|---|------|----------|---------|--------------------|
| 1 | `src/common/skia/SkiaOffscreenSurfaceCommon.cpp:27-33` | `SkiaGraphiteContext`'s one | snap → insert → submit(kNo) | **Chain-safe.** The production path: Qt `SkiaSceneBackend` (`src/spellcircle/qt/src/SkiaSceneBackend.cpp:44`), mac `SCKEngine` (`src/spellcircle/mac/bridge/SCKEngine.mm:555,604`), `WeaveGallery` (`src/sigilweave/examples/gallery/src/GalleryView.cpp:441`). Single render thread. Caveat A applies (`if (!recording) return;`). |
| 2 | `src/common/compose/bench/ComposeBench.cpp:370-376` (`submitGraphite`) | same | snap → insert → submit | **Chain-safe.** Caveat A. |
| 3 | `src/common/compose/sketch/ComposeSketchView.cpp:172-176` | same | snap → insert → async readback | **Chain-safe.** Caveat A. |
| 4 | `src/common/compose/gallery/GalleryScenes.h:264-269` | same | per-frame `flushHook`: snap → insert → submit(kYes) | **Chain-safe.** Caveat A. |
| 5 | `src/common/compose/gallery/GalleryScenes.h:430-435` | same | snap → insert → async readback (capture) | **Chain-safe.** Caveat A. |
| 6 | `src/common/compose/test/ComposeGpuTest.mm:62-66` | same | snap → insert → readback | **Chain-safe.** Caveat A. |
| 7 | `src/common/compose/test/ComposeGpuTest.mm:203-207` | same | snap → insert → readback | **Chain-safe.** Caveat A. |
| 8 | `src/common/scry/test/WebViewGpuTest.mm:65-71` | same | snap → insert → readback | **Chain-safe.** Caveat A. |
| 9 | `src/common/scry/test/WebViewGpuTest.mm:179-185` | same | snap → insert → submit | **Chain-safe.** `ASSERT_NE(recording, nullptr)` — the only site that even notices a null snap. |
| 10 | `src/common/scry/examples/scry_gpu_demo.mm:216-221` | same | snap → insert → submit | **Chain-safe.** Caveat A. |
| 11 | `src/common/scry/bench/WebBench.cpp:100-106` (`submitGraphite`) | same | snap → insert → submit | **Chain-safe.** Caveat A. |
| 12 | **`src/common/scry/bench/WebBench.cpp:186`** | same | **snap → DISCARD** | ⛔ **BREAKS THE CHAIN.** `BM_Draw_GraphiteRecord` snaps and deliberately drops the accumulated recording ("executing hundreds of thousands of full-screen draws would saturate the GPU"). ID consumed, never inserted → every subsequent insert in that process returns `kOutOfOrderRecording`, unchecked. `scry_bench --gpu` would keep printing timings for frames that never render. |
| 13 | `src/common/scry/metal/UltralightMetalDriver.mm:467-474` (`paintTexture`) | **its own** (`m_state->skiaRecorder`, `.mm:92`) | snap → insert → submit | **Chain-safe, and separate.** Created at `.mm:161-164` from a **second** `skgpu::graphite::Context` (`ContextFactory::MakeMetal`, `.mm:158-162`) on the same MTLDevice/queue. Different Context ⇒ different `QueueManager` ⇒ its own `fLastAddedRecordingIDs` row. Web thread only. **Note: it calls the bare `makeRecorder()` with no options** — so it gets neither our `CachingImageProvider` nor any flag we set in `makeRecorderOptions()`. Independent decision. |

Recorder/Recording creation sites, exhaustively: `SkiaGraphiteContextMetal.mm:34-35`,
`SkiaGraphiteContextVulkan.cpp:89-90`, `UltralightMetalDriver.mm:164`. No
others — `Composer`, `SigilCompose`'s `GpuImage.h:53` and `WebView::frame()`
only *borrow* a `Recorder*` (`canvas.recorder()`), they never create or snap
one. Skia's own `Context::makeInternalRecorder` (`Context.cpp:228-238`),
reached by the async-readback sites (`GalleryScenes.h:430`,
`ComposeSketchView.cpp:172`), explicitly forces
`fRequireOrderedRecordings = false` on its internal recorder — so internal
recorders can never break the chain, which strengthens the verdict. No
`makeDeferredCanvas()` anywhere, so no `InsertRecordingInfo::fTargetSurface`
deferred-target path is in play.

**Caveat A (applies to sites 1-8, 10-11).** All spell `if (auto recording = snap()) { insert }`
or `if (!recording) return;`. Today a null snap costs one dropped frame.
With the flag on it silently and permanently kills the Recorder (see
consequence 3/4 above). This is a *new* failure mode the flag introduces,
independent of any client sequencing error.

### Threading

Single-threaded per `SkiaGraphiteContext`, and the evidence is explicit:

- `SkiaGraphiteContextVulkan.cpp:77-78`: "the context and its one recorder
  live on the render thread, matching the Metal setup", with
  `skgpu::ThreadSafe::kNo` on the memory allocator.
- scry never touches the host Recorder off-thread: `WebInternal.h:206-212`
  states "the wrap itself happens on the recorder owner's thread", and
  `WebView::frame()` (`WebView.cpp:231-262`) is called by the host from
  `WebView::draw()` with `canvas.recorder()`.
- scry's own Graphite Context/Recorder is web-thread-only and, being a
  distinct Context, cannot interleave with the host's ordering chain.

No site snaps a Recording on one thread and inserts it on another; no site
holds two live Recordings simultaneously.

### Verdict

**SAFE — conditional.** The ordering property itself holds at every site
except one, and the enforcement is per-Recorder so scry's driver Context is
out of scope. Two conditions must be closed before the flip lands:

- **C1 (blocking).** `src/common/scry/bench/WebBench.cpp:186` must stop
  discarding a snapped Recording. Cheapest correct fix: insert it and
  submit once outside the timing loop, or (better for the bench's intent)
  give `BM_Draw_GraphiteRecord` its own throwaway `Recorder` via
  `context()->makeRecorder(...)` so dropping is legal. **Outside this
  audit's file boundary — flagged, not fixed.**
- **C2 (recommended, not blocking).** Nobody checks `insertRecording()`'s
  `InsertStatus`. Once the flag is on, `kOutOfOrderRecording` is a
  permanent, silent no-render. At minimum log it in
  `SkiaOffscreenSurfaceCommon.cpp` and in `submitGraphite` twins; ideally
  treat it as "rebuild the context".

Both are cheap. Neither justifies flipping the flag before the number
exists — see the gate below.

### The prepared patch — DO NOT APPLY without the measurement

ROADMAP §1 is gated on `BM_Draw_DenseText_PictureReplay_Graphite` (built
2026-07-26, `src/common/compose/bench/ComposeBench.cpp`). The flip changes
GPU behavior; it rides the number or it does not ride.

```cpp
// src/common/skia/SkiaGraphiteContextCommon.cpp — replaces lines 69-73.
// ONE seam for both backends: Metal and Vulkan both route makeRecorder()
// through this (SkiaGraphiteContextMetal.mm:34-35,
// SkiaGraphiteContextVulkan.cpp:89-90), so the two factories cannot drift.
skgpu::graphite::RecorderOptions SkiaGraphiteContext::makeRecorderOptions() {
  skgpu::graphite::RecorderOptions options;
  options.fImageProvider = sk_make_sp<CachingImageProvider>();
  // Unordered replay makes Recorder::snap() evict the glyph/path/clip
  // atlases every snap (Skia m151 Recorder.cpp:265-267 →
  // AtlasProvider::invalidateAtlases), so every glyph re-uploads once per
  // frame. Every host here snaps and inserts one Recording per frame on
  // one thread, in order — audited 2026-07-26, see
  // src/sigilweave/docs/graphite_ordering_audit.md.
  //
  // PRECONDITION, not a tradeoff: a snapped-but-never-inserted Recording
  // (or a snap() that returns nullptr) skips an ID and permanently kills
  // this Recorder — insertRecording then returns kOutOfOrderRecording and
  // renders nothing, forever, unchecked. Do not add a "snap to discard"
  // anywhere downstream of this line.
  options.fRequireOrderedRecordings = true;
  return options;
}
```

Also requires the header include already present (`<gpu/graphite/Recorder.h>`,
`SkiaGraphiteContextCommon.cpp:14`) — `RecorderOptions` is defined there, so
the patch is genuinely one line plus comment.

Confirm the win with a Metal capture on glyph-atlas upload bytes, as
ROADMAP §1 says; the source above predicts the delta is entirely
`evictAtlases()` per snap.

---

## 2026-07-26 — §7 sanity check: the clear-all shape cache

**Behavior confirmed.** `src/sigilweave/Shaper.cpp:175-177`:

```cpp
  if (implementation.shapeCache.size() >= FontContext::Impl::kMaxShapeEntries)
    implementation.shapeCache.clear();
  implementation.shapeCache.emplace(std::move(key), shapedWord);
```

It is a whole-map `clear()`, not an eviction — no LRU, no partial purge.
`kMaxShapeEntries = 1 << 17 = 131072` (`src/sigilweave/FontContextImpl.h:213`,
described in-source as a "blunt cap ... Clearing wholesale costs one cold
frame, then re-fills").

The cliff is deeper than "re-shape": the map owns the only strong reference
to each `ShapedWord`, and `ShapedWord::blobCache` (`Shaper.cpp:181-195`)
hangs off it. So one clear drops **every cached HarfBuzz shape and every
cached `SkTextBlob` at once**. Live `ParagraphLayout`s survive (their
`PositionedRun::shaped` are `shared_ptr`s), but the next relayout pays a
full cold shape + full blob rebuild for the entire corpus. That is exactly
what makes it the cliff under any blob/Slug caching from §2/§3.

**Reachability by the existing bench corpus: NO — by ~two orders of
magnitude.** `weave_bench` shares one process-wide `FontContext`
(`bench/weave_bench.cpp:32-35`), so entries accumulate across all arms, and
the ceiling is still nowhere near:

- vocabulary is fixed and tiny — 30 Latin words (`:37-46`) + 23 CJK
  (`:48-55`) + 20 Babel confetti tokens (`:636-639`) + soft-hyphen variants
  of ≤30 Latin words (`:434-448`);
- shaping identity multiplies that by the distinct sizes only: 16 (default),
  8 (`makeDrawStressParagraph`, `:572-574`), 18/20/22/24 (`BM_Update_SizeRestyle_500w`,
  `:187`), 14/16/19 × 3 typefaces (`BM_Layout_Warm_MultiFont_500w`, `:280-290`);
- upper bound is low four figures, against a cap of 131072.

The one arm that generates unbounded unique words —
`BM_Update_ReplaceWholeParagraph_Cold_500w` (`:339-366`) — calls
`fontContext().purgeShapeCache()` inside `PauseTiming()` every iteration
(`:347`; `FontContext.cpp:268` is a plain `shapeCache.clear()`), so it
measures a *cold refill of 500 words*, never the clear-all cliff at scale.

**Consequence for §7's measurement:** the existing corpus cannot reproduce
the cliff. Reproducing it needs a purpose-built oversubscribing arm — 131k+
distinct shape identities, then one more shape to trip the clear, timing the
frame *after* the clear on a warm 500-word paragraph. No such arm exists and
none was added here (§7 is diagnosis-only until §2/§3 make it load-bearing).

No fix applied.
