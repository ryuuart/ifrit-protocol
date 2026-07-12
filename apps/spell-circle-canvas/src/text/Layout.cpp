#include "textflow/Layout.h"
#include "LayoutInternal.h"
#include "textflow/FontContext.h"
#include "textflow/Shaper.h"

#include <include/core/SkBlurTypes.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkTextBlob.h>

#include <absl/container/flat_hash_map.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace textflow {

namespace detail {

namespace {

constexpr float kFitEps = 0.25f;
// After this many consecutive intervals rejected a word outright, it is
// force-placed (overflowing) rather than skipping arbitrarily far down the
// geometry — matches browser overflow behavior for unbreakably-wide content.
constexpr int kMaxIntervalSkips = 12;

enum class GapKind : uint8_t { kRigid, kSpace, kIdeographic };

GapKind gapKind(const std::vector<Word> &words, uint32_t i,
                const LayoutOptions &opt) {
  if (words[i].spaceWidth > 0)
    return GapKind::kSpace;
  if (opt.ideographicJustify &&
      (words[i].ideographic || words[i + 1].ideographic))
    return GapKind::kIdeographic;
  return GapKind::kRigid;
}

float wordEm(const Word &word) {
  return word.segments.empty() ? 16.0f : word.segments.front().shaped->size;
}

// Snaps a unit tangent to one of `steps` directions (512 → 0.7° steps —
// invisible). Continuously varying per-glyph rotations would otherwise mint
// a fresh glyph-atlas strike every frame for every glyph on a moving path,
// turning animated curved text into a per-frame mask-rasterization storm.
// See LayoutOptions::rotationSteps.
SkVector quantizeTangent(SkVector tan, int steps) {
  if (steps <= 0)
    return tan;
  constexpr float kTwoPi = 6.2831853f;
  const float angle = std::atan2(tan.fY, tan.fX);
  int i = static_cast<int>(std::lround(angle / kTwoPi * steps)) % steps;
  if (i < 0)
    i += steps;
  const float snapped = static_cast<float>(i) * kTwoPi / steps;
  return {std::cos(snapped), std::sin(snapped)};
}

// Per-glyph RSXform blob for rotated straight intervals and path contours.
sk_sp<SkTextBlob> buildTransformedBlob(const ShapedWord &sw,
                                       const LineInterval &iv, float penX,
                                       int rotationSteps) {
  if (sw.glyphs.empty())
    return nullptr;
  SkTextBlobBuilder builder;
  const SkFont font = makeFont(sw.typeface, sw.size);
  const int count = static_cast<int>(sw.glyphs.size());
  const auto &run = builder.allocRunRSXform(font, count);

  const float contourLength = iv.contour ? iv.contour->length() : 0;
  float penLocal = 0;
  for (int i = 0; i < count; ++i) {
    const float adv = sw.advances[i];
    // Offsets HarfBuzz applied on top of the pen position.
    const float offX = sw.positions[i].x() - penLocal;
    const float offY = sw.positions[i].y();

    SkPoint pos;
    SkVector tan;
    if (iv.contour) {
      float s =
          iv.contourStart + (penX + penLocal + adv * 0.5f) * iv.advanceScale;
      if (iv.contour->isClosed()) {
        // Closed contours wrap: text can march around the loop forever
        // (animate contourStart for an infinite marquee).
        s = std::fmod(s, contourLength);
        if (s < 0)
          s += contourLength;
      } else {
        s = std::clamp(s, 0.0f, contourLength);
      }
      if (!iv.contour->getPosTan(s, &pos, &tan)) {
        pos = {0, 0};
        tan = {1, 0};
      }
      // Rotation snaps; position stays exact.
      tan = quantizeTangent(tan, rotationSteps);
    } else {
      tan = iv.dir;
      const float d = penX + penLocal + adv * 0.5f;
      pos = iv.origin + SkVector{tan.x() * d, tan.y() * d};
      tan = quantizeTangent(tan, rotationSteps);
    }

    // Anchor the glyph's advance-center on the baseline point `pos`,
    // rotated to the local tangent. Center in glyph-local coordinates:
    const float cx = adv * 0.5f - offX;
    const float cy = -offY;
    run.glyphs[i] = sw.glyphs[i];
    run.xforms()[i] = {tan.x(), tan.y(), pos.x() - (tan.x() * cx - tan.y() * cy),
                       pos.y() - (tan.y() * cx + tan.x() * cy)};
    penLocal += adv;
  }
  return builder.make();
}

void emitSegment(Layout &out, const FlatInterval &fiv, const WordSegment &seg,
                 uint32_t wordIndex, float penX, const LayoutOptions &opt) {
  const ShapedWord &sw = *seg.shaped;
  if (sw.glyphs.empty())
    return;
  PlacedRun run;
  run.shaped = seg.shaped;
  run.styleIndex = seg.styleIndex;
  run.wordIndex = wordIndex;
  run.line = fiv.line;
  const bool straight = !fiv.iv.contour;
  const bool horizontal = straight && fiv.iv.dir.x() == 1 &&
                          fiv.iv.dir.y() == 0 &&
                          seg.form == SegmentForm::kFlow;
  const bool verticalColumn =
      straight && fiv.iv.dir.x() == 0 && fiv.iv.dir.y() == 1;
  if (horizontal) {
    run.blob = wordBlob(sw);
    run.origin = fiv.iv.origin + SkVector{penX, 0};
  } else if (verticalColumn && seg.form == SegmentForm::kUpright) {
    // Vertical-shaped word: positions already stack down the column.
    run.blob = wordBlob(sw);
    run.origin = fiv.iv.origin + SkVector{0, penX};
  } else if (verticalColumn && seg.form == SegmentForm::kTateChuYoko) {
    // Horizontal run set upright across the column, centred on its axis;
    // penX already points at the run's baseline (see Paragraph::analyze).
    run.blob = wordBlob(sw);
    run.origin = fiv.iv.origin + SkVector{-sw.advance * 0.5f, penX};
  } else {
    // Rotated/curved: bake per-glyph transforms (kRotated Latin in a
    // vertical column rotates 90° clockwise here via the interval tangent).
    run.blob = buildTransformedBlob(sw, fiv.iv, penX, opt.rotationSteps);
    run.origin = {0, 0};
    run.transformed = true;
  }
  if (run.blob)
    out.runs.push_back(std::move(run));
}

// UAX #9 rule L2 over per-word levels: reverse maximal runs of every level
// >= each odd level, highest level first.
void visualOrder(const std::vector<Word> &words, uint32_t first, uint32_t last,
                 std::vector<uint32_t> &order) {
  order.clear();
  order.reserve(last - first);
  uint8_t maxLevel = 0, minOdd = 255;
  for (uint32_t i = first; i < last; ++i) {
    order.push_back(i);
    const uint8_t level = words[i].bidiLevel;
    maxLevel = std::max(maxLevel, level);
    if (level & 1)
      minOdd = std::min(minOdd, level);
  }
  for (uint8_t level = maxLevel; level >= minOdd && minOdd != 255; --level) {
    size_t i = 0;
    while (i < order.size()) {
      if (words[order[i]].bidiLevel >= level) {
        size_t j = i;
        while (j < order.size() && words[order[j]].bidiLevel >= level)
          j++;
        std::reverse(order.begin() + i, order.begin() + j);
        i = j;
      } else {
        i++;
      }
    }
  }
}

} // namespace

float naturalWidth(const std::vector<Word> &words, uint32_t first,
                   uint32_t last) {
  float width = 0;
  for (uint32_t i = first; i < last; ++i) {
    width += words[i].width;
    if (i + 1 < last)
      width += words[i].spaceWidth;
  }
  return width;
}

void placeWords(const std::vector<Word> &words, uint32_t first, uint32_t last,
                const FlatInterval &fiv, Alignment alignment, bool lastLine,
                bool hyphenBreakTaken, const LayoutOptions &opt, Layout &out) {
  if (first >= last)
    return;

  const float hyphenWidth =
      hyphenBreakTaken ? words[last - 1].hyphenGlyph->advance : 0.0f;

  // Gap census for justification.
  int spaceGaps = 0, ideoGaps = 0;
  for (uint32_t i = first; i + 1 < last; ++i) {
    switch (gapKind(words, i, opt)) {
    case GapKind::kSpace:
      spaceGaps++;
      break;
    case GapKind::kIdeographic:
      ideoGaps++;
      break;
    case GapKind::kRigid:
      break;
    }
  }

  const float natural = naturalWidth(words, first, last) + hyphenWidth;
  const float extra = fiv.iv.length - natural;

  Alignment align = alignment;
  if (align == Alignment::kJustify && lastLine && !opt.justifyLastLine)
    align = opt.lastLineAlignment;

  float startOffset = 0, perSpace = 0, perIdeo = 0;
  switch (align) {
  case Alignment::kStart:
    break;
  case Alignment::kCenter:
    startOffset = std::max(0.0f, extra * 0.5f);
    break;
  case Alignment::kEnd:
    startOffset = std::max(0.0f, extra);
    break;
  case Alignment::kJustify: {
    if (extra > 0 && (spaceGaps + ideoGaps) > 0) {
      const float cap =
          opt.maxIdeographicExpansion * wordEm(words[first]);
      float perGap = extra / static_cast<float>(spaceGaps + ideoGaps);
      if (ideoGaps > 0 && perGap > cap) {
        perIdeo = cap;
        perSpace = spaceGaps > 0
                       ? (extra - perIdeo * static_cast<float>(ideoGaps)) /
                             static_cast<float>(spaceGaps)
                       : 0; // no spaces to absorb the rest: stay underfull
      } else {
        perSpace = perIdeo = perGap;
      }
    } else if (extra < 0 && (spaceGaps + ideoGaps) > 0) {
      // Shrink, but never beyond the shrink limits — a slightly overfull
      // line beats spaces collapsing to nothing. Ideographic gaps compress
      // a touch too, mirroring the breakers' shrink model (em * 0.03), so
      // a break the breaker deemed renderable never leaks past the measure.
      float totalGlue = 0;
      for (uint32_t i = first; i + 1 < last; ++i)
        totalGlue += words[i].spaceWidth;
      const float spaceCap =
          spaceGaps > 0
              ? totalGlue / static_cast<float>(spaceGaps) * opt.glueShrink
              : 0;
      const float ideoCap = 0.03f * wordEm(words[first]);
      const float capacity = spaceCap * static_cast<float>(spaceGaps) +
                             ideoCap * static_cast<float>(ideoGaps);
      if (capacity > 0) {
        const float k = std::min(1.0f, -extra / capacity);
        perSpace = -k * spaceCap;
        perIdeo = -k * ideoCap;
      }
    }
    break;
  }
  }

  // Visual reordering (no-op for pure-LTR lines).
  static thread_local std::vector<uint32_t> order;
  visualOrder(words, first, last, order);

  float pen = startOffset;
  for (size_t v = 0; v < order.size(); ++v) {
    const uint32_t i = order[v];
    const Word &word = words[i];
    for (const WordSegment &seg : word.segments)
      emitSegment(out, fiv, seg, i, pen + seg.xOffset, opt);
    if (word.placeholderIndex >= 0 && !fiv.iv.contour) {
      // Inline slot: report where it landed (blob-less run; draw() and
      // drawBatched() skip it, placeholderRects() surfaces it).
      PlacedRun run;
      run.origin = fiv.iv.origin +
                   SkVector{fiv.iv.dir.x() * pen, fiv.iv.dir.y() * pen};
      run.wordIndex = i;
      run.line = fiv.line;
      run.placeholder = word.placeholderIndex;
      out.runs.push_back(std::move(run));
    }
    pen += word.width;
    if (hyphenBreakTaken && i == last - 1) {
      // Discretionary break taken: render the hyphen right after the word.
      const uint32_t styleIndex =
          word.segments.empty() ? 0 : word.segments.back().styleIndex;
      emitSegment(out, fiv, WordSegment{word.hyphenGlyph, styleIndex, 0}, i,
                  pen, opt);
      pen += hyphenWidth;
    }
    if (v + 1 < order.size()) {
      // Glue between visual neighbors; logical == visual for LTR text.
      pen += word.spaceWidth;
      switch (gapKind(words, std::min(i, order[v + 1]), opt)) {
      case GapKind::kSpace:
        pen += perSpace;
        break;
      case GapKind::kIdeographic:
        pen += perIdeo;
        break;
      case GapKind::kRigid:
        break;
      }
    }
  }
}

} // namespace detail

Layout layoutParagraph(FontContext &ctx, Paragraph &paragraph,
                       FlowGeometry &geometry, const LayoutOptions &opt) {
  using namespace detail;

  paragraph.ensureShaped(ctx);

  const Paragraph::Strut strut = paragraph.strut(ctx);
  const float lineHeight = opt.lineHeight > 0 ? opt.lineHeight : strut.height;
  const float ascent = opt.ascent > 0 ? opt.ascent : strut.ascent;

  Layout out;
  const std::vector<Word> &words = paragraph.words();
  if (words.empty())
    return out;

  IntervalSequence seq(geometry, lineHeight, ascent,
                       opt.breaker == Breaker::kKnuthPlass ? opt.kpMinInterval
                                                           : 0.0f);

  if (opt.breaker == Breaker::kKnuthPlass)
    return knuthPlassLayout(words, seq, opt);

  // ── Greedy breaker ───────────────────────────────────────────────────
  size_t k = 0;
  const FlatInterval *fiv = seq.get(k);
  uint32_t first = 0;
  uint32_t i = 0;
  float pen = 0;
  int skips = 0;
  int lastLineUsed = -1;
  // Widest interval passed over during the current skip run — the fallback
  // landing spot if the geometry runs out while a wide word keeps skipping.
  size_t bestSkipK = SIZE_MAX;
  float bestSkipLen = -1;

  auto flush = [&](uint32_t upto, bool isLast) {
    if (fiv && first < upto) {
      placeWords(words, first, upto, *fiv, opt.alignment, isLast,
                 hyphenTakenAt(words, upto, isLast, opt), opt, out);
      lastLineUsed = std::max(lastLineUsed, fiv->line);
    }
    first = upto;
    pen = 0;
  };

  while (i < static_cast<uint32_t>(words.size())) {
    if (!fiv) {
      out.firstUnplacedWord = i;
      break;
    }
    const Word &word = words[i];
    const float glue = (i > first) ? words[i - 1].spaceWidth : 0;
    // Soft-hyphen words reserve room for the hyphen so a break taken right
    // after them always fits.
    const float hyphenReserve =
        (opt.hyphenate && word.hyphenBreak && word.hyphenGlyph)
            ? word.hyphenGlyph->advance
            : 0;
    const bool fits =
        pen + glue + word.width + hyphenReserve <= fiv->iv.length + kFitEps;
    const bool intervalEmpty = (i == first);

    if (fits || (intervalEmpty && skips >= kMaxIntervalSkips)) {
      pen += glue + word.width;
      i++;
      skips = 0;
      bestSkipK = SIZE_MAX;
      bestSkipLen = -1;
      if (word.mandatoryBreakAfter) {
        const int line = fiv->line;
        flush(i, /*isLast=*/true);
        lastLineUsed = std::max(lastLineUsed, line);
        do {
          fiv = seq.get(++k);
        } while (fiv && fiv->line == line); // hard break skips to a new line
      }
      continue;
    }

    if (intervalEmpty) {
      if (fiv->iv.length > bestSkipLen) {
        bestSkipLen = fiv->iv.length;
        bestSkipK = k;
      }
      skips++;
      fiv = seq.get(++k);
      if ((!fiv || skips >= kMaxIntervalSkips) && bestSkipK != SIZE_MAX) {
        // Geometry exhausted or skips spent: rather than dropping the rest
        // of the text (or jamming the word into whatever narrow interval
        // the skip run happened to stop on — visibly overflowing into an
        // exclusion shape), back up to the widest interval we passed and
        // force the word in there.
        k = bestSkipK;
        fiv = seq.get(k);
        skips = kMaxIntervalSkips;
        bestSkipK = SIZE_MAX;
        bestSkipLen = -1;
      }
      continue;
    }

    flush(i, /*isLast=*/false);
    fiv = seq.get(++k);
  }

  flush(i, /*isLast=*/true);
  out.lineCount = lastLineUsed + 1;
  return out;
}

namespace {

// Blur mask filters are immutable and cheap to share; building one per run
// per frame would churn allocations, so memoize by sigma.
const sk_sp<SkMaskFilter> &blurFilter(float sigma) {
  static thread_local absl::flat_hash_map<uint32_t, sk_sp<SkMaskFilter>> cache;
  uint32_t bits;
  std::memcpy(&bits, &sigma, sizeof bits);
  auto [it, inserted] = cache.try_emplace(bits);
  if (inserted)
    it->second = SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma);
  return it->second;
}

} // namespace

namespace {

const PaintStyle &resolvePaint(const std::vector<StyleSpan> &spans,
                               uint32_t styleIndex,
                               const PaintStyle *overridePaint) {
  static const PaintStyle kFallback;
  if (overridePaint)
    return *overridePaint;
  return styleIndex < spans.size() ? spans[styleIndex].style.paint : kFallback;
}

void applyPaint(SkPaint &paint, const PaintStyle &style) {
  paint.setColor(style.color);
  paint.setShader(style.shader);
  paint.setMaskFilter(style.maskFilter);
  paint.setBlendMode(style.blendMode);
}

} // namespace

void Layout::draw(SkCanvas *canvas, const Paragraph &paragraph,
                  const PaintStyle *overridePaint) const {
  const std::vector<StyleSpan> &spans = paragraph.spans();
  SkPaint paint;
  paint.setAntiAlias(true);
  for (const PlacedRun &run : runs) {
    if (!run.blob)
      continue;
    const PaintStyle &style =
        resolvePaint(spans, run.styleIndex, overridePaint);

    // Shadow passes first (back to front), then the main fill.
    for (const DropShadow &shadow : style.shadows) {
      SkPaint shadowPaint;
      shadowPaint.setAntiAlias(true);
      shadowPaint.setColor(shadow.color);
      if (shadow.blurSigma > 0)
        shadowPaint.setMaskFilter(blurFilter(shadow.blurSigma));
      canvas->drawTextBlob(run.blob.get(), run.origin.x() + shadow.offset.x(),
                           run.origin.y() + shadow.offset.y(), shadowPaint);
    }

    applyPaint(paint, style);
    canvas->drawTextBlob(run.blob.get(), run.origin.x(), run.origin.y(),
                         paint);
  }
}

std::vector<Layout::PlacedPlaceholder>
Layout::placeholderRects(const Paragraph &paragraph) const {
  std::vector<PlacedPlaceholder> out;
  for (const PlacedRun &run : runs) {
    if (run.placeholder < 0)
      continue;
    const Placeholder &ph =
        paragraph.placeholders()[static_cast<size_t>(run.placeholder)];
    out.push_back({run.placeholder,
                   SkRect::MakeXYWH(run.origin.x(),
                                    run.origin.y() - ph.height +
                                        ph.baselineDrop,
                                    ph.width, ph.height),
                   run.line});
  }
  return out;
}

void Layout::drawBatched(SkCanvas *canvas, const Paragraph &paragraph,
                         const PaintStyle *overridePaint) const {
  const std::vector<StyleSpan> &spans = paragraph.spans();

  // Buckets keyed by (typeface, size, resolved paint). A frame's worth of
  // horizontal runs collapses into one drawGlyphs call per bucket; scratch
  // storage persists across frames (styles copied by value — span pointers
  // would dangle between calls).
  struct Bucket {
    sk_sp<SkTypeface> typeface;
    float size = 0;
    PaintStyle style;
    std::vector<SkGlyphID> glyphs;
    std::vector<SkPoint> positions;
  };
  static thread_local std::vector<Bucket> buckets;
  if (buckets.size() > 64)
    buckets.clear(); // pathological style churn: don't grow forever
  for (Bucket &b : buckets) {
    b.glyphs.clear();
    b.positions.clear();
  }

  SkPaint paint;
  paint.setAntiAlias(true);

  for (const PlacedRun &run : runs) {
    if (!run.blob)
      continue;
    const PaintStyle &style =
        resolvePaint(spans, run.styleIndex, overridePaint);

    if (run.transformed || !run.shaped) {
      // Positions are baked into the blob; draw it directly (with shadows).
      for (const DropShadow &shadow : style.shadows) {
        SkPaint shadowPaint;
        shadowPaint.setAntiAlias(true);
        shadowPaint.setColor(shadow.color);
        if (shadow.blurSigma > 0)
          shadowPaint.setMaskFilter(blurFilter(shadow.blurSigma));
        canvas->drawTextBlob(run.blob.get(),
                             run.origin.x() + shadow.offset.x(),
                             run.origin.y() + shadow.offset.y(), shadowPaint);
      }
      applyPaint(paint, style);
      canvas->drawTextBlob(run.blob.get(), run.origin.x(), run.origin.y(),
                           paint);
      continue;
    }

    const ShapedWord &sw = *run.shaped;
    Bucket *bucket = nullptr;
    for (Bucket &b : buckets)
      if (b.typeface.get() == sw.typeface.get() && b.size == sw.size &&
          b.style == style) {
        bucket = &b;
        break;
      }
    if (!bucket) {
      buckets.push_back({sw.typeface, sw.size, style, {}, {}});
      bucket = &buckets.back();
    }
    for (size_t i = 0; i < sw.glyphs.size(); ++i) {
      bucket->glyphs.push_back(sw.glyphs[i]);
      bucket->positions.push_back(run.origin + sw.positions[i]);
    }
  }

  for (const Bucket &bucket : buckets) {
    if (bucket.glyphs.empty())
      continue;
    const SkFont font = makeFont(bucket.typeface, bucket.size);
    const SkSpan<const SkGlyphID> glyphs(bucket.glyphs.data(),
                                         bucket.glyphs.size());
    const SkSpan<const SkPoint> positions(bucket.positions.data(),
                                          bucket.positions.size());
    for (const DropShadow &shadow : bucket.style.shadows) {
      SkPaint shadowPaint;
      shadowPaint.setAntiAlias(true);
      shadowPaint.setColor(shadow.color);
      if (shadow.blurSigma > 0)
        shadowPaint.setMaskFilter(blurFilter(shadow.blurSigma));
      canvas->drawGlyphs(glyphs, positions,
                         {shadow.offset.x(), shadow.offset.y()}, font,
                         shadowPaint);
    }
    applyPaint(paint, bucket.style);
    canvas->drawGlyphs(glyphs, positions, {0, 0}, font, paint);
  }
}

} // namespace textflow
