# SigilWeave — the ledger of walls

Opened 2026-07-26 from the Skia m151 text-performance investigation.
Same contract as compose's ROADMAP: entry numbers are stable, a
documented limit is a claim until measured, and every entry names its
measurement. The standing hazard for this file: **weave_bench is CPU
raster only** (its own header says GPU is the production path) —
raster has no glyph atlas, so GPU-side items must be measured through
compose_bench's Graphite arms, and a raster number must never be
generalized to the production path.

Context for every entry: we are on Skia m151 (the sigil-vcpkg-registry
`skia` port, bumped from vcpkg's m148; the 2026-07 vcpkg-configuration
change is that bump, already built). The m148→m151 delta contains no
free text speedup — every ranked item below was available before the
bump except §5, which is the one recommended against.

## 1. Graphite flushes the glyph-upload cache every recording — one flag

`ContextOptions::fRequireOrderedRecordings` (m133; the release note
misspells it `fRequired…`) replaces `fDisableCachedGlyphUploads`: with
the default `false`, Graphite "has to flush some caches at the start
of each Recording" — the glyph-upload cache among them, so glyphs
potentially re-upload to the atlas every recording. We build the
context with default options (`SkiaGraphiteContextMetal.mm:30`,
`SkiaGraphiteContextVulkan.cpp:90` — one line each).
**Correctness precondition, not a tradeoff**: out-of-order replay
with the flag set is corruption, not slowdown — audit every
`insertRecording` site first (one Context, one Recorder today,
`SkiaGraphiteContextCommon.cpp:75-78`).
Measure: compose_bench Graphite arms (`BM_Draw_100Rows_Cached_
Graphite`, `BM_Draw_Bloom_PictureReplay_Graphite`) plus a Graphite
twin of `BM_Draw_DenseText_PictureReplay` — which does not exist yet
and is exactly the shape this flag moves. Confirm with a Metal
capture on atlas upload bytes.
**2026-07-26 — bench built, audit filed, still unmeasured.**
`BM_Draw_DenseText_PictureReplay_Graphite` (plus `_TextureBlit_Graphite`
as its floor) registered in compose_bench; Debug and Release both build;
smoke-run only. Audit: `docs/graphite_ordering_audit.md` — every
`insertRecording` and Recorder site, verdict SAFE-conditional, one-line
patch prepared there but NOT applied (it rides the number). Two
corrections to this entry from m151 source: the better seam is the
per-Recorder `RecorderOptions` field through the existing
`SkiaGraphiteContextCommon.cpp:69-73` funnel (one line, both backends at
once), and out-of-order replay returns `kOutOfOrderRecording` — a
permanent silent no-render, not corruption. One blocking site found
(`scry/bench/WebBench.cpp:186` snaps and discards).

## 2. drawBatched re-packs every glyph every frame

`ParagraphLayout.cpp:1184-1189`: every glyph ID and SkPoint is pushed
into thread-local buckets per call, even when the layout is
bit-identical to last frame (~10k IDs + 10k points on the 2000w
bench); bucket lookup is a linear scan (:1162-1171); `makeFont` runs
per bucket per frame (:1201-1202) and `drawPaintLayers` multiplies
the strike re-entry by paint-layer count (:1207).
Fix shape: hang packed buckets off the ParagraphLayout (invalidated
on runs generation / overridePaint / liveVariations); re-resolve only
font and paint per frame.
Measure FIRST — the wall bracket already exists: the gap between
`BM_Draw_Raster_300w` (blob path, no repacking) and
`BM_DrawBatched_Raster_300w`, then `_2000w` for scale. Packing does
not touch rasterization; the win is a fraction — believe the number,
not the story.

## 3. Slug-cache the moving-text picture path — measure before building

`sktext::gpu::Slug` (present in our build; `ConvertBlob` symbols
confirmed) does glyph→strike→atlas planning once at conversion, not
per replay. But compose already beats it for fully-static text
(`Cache::Texture` blits pixels); Slug only wins the middle case —
text that MOVES (defeating a texture bake) with fixed glyph content.
The kinetic path is RSXform, which `ConvertBlob(blob, origin, paint)`
does not cover.
Measure first: add `BM_Draw_DenseText_SlugReplay` beside the existing
`_PictureReplay` / `_TextureBlit` pair; if picture replay is not the
bottleneck at 60 fps, this is dead.
Risk: Slug lives in `include/private/chromium/` — unversioned, absent
from RELEASE_NOTES, free to vanish on a bump. Adoption goes behind a
thin seam or not at all.
**2026-07-26 — slot reserved, nothing integrated.**
`BM_Draw_DenseText_SlugReplay` is registered in compose_bench beside the
`_PictureReplay` / `_TextureBlit` pair and reports SKIPPED with the gate
in its message. No Slug header is included and no seam exists; filling
the slot stays gated on §1's Graphite dense-text number.

## 4. Skip-ink underlines re-enter the strike every frame

`forEachDecorationRect` runs per frame from both draw paths; inside:
`makeFont` + `getMetrics` per decoration group per frame
(`ParagraphLayout.cpp:787-790`), and TWO `SkTextBlob::getIntercepts`
calls per run per frame (:828/:832 group path, :952/:958 per-run
path) — each resolves a strike and walks glyph outlines. The per-run
path also heap-allocates a fresh vector per run per frame (:957); the
group path already uses thread_local scratch (:824).
Fix shape: intercepts depend only on (blob, band position, band
thickness) — all layout-stable; memoize beside `blobCache`. Give the
per-run path the group path's scratch.
Measure: no skip-ink draw bench exists — add
`BM_DrawBatched_Raster_300w_SkipInkUnderline` and diff against plain.
That bench IS the wall reproduction; it must come first.
**2026-07-26 — bench built, unmeasured.**
`BM_DrawBatched_Raster_300w_SkipInkUnderline` registered in weave_bench
beside plain `BM_DrawBatched_Raster_300w`, same corpus/flow/surface, one
default underline decoration the only difference. Added with it:
`BM_DrawBatched_Raster_300w_PlainUnderline` (skipInk off) — without that
control the diff conflates band-rect rasterization with the intercept
and strike cost the memo fix targets, and only the second is §4. Debug
and Release both build; smoke-run only. Raster-vs-raster, so the file's
GPU hazard does not apply here. The fix stays gated on the number.

## 5. SkStrikeRef / getWidthsStrided (the m151 headline) — REJECTED for now

m150/151's `SkFont::makeStrikeRef()` targets "text shaping engines
that make many getWidths calls" — but we take advances from HarfBuzz
(`Shaper.cpp:150-171`), not `SkFont::getWidths`; the library's only
getWidths is the one-time variable-axis probe (`FontContext.cpp:
258-261`, cached). Applicable in principle, negligible in practice.
Revisit only if a path needs true per-glyph ink bounds from Skia —
then one strikeRef per (typeface, size) with `getWidthsStrided`
reading straight out of `ShapedWord::glyphs` is the right shape.

## 6. Strike/atlas cache bounds are all defaults

No `SkGraphics::SetFontCacheLimit` anywhere; Graphite
`fGlyphCacheTextureMaximumBytes` at the 8 MB default. The subpixel
cutoff at `Shaper.cpp:25` exists because the atlas WAS the cap once —
live territory. Measure atlas residency/eviction on a kinetic-heavy
sketch before touching any number; if purging is spiky, m149's timed
`performDeferredCleanup(maxDuration)` is the capping tool (this one
does need the bump).

## 7. The shape cache is clear-all, not LRU

`Shaper.cpp:175-177`: at `kMaxShapeEntries` the whole map clears —
every ShapedWord AND every cached blob at once. Not a Skia issue, but
it is the cliff under any blob/Slug caching above (§2/§3 degrade
gracefully only if this does). Measure: the cold-path bench
(`BM_Update_ReplaceWholeParagraph_Cold_500w`) plus a corpus that
oversubscribes `kMaxShapeEntries`.

## What is already right (verified, so nobody re-derives it)

Blob caching is sound: `ShapedWord::blobCache` builds once per
distinct shaped word, not per draw (`Shaper.cpp:181-195`), keyed on
the full shape identity. Font config is deliberate (`Shaper.cpp:
11-29`): edging, subpixel-below-48px, no hinting, linear metrics.
The kinetic path already quantizes alpha (32 steps) and angles to
hold atlas cardinality down (`Paint.cpp:684-687`) and hard-disables
subpixel (`Choreograph.h:152-155`). SkShaper/SkParagraph remain
banned — shaping is HarfBuzz/ICU in-house, settled law.

## The measurement gap that precedes everything

Dense static text on Graphite genuinely replays draw calls every
frame (compose's texture promotion is off by default there, by
measured design), which is precisely where §1 and §3 bite — and
precisely where bench coverage is thinnest. The Graphite dense-text
bench arm is the first thing to build; every ranked item above
funnels through it.
**2026-07-26: it is built** (`BM_Draw_DenseText_PictureReplay_Graphite`,
compose_bench) **and still unmeasured** — the machine was not quiet. The
gap closes when that arm has a Release number, not before.
