#pragma once

/** @file
 * @ingroup animation
 *
 * Per-glyph choreography utilities — the "letters leave their lines"
 * pattern (rain, ripples, marquees, staggered reveals) distilled from the
 * demos and the gallery. Optional layer: nothing in the core pipeline
 * includes this.
 *
 * The recipe: lay the paragraph out normally, walk every placed glyph with
 * forEachPlacedGlyph, dress it however the effect wants — displaced,
 * rotated, faded, tinted, drawn through another face, or placed by a full
 * matrix where an RSXform cannot reach — and accumulate into
 * GlyphRSXformBatches, which is one drawGlyphsRSXform call per (font, paint
 * pass) instead of thousands of per-glyph draws.
 *
 * A PlacedGlyph carries the identity an effect selects and staggers on —
 * which glyph of which word, line, style span and sentence it is — beside
 * the geometry it draws with, and the span's complete PaintStyle, so an
 * animated letter keeps the gradients, strokes and glow passes its span was
 * styled with.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRSXform.h>
#include <include/effects/SkColorMatrix.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <list>
#include <map>
#include <numbers>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include "Paragraph.h"
#include "ParagraphLayout.h"
#include "Shaper.h"
#include "Style.h"

namespace sigil::weave {

/// One glyph of a finished layout: where it rests, what it draws with, and
/// where it sits in the text. Every index is a fact about *this* layout of
/// *this* paragraph; nothing here is stored during layout.
struct PlacedGlyph {
  const ShapedWord* shaped = nullptr;  ///< glyph source: typeface, size, scaleX
  SkGlyphID glyph = 0;                 ///< glyph ID in `shaped->typeface`
  float advance = 0;                   ///< this glyph's pen travel
  SkPoint rest = {0, 0};               ///< absolute origin the layout placed
                                       ///< it at (the effect's "rest" pose)
  /// The style span's draw-time paint — foreground plus its ordered
  /// underlay/overlay passes. Never null: it points into the paragraph's
  /// spans, so it is re-read every walk and setPaint() shows up with no
  /// relayout. Feed it straight to GlyphRSXformBatches::addGlyph.
  const PaintStyle* paint = nullptr;
  SkColor color = SK_ColorBLACK;  ///< paint->foreground's color, for effects
                                  ///< that only tint

  uint32_t ordinal = 0;     ///< 0-based position in this walk's order
  uint32_t glyphIndex = 0;  ///< index into `shaped->glyphs`
  /// UTF-16 offset of the glyph's cluster inside the text its run shaped —
  /// glyphs of one cluster (a base and its combining marks, a ligature's
  /// parts) share one value.
  uint32_t cluster = 0;
  /// The same cluster as an offset into Paragraph::text(), clamped to the
  /// word that produced it. Length-changing case mapping
  /// (ShapingStyle::textTransform) makes it approximate.
  uint32_t textIndex = 0;
  uint32_t wordIndex = 0;      ///< index into Paragraph::words()
  int lineIndex = 0;           ///< flow line; matches PositionedRun::lineIndex
  uint32_t styleIndex = 0;     ///< index into Paragraph::spans()
  uint32_t sentenceIndex = 0;  ///< 0-based sentence, per Paragraph::
                               ///< sentenceStarts(); 0 for empty text

  /// The layout TURNED this glyph — it rides a contour, or a rotated
  /// interval — rather than sitting on a horizontal baseline. `rest` is
  /// still the absolute origin, and `tangent` is the direction the glyph
  /// was turned to; an untransformed glyph reports (1, 0).
  bool transformed = false;
  SkVector tangent = {1, 0};  ///< unit direction, already snapped

  /// WHERE ALONG ITS FLOW INTERVAL the glyph's ADVANCE CENTRE sits, in
  /// advance units, together with which interval that is (an index into
  /// ParagraphLayout::intervals). Feed the pair back to
  /// LineInterval::placeAt to re-place the glyph — with a phase, to run a
  /// marquee around a closed contour without laying the paragraph out
  /// again. `intervalIndex` is -1 when the layout reported no geometry.
  float pen = 0;
  int intervalIndex = -1;
};

/** Callable accepted by forEachPlacedGlyph(): `fn(const PlacedGlyph &)`. */
template <typename Visitor>
concept PlacedGlyphVisitor = std::invocable<Visitor&, const PlacedGlyph&>;

/** Visits every placed glyph of `layout` in draw order, each with its rest
 * position, its span's paint, and its position in the text.
 *
 * Enumeration order is stable across relayouts as long as the text itself is
 * unchanged — which is what lets per-glyph particle state keyed by `ordinal`
 * survive a per-frame relayout.
 *
 * The first walk after a text edit resolves sentence boundaries once (see
 * Paragraph::sentenceStarts()); every later walk of unchanged text reuses
 * them.
 */
template <PlacedGlyphVisitor Visitor>
inline void forEachPlacedGlyph(const ParagraphLayout& layout,
                               const Paragraph& paragraph, Visitor&& visitor) {
  static thread_local std::vector<uint32_t> segmentCounters;
  segmentCounters.assign(paragraph.words().size(), 0);
  const std::span<const uint32_t> sentenceStarts = paragraph.sentenceStarts();
  const std::vector<StyleSpan>& spans = paragraph.spans();
  static const PaintStyle kUnstyled;

  PlacedGlyph placed;
  for (const PositionedRun& run : layout.runs) {
    if (!run.shaped) continue;  // placeholder slots carry no glyphs
    const Word& word = paragraph.words()[run.wordIndex];

    // Runs of a word arrive in segment order, so one cursor per word finds
    // the segment a run came from. A run whose glyphs are not a segment's —
    // a rendered discretionary hyphen, an overflow ellipsis — matches
    // nothing, leaves the cursor alone, and anchors its text offsets at the
    // word it follows.
    uint32_t& segmentCursor = segmentCounters[run.wordIndex];
    const WordSegment* segment = nullptr;
    if (segmentCursor < word.segments.size() &&
        word.segments[segmentCursor].shaped.get() == run.shaped.get())
      segment = &word.segments[segmentCursor++];

    placed.shaped = run.shaped.get();
    placed.paint = run.styleIndex < spans.size()
                       ? &spans[run.styleIndex].style.paint
                       : &kUnstyled;
    placed.color = placed.paint->foreground.getColor();
    placed.wordIndex = run.wordIndex;
    placed.lineIndex = run.lineIndex;
    placed.styleIndex = run.styleIndex;
    placed.transformed = run.transformed;
    placed.intervalIndex = run.intervalIndex;
    // The interval this run was placed on, when the layout kept one. A
    // transformed run's rest position is READ BACK FROM IT rather than from
    // run.origin, which such a run leaves at zero because its placement is
    // baked per glyph.
    const LineInterval* interval =
        run.intervalIndex >= 0 &&
                (size_t)run.intervalIndex < layout.intervals.size()
            ? &layout.intervals[(size_t)run.intervalIndex]
            : nullptr;
    float penLocal = 0;

    const uint32_t textBegin = segment ? segment->textBegin : word.textBegin;
    const uint32_t textLimit =
        word.textEnd > textBegin ? word.textEnd - 1 : textBegin;
    for (size_t glyphIndex = 0; glyphIndex < placed.shaped->glyphs.size();
         ++glyphIndex) {
      placed.glyph = placed.shaped->glyphs[glyphIndex];
      placed.advance = placed.shaped->advances[glyphIndex];
      placed.pen = run.penOffset + penLocal + placed.advance * 0.5f;
      if (run.transformed && interval) {
        SkPoint centre;
        interval->placeAt(placed.pen, 0.0f, layout.tangentRotationSteps,
                          &centre, &placed.tangent);
        // From the advance CENTRE back to the glyph's origin, through the
        // same back-out of HarfBuzz's offsets the blob was baked with —
        // otherwise an accented glyph's rest drifts off the curve.
        const float offsetX =
            placed.shaped->positions[glyphIndex].x() - penLocal;
        const float offsetY = placed.shaped->positions[glyphIndex].y();
        const float centreX = placed.advance * 0.5f - offsetX;
        const float centreY = -offsetY;
        placed.rest = {centre.x() - (placed.tangent.x() * centreX -
                                     placed.tangent.y() * centreY),
                       centre.y() - (placed.tangent.y() * centreX +
                                     placed.tangent.x() * centreY)};
      } else {
        placed.tangent = {1, 0};
        placed.rest = run.origin + placed.shaped->positions[glyphIndex];
      }
      penLocal += placed.advance;
      placed.glyphIndex = static_cast<uint32_t>(glyphIndex);
      placed.cluster = glyphIndex < placed.shaped->clusters.size()
                           ? placed.shaped->clusters[glyphIndex]
                           : 0;
      placed.textIndex = std::min(textBegin + placed.cluster, textLimit);
      placed.sentenceIndex =
          sentenceStarts.empty()
              ? 0
              : static_cast<uint32_t>(std::upper_bound(sentenceStarts.begin(),
                                                       sentenceStarts.end(),
                                                       placed.textIndex) -
                                      sentenceStarts.begin() - 1);
      visitor(placed);
      ++placed.ordinal;
    }
  }
}

/** Quantizes `angle` to `cosine`/`sine` on a 64-step table (≈5.6° per step,
 * visually indistinguishable for tumbling letters): continuous per-letter
 * angles would re-rasterize every glyph mask every frame on the CPU raster
 * backend. The GPU backend doesn't need this, but it doesn't hurt there
 * either.
 */
inline void quantizeAngle(float angle, float& cosine, float& sine) {
  constexpr int kSteps = 64;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  static const auto angleTable = [] {
    std::array<std::pair<float, float>, kSteps> table;
    for (int stepIndex = 0; stepIndex < kSteps; ++stepIndex)
      table[static_cast<size_t>(stepIndex)] = {
          std::cos(stepIndex * kTwoPi / kSteps),
          std::sin(stepIndex * kTwoPi / kSteps)};
    return table;
  }();
  int stepIndex =
      static_cast<int>(std::lround(angle / kTwoPi * kSteps)) % kSteps;
  if (stepIndex < 0) stepIndex += kSteps;
  cosine = angleTable[static_cast<size_t>(stepIndex)].first;
  sine = angleTable[static_cast<size_t>(stepIndex)].second;
}

/** The same snap on a CALLER-CHOSEN ladder: `steps` directions round the
 * circle, computed rather than tabled because a caller cuts the ladder by
 * rendered glyph size — one step turns an outline by a fixed angle, a fixed
 * angle displaces a glyph's extremity by more pixels the larger the glyph
 * is drawn, so a size-blind ladder that vanishes on a caption ticks
 * visibly on display type. At 64 steps this answers bit for bit what the
 * tabled overload answers. `steps <= 0` is the exact angle — the
 * continuous opt-out spelled as a ladder of none. */
inline void quantizeAngle(float angle, int steps, float& cosine, float& sine) {
  if (steps <= 0) {
    cosine = std::cos(angle);
    sine = std::sin(angle);
    return;
  }
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  int stepIndex = static_cast<int>(std::lround(angle / kTwoPi * steps)) % steps;
  if (stepIndex < 0) stepIndex += steps;
  cosine = std::cos(stepIndex * kTwoPi / steps);
  sine = std::sin(stepIndex * kTwoPi / steps);
}

/** Returns the colour filter that scales a pass's RED, GREEN and BLUE by
 * `tint`, then adds `add` and screens `screen` over the result — composed
 * over whatever filter the pass already carried (that one runs first, so
 * the modulation applies to the finished colour). Alpha is left alone: a
 * per-glyph fade rides the paint's own alpha instead.
 *
 * ALL THREE TERMS RIDE ONE COLOUR MATRIX, because screening against a
 * constant is affine per channel — c → c(1−s) + s — so multiply, add and
 * screen fold into one scale-and-bias: c·tint·(1−s) + (add·(1−s) + s).
 * The matrix filter clamps its output, which is where "added, clamped"
 * happens on this path. Neutral add and screen (all zero) contribute a
 * zero bias, leaving exactly the scale-only matrix a bare tint builds.
 *
 * MEMOIZED, and that is a correctness requirement rather than a saving: a
 * batch's key is a whole SkPaint, and SkPaint compares its colour filter by
 * POINTER, so a freshly built filter per glyph would mint a bucket per glyph
 * and undo the batching entirely. Callers quantize the tint for the same
 * reason they quantize alpha; the cap is what keeps a caller that does not
 * from growing the table without bound.
 *
 * Past the cap the LEAST RECENTLY USED entry goes, one at a time. What that
 * buys over emptying the table is the case where the cap is reached at all:
 * a caller whose live tints sit just over the cap would, with a wholesale
 * drop, lose every filter it is still using — including the ones it asks for
 * again on the same frame — and rebuild them all, repeatedly. Evicting the
 * coldest entry instead costs a caller only the tints it has stopped using,
 * so a working set at the cap keeps its identities stable and its batching
 * intact.
 *
 * The composed filter holds a reference to `under`, and the table holds the
 * composed filter, so the address `under` contributes to the key cannot be
 * recycled underneath a live entry.
 */
inline sk_sp<SkColorFilter> tintFilter(const SkColor4f& tint,
                                       sk_sp<SkColorFilter> under,
                                       const SkColor4f& add = {0, 0, 0, 0},
                                       const SkColor4f& screen = {0, 0, 0, 0}) {
  using Key = std::pair<std::array<uint32_t, 9>, const void*>;
  struct Entry {
    Key key;
    sk_sp<SkColorFilter> filter;
  };
  // Most recently used at the front. The map holds iterators into the list,
  // which std::list keeps valid across splice and across every insertion.
  static thread_local std::list<Entry> order;
  static thread_local std::map<Key, std::list<Entry>::iterator> table;
  const Key key{
      {std::bit_cast<uint32_t>(tint.fR), std::bit_cast<uint32_t>(tint.fG),
       std::bit_cast<uint32_t>(tint.fB), std::bit_cast<uint32_t>(add.fR),
       std::bit_cast<uint32_t>(add.fG), std::bit_cast<uint32_t>(add.fB),
       std::bit_cast<uint32_t>(screen.fR), std::bit_cast<uint32_t>(screen.fG),
       std::bit_cast<uint32_t>(screen.fB)},
      (const void*)under.get()};
  const auto found = table.find(key);
  if (found != table.end()) {
    order.splice(order.begin(), order, found->second);
    return found->second->filter;
  }
  // The one affine map: scale by tint·(1−screen), bias by add·(1−screen) +
  // screen. Translate rides the matrix in the same normalized [0,1] units
  // the scale reads.
  const float headroom[3] = {1 - screen.fR, 1 - screen.fG, 1 - screen.fB};
  SkColorMatrix scale;
  scale.setScale(tint.fR * headroom[0], tint.fG * headroom[1],
                 tint.fB * headroom[2], 1.0f);
  scale.postTranslate(add.fR * headroom[0] + screen.fR,
                      add.fG * headroom[1] + screen.fG,
                      add.fB * headroom[2] + screen.fB, 0.0f);
  sk_sp<SkColorFilter> filter = SkColorFilters::Matrix(scale);
  if (under) filter = SkColorFilters::Compose(filter, std::move(under));
  constexpr size_t kTintCap = 512;
  if (table.size() >= kTintCap) {
    table.erase(order.back().key);
    order.pop_back();
  }
  order.push_front({key, filter});
  table.emplace(key, order.begin());
  return filter;
}

/// How one glyph is DRESSED for a batched draw: where it lands, what it is
/// faded and tinted by, and which face it draws with.
///
/// Everything here is per-GLYPH and nothing here is per-pass: a dressed glyph
/// still draws its span's whole PaintStyle, one bucket per pass.
struct GlyphDress {
  SkPoint center = {0, 0};  ///< where the glyph's advance-centre lands
  float cosine = 1;         ///< rotation with the uniform scale folded in,
  float sine = 0;           ///< the RSXform convention
  /// Multiplies every pass's alpha. Quantize it if an effect drives it
  /// continuously — distinct alphas are distinct buckets.
  float alphaScale = 1.0f;
  /// Multiplies every pass's colour, channel by channel. A pass painting a
  /// flat colour multiplies that colour; a pass painting a shader (or
  /// already carrying a colour filter) gets the equivalent modulating
  /// filter, so a gradient keeps its ramp and takes the tint over it.
  /// Alpha here folds into `alphaScale`. White is no tint.
  SkColor4f colorMul = {1, 1, 1, 1};
  /// Added to every pass's colour after the multiply, clamped at the draw
  /// — the flash a multiplier cannot brighten into. RGB only; the alpha
  /// component is never read, coverage being `alphaScale`'s lane. Zero is
  /// no flash, and keeps the untouched-paint fast path.
  SkColor4f colorAdd = {0, 0, 0, 0};
  /// Screened over every pass's colour after the add — c becomes
  /// 1 − (1 − c)(1 − screen) — the glow that lifts each channel by its
  /// headroom and never clips. RGB only, as `colorAdd`. Zero is no glow.
  /// Screening against a constant is affine per channel, so all three
  /// colour terms ride the one memoized matrix filter together.
  SkColor4f colorScreen = {0, 0, 0, 0};
  /// The face to draw with, or null for the shaped word's own — a varied
  /// clone for a glyph whose effect drives a variable-font axis. It is part
  /// of the bucket key, so two faces are two buckets.
  sk_sp<SkTypeface> face;
  /// Non-null: the glyph-local vector from the glyph's DRAW ORIGIN to the
  /// pose centre `center` names — the point the rotation and the scale turn
  /// about. Null keeps the horizontal convention, (halfAdvance, 0).
  ///
  /// A vertical column needs it: an upright glyph's advance runs down the
  /// page while the glyph itself is drawn from a horizontal origin, so half
  /// its advance to the RIGHT is half a column pitch away from anything the
  /// eye would call its centre. Borrowed for the duration of the call.
  const SkVector* centreOffset = nullptr;
  /// Non-null: draw this glyph under this MATRIX instead of an RSXform,
  /// which is the only way to place a shear or a non-uniform scale. It
  /// carries the whole placement — centre, rotation and scale included — so
  /// `center`, `cosine` and `sine` are unread when it is set. Borrowed for
  /// the duration of the addGlyph call.
  const SkMatrix* matrix = nullptr;
};

/// Glyphs grouped by (font, paint pass): a frame of thousands of animated
/// letters collapses into a handful of drawGlyphsRSXform calls. Reuse one
/// instance across frames — clear() keeps the allocations.
///
/// A glyph is added with a whole PaintStyle and lands in one batch per pass
/// it draws: each underlay in order, then the foreground, then each overlay.
/// Because a batch's key is a complete SkPaint, a pass keeps its gradient,
/// stroke, blend mode and mask filter — animating letters and styling them
/// are not alternatives. Batches draw band by band — every underlay batch,
/// then every foreground batch, then every overlay batch, each band in
/// creation order — so every underlay lands beneath every foreground even
/// when per-glyph fades split one style into several buckets. Creation order
/// alone cannot promise that: the first glyph at a new fade mints its
/// underlay bucket after every earlier fade's foreground bucket, and a
/// blurred halo reaches past its own glyph onto its neighbours' strokes.
///
/// A GlyphDress carries what varies per glyph rather than per pass — the
/// placement, the fade, the tint, a varied face, and the matrix a shear or a
/// non-uniform scale needs. The face joins the bucket key; the fade and the
/// tint change only the resolved paint.
struct GlyphRSXformBatches {
  /// Which stratum of a PaintStyle a bucket's pass came from. The draw
  /// walks these in declaration order, so a bucket's band — not when it was
  /// minted — decides what it composites over.
  enum class PassBand : uint8_t { Underlay, Foreground, Overlay };

  /// One (font, paint pass) bucket: parallel glyph/transform arrays that feed
  /// a single drawGlyphsRSXform call. The font is held as the identity
  /// makeFont() needs rather than as the shaped word it came from, so words
  /// set in the same face and size share one bucket — and so a bucket kept
  /// across frames never outlives a shaped word the cache has since evicted.
  struct Batch {
    sk_sp<SkTypeface> typeface;  ///< bucket key: the face to draw with
    float fontSize = 0;          ///< bucket key: px size
    float scaleX = 1.0f;         ///< bucket key: horizontal
                                 ///< condensation baked into shaping
    bool aliased = false;        ///< bucket key: hard-edged raster
    SkPaint paint;               ///< bucket key: the complete pass
    SkVector offset = {0, 0};    ///< bucket key: the pass's own
                                 ///< translation (shadows, echoes)
    /// Bucket key: the stratum this pass draws in. The same paint used as
    /// one style's underlay and another's foreground is two buckets,
    /// because the two composite differently.
    PassBand band = PassBand::Foreground;
    std::vector<SkGlyphID> glyphs;      ///< parallel to `transforms`
    std::vector<SkRSXform> transforms;  ///< per-glyph scale/rotate/translate
    /// The glyphs of this bucket that an RSXform cannot place — a shear, a
    /// non-uniform scale — with the matrix each draws under. They cost one
    /// canvas concat and one draw apiece and are the reason to keep them a
    /// separate lane: a glyph whose deviation IS an RSXform never pays for
    /// a neighbour that is not.
    std::vector<SkGlyphID> matrixGlyphs;  ///< parallel to `matrices`
    std::vector<SkMatrix> matrices;
  };
  /** THE GLYPHS ADDED HERE MOVE BETWEEN FRAMES, so their origins are placed
   *  on Skia's SUBPIXEL PHASE GRID instead of on whole pixels.
   *
   *  A glyph mask is rasterized for a quantized origin. Left on whole
   *  pixels, a run creeping along by a fraction of a pixel per frame does
   *  not creep at all: each letter stands still until its own origin
   *  crosses a pixel boundary and then HOPS a whole pixel, at its own
   *  moment, which is exactly the unsteadiness a turning ring shows. On the
   *  phase grid the same creep advances a quarter pixel at a time, and the
   *  hop is a quarter of what it was.
   *
   *  IT IS OFF BY DEFAULT because the grid is the second factor in a
   *  product. Every mask is a (glyph, rotation, phase) triple: the phases
   *  multiply what a rotation ladder has already multiplied, on both axes
   *  for an off-axis run. A run at REST gains nothing from it — its letters
   *  are not creeping anywhere — and would pay the multiplied population
   *  for a placement no one can see move, which is why settled type keeps
   *  whole-pixel origins.
   *
   *  A MOVING run's arithmetic is the other way round. Its masks were never
   *  going to be reused: the rotation it needs this frame is a different
   *  rotation next frame, so the population it mints is per-frame either
   *  way, and the phase grid only refines a mask it was going to rasterize
   *  regardless. This is the same trade the rotation ladder makes and not a
   *  competing one — the ladder still bounds the ROTATIONS, and dropping it
   *  in exchange costs several times what the grid does. */
  bool subpixel = false;
  std::vector<Batch> batches;  ///< one entry per distinct (font, pass) pair
  /// Where the last pass landed. Neighbouring glyphs repeat a pass, and a
  /// full SkPaint is dearer to compare than a color, so the scan starts
  /// where it last succeeded. Bounds-checked: callers own `batches`.
  size_t recentBatch = 0;

  /** Returns the batch for one shaped word's font and one resolved pass.
   * `face` overrides the shaped word's own typeface when it is non-null —
   * the varied clone a driven variable-font axis asks for — and is part of
   * the key, so the same word set at two axis coordinates is two buckets. */
  [[nodiscard]] Batch& batchForPass(const ShapedWord* font,
                                    const sk_sp<SkTypeface>& face,
                                    const SkPaint& paint, SkVector offset,
                                    PassBand band) {
    const sk_sp<SkTypeface>& resolved = face ? face : font->typeface;
    auto matches = [&](const Batch& batch) {
      return batch.typeface.get() == resolved.get() &&
             batch.fontSize == font->fontSize && batch.scaleX == font->scaleX &&
             batch.aliased == font->aliased && batch.offset == offset &&
             batch.band == band && batch.paint == paint;
    };
    if (recentBatch < batches.size() && matches(batches[recentBatch]))
      return batches[recentBatch];
    for (size_t index = 0; index < batches.size(); ++index)
      if (matches(batches[index])) {
        recentBatch = index;
        return batches[index];
      }
    recentBatch = batches.size();
    batches.push_back({resolved,
                       font->fontSize,
                       font->scaleX,
                       font->aliased,
                       paint,
                       offset,
                       band,
                       {},
                       {},
                       {},
                       {}});
    return batches.back();
  }

  /** Appends one glyph — once per pass of `style` — anchored at its
   * advance-centre `centerPosition` and rotated by (`cosine`, `sine`), the
   * placement convention the effects use.
   *
   * `alphaScale` multiplies every pass's alpha, which is how a per-glyph
   * fade stays batched: the style itself is untouched and only the resolved
   * paint differs. Quantize it if the effect drives it continuously —
   * distinct alphas are distinct buckets. Passes with nothing to draw are
   * skipped, so a fully faded glyph costs no bucket at all.
   */
  void addGlyph(const ShapedWord* font, const PaintStyle& style,
                SkGlyphID glyph, float halfAdvance, SkPoint centerPosition,
                float cosine = 1, float sine = 0, float alphaScale = 1.0f) {
    addGlyph(font, style, glyph, halfAdvance,
             GlyphDress{.center = centerPosition,
                        .cosine = cosine,
                        .sine = sine,
                        .alphaScale = alphaScale});
  }

  /** Appends one dressed glyph — once per pass of `style` — placed, faded,
   * tinted and faced as `dress` says.
   *
   * The tint and the fade never touch the style itself: only the RESOLVED
   * paint of each pass differs, which is what keeps a coloured, faded letter
   * in the same handful of buckets as its neighbours. Passes with nothing to
   * draw are skipped, so a fully faded glyph costs no bucket at all.
   */
  void addGlyph(const ShapedWord* font, const PaintStyle& style,
                SkGlyphID glyph, float halfAdvance, const GlyphDress& dress) {
    const float alpha = dress.alphaScale * dress.colorMul.fA;
    // Any of the three colour terms off neutral takes the modulated path;
    // all neutral leaves the source paint untouched, byte for byte.
    const bool tinted = dress.colorMul.fR != 1.0f ||
                        dress.colorMul.fG != 1.0f ||
                        dress.colorMul.fB != 1.0f || dress.colorAdd.fR != 0 ||
                        dress.colorAdd.fG != 0 || dress.colorAdd.fB != 0 ||
                        dress.colorScreen.fR != 0 ||
                        dress.colorScreen.fG != 0 || dress.colorScreen.fB != 0;
    const SkVector local =
        dress.centreOffset ? *dress.centreOffset : SkVector{halfAdvance, 0};
    const SkRSXform transform = {
        dress.cosine, dress.sine,
        dress.center.x() - (dress.cosine * local.x() - dress.sine * local.y()),
        dress.center.y() - (dress.sine * local.x() + dress.cosine * local.y())};
    auto place = [&](Batch& batch) {
      if (dress.matrix) {
        batch.matrixGlyphs.push_back(glyph);
        batch.matrices.push_back(*dress.matrix);
      } else {
        batch.glyphs.push_back(glyph);
        batch.transforms.push_back(transform);
      }
    };
    auto addPass = [&](const SkPaint& source, SkVector offset, PassBand band) {
      if (alpha >= 1.0f && !tinted) {
        if (source.nothingToDraw()) return;
        place(batchForPass(font, dress.face, source, offset, band));
        return;
      }
      SkPaint dressed = source;
      if (alpha != 1.0f) dressed.setAlphaf(source.getAlphaf() * alpha);
      if (tinted) {
        // A flat pass takes the modulation in its colour; a pass whose
        // colour is decided downstream — by a shader, or by a filter
        // already on it — takes the equivalent modulation as a filter over
        // that colour. Same arithmetic both ways: multiply, add, clamp,
        // then screen.
        if (dressed.getShader() || dressed.getColorFilter()) {
          dressed.setColorFilter(tintFilter(dress.colorMul,
                                            dressed.refColorFilter(),
                                            dress.colorAdd, dress.colorScreen));
        } else {
          const auto channel = [&](float base, float mul, float add,
                                   float screen) {
            const float lit = std::clamp(base * mul + add, 0.0f, 1.0f);
            return lit + (1.0f - lit) * screen;
          };
          const SkColor4f base = dressed.getColor4f();
          dressed.setColor4f({channel(base.fR, dress.colorMul.fR,
                                      dress.colorAdd.fR, dress.colorScreen.fR),
                              channel(base.fG, dress.colorMul.fG,
                                      dress.colorAdd.fG, dress.colorScreen.fG),
                              channel(base.fB, dress.colorMul.fB,
                                      dress.colorAdd.fB, dress.colorScreen.fB),
                              base.fA},
                             nullptr);
        }
      }
      if (dressed.nothingToDraw()) return;
      place(batchForPass(font, dress.face, dressed, offset, band));
    };
    for (const PaintLayer& layer : style.underlays)
      addPass(layer.paint, layer.offset, PassBand::Underlay);
    addPass(style.foreground, {0, 0}, PassBand::Foreground);
    for (const PaintLayer& layer : style.overlays)
      addPass(layer.paint, layer.offset, PassBand::Overlay);
  }

  /** Appends a visited glyph at `centerPosition`, taking its font, advance
   * and span paint from the walk. The ergonomic pairing with
   * forEachPlacedGlyph.
   */
  void addGlyph(const PlacedGlyph& placed, SkPoint centerPosition,
                float cosine = 1, float sine = 0, float alphaScale = 1.0f) {
    addGlyph(placed.shaped, *placed.paint, placed.glyph, placed.advance * 0.5f,
             centerPosition, cosine, sine, alphaScale);
  }

  /** Appends a visited glyph dressed as `dress` says, taking its font,
   * advance and span paint from the walk. `glyph` overrides the walk's own
   * glyph ID, which is how a code-point substitution draws a different
   * letter at the original's pen position. */
  void addGlyph(const PlacedGlyph& placed, const GlyphDress& dress,
                SkGlyphID glyph) {
    addGlyph(placed.shaped, *placed.paint, glyph, placed.advance * 0.5f, dress);
  }
  void addGlyph(const PlacedGlyph& placed, const GlyphDress& dress) {
    addGlyph(placed, dress, placed.glyph);
  }

  /** Clears glyph data while retaining batch allocations for the next frame.
   *
   * A frame that minted a pathological number of buckets releases them
   * instead, because a retained bucket also retains its paint — and with it
   * every shader, filter and blender that paint holds a reference to.
   */
  void clear() {
    constexpr size_t kRetainedBucketCap = 256;
    recentBatch = 0;
    // Back to whole-pixel origins: the motion declaration belongs to the run
    // that is about to be added, never to the one that just drew.
    subpixel = false;
    if (batches.size() > kRetainedBucketCap) {
      batches.clear();
      return;
    }
    for (Batch& batch : batches) {
      batch.glyphs.clear();
      batch.transforms.clear();
      batch.matrixGlyphs.clear();
      batch.matrices.clear();
    }
  }

  /** Draws every batch — underlay buckets, then foreground buckets, then
   * overlay buckets, each band in creation order — and returns the number
   * of glyph draws it issued, one per glyph per pass. The band walk is what
   * keeps a blurred halo beneath a neighbouring letter's stroke when
   * per-glyph fades have split the style across several buckets. */
  int draw(SkCanvas* canvas) const {
    int total = 0;
    for (const PassBand band :
         {PassBand::Underlay, PassBand::Foreground, PassBand::Overlay})
      for (const Batch& batch : batches) {
        if (batch.band != band) continue;
        total += drawBatch(canvas, batch, subpixel);
      }
    return total;
  }

 private:
  /** One bucket's draws: the shared RSXform lane, then its matrix lane. */
  static int drawBatch(SkCanvas* canvas, const Batch& batch, bool subpixel) {
    if (batch.glyphs.empty() && batch.matrixGlyphs.empty()) return 0;
    int total = 0;
    SkFont font =
        makeFont(batch.typeface, batch.fontSize, batch.scaleX, batch.aliased);
    // Whole-pixel origins unless the run declared itself in motion — see
    // `subpixel` above for which way that trade runs.
    font.setSubpixel(subpixel);
    if (!batch.glyphs.empty()) {
      total += static_cast<int>(batch.glyphs.size());
      canvas->drawGlyphsRSXform(
          SkSpan<const SkGlyphID>(batch.glyphs.data(), batch.glyphs.size()),
          SkSpan<const SkRSXform>(batch.transforms.data(),
                                  batch.transforms.size()),
          {batch.offset.x(), batch.offset.y()}, font, batch.paint);
    }
    // The bucket's matrix lane, inside the bucket's own place in the pass
    // order: one save/concat and one draw per glyph, but still one font
    // and one paint, and still beneath whatever this style's later passes
    // put over it.
    constexpr SkPoint kAtTheMatrixOrigin{0, 0};
    for (size_t index = 0; index < batch.matrixGlyphs.size(); ++index) {
      canvas->save();
      canvas->translate(batch.offset.x(), batch.offset.y());
      canvas->concat(batch.matrices[index]);
      canvas->drawGlyphs(SkSpan<const SkGlyphID>(&batch.matrixGlyphs[index], 1),
                         SkSpan<const SkPoint>(&kAtTheMatrixOrigin, 1), {0, 0},
                         font, batch.paint);
      canvas->restore();
      ++total;
    }
    return total;
  }
};

}  // namespace sigil::weave
