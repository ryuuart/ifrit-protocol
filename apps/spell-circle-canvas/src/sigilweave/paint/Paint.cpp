/** @file
 * The two draws of a finished layout: draw() puts one blob per run and
 * paint pass on the canvas, and drawBatched() merges horizontal runs into
 * one drawGlyphs call per (font, paint) bucket and pass, with the
 * decoration bands emitted beneath and above the glyphs in the order the
 * decoration walk defines. Every underlay draws before every foreground,
 * and every foreground before every overlay. A style per glyph draws the same
 * batches with the glyphs bucketed by the style each names. A pass carrying a
 * material is shaded through the registered resolver over the bounds of what it
 * covers.
 */

#include "sigilweave/paint/Paint.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkTextBlob.h>
#include <src/core/SkScopeExit.h>

#include <algorithm>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "sigilgeometry/advanced/Skia.h"
#include "sigilweave/advanced/DecorationRects.h"
#include "sigilweave/advanced/Skia.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

namespace paint {
namespace {
MaterialResolver& resolverSlot() {
  static MaterialResolver resolver;
  return resolver;
}
}  // namespace

void setMaterialResolver(MaterialResolver resolver) {
  resolverSlot() = std::move(resolver);
}

bool hasMaterialResolver() { return (bool)resolverSlot(); }
}  // namespace paint

namespace {

using detail::DecorationPhase;
using detail::forEachDecorationRect;
using detail::resolvePaint;

/** A material replaces only the configured paint's shader. An unavailable
 *  resolver or shader leaves the paint's own settings in force. */
template <typename DrawPass>
void drawMaterialPaint(const SkPaint& configured,
                       const material::Material* material, const SkRect& bounds,
                       SkVector offset, DrawPass&& drawPass) {
  if (material && paint::hasMaterialResolver()) {
    SkPaint shaded = configured;
    if (sk_sp<SkShader> shader = paint::resolverSlot()(*material, bounds))
      shaded.setShader(std::move(shader));
    if (!shaded.nothingToDraw()) drawPass(shaded, offset);
    return;
  }
  if (!configured.nothingToDraw()) drawPass(configured, offset);
}

/** A layer carries its own settings, taking the foreground's colour
 *  only where it states none. */
template <typename DrawPass>
void drawLayer(const PaintLayer& layer, const SkPaint& foreground,
               const SkRect& bounds, DrawPass&& drawPass) {
  const SkPaint own = layer.resolvedPaint(foreground);
  drawMaterialPaint(own, layer.material.get(), bounds,
                    geometry::path::toSk(layer.offset),
                    std::forward<DrawPass>(drawPass));
}

enum class PaintBand { Underlay, Foreground, Overlay };
constexpr PaintBand kPaintBands[] = {PaintBand::Underlay, PaintBand::Foreground,
                                     PaintBand::Overlay};

template <typename DrawPass>
void drawPaintBand(const PaintStyle& style, const SkRect& bounds,
                   PaintBand band, DrawPass&& drawPass) {
  if (band == PaintBand::Foreground) {
    drawMaterialPaint(style.foreground, style.foregroundMaterial.get(), bounds,
                      SkVector{0, 0}, drawPass);
    return;
  }
  const auto& layers =
      band == PaintBand::Underlay ? style.underlays : style.overlays;
  for (const PaintLayer& layer : layers)
    drawLayer(layer, style.foreground, bounds, drawPass);
}

bool anyMaterial(const PaintStyle& style) {
  if (style.foregroundMaterial) return true;
  for (const PaintLayer& layer : style.underlays)
    if (layer.material) return true;
  for (const PaintLayer& layer : style.overlays)
    if (layer.material) return true;
  return false;
}

/** Skia measures the scaled glyph ink, including bearings and overhangs;
 *  origins and font-wide metrics alone cannot bound an individual glyph. */
SkRect glyphBounds(const SkFont& font, SkSpan<const SkGlyphID> glyphs,
                   SkSpan<const SkPoint> positions) {
  static thread_local std::vector<SkRect> ink;
  ink.resize(glyphs.size());
  font.getBounds(glyphs, {ink.data(), ink.size()}, nullptr);
  SkRect bounds = SkRect::MakeEmpty();
  for (size_t index = 0; index < ink.size(); ++index)
    bounds.join(ink[index].makeOffset(positions[index]));
  return bounds;
}

/** Straight runs use their glyph ink rather than the blob's conservative
 *  culling box. Transformed or unshaped blobs supply their own bounds. */
SkRect runBounds(const PositionedRun& run) {
  if (run.transformed || !run.shaped)
    return run.blob->bounds().makeOffset(run.origin.x, run.origin.y);
  const ShapedWord& word = *run.shaped;
  const SkFont font = makeFont(word.typeface, word.fontSize,
                               word.scaleX * run.fit.glyphScale, word.aliased);
  static thread_local std::vector<SkPoint> positions;
  positions.resize(word.glyphs.size());
  uint32_t clustersBefore = 0;
  for (size_t index = 0; index < word.glyphs.size(); ++index) {
    positions[index] = geometry::path::toSk(run.origin) +
                       SkVector{run.fit.offsetOf(word, index, clustersBefore),
                                word.positions[index].y};
    if (GlyphFit::endsCluster(word, index)) ++clustersBefore;
  }
  return glyphBounds(font, {word.glyphs.data(), word.glyphs.size()},
                     {positions.data(), positions.size()});
}

/** An entry is a bucket's index or a blob fallback. Both enter the draw
 *  order when first encountered, and every band visits the same order. */
struct PaintEntry {
  size_t bucket = 0;
  const PositionedRun* run = nullptr;
  const PaintStyle* style = nullptr;
  SkRect bounds = SkRect::MakeEmpty();
};

void drawBlobBand(SkCanvas* canvas, const PaintEntry& entry, PaintBand band) {
  const PositionedRun& run = *entry.run;
  drawPaintBand(*entry.style, entry.bounds, band,
                [&](const SkPaint& paint, SkVector offset) {
                  canvas->drawTextBlob(run.blob.get(),
                                       run.origin.x + offset.x(),
                                       run.origin.y + offset.y(), paint);
                });
}

/** THE NESTING OF DRAWS ON ONE THREAD. A material resolver runs inside a
 *  draw's band loop and may itself draw a paragraph on the same thread;
 *  the thread's scratch then still holds the outer draw's buckets. Only
 *  the outermost draw borrows that scratch, and a nested one keeps vectors
 *  of its own for as long as it runs. */
class DrawNesting {
 public:
  DrawNesting() : m_outermost(depth()++ == 0) {}
  ~DrawNesting() { --depth(); }
  DrawNesting(const DrawNesting&) = delete;
  DrawNesting& operator=(const DrawNesting&) = delete;

  /** The thread's scratch for the outermost draw, else this draw's own. */
  template <typename Value>
  std::vector<Value>& scratch(std::vector<Value>& shared,
                              std::vector<Value>& own) const {
    return m_outermost ? shared : own;
  }

 private:
  static int& depth() {
    static thread_local int value = 0;
    return value;
  }
  const bool m_outermost;
};

}  // namespace

void ParagraphLayout::draw(SkCanvas* canvas, const Paragraph& paragraph,
                           const PaintStyle* overridePaint) const {
  const std::vector<StyleSpan>& spans = paragraph.spans();
  const auto drawRect = [&](SkRect rect, const SkPaint& paint) {
    canvas->drawRect(rect, paint);
  };

  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kBelowGlyphs, drawRect);
  const DrawNesting nesting;
  static thread_local std::vector<PaintEntry> sharedEntries;
  std::vector<PaintEntry> ownEntries;
  std::vector<PaintEntry>& entries = nesting.scratch(sharedEntries, ownEntries);
  entries.clear();
  const SkScopeExit releaseEntries([&] { entries.clear(); });
  for (const PositionedRun& run : runs) {
    if (!run.blob) continue;
    const PaintStyle& style = resolvePaint(spans, run, overridePaint);

    const SkRect bounds =
        anyMaterial(style) ? runBounds(run) : SkRect::MakeEmpty();
    entries.push_back({0, &run, &style, bounds});
  }
  for (PaintBand band : kPaintBands)
    for (const PaintEntry& entry : entries) drawBlobBand(canvas, entry, band);
  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kAboveGlyphs, drawRect);
}

void ParagraphLayout::drawBatched(SkCanvas* canvas, const Paragraph& paragraph,
                                  const PaintStyle* overridePaint,
                                  const LiveVariations* liveVariations) const {
  const std::vector<StyleSpan>& spans = paragraph.spans();

  // Buckets keyed by (typeface, font size, resolved paint). A frame's worth of
  // horizontal runs collapses into one drawGlyphs call per bucket and layer;
  // Vector capacity persists across frames; fonts and paints belong only
  // to the active draw.
  struct Bucket {
    sk_sp<SkTypeface> typeface;
    float fontSize = 0;
    float scaleX = 1.0f;
    bool aliased = false;
    PaintStyle style;
    SkFont font;
    SkRect materialBounds;
    std::vector<SkGlyphID> glyphs;
    std::vector<SkPoint> positions;
  };
  const DrawNesting nesting;
  static thread_local std::vector<Bucket> sharedBuckets;
  std::vector<Bucket> ownBuckets;
  std::vector<Bucket>& buckets = nesting.scratch(sharedBuckets, ownBuckets);
  size_t activeBucketCount = 0;
  static thread_local std::vector<PaintEntry> sharedEntries;
  std::vector<PaintEntry> ownEntries;
  std::vector<PaintEntry>& entries = nesting.scratch(sharedEntries, ownEntries);
  entries.clear();

  // Decoration rects accumulate during the run walk and flush after the
  // glyph buckets, so strikethroughs/overlines land above the batched text.
  // Paints are copied by value: the resolved band paint lives on the
  // emitter's stack.
  struct DecorationRect {
    SkRect rect;
    SkPaint paint;
  };
  static thread_local std::vector<DecorationRect> sharedDecorationRects;
  std::vector<DecorationRect> ownDecorationRects;
  std::vector<DecorationRect>& decorationRects =
      nesting.scratch(sharedDecorationRects, ownDecorationRects);
  decorationRects.clear();
  const SkScopeExit releaseScratch([&] {
    entries.clear();
    decorationRects.clear();
    buckets.resize(activeBucketCount);
    for (Bucket& bucket : buckets) {
      bucket.typeface.reset();
      bucket.font.setTypeface(nullptr);
      bucket.style.foreground.reset();
      bucket.style.foregroundMaterial.reset();
      bucket.style.underlays.clear();
      bucket.style.overlays.clear();
      bucket.style.decorations.clear();
    }
  });

  // Highlights go straight to the canvas: every glyph pass draws after
  // them. The above-glyph decorations accumulate and flush past the
  // buckets so strikethroughs land above the batched text.
  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kBelowGlyphs,
                        [&](SkRect rect, const SkPaint& paint) {
                          canvas->drawRect(rect, paint);
                        });
  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kAboveGlyphs,
                        [&](SkRect rect, const SkPaint& paint) {
                          decorationRects.push_back({rect, paint});
                        });

  for (const PositionedRun& run : runs) {
    if (!run.blob) continue;
    const PaintStyle& style = resolvePaint(spans, run, overridePaint);

    if (run.transformed || !run.shaped) {
      const SkRect bounds =
          anyMaterial(style) ? runBounds(run) : SkRect::MakeEmpty();
      entries.push_back({0, &run, &style, bounds});
      continue;
    }

    const ShapedWord& shapedWord = *run.shaped;
    const float scaleX = shapedWord.scaleX * run.fit.glyphScale;
    Bucket* bucket = nullptr;
    for (Bucket& candidate :
         std::span<Bucket>(buckets.data(), activeBucketCount))
      if (candidate.typeface.get() == shapedWord.typeface.identity() &&
          candidate.fontSize == shapedWord.fontSize &&
          candidate.scaleX == scaleX &&
          candidate.aliased == shapedWord.aliased && candidate.style == style) {
        bucket = &candidate;
        break;
      }
    if (!bucket) {
      if (activeBucketCount == buckets.size()) buckets.push_back({});
      bucket = &buckets[activeBucketCount++];
      bucket->typeface = shapedWord.typeface;
      bucket->fontSize = shapedWord.fontSize;
      bucket->scaleX = scaleX;
      bucket->aliased = shapedWord.aliased;
      bucket->style = style;
      bucket->glyphs.clear();
      bucket->positions.clear();
      entries.push_back({activeBucketCount - 1});
    }
    uint32_t clustersBefore = 0;
    for (size_t glyphIndex = 0; glyphIndex < shapedWord.glyphs.size();
         ++glyphIndex) {
      bucket->glyphs.push_back(shapedWord.glyphs[glyphIndex]);
      // The line's fit changes both the glyph origins and their horizontal
      // scale. Shaping positions alone describe the unfitted word.
      bucket->positions.push_back(
          geometry::path::toSk(run.origin) +
          SkVector{run.fit.offsetOf(shapedWord, glyphIndex, clustersBefore),
                   shapedWord.positions[glyphIndex].y});
      if (GlyphFit::endsCluster(shapedWord, glyphIndex)) ++clustersBefore;
    }
  }

  for (Bucket& bucket : std::span<Bucket>(buckets.data(), activeBucketCount)) {
    if (bucket.glyphs.empty()) continue;
    sk_sp<SkTypeface> typeface = bucket.typeface;
    if (liveVariations && liveVariations->fonts &&
        !liveVariations->variations.empty())
      typeface = liveVariations->fonts->variedTypeface(
          typeface, liveVariations->variations);
    bucket.font =
        makeFont(typeface, bucket.fontSize, bucket.scaleX, bucket.aliased);
    const SkSpan<const SkGlyphID> glyphs(bucket.glyphs.data(),
                                         bucket.glyphs.size());
    const SkSpan<const SkPoint> positions(bucket.positions.data(),
                                          bucket.positions.size());
    bucket.materialBounds = anyMaterial(bucket.style)
                                ? glyphBounds(bucket.font, glyphs, positions)
                                : SkRect::MakeEmpty();
  }

  for (PaintBand band : kPaintBands)
    for (const PaintEntry& entry : entries) {
      if (entry.run) {
        drawBlobBand(canvas, entry, band);
        continue;
      }
      const Bucket& bucket = buckets[entry.bucket];
      if (bucket.glyphs.empty()) continue;
      drawPaintBand(bucket.style, bucket.materialBounds, band,
                    [&](const SkPaint& paint, SkVector offset) {
                      canvas->drawGlyphs(
                          {bucket.glyphs.data(), bucket.glyphs.size()},
                          {bucket.positions.data(), bucket.positions.size()},
                          {offset.x(), offset.y()}, bucket.font, paint);
                    });
    }

  for (const DecorationRect& decorationRect : decorationRects)
    canvas->drawRect(decorationRect.rect, decorationRect.paint);
}

void ParagraphLayout::drawBatched(SkCanvas* canvas, const Paragraph& paragraph,
                                  const GlyphStyles& glyphStyles,
                                  const PaintStyle* overridePaint) const {
  const std::vector<StyleSpan>& spans = paragraph.spans();
  constexpr uint32_t kSpanStyle = ~0u;
  const auto styleAt = [&](uint32_t ordinal) {
    if (ordinal >= glyphStyles.styleOfGlyph.size()) return kSpanStyle;
    const uint32_t named = glyphStyles.styleOfGlyph[ordinal];
    return named < glyphStyles.styles.size() ? named : kSpanStyle;
  };

  // A bucket BORROWS its style: every style this draw reads outlives it,
  // the caller's list and the paragraph's spans alike, so a style per word
  // costs no copy. `named` is the entry of the caller's list, or the span
  // marker for a glyph that draws with its span's paint.
  struct Bucket {
    const ShapedWord* font = nullptr;
    float scaleX = 1.0f;
    const PaintStyle* style = nullptr;
    uint32_t named = kSpanStyle;
    std::vector<SkGlyphID> glyphs;
    std::vector<SkPoint> positions;
    std::optional<SkRect> materialBounds;
  };
  const DrawNesting nesting;
  static thread_local std::vector<Bucket> sharedBuckets;
  std::vector<Bucket> ownBuckets;
  std::vector<Bucket>& buckets = nesting.scratch(sharedBuckets, ownBuckets);
  size_t activeBucketCount = 0;
  static thread_local std::vector<PaintEntry> sharedEntries;
  std::vector<PaintEntry> ownEntries;
  std::vector<PaintEntry>& entries = nesting.scratch(sharedEntries, ownEntries);
  entries.clear();
  // The caller's styles arrive in increasing order along the walk when each
  // names a unit, so a style past the highest one seen is a new bucket
  // without a search — which keeps a style per glyph linear in the glyphs.
  uint32_t highestNamed = 0;
  bool anyNamed = false;
  const auto bucketFor = [&](const ShapedWord& word, float scaleX,
                             const PaintStyle& style, uint32_t named) {
    const bool unseen =
        named != kSpanStyle && (!anyNamed || named > highestNamed);
    if (!unseen)
      for (size_t index = activeBucketCount; index-- > 0;) {
        Bucket& candidate = buckets[index];
        if (candidate.style == &style && candidate.named == named &&
            candidate.font->typeface.identity() == word.typeface.identity() &&
            candidate.font->fontSize == word.fontSize &&
            candidate.font->aliased == word.aliased &&
            candidate.scaleX == scaleX)
          return &candidate;
      }
    if (named != kSpanStyle) {
      highestNamed = anyNamed ? std::max(highestNamed, named) : named;
      anyNamed = true;
    }
    if (activeBucketCount == buckets.size()) buckets.push_back({});
    Bucket* bucket = &buckets[activeBucketCount++];
    bucket->font = &word;
    bucket->scaleX = scaleX;
    bucket->style = &style;
    bucket->named = named;
    bucket->glyphs.clear();
    bucket->positions.clear();
    bucket->materialBounds.reset();
    entries.push_back({activeBucketCount - 1});
    return bucket;
  };

  struct DecorationRect {
    SkRect rect;
    SkPaint paint;
  };
  static thread_local std::vector<DecorationRect> sharedDecorationRects;
  std::vector<DecorationRect> ownDecorationRects;
  std::vector<DecorationRect>& decorationRects =
      nesting.scratch(sharedDecorationRects, ownDecorationRects);
  decorationRects.clear();
  const SkScopeExit releaseScratch([&] {
    entries.clear();
    decorationRects.clear();
    buckets.resize(activeBucketCount);
    for (Bucket& bucket : buckets) {
      bucket.font = nullptr;
      bucket.style = nullptr;
    }
  });
  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kBelowGlyphs,
                        [&](SkRect rect, const SkPaint& paint) {
                          canvas->drawRect(rect, paint);
                        });
  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kAboveGlyphs,
                        [&](SkRect rect, const SkPaint& paint) {
                          decorationRects.push_back({rect, paint});
                        });

  // Turned runs keep their blob placement and join the same paint bands.
  uint32_t ordinal = 0;
  for (const PositionedRun& run : runs) {
    const uint32_t first = ordinal;
    if (run.shaped) ordinal += (uint32_t)run.shaped->glyphs.size();
    if (!run.blob) continue;
    const PaintStyle& spanStyle = resolvePaint(spans, run, overridePaint);
    if (run.transformed || !run.shaped) {
      const uint32_t named = run.shaped ? styleAt(first) : kSpanStyle;
      const PaintStyle& style =
          named == kSpanStyle ? spanStyle : glyphStyles.styles[named];
      const SkRect bounds =
          anyMaterial(style) ? runBounds(run) : SkRect::MakeEmpty();
      entries.push_back({0, &run, &style, bounds});
      continue;
    }
    const ShapedWord& word = *run.shaped;
    const float scaleX = word.scaleX * run.fit.glyphScale;
    Bucket* bucket = nullptr;
    uint32_t bucketNamed = kSpanStyle;
    uint32_t clustersBefore = 0;
    for (size_t glyphIndex = 0; glyphIndex < word.glyphs.size(); ++glyphIndex) {
      const uint32_t named = styleAt(first + (uint32_t)glyphIndex);
      if (!bucket || named != bucketNamed) {
        bucket = bucketFor(
            word, scaleX,
            named == kSpanStyle ? spanStyle : glyphStyles.styles[named], named);
        bucketNamed = named;
      }
      bucket->glyphs.push_back(word.glyphs[glyphIndex]);
      bucket->positions.push_back(
          geometry::path::toSk(run.origin) +
          SkVector{run.fit.offsetOf(word, glyphIndex, clustersBefore),
                   word.positions[glyphIndex].y});
      if (GlyphFit::endsCluster(word, glyphIndex)) ++clustersBefore;
    }
  }

  for (PaintBand band : kPaintBands)
    for (const PaintEntry& entry : entries) {
      if (entry.run) {
        drawBlobBand(canvas, entry, band);
        continue;
      }
      Bucket& bucket = buckets[entry.bucket];
      if (bucket.glyphs.empty()) continue;
      const SkFont font = makeFont(bucket.font->typeface, bucket.font->fontSize,
                                   bucket.scaleX, bucket.font->aliased);
      const SkSpan<const SkGlyphID> glyphs(bucket.glyphs.data(),
                                           bucket.glyphs.size());
      const SkSpan<const SkPoint> positions(bucket.positions.data(),
                                            bucket.positions.size());
      if (!bucket.materialBounds) {
        bucket.materialBounds = anyMaterial(*bucket.style)
                                    ? glyphBounds(font, glyphs, positions)
                                    : SkRect::MakeEmpty();
      }
      const auto drawPass = [&](const SkPaint& paint, SkVector offset) {
        canvas->drawGlyphs(glyphs, positions, {offset.x(), offset.y()}, font,
                           paint);
      };
      drawPaintBand(*bucket.style, *bucket.materialBounds, band, drawPass);
    }

  for (const DecorationRect& decorationRect : decorationRects)
    canvas->drawRect(decorationRect.rect, decorationRect.paint);
}

}  // namespace sigil::weave
