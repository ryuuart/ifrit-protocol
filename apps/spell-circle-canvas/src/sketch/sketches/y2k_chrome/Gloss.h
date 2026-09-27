#pragma once

// THE SATIN BAND that follows the shape: the shape's own blurred coverage
// remapped through a 256-entry ring table, tinted and clipped inside the
// shape. A plain gradient cannot fake it, because it follows the shape's
// distance field rather than a screen axis: on a blob it curves with the
// blob. It is one image-filter chain (blur, then an alpha table), so it
// composes with a node's other decorations inside a single paint rather
// than forcing a layer. Attach it as a foreground: it reads the node's
// outline and paints over the fill.
//
// The Flash-portfolio study includes this header from here.

#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkPaint.h>
#include <include/effects/SkImageFilters.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmaterial/color/Color.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace y2k {

/** The contour table: a smooth ring of coverage centred on @p center,
 *  @p width wide. */
inline std::array<uint8_t, 256> contourRing(float center = 0.55f,
                                            float width = 0.35f) {
  std::array<uint8_t, 256> table{};
  for (int i = 0; i < 256; ++i) {
    const float a = (float)i / 255.0f;
    const float d = std::abs(a - center) / std::max(0.05f, width * 0.5f);
    const float peak = std::max(0.0f, 1.0f - d);
    // The cubic ease, spelled here rather than reached for: this table is
    // data with no clock behind it.
    const float eased = peak * peak * (3.0f - 2.0f * peak);
    table[(size_t)i] = (uint8_t)std::lround(255.0f * eased);
  }
  return table;
}

struct GlossContour {
  sigil::material::Color color = {1, 1, 1, 0.85f};
  float sigma = 6.0f;
  SkVector offset = {0, -3};
  std::array<uint8_t, 256> table{};

  bool operator==(const GlossContour& o) const {
    return color == o.color && sigma == o.sigma && offset == o.offset &&
           table == o.table;
  }
  float bleed() const { return sigma * 3.0f; }

  void paint(SkCanvas& c, const sigil::compose::PaintContext& ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    // The ring table reads blurred COVERAGE, so the outline is drawn opaque
    // and the colour's alpha is applied after the table: scaling the
    // coverage first would move the ring.
    p.setColor4f({color.r, color.g, color.b, 1.0f}, nullptr);
    const float alphaScale[20] = {1, 0, 0, 0,       0,  //
                                  0, 1, 0, 0,       0,  //
                                  0, 0, 1, 0,       0,  //
                                  0, 0, 0, color.a, 0};
    p.setImageFilter(SkImageFilters::ColorFilter(
        SkColorFilters::Compose(
            SkColorFilters::Matrix(alphaScale),
            SkColorFilters::TableARGB(table.data(), nullptr, nullptr, nullptr)),
        SkImageFilters::Blur(sigma, sigma, nullptr)));
    c.save();
    c.clipPath(ctx.outline, true);  // satin lives INSIDE the shape
    c.translate(offset.fX, offset.fY);
    c.drawPath(ctx.outline, p);
    c.restore();
  }
};

/** The gloss band in @p color, blurred by @p sigma and shifted by
 *  @p offset, lit where the coverage crosses the ring. */
inline GlossContour gloss(sigil::material::Color color = {1, 1, 1, 0.85f},
                          float sigma = 6.0f, SkVector offset = {0, -3},
                          float ringCenter = 0.55f, float ringWidth = 0.35f) {
  GlossContour g;
  g.color = color;
  g.sigma = sigma;
  g.offset = offset;
  g.table = contourRing(ringCenter, ringWidth);
  return g;
}

}  // namespace y2k
