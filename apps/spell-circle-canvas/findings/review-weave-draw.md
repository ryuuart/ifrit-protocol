# Merge-readiness review: SigilWeave and SigilDraw

Branch `sigil/library-campaigns` against merge base `aabd3fe1b224`. Scope:
`src/sigilweave` (text engine) and `src/common/draw` (pen, brush, brush
formats). Read-only; every line below was verified against the code as it
stands on HEAD. Paths are under
`/Users/long/REI/ifrit-protocol/apps/spell-circle-canvas/`.

Form of each finding: severity — `path:line` — what the code does; what it
should do; fix.

## Top ten

1. **should-fix** — `src/common/draw/brush/format/Zip.cpp:65-73` — sizes
   every entry from `mz_zip_reader_entry_save_buffer_length` and calls
   `entry.bytes.resize(size)` before reading a byte, so a 200-byte archive
   whose central directory claims a 2 GiB entry allocates 2 GiB (or throws
   `bad_alloc` out of a decoder the hub runs); should refuse an entry whose
   claimed size a brush could never need; fix: reject `size` above a fixed
   ceiling (a few tens of MiB) or above `archive.size() * 1024`, before the
   resize.
2. **should-fix** — `src/common/draw/brush/format/Photoshop.cpp:142-143` —
   allocates `raw` at `height * width * bytesPerSample` (up to 8192·8192·2
   = 128 MiB) from four header integers before any pixel byte is checked,
   so a 60-byte `.abr` costs 128 MiB per sampled block; should bound the
   allocation by the bytes the block can hold; fix: return `finish(nullopt)`
   when `rowBytes * height > (blockEnd - cursor.at()) * 129` (PackBits
   expands at most 129:2) and, for `compression == 0`, when it exceeds
   `blockEnd - cursor.at()`.
3. **should-fix** — `src/sigilweave/layout/InitialLetter.cpp:245-272` — the
   notch is cut only on bands whose `request.blockIndex` is the initial's
   block, so a block with fewer lines than the initial sinks (a one-line
   paragraph declaring `lines = 3`) leaves the next block's first lines, or
   the frame edge, running under the cap's ink, and `placeInitialLetter`
   still sinks the cap `sinkOffset` down regardless of how many bands were
   handed out; should keep the cap clear of whatever follows; fix: count
   bands the wrapper actually cut and, when the block ends early, keep
   cutting the following blocks' bands (they are bands of the same passage)
   or clamp `sinkOffset` to the last band cut and report it in
   `PlacedInitial`.
4. **should-fix** — `src/sigilweave/layout/InitialLetter.cpp:299-309` and
   `src/sigilweave/layout/LineBreak.cpp:1889-1891` — an initial on any
   block but the first is placed with `cap.lineIndex = 0`, inserted at
   `runs.begin()` (breaking the "runs in logical word order" the header
   promises, and putting a block-N word before block 0's), and its fallback
   seat is requested with `blocks.front().pitch/ascent` rather than the
   initial's block's; should carry the block's first line index and metrics;
   fix: store `firstLineIndex`, `pitch` and `ascent` on `InitialLetterPlan`,
   set `lineIndex` from it and insert the two runs before the first run
   whose `wordIndex >= plan.wordIndex`.
5. **should-fix** — `src/sigilweave/layout/InitialLetter.cpp:288-298,
   313-318` — in a column flow the initial is shaped horizontally
   (`shapeWord(..., false, false)` at :142) and drawn as a horizontal blob
   whose baseline is `seat.origin` = the column's TOP edge, so the ink lands
   above the frame and `PlacedInitial::box` reports a rectangle above it,
   while the notch taken down the column is the glyphs' horizontal advance;
   should set a column's initial upright with its top at the column head
   and the notch its vertical extent; fix: shape with `vertical = true`
   when the seat's direction is `{0, 1}`, place the origin at
   `seat.origin + {0, ascent}` and take the vertical advance as the notch.
6. **should-fix** — `src/common/draw/brush/format/test/FormatTest.cpp`
   (whole file) — every negative case is "bytes that are no brush"; there
   is no truncated `.abr`, no `samp` section whose length runs past the
   file, no block claiming 8192×8192, no zero-sized bitmap, no depth other
   than 8/16, no malformed `brush.json`, no zip entry claiming a huge
   uncompressed size and no zip with only a directory entry; should hold
   each reader to "a short answer and never a fault" (the promise
   `Photoshop.cpp:18-20` makes); fix: one case per format that truncates
   the hand-built fixture at every byte offset and asserts no throw and
   either `nullopt`/empty or a well-formed tool, plus one case per
   hostile-size field.
7. **should-fix** — `src/common/draw/brush/format/Native.cpp:242-298` with
   `src/common/draw/README.md:459` — `encodeBrush` writes and
   `readDescription` reads only tip, colour, width, opacity, spacing,
   scatter, density, angle, aspect, the three jitters, rotation, shape,
   grain and dynamics, so a tool round-tripped through the native format
   loses its pressure envelope (reset to `{1,1,1}` with `variation` cleared
   at :185-186), `markerTip` (forced false), `bristles`, `blend`,
   `speedSize/Opacity/Reference`, `pressureSize/Opacity`, `tilt*`,
   `sharpness` and `noise`, while the README says `brush.json` "names the
   tool's own fields"; should either carry every scalar field of `Tool` or
   state the subset; fix: add the missing keys to both functions (they are
   all floats/bools) and a round-trip test that compares every field.
8. **should-fix** — `src/sigilweave/layout/Flow.cpp:314-404` —
   `DilatedCoverage` rasterises a PATH silhouette to A8, builds a Euclidean
   distance field and run-length scans it to answer a disc offset, with a
   2048-pixel ceiling that turns the standoff into a staircase on large
   shapes, and the comment (:318-321) says nothing about what Skia offered;
   Skia's path ops give the exact disc offset of a filled path as the union
   of the fill and its own outline stroked with width `2·margin`, round
   join and round cap, which `bandOccupancy` already scans exactly; should
   use the exact outline for `silhouette::path`/`ellipse` and keep the
   field for `silhouette::coverage`, whose input is pixels; fix: in
   `PathSilhouette::bandSpans` build `Op(fill, stroke(2·margin, round), kUnion)`
   once per margin and hand it to `bandOccupancy`; state in the comment
   that the raster path remains for coverage because pixels have no outline.
9. **should-fix** — `src/sigilweave/include/sigilweave/layout/Flow.h:305`
   versus `src/sigilweave/include/sigilweave/layout/LayoutOptions.h:186` —
   the same concept is `ExclusionFlow::setMinIntervalWidth` on the geometry
   and `KnuthPlassOptions::minimumIntervalWidth` on the options, and "Min"
   is an invented abbreviation; should be one spelling; fix: rename to
   `setMinimumIntervalWidth` (callers: `examples/demo/SceneShapes.cpp:71`,
   `examples/gallery/src/scenes/ExclusionsScene.cpp:103`,
   `LayoutOptions.h:19`).
10. **should-fix** — `src/common/draw/brush/format/Zip.cpp`, `Zip.h` — a
    zip archive reader lives privately inside SigilDrawBrushFormat while
    SigilIO owns resource access and has no archive source at all (grep of
    `src/common/io` finds "zip" only in the README's mention of `.brush`);
    the next consumer of a zip (a font pack, a scene bundle) will write a
    second one; should be an archive byte source in SigilIO that the brush
    formats consume; fix: move `readZip`/`isZip` to
    `sigilio/source/Archive.h` as a `ByteSource` over an in-memory archive
    and have `decodeBrush`/`decodeProcreateBrush` walk it.

## 1. Correctness

Verified without defect, stated because the brief asked: every producer of
`PositionedRun::shaped` (`LineBreak.cpp:216, 415, 1017`;
`InitialLetter.cpp:300, 315`) points at a `ShapedWord` owned through a
`std::shared_ptr<const ShapedWord>` held either by the paragraph's word
segments or by `ParagraphLayout::shapedByTheLayout`; reallocation of `runs`
or of `shapedByTheLayout` moves handles, never pointees, so the borrowed
pointer survives every producer's growth. `ExclusionFlow::lineIntervals`
(`Flow.cpp:533-598`) handles a band fully excluded (empty `availableSpans`
→ empty intervals, `true`), an exclusion wider than the frame
(`subtractSpan` removes the whole span) and zero margin (exact outline
path); `RectangleSilhouette::bandSpans` (:267-277) answers both branches.

- **should-fix** — top ten 1, 2, 3, 4, 5.
- **should-fix** — `src/common/draw/brush/format/Native.cpp:84-88` —
  `decimal()` prints with `%.6g` and its comment says "enough digits to
  survive a round trip", which a `float` does not at six significant digits
  (`1.0f/3` reads back as a different float); should round-trip; fix:
  `%.9g`.
- **nit** — `src/sigilweave/layout/InitialLetter.cpp:317-318` — the split
  word's remainder is placed at `seat.origin + direction · notchAt(0)` even
  when the seat interval (band 0's first interval before the cut) is shorter
  than `notch + tail` because an exclusion sits beside the initial, so the
  remainder is drawn into the exclusion while `lineIntervals` correctly
  carried the cut into the next interval; should place the remainder on
  the interval the cut actually landed in; fix: record, while cutting band
  0, the interval and pen where `tail` began and place the remainder
  there.
- **nit** — `src/sigilweave/layout/InitialLetter.cpp:292-295` — a
  `LineSetFlow` whose first interval runs in any direction other than
  `{1,0}` or `{0,1}` gets its notch cut (the wrapper does not check
  direction) but no cap placed (`return` here), so the block opens with a
  hole; should either place along the interval's direction or cut nothing;
  fix: rotate the cap blob to `seat.direction` the way a transformed run is
  built, or skip the wrapper when the seat is not axis-aligned.
- **nit** — `src/sigilweave/layout/InitialLetter.cpp:226-233` — the
  kGlyph reach reads a curve's CONTROL points, which lie outside the curve,
  so a bowl's notch is measured to the control polygon rather than the
  outline; should read the curve's extreme within the band; fix: flatten
  the outline (`geometry::path::flatten`) before scanning, as
  `PathSilhouette` does.
- **nit** — `src/sigilweave/layout/InitialLetter.cpp:285-287` —
  `direction = tangent` is assigned on the contour branch and never read;
  the cap is drawn as an upright blob regardless; should not carry a dead
  store; fix: drop the assignment (or use it — see previous item).
- **nit** — `src/common/draw/brush/format/Photoshop.cpp:126-132, 145,
  152-157` — every read inside a sampled block is bounded by the FILE, not
  by `blockEnd`, so a block whose `blockSize` is smaller than its preamble
  reads the next block's bytes as its bounds and bitmap and returns a
  garbage tool; should stay inside the block; fix: construct a `Cursor`
  over `bytes.subspan(blockStart + 4, padded)` for the block body.
- **nit** — `src/common/draw/brush/format/Photoshop.cpp:116` — a
  `blockSize <= 0` returns without seeking to a block end, and the caller's
  loop (:208-212) then re-reads from four bytes on, stepping through the
  section one integer at a time; should stop at the first malformed block;
  fix: `break` when `readSampledBrush` returned `nullopt` with the cursor
  not past `blockStart + 4`.
- **nit** — `src/common/draw/brush/format/Zip.cpp:53-55` —
  `(int32_t)archive.size()` truncates an archive of 2 GiB or more into a
  negative length handed to minizip; should refuse rather than truncate;
  fix: return empty when `archive.size() > INT32_MAX`.
- **nit** — `src/sigilweave/layout/Flow.cpp:406-443` — a path with an
  INVERSE fill type is treated as its non-inverse fill at zero margin
  (`bandOccupancy` reads only even-odd vs winding) but rasterised by Skia
  with the inverse honoured at any positive margin, so the same silhouette
  flips meaning when a margin is added; should be one meaning; fix: reject
  or normalise inverse fill types in the constructor.
- **nit** — `src/sigilweave/layout/LayoutMetrics.cpp` (`lineMetrics`,
  around :31-51) — the initial's cap run is an ordinary run on line 0 with
  a font several lines tall, so line 0's `LineMetrics::ascent`, and every
  selection band built from it, grows to the cap's height; should report
  the line's own band; fix: skip runs whose `wordIndex` is the placed
  initial's when `layout.initial.placed`.
- **nit** — `src/sigilweave/cache/SingleLineParagraphCache.cpp:79-80` — at
  capacity the whole map is cleared before the insert, invalidating every
  reference handed out, including the one a caller is still laying out in
  the same frame (`src/spellcircle/shared/scene/SceneRenderer.cpp:110-116`
  holds `label` across the insert of the next label only in sequence, so
  it is safe today by ordering alone); should evict one entry, not all;
  fix: evict the least recently used entry, keeping the node-map promise.

## 2. Public API

- **should-fix** — top ten 7, 9.
- **nit** — `src/common/draw/include/sigildraw/brush/format/Load.h:40-41`
  and `src/common/draw/brush/format/Native.cpp:238` — `decodeBrush`'s doc
  says `hint` is "used only to sharpen the sniff" while the body ends in
  `(void)hint;`; should say what it does; fix: drop the parameter from
  `decodeBrush` (the `BrushDecoder` adapter keeps the hub's signature) or
  use it to prefer the Procreate reader for a `.brush` name.
- **nit** — `src/common/draw/include/sigildraw/brush/format/Load.h:42` —
  `decodeBrush` takes `const io::Bytes&` while `assembleBrush`,
  `decodePhotoshopBrushes` and `decodeProcreateBrush` take
  `std::span<const std::byte>`; the same input is spelt two ways; fix:
  take a span and let `BrushDecoder::decode` unwrap the hub's `Bytes`.
- **nit** — `src/common/draw/include/sigildraw/brush/format/Load.h:81-95` —
  `loadBrush` fetches `<uri>/brush.json` and `<uri>/shape.png` before
  `<uri>` itself, so loading any one-file brush costs two failed fetches
  through the source first; fix: fetch `uri` first and fall through to the
  parts only when it answers nothing.
- **nit** — `src/common/draw/include/sigildraw/brush/Tool.h:64-67` versus
  `src/common/draw/include/sigildraw/brush/Shape.h:26-28` — `spacing` and
  `scatter` exist on both `Tool` (canvas units) and `Shape` (fractions of
  the width), so one tool carries two `scatter`s in different units, told
  apart only by `spacingOf`; fix: name the shape's for what they are
  (`spacingFraction`, `scatterFraction`) or state on `Tool` which wins
  when both are set.
- **nit** — `src/sigilweave/include/sigilweave/layout/Flow.h:223-224` —
  `Silhouette::bandSpans(axis, bandStart, bandEnd, margin, spans)` passes
  the band as two floats where `LineRequest` already made the same band a
  value; fix: a `Band {start, end}` struct (the `Span` type beside it is
  already that shape).
- **nit** — `src/sigilweave/include/sigilweave/query/Selector.h:20-35,
  89-102` — `Kind::Word` and `Kind::Words` are two kinds while
  `sel::line` and `sel::lines` share `Kind::Line`; fix: one kind per unit
  with `lo/hi`, and drop `Words`.
- **nit** — `src/sigilweave/include/sigilweave/paragraph/Unit.h:4-11` —
  `Unit::Word` and `unit::Word` spell the same five names twice; fix: keep
  the enum and delete the `unit` namespace, or the reverse.
- **nit** — `src/sigilweave/layout/LineBreak.cpp:131` — `using LineFit =
  GlyphFit;` gives one concept two names inside the same translation unit;
  fix: spell `GlyphFit`.
- **nit** — `src/sigilweave/layout/Flow.cpp:293, 575` and `:273-274` — a
  negative `Exclusion::margin` is clamped to zero in `ExclusionFlow`, again
  in `CircleSilhouette`, and handled as "no margin" by
  `RectangleSilhouette`: three places deciding one thing; fix: clamp once
  in `ExclusionFlow` and document the field as non-negative.

## 3. README and FEATURES drift

Every identifier spelt in backticks in `src/sigilweave/README.md` (137),
`src/sigilweave/FEATURES.md` (385) and `src/sigilweave/kit/README.md` (50)
exists in a header or CMake file, and every `Scope::member` pairing
resolves in the header that declares the scope (`ports::face`,
`ports::pickTypeface`, `features::tabularNumbers`, `Element::initialLetter`,
`Element::flowAround` included). The 31 misses in `src/common/draw/README.md`
are all p5 names under "Not provided" (:510-527). The test suites the weave
README lists (:380-391) all exist under `layout/test/`.

- **should-fix** — top ten 7 (`src/common/draw/README.md:459`).
- **nit** — `src/sigilweave/README.md:322-324` and
  `src/sigilweave/include/sigilweave/layout/ParagraphLayout.h:51-55` — both
  say the layout retains "a tab leader, an overflow marker" while
  `InitialLetter.cpp:308, 324` also retain the initial's glyphs and the
  split word's remainder; fix: add the initial to both lists.
- **nit** — `src/sigilweave/include/sigilweave/layout/Flow.h:11-13` — the
  file comment lists the stock silhouettes as "a rect, a circle, any filled
  SkPath, an image's own alpha, or one a caller writes" and omits
  `silhouette::ellipse` (:252); fix: add it.
- **nit** — `src/sigilweave/FEATURES.md:572` — "see the options structs"
  points at code instead of stating the controls; fix: name
  `TabStopOptions`, `OverflowOptions::ellipsis`, `OverflowOptions::maxLines`.

## 4. Comment rules

- **should-fix** — `src/sigilweave/examples/demo/DemoScenes.h:3-4` — "One
  entry point per scene file in src/text/demo/ … in the order the README
  describes them": a stale path and a document citation; fix: "one entry
  point per `Scene*.cpp` beside this header", drop the README reference.
- **should-fix** — `src/sigilweave/examples/demo/DemoSupport.h:3` —
  "(`src/text/demo/Scene*.cpp`)" stale path citation; fix: drop the path.
- **should-fix** — `src/sigilweave/examples/gallery/src/scenes/SceneSupport.h:3,5`
  — "(src/gallery/scenes/*.cpp)" and "(src/text/kit)" are stale paths;
  fix: drop both, the target name `SigilWeaveKit` suffices.
- **should-fix** — `src/sigilweave/include/sigilweave/kit/SigilWeaveKit.h:5`
  — "See kit/README.md for the philosophy"; fix: delete the pointer, the
  next sentence already states the rule.
- **should-fix** — `src/sigilweave/examples/demo/SceneNewFeatures.cpp:1`,
  `:238` (`printf("Scene K — new-features panel written")`),
  `DemoScenes.h:39`, `weave_demo.cpp:4` — "the standalone-polish pass" is a
  campaign name and "NewFeatures"/"new-features" is history in a file name,
  a function name and a shipped string; fix: rename to what the panel
  shows (`SceneDecorations.cpp` / `sceneDecorations`), updating
  `DemoScenes.h`, `weave_demo.cpp:42` and `examples/demo/CMakeLists.txt`.
- **should-fix** — `src/sigilweave/examples/demo/SceneShapes.cpp:30` —
  shipped demo copy "Text no longer flows around circles and boxes alone"
  contrasts with a past state; fix: "Any SkPath carves its exact silhouette
  out of the line bands".
- **should-fix** — `src/sigilweave/ports/SystemFontManager.cpp:27` —
  "Windows bring-up draft: compiles out on macOS, untested until the
  Windows port lands" is a TODO written as prose; fix: state the standing
  constraint ("compiled only where `_WIN32` is defined") or cut it.
- **nit** — `src/sigilweave/include/sigilweave/SigilWeave.h:11`,
  `src/sigilweave/CMakeLists.txt:8`, `src/common/draw/CMakeLists.txt:3` —
  "README.md … is the map/canon" names a document as authority; fix: drop.
- **nit** — `src/sigilweave/paragraph/Paragraph.cpp:705` — "via LB13"
  requires opening UAX #14 to know the rule; fix: "trailing closing
  punctuation glues to the character before it".
- **nit** — `src/sigilweave/README.md:496` — "Skia's Debug recording path
  is dramatically slower on glyph-heavy scenes" is a benchmark's claim;
  fix: keep "judge on a Release build" and drop the comparison.
- **nit** — `src/sigilweave/fonts/Shaper.cpp:32` — "whole-pixel snapping at
  48px+ is imperceptible (<2% of an em)" is a perceptual measurement; fix:
  state the constant (`fontSize < 48` keeps subpixel) and the reason
  (strike count).
- **nit** — `src/common/draw/brush/format/Photoshop.cpp:86` —
  `expandPackBits` is hand-written with no line saying what the
  dependencies offered; fix: one line: Skia's PackBits lives inside its
  codecs behind `SkCodec` and is not callable on a bare row.
- (counted in top ten 8) `src/sigilweave/layout/Flow.cpp:318-321` — the
  comment explains why a polygon offset is hard but not what Skia's path
  ops offer.

The single `workaround:` marker (`src/sigilweave/choreograph/Choreograph.cpp:252`)
states what Skia's direct RSXform `drawGlyphs` does (bounds computed with
rotation and translation swapped, quick-rejecting the batch) and why the
blob path serves; compliant. No unmarked dependency compensation found.

## 5. Leftovers

No debug prints in any library target (the `printf`s in `examples/demo`
are that CLI's reporting path), no `#if 0`, no commented-out code, no
`using namespace` in a header, no unreferenced function in scope (every
anonymous-namespace and header-declared free function has a call site).

- **should-fix** — `src/sigilweave/layout/InitialLetter.cpp:342` —
  `(void)paragraph;` — `placeInitialLetter` never reads it; fix: drop the
  parameter at `ParagraphLayoutInternal.h:478` and `LineBreak.cpp:1893`.
- **should-fix** — `src/sigilweave/layout/LineBreak.cpp:380` —
  `static_cast<void>(options);` in `emitLeader`; fix: drop the parameter
  and the argument at :888.
- **should-fix** — `src/sigilweave/layout/LayoutMetrics.cpp:94` —
  `ParagraphLayout::glyphOutline(const Paragraph&)` ignores its only
  parameter; fix: drop it from `ParagraphLayout.h:168` and the two
  callers in `src/common/compose/core/{Derive,StackingPainter}.cpp`.
- **should-fix** — `src/sigilweave/examples/demo/weave_demo.cpp:26` —
  `static_cast<void>(argc); static_cast<void>(argv);`; fix: `int main()`.
- **nit** — `src/sigilweave/paragraph/Paragraph.cpp:601` —
  `static_cast<void>(fontContext)` on `analyze`; the reason is stated; fix:
  drop the parameter or leave as is.
- **nit** — `src/sigilweave/unicode/Unicode.cpp:342` —
  `static_cast<void>(firstCodePoint)`; fix: `[[maybe_unused]]` on the
  declaration.
- **nit** — `src/common/draw/brush/format/Procreate.cpp:20-21` —
  `std::tolower` with no `<cctype>` included; fix: include it.
- **nit** — unused includes, each verified by symbol grep:
  `src/sigilweave/paragraph/Paragraph.cpp:17` `<cassert>`;
  `src/sigilweave/paragraph/Paragraph.cpp:18` `<memory>`;
  `src/sigilweave/fonts/OpticalKerning.h:28` `<vector>`;
  `src/sigilweave/examples/demo/weave_demo.cpp:17` `<algorithm>`;
  `src/common/draw/include/sigildraw/Retained.h:11` `<cstring>`;
  `src/common/draw/include/sigildraw/Retained.h:12` `<functional>`;
  `src/common/draw/include/sigildraw/brush/Catalogue.h:11` `<functional>`;
  `src/common/draw/include/sigildraw/brush/Engine.h:27` `<functional>`;
  `src/common/draw/include/sigildraw/brush/Dynamics.h:8` `<algorithm>`;
  `src/common/draw/brush/Polygon.cpp:14` `<algorithm>`;
  `src/common/draw/brush/format/Native.cpp:11` `<algorithm>`. Fix: remove
  each. (Eleven nits.)

## 6. Duplicated mechanisms

Verified not duplicates: `format::coverageImage` (SigilImage offers only
the reverse, `coverageMask`); `draw::NoiseField` (its header states why it
is a different field from `geometry::path::valueNoise`); `draw/Text.cpp`
goes through `weave::layoutParagraph`/`layoutSingleLine`; the brush
interiors go through `sigilgeometry/path`.

- **should-fix** — top ten 8 (disc offset via Skia path ops) and 10 (zip
  reader belongs to SigilIO).
- **nit** — `src/sigilweave/layout/KnuthPlass.cpp:42-49, 60` — a
  hand-rolled FNV-1a fold with its own offset and prime beside
  `sigilcore/compute/Hash.h` (`hash::kFnvOffset`, `kFnvPrime`, `fnv1a`);
  fix: fold through `core::hash::fnv1a` over the value's bytes.
- **nit** — `src/common/draw/brush/format/Photoshop.cpp:168-173`,
  `Procreate.cpp:58-63`, `Native.cpp:181-186` — the same six-line "an
  imported tool's defaults" block three times; fix: one `importedTool()`
  in `Images.h` (renamed `Import.h`) that all three call.
- **nit** — `src/common/draw/brush/format/Procreate.cpp:18-24` —
  `lowered()` duplicates the lower-casing lambda at
  `src/common/image/encode/Encode.cpp:85`; fix: fold into the extension
  check that SigilImage already exposes for a name, or leave as a
  five-line helper.

## 7. Files over ~600 lines

- **nit** — `src/sigilweave/layout/LineBreak.cpp` (1911) — split by
  subject: `Placement.cpp` (emitSegment, emitLeader, visualOrder,
  placeWords :30-918), `Ellipsis.cpp` (applyEllipsis :918-1071),
  `Blocks.cpp` (resolveBlocks, enforceKeeps, distributeInFrame
  :1082-1626), `Greedy.cpp` (greedyBlock, remainderLines :1281-1462),
  `LayoutParagraph.cpp` (:1628-1911).
- **nit** — `src/common/draw/Pen.cpp` (1085) — `PenStyle.cpp` (paints,
  blend, colour, dash, modes :245-540), `PenClip.cpp` (:556-600),
  `PenShapes.cpp` (:602-988), `PenImageTransform.cpp` (:988-1060),
  `PenRandom.cpp` (:1061-1085).
- **nit** — `src/sigilweave/paragraph/Paragraph.cpp` (1009) —
  `ParagraphEdits.cpp` (edits, spans, paint :85-400),
  `ParagraphAnalysis.cpp` (strut, analyze, pattern breaks :405-793),
  `ParagraphShaping.cpp` (shapeWordContent :793-1009).
- **nit** — `src/sigilweave/FEATURES.md` (1002) — move the parity table
  (:590-end) to `PARITY.md` beside it.
- **nit** — `src/common/draw/test/DrawTest.cpp` (999) — 50 `Pen` cases
  and 3 `Graphics`: `PenStyleTest.cpp`, `PenShapeTest.cpp`,
  `PenTransformTest.cpp`, `PenImageTest.cpp`, `GraphicsTest.cpp`.
- **nit** — `src/sigilweave/examples/gallery/src/GalleryView.cpp` (782) —
  `GalleryFallback.cpp` (the resolver :36-160), `GalleryRenderer.cpp`
  (:160-480), `GalleryView.cpp` (the Qt item :483-782).
- **nit** — `src/sigilweave/layout/bench/LayoutBench.cpp` (778) —
  `LayoutBench.cpp` (`BM_Layout_*`), `UpdateBench.cpp` (`BM_Update_*`),
  `ConfettiBench.cpp`.
- **nit** — `src/sigilweave/layout/KnuthPlass.cpp` (746) —
  `knuthPlassBlock` is one 580-line function: lift the prefix tables
  (:165-300) into `KnuthPlassTables.cpp` and the node arena search into
  its own function.
- **nit** — `src/common/draw/README.md` (736) — the brush chapter
  (:286-510) to `src/common/draw/brush/README.md`.
- **nit** — `src/sigilweave/include/sigilweave/layout/LayoutOptions.h`
  (692) — one header per options struct it groups: `Justification.h`,
  `Overflow.h`, `Frame.h`, `Mojikumi.h`, `ParagraphStyle.h`, with
  `LayoutOptions.h` keeping `ParagraphLayoutOptions`.
- **nit** — `src/common/draw/include/sigildraw/Pen.h` (678) — one class;
  lift `ClipOptions`, `Frame` and the retained-guest types into
  `PenTypes.h`.
- **nit** — `src/sigilweave/layout/Flow.cpp` (644) — `Silhouette.cpp`
  (:243-513) and `Flow.cpp` (the geometries).
- **nit** — `src/sigilweave/kit/Hyphenation.cpp` (636) — the English
  pattern string (:223-636) to `EnglishPatterns.cpp`.

## 8. Test gaps

- **should-fix** — top ten 6 (hostile bytes per brush format).
- **should-fix** — `src/sigilweave/layout/test/InitialLetterTest.cpp` —
  no case for a block with fewer lines than the initial sinks, an initial
  on a block other than the first, `Wrap::kGlyph`, a negative `sink`,
  `graphemes` of 2 and of more than the opening word holds, or a resumed
  pass (`firstWord > 0`) not re-opening the initial; fix: one case each,
  asserting the cap's `lineIndex`, its position against the frame and the
  runs' word order.
- **nit** — `src/sigilweave/layout/test/FlowTest.cpp`,
  `SilhouetteTest.cpp` — no case for an exclusion wider than the frame, a
  non-round `silhouette::ellipse`, a coverage silhouette under
  `FlowAxis::kColumns`, or a rectangle's flat side standing off by exactly
  the margin; fix: one case each.
- **nit** — `src/sigilweave/layout/test/` — nothing asserts that a tab
  leader's, an overflow marker's or the initial's run has its `shaped`
  pointer in `shapedByTheLayout`, which is the only thing keeping the
  borrowed pointer alive; fix: a `LayoutTest` case that lays out with a
  leader and an ellipsis, moves the layout, and checks each run's `shaped`
  is either a paragraph segment's or one of `shapedByTheLayout`.
- **nit** — `src/sigilweave/choreograph/test/GlyphBatchesTest.cpp` — the
  RSXform blob workaround is held only by
  `src/common/compose/typography/test/ComposeTestContentText.cpp`; fix: a
  weave-side case drawing a rotated batch into a surface sized to its own
  bounds and asserting ink.

## Counts

- blocker: 0
- should-fix: 23 (top ten 10; decimal round-trip 1; comment rules 7;
  leftovers 4; InitialLetter test gap 1)
- nit: 60 (correctness 10; API 9; docs 3; comment rules 5; leftovers 14,
  of which 11 unused includes; duplication 3; file splits 13; test gaps 3)
