/** @file
 * The billboard rasterizer: every point drawn as a camera-facing
 * sprite on an ordinary canvas, sized by its lane and by perspective,
 * painted back to front.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkPaint.h>

#include <algorithm>
#include <cmath>

#include "sigilgeometry/pop/Points.h"

namespace sigil::geometry::points {

void drawBillboards(SkCanvas& canvas, const Cloud& cloud,
                    const space::Camera& camera, SkSize viewport,
                    const BillboardStyle& style) {
  const size_t n = cloud.size();
  if (n == 0) return;
  const glm::mat4 vp = camera.viewProjection(viewport);
  const glm::mat4 view = camera.view();

  struct Splat {
    SkPoint screen;
    float px;  // half-size in pixels
    float depth;
    glm::vec4 tint;
  };
  std::vector<Splat> splats;
  splats.reserve(n);

  const std::vector<float>* sizeLane =
      style.sizeLane.empty() ? nullptr : cloud.scalarIf(style.sizeLane);
  const std::vector<glm::vec4>* tintLane =
      style.tintLane.empty() ? nullptr : cloud.colorIf(style.tintLane);

  // Pixels per world unit at distance d: focal / d * (h/2).
  const float focal = 1.0f / std::tan(camera.fovYDeg * (float)M_PI / 360.0f);
  const float halfH = viewport.height() * 0.5f;

  for (size_t i = 0; i < n; ++i) {
    const glm::vec3& p = cloud.positions[i];
    const glm::vec4 clip = vp * glm::vec4{p, 1};
    if (clip.w <= 1e-4f) continue;
    const glm::vec4 eye = view * glm::vec4{p, 1};
    Splat splat;
    splat.screen = {clip.x / clip.w, clip.y / clip.w};
    splat.depth = eye.z;
    const float size =
        style.size * (sizeLane && i < sizeLane->size() ? (*sizeLane)[i] : 1.0f);
    splat.px = style.perspective ? std::max(0.25f, size * 0.5f * focal * halfH /
                                                       std::max(-eye.z, 1e-3f))
                                 : size * 0.5f;
    glm::vec4 tint = style.tint;
    if (tintLane && i < tintLane->size()) tint *= (*tintLane)[i];
    splat.tint = tint;
    splats.push_back(splat);
  }
  if (style.depthSort)
    std::sort(splats.begin(), splats.end(), [](const Splat& a, const Splat& b) {
      return a.depth < b.depth;  // far first
    });

  // The default sprite: a CPU-baked soft white dot (quadratic falloff).
  static const sk_sp<SkImage> softDot = [] {
    constexpr int kSize = 64;
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(kSize, kSize));
    for (int y = 0; y < kSize; ++y) {
      uint32_t* row = bm.getAddr32(0, y);
      for (int x = 0; x < kSize; ++x) {
        const float dx = ((float)x + 0.5f) / kSize * 2 - 1;
        const float dy = ((float)y + 0.5f) / kSize * 2 - 1;
        const float r = std::sqrt(dx * dx + dy * dy);
        const float a = std::clamp(1.0f - r, 0.0f, 1.0f);  // linear edge...
        const float soft = a * a * (3 - 2 * a);            // ...smoothstepped
        const uint32_t v = (uint32_t)std::lround(soft * 255.0f);
        row[x] = (v << SK_A32_SHIFT) | (v << SK_R32_SHIFT) |
                 (v << SK_G32_SHIFT) | (v << SK_B32_SHIFT);  // premul white
      }
    }
    bm.setImmutable();
    return bm.asImage();
  }();

  SkPaint paint;
  paint.setAntiAlias(true);
  if (style.additive) paint.setBlendMode(SkBlendMode::kPlus);
  const SkSamplingOptions sampling(SkFilterMode::kLinear,
                                   SkMipmapMode::kLinear);
  const sk_sp<SkImage>& sprite = style.sprite ? style.sprite : softDot;

  for (const Splat& splat : splats) {
    // Full-color tint via modulate — works for any sprite (a white
    // sprite tints exactly).
    const SkColor4f tintColor = {splat.tint.r, splat.tint.g, splat.tint.b,
                                 splat.tint.a};
    paint.setColorFilter(
        SkColorFilters::Blend(tintColor.toSkColor(), SkBlendMode::kModulate));
    const SkRect dst =
        SkRect::MakeXYWH(splat.screen.fX - splat.px, splat.screen.fY - splat.px,
                         splat.px * 2, splat.px * 2);
    canvas.drawImageRect(sprite, dst, sampling, &paint);
  }
}

}  // namespace sigil::geometry::points
