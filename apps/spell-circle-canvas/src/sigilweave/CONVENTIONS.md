# SigilWeave conventions and gotchas

What is not discoverable from a signature: what is safe from which
thread, what a unit means, what a cache keys on, and the shapes the
library refuses. Read it before writing against the library; the
catalogue of what it covers is [FEATURES.md](FEATURES.md).

**Threading.** A `FontContext` is single-threaded by contract and contains no
locks. The shape cache and the HarfBuzz buffer are reused scratch, not
per-call state. Create one per layout thread; parallelism belongs above the
library, one paragraph per task with zero shared state. Several hot paths
also use `thread_local` scratch (the ICU break iterators and bidi analyzer
among them), so a context must not migrate between threads mid-use.

**Typeface lifetime.** Every cache keys off `SkTypeface::uniqueID()`.
Typefaces must outlive the context, or be consistently owned by it.

**Shape-cache eviction is a wholesale clear**, not LRU: past its cap the
shape cache empties in one go and re-fills, costing one cold frame. The
per-typeface, fallback, and varied-typeface maps are never pruned at all —
`purgeAllCaches()` is the manual reset for a long-lived process whose
typeface population churns. It is safe to call while shaped-word references
are outstanding, because a `ShapedWord` owns its own data. The full purge
also releases optical-kerning profiles and per-face reference gaps; a shape-
only purge retains those measurements. The font statistics count their
queries independently of shaping calls. (The tint-filter
table behind `GlyphRSXformBatches` is the one LRU: past its cap it drops
its coldest entry rather than everything, so a working set sitting at the
cap keeps the filter identities its batching depends on.)

**A varied clone from `variedTypeface()` is retained forever.** The memo is
keyed on the coordinate's exact bytes, has no cap and no eviction, and
`purgeAllCaches()` is the only thing that empties it. That is right for a
coordinate drawn from a bounded set and wrong for one that varies
continuously, which would add a permanently held clone per frame for the
life of the process. `variedTypefaceTransient()` is the entry point for the
latter: it builds the clone and retains nothing, so the cost is constant
per frame instead of growing, and the face has no stable identity — which
rules it out of `ShapingStyle::variations` and suits a draw-time drive,
where the identity is only a batch key inside one frame.

**All range APIs are UTF-16 code-unit offsets, end-exclusive.** UTF-8 entry
points take `std::u8string_view` specifically, so the encoding contract rides
the type — use `u8` literals or `std::u8string`.

**Coordinates are Skia's: y grows down.** A decoration's `offset` is the
band's *top edge relative to the baseline*, positive meaning below it. Ascent
and descent are reported as positive magnitudes. The horizontal fast path
tests for a direction of exactly (1, 0) and the vertical one for exactly
(0, 1); anything else takes the transformed path.

**On contour intervals, length, fitting and alignment stay in unscaled
advance units.** Only the pen-to-arc mapping is scaled by `advanceScale`. To
offer a whole contour, set `length = arcLength / advanceScale`.

**Rendering must match shaping.** Build draw fonts with `makeFont()` — it
sets the unhinted, linear-metrics, size-gated-subpixel configuration the
shaper measured against — or glyphs drift off their shaped positions. Related:
Skia takes glyph edging from the *font*, never the paint, so
`paint.setAntiAlias(false)` is silently ignored for text. Ask for hard edges
with `ShapingStyle::aliased` instead.

**A per-glyph walk is stable, and its batches are keyed by paint.**
`forEachPlacedGlyph()` enumerates in draw order, and that order does not
change across relayouts while the text is unchanged — which is what lets an
effect key particle state on a glyph's position in the walk. Sentence indices
come from an ICU pass over the text that runs on the first walk after an edit
and is reused by every walk after it; a paint edit does not invalidate it.
`GlyphRSXformBatches` buckets on (typeface, size, condensation, edging,
resolved paint pass, pass band), and a glyph is added once per pass of its
`PaintStyle` — each underlay in order, then the foreground, then each
overlay — so an animated letter keeps its gradients, strokes and mask
filters, and each pass costs one more `drawGlyphsRSXform` call. Buckets
draw band by band — every underlay bucket, then every foreground bucket,
then every overlay bucket, each band in creation order — so every underlay
lands beneath every foreground even when per-glyph fades split one style
into several buckets; a blurred halo reaches past its own glyph, so
creation order alone would lay a late-fading letter's halo over its
neighbour's stroke. A per-glyph fade rides `alphaScale` instead of a
per-glyph style; quantize it when an effect drives it continuously, because
distinct alphas are distinct buckets.
Batched glyphs draw with their rotations quantized: a continuous per-letter
angle mints a fresh glyph-atlas strike per letter per frame.

**`GlyphRSXformBatches::subpixel` is the caller's declaration that the
glyphs it is adding MOVE between frames**, and it decides whether their
origins land on Skia's subpixel phase grid or on whole pixels. It is off by
default, because the phases are the second factor in a product: every mask
is a (glyph, rotation, phase) triple, and the phases multiply what a
rotation ladder has already multiplied, on both axes for an off-axis run. A
run at REST gains nothing — its letters are not creeping anywhere — and
would pay that multiplied population for a placement no one can see move. A
MOVING run's arithmetic runs the other way: its masks were never going to be
re-used, since the rotation it needs this frame is a different rotation next
frame, so the phase grid only refines a mask it was going to rasterize
regardless. Left on whole pixels, a run creeping by a fraction of a pixel
per frame does not creep at all — each letter stands still until its own
origin crosses a pixel boundary and then hops a whole one. This is the same
trade the rotation ladder makes and not a competing one: the ladder still
bounds the rotations, and dropping it in exchange multiplies the mask
population by the rotation count.

**A `GlyphDress` carries what varies per glyph** rather than per pass — the
placement, the fade, three colour terms (a `colorMultiplier` tint, a `colorAdd`
flash added after it, and a `colorScreen` glow screened over both — the two
brightening terms a multiplier cannot say), a `face` override for a glyph
drawn through a varied clone, and a `matrix` for the placements an RSXform
cannot express (a shear, a non-uniform scale). The face joins the bucket
key; the fade and the colour terms change only each pass's resolved paint,
and on a shader pass all three terms fold into one memoized modulating
colour filter — screening against a constant is affine per channel — because
a batch's key is a whole `SkPaint` and `SkPaint` compares its colour filter
by pointer. A
matrix glyph draws in its own bucket's lane, after that bucket's RSXform
glyphs — same font, same paint, same place in the pass order, at the cost of
one canvas concat and one draw each.

**Shaping style versus paint style.** Any change to a shaping field re-shapes
the words it covers. Paint changes never re-shape and never relayout, and
they are visible to an *already-computed* `ParagraphLayout`, because `draw()`
resolves paint per span at draw time. `wordSpacing` is the odd one out: it
lives in the shaping style and is compared for restyle detection, but it is
not part of the shape-cache key — it is applied to whitespace after
measurement, so changing it re-derives words at pure cache-hit cost.

**Variable-font variation lists are order-sensitive for memo identity.** A
permuted list resolves to an equivalent face but occupies a second memo
entry, so keep the order stable across call sites. For draw-time animation
only advance-invariant axes are safe; ask
`FontContext::axisIsAdvanceInvariant()` before driving one through
`LiveVariations`. An axis that fails that test belongs in
`ShapingStyle::variations`, which re-shapes.

**Placeholders match records by occurrence order** of the object-replacement
character (U+FFFC) in the text, so a direct text edit must not add or remove
one.

**Two `[[nodiscard]]` returns mean "rebuild your ranges".**
`Paragraph::editsSince()` and `MarkerSet::synchronize()` both return false
when the bounded edit log no longer reaches back to the caller's revision.
Ignoring that silently corrupts tracked ranges. The log is halved when it
fills rather than trimmed one entry at a time, so the lookback you can count
on is half the cap, not the cap.

**The `languageTag` handed to a custom fallback resolver is a borrowed view**,
valid only for that call, and it is *not* guaranteed to be NUL-terminated.
Copy it before handing it to any C API; never pass its `.data()` through
directly.

**Several things silently no-op outside their scope.** Decorations render on
straight runs, set either way; a TRANSFORMED run (on a path, on a rotated
interval) skips them, and a column's band never skips ink. The
ellipsis marker requires the final interval to be straight and not a
contour — a line takes it at its end and a column at its foot, but a loop
has no end to put one at. `lineMetrics()` skips transformed and vertical runs, and omits
lines whose geometry placed nothing — `columnMetrics()` is what answers
there. Tab stops are line-local and scoped to
straight horizontal left-to-right intervals.

**Geometry is re-queried on every layout pass and never cached between
passes**, so an implementation may depend freely on animated state. For
exclusion flows, animate through `Exclusion::offset`: a flow shape caches
what answering costs it — a flattening, a grown outline, a distance field
— and
rigid motion reuses all of it, while a rebuilt shape (a morphing `SkPath`,
a new video frame) re-measures from scratch.

**Lazy shaping is ascending and idempotent only.** `ensureShapedTo()` with a
decreasing word count is not supported.
