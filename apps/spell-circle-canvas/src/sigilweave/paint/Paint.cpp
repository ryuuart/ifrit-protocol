/** @file
 * The two draws of a finished layout: draw() puts one blob per run and
 * paint pass on the canvas, and drawBatched() merges horizontal runs into
 * one drawGlyphs call per (font, paint) bucket and pass, with the
 * decoration bands emitted beneath and above the glyphs in the order the
 * decoration walk defines. A pass carrying a material is shaded through
 * the registered resolver over the bounds of what it covers.
 */

#include "sigilweave/paint/Paint.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPaint.h>
#include <include/core/SkTextBlob.h>

#include <span>
#include <vector>

#include "sigilweave/decoration/DecorationRects.h"
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

/** The paint a layer draws with: its own, or — when it carries a material
 *  and a resolver is registered — a copy shading with the material over
 *  @p bounds. */
template <typename DrawPass>
void drawLayer(const PaintLayer& layer, const SkRect& bounds,
               DrawPass&& drawPass) {
  if (layer.material && paint::hasMaterialResolver()) {
    SkPaint shaded = layer.paint;
    shaded.setShader(paint::resolverSlot()(*layer.material, bounds));
    if (!shaded.nothingToDraw()) drawPass(shaded, layer.offset);
    return;
  }
  if (!layer.paint.nothingToDraw()) drawPass(layer.paint, layer.offset);
}

template <typename DrawPass>
void drawPaintLayers(const PaintStyle& style, const SkRect& bounds,
                     DrawPass&& drawPass) {
  for (const PaintLayer& layer : style.underlays)
    drawLayer(layer, bounds, drawPass);
  if (!style.foreground.nothingToDraw())
    drawPass(style.foreground, SkVector{0, 0});
  for (const PaintLayer& layer : style.overlays)
    drawLayer(layer, bounds, drawPass);
}

/** Where a run's glyphs land on the canvas: the blob's bounds at its
 *  origin. Computed only for a pass that asks, since every other pass
 *  never reads it. */
SkRect runBounds(const PositionedRun& run) {
  return run.blob->bounds().makeOffset(run.origin.x(), run.origin.y());
}

bool anyMaterial(const PaintStyle& style) {
  for (const PaintLayer& layer : style.underlays)
    if (layer.material) return true;
  for (const PaintLayer& layer : style.overlays)
    if (layer.material) return true;
  return false;
}

}  // namespace

void ParagraphLayout::draw(SkCanvas* canvas, const Paragraph& paragraph,
                           const PaintStyle* overridePaint) const {
  const std::vector<StyleSpan>& spans = paragraph.spans();
  const auto drawRect = [&](SkRect rect, const SkPaint& paint) {
    canvas->drawRect(rect, paint);
  };

  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kBelowGlyphs, drawRect);
  for (const PositionedRun& run : runs) {
    if (!run.blob) continue;
    const PaintStyle& style =
        resolvePaint(spans, run.styleIndex, overridePaint);

    const SkRect bounds =
        anyMaterial(style) ? runBounds(run) : SkRect::MakeEmpty();
    drawPaintLayers(style, bounds, [&](const SkPaint& paint, SkVector offset) {
      canvas->drawTextBlob(run.blob.get(), run.origin.x() + offset.x(),
                           run.origin.y() + offset.y(), paint);
    });
  }
  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kAboveGlyphs, drawRect);
}

void ParagraphLayout::drawBatched(SkCanvas* canvas, const Paragraph& paragraph,
                                  const PaintStyle* overridePaint,
                                  const LiveVariations* liveVariations) const {
  const std::vector<StyleSpan>& spans = paragraph.spans();

  // Buckets keyed by (typeface, font size, resolved paint). A frame's worth of
  // horizontal runs collapses into one drawGlyphs call per bucket and layer;
  // scratch storage persists across frames (styles copied by value — span
  // pointers would dangle between calls).
  struct Bucket {
    sk_sp<SkTypeface> typeface;
    float fontSize = 0;
    float scaleX = 1.0f;
    bool aliased = false;
    PaintStyle style;
    std::vector<SkGlyphID> glyphs;
    std::vector<SkPoint> positions;
  };
  static thread_local std::vector<Bucket> buckets;
  if (buckets.size() > 64)
    buckets.clear();  // release pathological one-frame style cardinality
  size_t activeBucketCount = 0;

  // Decoration rects accumulate during the run walk and flush after the
  // glyph buckets, so strikethroughs/overlines land above the batched text.
  // Paints are copied by value: the resolved band paint lives on the
  // emitter's stack.
  struct DecorationRect {
    SkRect rect;
    SkPaint paint;
  };
  static thread_local std::vector<DecorationRect> decorationRects;
  decorationRects.clear();

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
    const PaintStyle& style =
        resolvePaint(spans, run.styleIndex, overridePaint);

    if (run.transformed || !run.shaped) {
      // Positions are baked into the blob; draw every configured pass
      // directly. Arbitrary SkPaint effects remain available on this path.
      const SkRect bounds =
          anyMaterial(style) ? runBounds(run) : SkRect::MakeEmpty();
      drawPaintLayers(
          style, bounds, [&](const SkPaint& paint, SkVector offset) {
            canvas->drawTextBlob(run.blob.get(), run.origin.x() + offset.x(),
                                 run.origin.y() + offset.y(), paint);
          });
      continue;
    }

    const ShapedWord& shapedWord = *run.shaped;
    Bucket* bucket = nullptr;
    for (Bucket& candidate :
         std::span<Bucket>(buckets.data(), activeBucketCount))
      if (candidate.typeface.get() == shapedWord.typeface.get() &&
          candidate.fontSize == shapedWord.fontSize &&
          candidate.scaleX == shapedWord.scaleX &&
          candidate.aliased == shapedWord.aliased && candidate.style == style) {
        bucket = &candidate;
        break;
      }
    if (!bucket) {
      if (activeBucketCount == buckets.size()) buckets.push_back({});
      bucket = &buckets[activeBucketCount++];
      bucket->typeface = shapedWord.typeface;
      bucket->fontSize = shapedWord.fontSize;
      bucket->scaleX = shapedWord.scaleX;
      bucket->aliased = shapedWord.aliased;
      bucket->style = style;
      bucket->glyphs.clear();
      bucket->positions.clear();
    }
    for (size_t glyphIndex = 0; glyphIndex < shapedWord.glyphs.size();
         ++glyphIndex) {
      bucket->glyphs.push_back(shapedWord.glyphs[glyphIndex]);
      bucket->positions.push_back(run.origin +
                                  shapedWord.positions[glyphIndex]);
    }
  }

  for (const Bucket& bucket :
       std::span<const Bucket>(buckets.data(), activeBucketCount)) {
    if (bucket.glyphs.empty()) continue;
    sk_sp<SkTypeface> typeface = bucket.typeface;
    if (liveVariations && liveVariations->fonts &&
        !liveVariations->variations.empty())
      typeface = liveVariations->fonts->variedTypeface(
          typeface, liveVariations->variations);
    const SkFont font =
        makeFont(typeface, bucket.fontSize, bucket.scaleX, bucket.aliased);
    const SkSpan<const SkGlyphID> glyphs(bucket.glyphs.data(),
                                         bucket.glyphs.size());
    const SkSpan<const SkPoint> positions(bucket.positions.data(),
                                          bucket.positions.size());
    // A material pass shades over the bucket's glyph extent: the bounds of
    // its positions grown by the font's ascent and descent.
    SkRect bounds = SkRect::MakeEmpty();
    if (anyMaterial(bucket.style)) {
      bounds.setBounds({bucket.positions.data(), bucket.positions.size()});
      SkFontMetrics metrics;
      font.getMetrics(&metrics);
      bounds.fTop += metrics.fAscent;
      bounds.fBottom += metrics.fDescent;
    }
    drawPaintLayers(bucket.style, bounds,
                    [&](const SkPaint& paint, SkVector offset) {
                      canvas->drawGlyphs(glyphs, positions,
                                         {offset.x(), offset.y()}, font, paint);
                    });
  }

  for (const DecorationRect& decorationRect : decorationRects)
    canvas->drawRect(decorationRect.rect, decorationRect.paint);
  decorationRects.clear();  // paints hold shader refs; don't pin past the frame
}

}  // namespace sigil::weave
