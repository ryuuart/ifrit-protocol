/** @file
 * The choreography bodies that are not templates: the rotation snaps and
 * the memoized tint filter a dressed glyph reaches for, and the bucket
 * lookup, the per-pass dressing and the band-ordered draw of
 * GlyphRSXformBatches.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkFont.h>
#include <include/core/SkSpan.h>
#include <include/effects/SkColorMatrix.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <list>
#include <map>
#include <numbers>
#include <utility>

#include "sigilweave/choreograph/GlyphBatches.h"
#include "sigilweave/choreograph/GlyphDress.h"

namespace sigil::weave {

void quantizeAngle(float angle, float& cosine, float& sine) {
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

void quantizeAngle(float angle, int steps, float& cosine, float& sine) {
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

sk_sp<SkColorFilter> tintFilter(const SkColor4f& tint,
                                sk_sp<SkColorFilter> under,
                                const SkColor4f& add, const SkColor4f& screen) {
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

GlyphRSXformBatches::Batch& GlyphRSXformBatches::batchForPass(
    const ShapedWord* font, const sk_sp<SkTypeface>& face, const SkPaint& paint,
    SkVector offset, PassBand band) {
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

void GlyphRSXformBatches::addGlyph(const ShapedWord* font,
                                   const PaintStyle& style, SkGlyphID glyph,
                                   float halfAdvance, const GlyphDress& dress) {
  const float alpha = dress.alphaScale * dress.colorMul.fA;
  // Any of the three colour terms off neutral takes the modulated path;
  // all neutral leaves the source paint untouched, byte for byte.
  const bool tinted = dress.colorMul.fR != 1.0f || dress.colorMul.fG != 1.0f ||
                      dress.colorMul.fB != 1.0f || dress.colorAdd.fR != 0 ||
                      dress.colorAdd.fG != 0 || dress.colorAdd.fB != 0 ||
                      dress.colorScreen.fR != 0 || dress.colorScreen.fG != 0 ||
                      dress.colorScreen.fB != 0;
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

void GlyphRSXformBatches::clear() {
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

int GlyphRSXformBatches::draw(SkCanvas* canvas) const {
  int total = 0;
  for (const PassBand band :
       {PassBand::Underlay, PassBand::Foreground, PassBand::Overlay})
    for (const Batch& batch : batches) {
      if (batch.band != band) continue;
      total += drawBatch(canvas, batch, subpixel);
    }
  return total;
}

int GlyphRSXformBatches::drawBatch(SkCanvas* canvas, const Batch& batch,
                                   bool subpixel) {
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

}  // namespace sigil::weave
