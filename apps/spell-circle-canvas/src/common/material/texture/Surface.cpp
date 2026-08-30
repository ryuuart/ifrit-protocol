/** @file
 * The environment a surface reflects — the procedural studio and sunset
 * bakes, a loaded equirectangular panorama, the roughness blurs cached
 * per bucket — and the bevel normal map differentiated from an outline's
 * blurred coverage.
 */

#include "sigilmaterial/texture/Surface.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkM44.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace sigil::material {

namespace {

sk_sp<SkImage> bakeEquirect(int width,
                            const std::function<SkV3(float u, float v)>& fn) {
  const int height = std::max(width / 2, 8);
  std::vector<float> pixels((size_t)width * height * 4);
  for (int y = 0; y < height; ++y) {
    const float v = ((float)y + 0.5f) / (float)height;
    for (int x = 0; x < width; ++x) {
      const float u = ((float)x + 0.5f) / (float)width;
      const SkV3 c = fn(u, v);
      float* px = &pixels[((size_t)y * width + x) * 4];
      px[0] = c.x;
      px[1] = c.y;
      px[2] = c.z;
      px[3] = 1;
    }
  }
  const SkImageInfo info = SkImageInfo::Make(
      width, height, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  return SkImages::RasterFromPixmapCopy(
      {info, pixels.data(), (size_t)width * 4 * sizeof(float)});
}

float gauss(float x, float sigma) {
  return std::exp(-(x * x) / (2 * sigma * sigma));
}

/** Angular distance on the equirect u axis (wraps). */
float du(float a, float b) {
  const float d = std::abs(a - b);
  return std::min(d, 1.0f - d);
}

}  // namespace

Environment Environment::studio(int width) {
  Environment env;
  env.m_base = bakeEquirect(width, [](float u, float v) -> SkV3 {
    // Graded neutral shell: bright zenith, dim floor.
    const float sky = std::pow(std::clamp(1.0f - v, 0.0f, 1.0f), 1.4f);
    SkV3 c = {0.10f + 0.55f * sky, 0.11f + 0.56f * sky, 0.13f + 0.60f * sky};
    // Floor bounce card below the horizon.
    if (v > 0.62f) {
      const float f = gauss(v - 0.78f, 0.10f) * 0.5f;
      c += {f * 0.9f, f * 0.85f, f * 0.75f};
    }
    // Three softboxes: key, fill, rim strip.
    const float key =
        gauss(du(u, 0.30f), 0.055f) * gauss(v - 0.30f, 0.09f) * 3.2f;
    const float fill =
        gauss(du(u, 0.72f), 0.075f) * gauss(v - 0.38f, 0.12f) * 1.1f;
    const float rim =
        gauss(du(u, 0.02f), 0.02f) * gauss(v - 0.22f, 0.20f) * 2.2f;
    c += {key + fill + rim, key + fill + rim, key + fill + rim};
    return c;
  });
  env.m_blurs = std::make_shared<std::map<int, sk_sp<SkImage>>>();
  return env;
}

Environment Environment::sunset(int width) {
  Environment env;
  env.m_base = bakeEquirect(width, [](float u, float v) -> SkV3 {
    const float horizon = 0.52f;
    if (v < horizon) {
      // Banded sky falling toward a hot horizon stripe.
      const float t = v / horizon;  // 0 zenith -> 1 horizon
      SkV3 top = {0.05f, 0.10f, 0.30f};
      SkV3 low = {1.05f, 0.45f, 0.15f};
      SkV3 c = top + (low - top) * std::pow(t, 1.6f);
      // The classic chrome bands.
      const float band =
          0.5f + 0.5f * std::sin(t * 40.0f + std::cos(t * 13.0f));
      c *= 0.82f + 0.18f * band * std::pow(t, 2.0f);
      // Sun blob.
      const float sun =
          gauss(du(u, 0.5f), 0.035f) * gauss(v - horizon + 0.06f, 0.045f);
      c += SkV3{2.6f, 1.6f, 0.7f} * sun;
      // Horizon flare line.
      c += SkV3{1.6f, 0.9f, 0.45f} * gauss(v - horizon, 0.008f);
      return c;
    }
    // Ground: near-black violet falling away.
    const float t = (v - horizon) / (1.0f - horizon);
    SkV3 c = {0.16f, 0.05f, 0.16f};
    c = c * (1.0f - t * 0.8f);
    c += SkV3{0.5f, 0.2f, 0.4f} * gauss(v - horizon, 0.02f);
    return c;
  });
  env.m_blurs = std::make_shared<std::map<int, sk_sp<SkImage>>>();
  return env;
}

Environment Environment::fromEquirect(sk_sp<SkImage> image) {
  Environment env;
  env.m_base = std::move(image);
  env.m_blurs = std::make_shared<std::map<int, sk_sp<SkImage>>>();
  return env;
}

namespace {

/** Three-pass box blur ~= gaussian, run on F32 pixels with horizontal
 *  WRAP (an equirect's u axis is periodic — Skia's blur filter can't
 *  know that) and vertical clamp. */
void boxBlurF32(std::vector<float>& pixels, int w, int h, int radius) {
  if (radius < 1) return;
  std::vector<float> tmp(pixels.size());
  const int window = radius * 2 + 1;
  for (int pass = 0; pass < 3; ++pass) {
    // Horizontal, wrapped.
    for (int y = 0; y < h; ++y) {
      float* row = &pixels[(size_t)y * w * 4];
      float acc[4] = {0, 0, 0, 0};
      for (int k = -radius; k <= radius; ++k) {
        const int x = ((k % w) + w) % w;
        for (int c = 0; c < 4; ++c) acc[c] += row[x * 4 + c];
      }
      float* out = &tmp[(size_t)y * w * 4];
      for (int x = 0; x < w; ++x) {
        for (int c = 0; c < 4; ++c) out[x * 4 + c] = acc[c] / (float)window;
        const int drop = (((x - radius) % w) + w) % w;
        const int add = (x + radius + 1) % w;
        for (int c = 0; c < 4; ++c)
          acc[c] += row[add * 4 + c] - row[drop * 4 + c];
      }
    }
    // Vertical, clamped.
    for (int x = 0; x < w; ++x) {
      float acc[4] = {0, 0, 0, 0};
      for (int k = -radius; k <= radius; ++k) {
        const int y = std::clamp(k, 0, h - 1);
        for (int c = 0; c < 4; ++c) acc[c] += tmp[((size_t)y * w + x) * 4 + c];
      }
      for (int y = 0; y < h; ++y) {
        for (int c = 0; c < 4; ++c)
          pixels[((size_t)y * w + x) * 4 + c] = acc[c] / (float)window;
        const int drop = std::clamp(y - radius, 0, h - 1);
        const int add = std::clamp(y + radius + 1, 0, h - 1);
        for (int c = 0; c < 4; ++c)
          acc[c] += tmp[((size_t)add * w + x) * 4 + c] -
                    tmp[((size_t)drop * w + x) * 4 + c];
      }
    }
  }
}

}  // namespace

sk_sp<SkImage> Environment::image(float roughness) const {
  if (!m_base) return nullptr;
  roughness = std::clamp(roughness, 0.0f, 1.0f);
  const int bucket = (int)std::lround(roughness * 8.0f);
  if (bucket == 0) return m_base;
  if (m_blurs) {
    if (auto it = m_blurs->find(bucket); it != m_blurs->end())
      return it->second;
  }
  const int w = m_base->width(), h = m_base->height();
  const SkImageInfo info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  std::vector<float> pixels((size_t)w * h * 4);
  const SkPixmap pixmap(info, pixels.data(), (size_t)w * 4 * sizeof(float));
  if (!m_base->readPixels(nullptr, pixmap, 0, 0)) return m_base;
  // Box radius from the bucket: three passes triple the effective
  // spread, so keep the per-pass radius modest.
  const int radius =
      std::max(1, (int)std::lround(std::pow((float)bucket / 8.0f, 1.5f) *
                                   (float)w * 0.045f));
  boxBlurF32(pixels, w, h, radius);
  sk_sp<SkImage> blurred = SkImages::RasterFromPixmapCopy(pixmap);
  if (!blurred) return m_base;
  if (m_blurs) (*m_blurs)[bucket] = blurred;
  return blurred;
}

Texture Environment::texture(float roughness) const {
  // Repeat in u so azimuth wraps seamlessly; clamp at the poles.
  return Texture::of(image(roughness))
      .tile(SkTileMode::kRepeat, SkTileMode::kClamp);
}

SkISize Environment::size() const {
  return m_base ? m_base->dimensions() : SkISize::MakeEmpty();
}

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

  // Height with a smoothstep shoulder, then Sobel -> normals.
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
      row[x] = (0xffu << SK_A32_SHIFT) | (enc(nx) << SK_R32_SHIFT) |
               (enc(ny) << SK_G32_SHIFT) | (enc(nz) << SK_B32_SHIFT);
    }
  }
  out.setImmutable();
  return out.asImage();
}

}  // namespace

Texture bevelNormals(const SkPath& path, SkIRect bounds, float bevelPx,
                     float heightScale) {
  return Texture::of(bevelImage(path, bounds, bevelPx, heightScale))
      .at({(float)bounds.left(), (float)bounds.top()});
}

Texture bevelNormals(const SkPath& path, float bevelPx, float heightScale) {
  SkRect b = path.computeTightBounds();
  b.outset(bevelPx + 2, bevelPx + 2);
  return bevelNormals(path, b.roundOut(), bevelPx, heightScale);
}

}  // namespace sigil::material
