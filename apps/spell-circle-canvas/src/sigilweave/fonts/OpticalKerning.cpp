/** @file
 * A glyph's edges, measured off its outline by rasterising it once, and the
 * distance two glyphs set adjacent leave between them.
 */

#include "OpticalKerning.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace sigil::weave::detail {

namespace {

// The em the profile is measured at. Edges are kept in ems, so one profile
// serves every size the glyph is ever set at; this is only the resolution
// the outline is sampled with, and it is fixed so the answer for a glyph
// never depends on the size a caller happened to ask about first.
constexpr float kProfileEm = 96.0f;

}  // namespace

GlyphProfile measureProfile(const SkTypeface& typeface, uint16_t glyph) {
  GlyphProfile profile;
  profile.left.fill(GlyphProfile::kNoInk);
  profile.right.fill(GlyphProfile::kNoInk);

  SkFont font(sk_ref_sp(const_cast<SkTypeface*>(&typeface)), kProfileEm);
  font.setHinting(SkFontHinting::kNone);
  font.setLinearMetrics(true);
  float advance = 0;
  const SkGlyphID glyphId = glyph;
  font.getWidths(SkSpan<const SkGlyphID>(&glyphId, 1),
                 SkSpan<SkScalar>(&advance, 1));
  profile.advance = advance / kProfileEm;

  const std::optional<SkPath> outlineOrNone = font.getPath(glyph);
  if (!outlineOrNone || outlineOrNone->isEmpty()) return profile;
  const SkPath& outline = *outlineOrNone;
  const SkRect ink = outline.getBounds();
  if (ink.isEmpty()) return profile;
  profile.top = -ink.top() / kProfileEm;
  profile.bottom = -ink.bottom() / kProfileEm;

  // The outline is rasterised once into a one-channel mask and the mask is
  // scanned band by band. A path can be measured for its extremes without
  // a raster, but only by flattening every curve and tracking crossings —
  // the raster IS that flattening, done by the same code that will draw
  // the glyph, so the edges measured here are the edges the reader sees.
  const int width = std::max(1, static_cast<int>(std::ceil(ink.width())) + 2);
  const int height = std::max(1, static_cast<int>(std::ceil(ink.height())) + 2);
  SkBitmap mask;
  if (!mask.tryAllocPixels(SkImageInfo::MakeA8(width, height))) return profile;
  mask.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(mask);
  canvas.translate(-ink.left() + 1.0f, -ink.top() + 1.0f);
  SkPaint paint;
  paint.setAntiAlias(true);
  canvas.drawPath(outline, paint);

  // A band is a slice of the ink height; the profile is the first and last
  // column in it that the glyph actually covers. Half coverage is the
  // threshold: an antialiased edge fades over about a pixel, and taking
  // the faintest of it as ink would measure the blur rather than the
  // letter.
  constexpr uint8_t kInkThreshold = 128;
  for (int band = 0; band < kProfileBands; ++band) {
    const int first = band * height / kProfileBands;
    const int last = std::max(first + 1, (band + 1) * height / kProfileBands);
    int leftMost = width;
    int rightMost = -1;
    for (int row = first; row < last && row < height; ++row) {
      const uint8_t* pixels = mask.getAddr8(0, row);
      for (int column = 0; column < width; ++column)
        if (pixels[column] >= kInkThreshold) {
          leftMost = std::min(leftMost, column);
          rightMost = std::max(rightMost, column);
        }
    }
    if (rightMost < 0) continue;
    // Back into the glyph's own space: the mask started one pixel left of
    // the ink box, which itself sits at ink.left() from the pen.
    const float originX = ink.left() - 1.0f;
    profile.left[static_cast<size_t>(band)] =
        (originX + static_cast<float>(leftMost)) / kProfileEm;
    profile.right[static_cast<size_t>(band)] =
        (originX + static_cast<float>(rightMost) + 1.0f) / kProfileEm;
    profile.inked = true;
  }
  return profile;
}

float gapBetween(const GlyphProfile& left, const GlyphProfile& right) {
  if (!left.inked || !right.inked) return GlyphProfile::kNoInk;
  // The two profiles are indexed by their OWN ink heights, so a band of one
  // is not a band of the other. They are compared where they overlap on the
  // baseline: for each band of the left glyph, the band of the right glyph
  // that stands at the same height.
  const float leftHeight = left.top - left.bottom;
  const float rightHeight = right.top - right.bottom;
  if (leftHeight <= 0 || rightHeight <= 0) return GlyphProfile::kNoInk;
  float closest = GlyphProfile::kNoInk;
  for (int band = 0; band < kProfileBands; ++band) {
    if (left.right[static_cast<size_t>(band)] == GlyphProfile::kNoInk) continue;
    // The height this band's middle stands at, and the right glyph's band
    // holding it.
    const float height =
        left.top - leftHeight * (static_cast<float>(band) + 0.5f) /
                       static_cast<float>(kProfileBands);
    const float position = (right.top - height) / rightHeight;
    if (position < 0 || position >= 1) continue;
    const auto other = static_cast<size_t>(
        position * static_cast<float>(kProfileBands));
    if (right.left[other] == GlyphProfile::kNoInk) continue;
    const float gap = (left.advance - left.right[static_cast<size_t>(band)]) +
                      right.left[other];
    closest = std::min(closest, gap);
  }
  return closest;
}

}  // namespace sigil::weave::detail
