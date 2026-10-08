/** @file
 * The bevel normal map, differentiated from an outline's blurred
 * coverage: the shape is drawn white, blurred by the shoulder width,
 * smoothstepped into a height field and differentiated into normals.
 */

#include "sigilmaterial/skia/Bevel.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <sigilmedia/advanced/Skia.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace sigil::material::skia {

namespace {

sk_sp<SkImage> bevelImage(const SkPath& path, SkIRect bounds, float bevelPx,
                          float heightScale) {
  const int w = std::max(bounds.width(), 1);
  const int h = std::max(bounds.height(), 1);

  // Coverage, blurred into a bevel ramp.
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  if (!surface) return nullptr;
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  SkPaint white;
  white.setAntiAlias(true);
  white.setColor(SK_ColorWHITE);
  if (bevelPx > 0.01f)
    white.setImageFilter(
        SkImageFilters::Blur(bevelPx * 0.5f, bevelPx * 0.5f, nullptr));
  canvas->save();
  canvas->translate(-(float)bounds.left(), -(float)bounds.top());
  canvas->drawPath(path, white);
  canvas->restore();

  SkBitmap ramp;
  ramp.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  if (!surface->readPixels(ramp.pixmap(), 0, 0)) return nullptr;

  // Height with a smoothstep shoulder, then central differences -> normals.
  std::vector<float> height((size_t)w * h);
  for (int y = 0; y < h; ++y) {
    const uint32_t* row = ramp.getAddr32(0, y);
    for (int x = 0; x < w; ++x) {
      const float c = (float)((row[x] >> SK_R32_SHIFT) & 0xffu) / 255.0f;
      height[(size_t)y * w + x] = c * c * (3.0f - 2.0f * c);
    }
  }

  SkBitmap out;
  out.allocPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  // RGBA is asked for, so RGBA is written: byte 0 red, byte 3 alpha. The
  // N32 shift macros describe the platform's own 32-bit order, which is
  // this one on every target this builds for and is not the same claim.
  const float steep = std::max(bevelPx, 1.0f) * heightScale;
  auto at = [&](int x, int y) {
    x = std::clamp(x, 0, w - 1);
    y = std::clamp(y, 0, h - 1);
    return height[(size_t)y * w + x];
  };
  for (int y = 0; y < h; ++y) {
    uint32_t* row = (uint32_t*)out.getAddr32(0, y);
    for (int x = 0; x < w; ++x) {
      const float dx = (at(x + 1, y) - at(x - 1, y)) * 0.5f * steep;
      const float dy = (at(x, y + 1) - at(x, y - 1)) * 0.5f * steep;
      float nx = -dx, ny = -dy, nz = 1.0f;
      const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
      nx /= len;
      ny /= len;
      nz /= len;
      const auto enc = [](float f) {
        return (uint32_t)std::clamp(
            (int)std::lround((f * 0.5f + 0.5f) * 255.0f), 0, 255);
      };
      row[x] = enc(nx) | (enc(ny) << 8) | (enc(nz) << 16) | (0xffu << 24);
    }
  }
  out.setImmutable();
  return out.asImage();
}

}  // namespace

Texture bevelNormals(const SkPath& path, SkIRect bounds, float bevelPx,
                     float heightScale) {
  return Texture(bevelImage(path, bounds, bevelPx, heightScale))
      .at({(float)bounds.left(), (float)bounds.top()});
}

Texture bevelNormals(const SkPath& path, float bevelPx, float heightScale) {
  SkRect b = path.computeTightBounds();
  b.outset(bevelPx + 2, bevelPx + 2);
  return bevelNormals(path, b.roundOut(), bevelPx, heightScale);
}

Texture reliefNormals(const SkPath& outline, const ReliefOptions& options,
                      float density, const glm::mat3& localToPaint) {
  // The scratch a relief rasterises — coverage, height and normals — is
  // bounded whatever the outline's or the canvas's size.
  constexpr double kMaxPixels = 16.0 * 1024 * 1024;
  if (!std::isfinite(options.shoulder) || options.shoulder <= 0 ||
      !std::isfinite(options.depth) || options.depth == 0 ||
      !std::isfinite(density) || density <= 0 || outline.isEmpty())
    return {};
  const float shoulder = options.shoulder * density;
  // The coverage differentiator squares its slopes when normalising.
  if (!std::isfinite(shoulder) ||
      std::abs(options.depth) > std::sqrt(std::numeric_limits<float>::max()) /
                                    std::max(shoulder, 1.0f))
    return {};
  const SkPath scaled =
      outline.makeTransform(SkMatrix::Scale(density, density));
  if (!scaled.isFinite()) return {};
  SkRect bounds = scaled.computeTightBounds();
  bounds.outset(shoulder + 2, shoulder + 2);
  constexpr double kIntLimit = std::numeric_limits<int>::max();
  const double width = std::ceil(bounds.right()) - std::floor(bounds.left());
  const double height = std::ceil(bounds.bottom()) - std::floor(bounds.top());
  if (!bounds.isFinite() || width <= 0 || height <= 0 ||
      width * height > kMaxPixels ||
      std::max({std::abs(double(bounds.left())), std::abs(double(bounds.top())),
                std::abs(double(bounds.right())),
                std::abs(double(bounds.bottom()))}) >= kIntLimit)
    return {};
  Texture normal =
      bevelNormals(scaled, bounds.roundOut(), shoulder, options.depth);
  glm::mat3 toLogical(1.0f);
  toLogical[0][0] = toLogical[1][1] = 1.0f / density;
  normal.uv(localToPaint * toLogical * normal.uv());
  return normal;
}

}  // namespace sigil::material::skia
