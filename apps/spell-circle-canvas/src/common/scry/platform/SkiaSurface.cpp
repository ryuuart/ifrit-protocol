/** @file
 * The surface's reallocation: premultiplied BGRA in sRGB with 16-byte
 * row alignment, cleared to transparent.
 */

#include "SkiaSurface.h"

#include <include/core/SkColorSpace.h>
#include <include/core/SkImageInfo.h>

namespace sigil::scry {

void SkiaSurface::Resize(uint32_t width, uint32_t height) {
  if (m_bitmap.width() == static_cast<int>(width) &&
      m_bitmap.height() == static_cast<int>(height))
    return;

  SkImageInfo info = SkImageInfo::Make(
      static_cast<int>(width), static_cast<int>(height), kBGRA_8888_SkColorType,
      kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
  // 16-byte row alignment keeps Ultralight's SIMD paint paths on their
  // fast lane (same intent as Config::bitmap_alignment for the default
  // BitmapSurface).
  size_t rowBytes = (info.minRowBytes() + 15) & ~static_cast<size_t>(15);
  m_bitmap.allocPixels(info, rowBytes);
  m_bitmap.eraseColor(SK_ColorTRANSPARENT);
}

}  // namespace sigil::scry
