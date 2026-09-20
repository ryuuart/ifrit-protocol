#pragma once

/** @file
 * WHAT A LAYER LOOKS LIKE AFTER AN EFFECT HAS RUN OVER IT, for a case
 * that has to read it back.
 *
 * A post-processing effect is a value until something paints a layer
 * through it, so every case that asks what one COMPUTES paints the same
 * source — a black field with one bright square in it — as one layer
 * with the effect on it, and then reads texels. The surface is F32 on
 * purpose: a bloom adds light, so a sum above one has to survive the
 * read-back or the arithmetic under test is clipped away before the
 * case sees it.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSurface.h>

#include <cstddef>
#include <vector>

namespace sigil::material::test {

/** A source for a bloom: a 64x64 black field with a bright 24x24 square
 *  of @p color in the middle, painted through @p filter as one layer
 *  onto an F32 surface so a sum above one survives the read-back. */
inline std::vector<float> bloomThrough(const sk_sp<SkImageFilter>& filter,
                                       SkColor4f color) {
  const SkImageInfo info =
      SkImageInfo::Make(64, 64, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorBLACK);
  SkPaint layer;
  layer.setImageFilter(filter);
  canvas.saveLayer(nullptr, &layer);
  canvas.clear(SK_ColorBLACK);
  SkPaint ink;
  ink.setColor(color);
  canvas.drawRect(SkRect::MakeXYWH(20, 20, 24, 24), ink);
  canvas.restore();
  std::vector<float> px((size_t)64 * 64 * 4);
  EXPECT_TRUE(surface->readPixels(SkPixmap(info, px.data(), 64 * 4 * 4), 0, 0));
  return px;
}

/** The four channels at @p x, @p y of a field that reading returned. */
inline const float* texel(const std::vector<float>& px, int x, int y) {
  return px.data() + ((size_t)y * 64 + x) * 4;
}

}  // namespace sigil::material::test
