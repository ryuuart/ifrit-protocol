/** @file
 * The environment map: the procedural bakes, the cube sources resampled
 * into the one equirect form, the wrap-aware roughness blurs, the mip
 * chain a device binds, the cosine convolution a diffuse term reads, and
 * the solid-angle mean.
 */

#include "sigilmaterial/texture/EnvironmentMap.h"

#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <cmath>
#include <functional>
#include <mutex>

namespace sigil::material {

namespace {

constexpr float kPi = 3.14159265358979323846f;

/** A panorama's pixels, unpremultiplied by nothing and clamped by
 *  nothing: four floats a texel, which is the only form the blur, the
 *  resample and the convolution below work in. */
struct Grid {
  int w = 0;
  int h = 0;
  std::vector<float> px;

  bool empty() const { return w <= 0 || h <= 0; }
  const float* at(int x, int y) const { return &px[((size_t)y * w + x) * 4]; }
  float* at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }

  /** Bilinear, wrapping in u and clamping in v — the sampling an equirect
   *  panorama has: azimuth is periodic, the poles are not. */
  SkV4 sample(float u, float v) const {
    const float fx = u * (float)w - 0.5f;
    const float fy = v * (float)h - 0.5f;
    const int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
    const float tx = fx - (float)x0, ty = fy - (float)y0;
    const auto wrapX = [&](int x) { return ((x % w) + w) % w; };
    const auto clampY = [&](int y) { return std::clamp(y, 0, h - 1); };
    const int xa = wrapX(x0), xb = wrapX(x0 + 1);
    const int ya = clampY(y0), yb = clampY(y0 + 1);
    SkV4 out{0, 0, 0, 0};
    for (int c = 0; c < 4; ++c) {
      const float top = at(xa, ya)[c] * (1 - tx) + at(xb, ya)[c] * tx;
      const float bot = at(xa, yb)[c] * (1 - tx) + at(xb, yb)[c] * tx;
      (&out.x)[c] = top * (1 - ty) + bot * ty;
    }
    return out;
  }
};

Grid readGrid(const sk_sp<SkImage>& image) {
  Grid grid;
  if (!image) return grid;
  grid.w = image->width();
  grid.h = image->height();
  grid.px.assign((size_t)grid.w * grid.h * 4, 0.0f);
  const SkImageInfo info = SkImageInfo::Make(
      grid.w, grid.h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  const SkPixmap pixmap(info, grid.px.data(),
                        (size_t)grid.w * 4 * sizeof(float));
  if (!image->readPixels(nullptr, pixmap, 0, 0)) return Grid{};
  return grid;
}

sk_sp<SkImage> gridImage(const Grid& grid) {
  if (grid.empty()) return nullptr;
  const SkImageInfo info = SkImageInfo::Make(
      grid.w, grid.h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  return SkImages::RasterFromPixmapCopy(
      {info, grid.px.data(), (size_t)grid.w * 4 * sizeof(float)});
}

/** Box-filtered resample: every destination texel averages the source
 *  texels it covers, so shrinking a panorama by a factor loses no energy
 *  and grows no aliasing. Wraps in u, clamps in v. */
Grid resample(const Grid& src, int w, int h) {
  Grid out;
  if (src.empty() || w <= 0 || h <= 0) return out;
  out.w = w;
  out.h = h;
  out.px.assign((size_t)w * h * 4, 0.0f);
  const float sx = (float)src.w / (float)w;
  const float sy = (float)src.h / (float)h;
  const int taps = std::max(1, (int)std::ceil(sx));
  const int tapsY = std::max(1, (int)std::ceil(sy));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float acc[4] = {0, 0, 0, 0};
      for (int j = 0; j < tapsY; ++j) {
        for (int i = 0; i < taps; ++i) {
          const float u =
              ((float)x + ((float)i + 0.5f) / (float)taps) / (float)w;
          const float v =
              ((float)y + ((float)j + 0.5f) / (float)tapsY) / (float)h;
          const SkV4 c = src.sample(u, v);
          for (int k = 0; k < 4; ++k) acc[k] += (&c.x)[k];
        }
      }
      const float n = (float)(taps * tapsY);
      float* dst = out.at(x, y);
      for (int k = 0; k < 4; ++k) dst[k] = acc[k] / n;
    }
  }
  return out;
}

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

/** Which cube face a direction falls on, and where on it. Faces are
 *  indexed +x -x +y -y +z -z and oriented the way every graphics API
 *  orients them, so a set exported by any of them loads without a
 *  transposition table. */
struct FaceHit {
  int face;
  float u;
  float v;
};

FaceHit faceOf(SkV3 d) {
  const float ax = std::abs(d.x), ay = std::abs(d.y), az = std::abs(d.z);
  int face;
  float sc, tc, ma;
  if (ax >= ay && ax >= az) {
    face = d.x > 0 ? 0 : 1;
    ma = ax;
    sc = d.x > 0 ? -d.z : d.z;
    tc = -d.y;
  } else if (ay >= az) {
    face = d.y > 0 ? 2 : 3;
    ma = ay;
    sc = d.x;
    tc = d.y > 0 ? d.z : -d.z;
  } else {
    face = d.z > 0 ? 4 : 5;
    ma = az;
    sc = d.z > 0 ? d.x : -d.x;
    tc = -d.y;
  }
  return {face, (sc / ma + 1.0f) * 0.5f, (tc / ma + 1.0f) * 0.5f};
}

/** Bilinear from a face grid, clamped on both axes — a face has edges. */
SkV4 sampleFace(const Grid& g, float u, float v) {
  const float fx = std::clamp(u * (float)g.w - 0.5f, 0.0f, (float)g.w - 1);
  const float fy = std::clamp(v * (float)g.h - 0.5f, 0.0f, (float)g.h - 1);
  const int x0 = (int)fx, y0 = (int)fy;
  const int x1 = std::min(x0 + 1, g.w - 1), y1 = std::min(y0 + 1, g.h - 1);
  const float tx = fx - (float)x0, ty = fy - (float)y0;
  SkV4 out{0, 0, 0, 0};
  for (int c = 0; c < 4; ++c) {
    const float top = g.at(x0, y0)[c] * (1 - tx) + g.at(x1, y0)[c] * tx;
    const float bot = g.at(x0, y1)[c] * (1 - tx) + g.at(x1, y1)[c] * tx;
    (&out.x)[c] = top * (1 - ty) + bot * ty;
  }
  return out;
}

Grid facesToEquirect(const std::array<Grid, 6>& faces, int width) {
  Grid out;
  const int height = std::max(width / 2, 4);
  out.w = width;
  out.h = height;
  out.px.assign((size_t)width * height * 4, 0.0f);
  for (int y = 0; y < height; ++y) {
    const float v = ((float)y + 0.5f) / (float)height;
    for (int x = 0; x < width; ++x) {
      const float u = ((float)x + 0.5f) / (float)width;
      const FaceHit hit = faceOf(equirectDirection({u, v}));
      const Grid& face = faces[hit.face];
      float* dst = out.at(x, y);
      if (face.empty()) {
        dst[3] = 1;
        continue;
      }
      const SkV4 c = sampleFace(face, hit.u, hit.v);
      for (int k = 0; k < 4; ++k) dst[k] = (&c.x)[k];
    }
  }
  return out;
}

/** One face cut out of a sheet, as a grid. */
Grid cut(const Grid& sheet, int col, int row, int edge) {
  Grid out;
  out.w = edge;
  out.h = edge;
  out.px.assign((size_t)edge * edge * 4, 0.0f);
  for (int y = 0; y < edge; ++y)
    for (int x = 0; x < edge; ++x) {
      const int sx = col * edge + x, sy = row * edge + y;
      if (sx >= sheet.w || sy >= sheet.h) continue;
      const float* src = sheet.at(sx, sy);
      float* dst = out.at(x, y);
      for (int c = 0; c < 4; ++c) dst[c] = src[c];
    }
  return out;
}

}  // namespace

SkV2 equirectUv(SkV3 d) {
  const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  if (len <= 0) return {0.5f, 0.5f};
  d = {d.x / len, d.y / len, d.z / len};
  return {0.5f + std::atan2(d.x, -d.z) / (2 * kPi),
          std::acos(std::clamp(d.y, -1.0f, 1.0f)) / kPi};
}

SkV3 equirectDirection(SkV2 uv) {
  const float theta = uv.y * kPi;
  const float phi = (uv.x - 0.5f) * 2 * kPi;
  const float s = std::sin(theta);
  return {s * std::sin(phi), std::cos(theta), -s * std::cos(phi)};
}

/** Everything a panorama derives, built once and shared by every copy of
 *  the value: the blurs by roughness bucket, the mip chain by its level-0
 *  width, the cosine convolution and the mean. */
struct EnvironmentMap::State {
  sk_sp<SkImage> base;

  mutable std::mutex lock;
  mutable boost::container::flat_map<int, sk_sp<SkImage>> blurs;
  mutable boost::container::flat_map<int, std::vector<sk_sp<SkImage>>> chains;
  mutable sk_sp<SkImage> cosine;
  mutable SkColor4f mean{0, 0, 0, 0};
  mutable bool meanDone = false;
};

EnvironmentMap EnvironmentMap::baked(
    int width, const std::function<SkV3(float u, float v)>& radiance) {
  return fromEquirect(bakeEquirect(width, radiance));
}

EnvironmentMap EnvironmentMap::fromEquirect(sk_sp<SkImage> image) {
  EnvironmentMap env;
  if (!image) return env;
  env.m_state = std::make_shared<State>();
  env.m_state->base = std::move(image);
  return env;
}

EnvironmentMap EnvironmentMap::fromFaces(const Faces& faces, int width) {
  std::array<Grid, 6> grids;
  int edge = 0;
  for (int i = 0; i < 6; ++i) {
    grids[i] = readGrid(faces[i]);
    edge = std::max(edge, grids[i].w);
  }
  if (edge <= 0) return {};
  // Four faces stand end to end around the equator, so an equirect at
  // four times the edge keeps the density a face had there.
  const int w = width > 0 ? width : edge * 4;
  return fromEquirect(gridImage(facesToEquirect(grids, w)));
}

EnvironmentMap EnvironmentMap::fromCubeMap(sk_sp<SkImage> sheet) {
  const Grid grid = readGrid(sheet);
  if (grid.empty()) return {};

  // Where each face sits in the layout the sheet's aspect ratio names,
  // as {column, row} in face-edge units, in the +x -x +y -y +z -z order.
  struct Layout {
    int cols;
    int rows;
    std::array<std::array<int, 2>, 6> at;
  };
  static constexpr Layout kHorizontalCross{
      4, 3, {{{2, 1}, {0, 1}, {1, 0}, {1, 2}, {1, 1}, {3, 1}}}};
  static constexpr Layout kVerticalCross{
      3, 4, {{{2, 1}, {0, 1}, {1, 0}, {1, 2}, {1, 1}, {1, 3}}}};
  static constexpr Layout kRow{
      6, 1, {{{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}}}};
  static constexpr Layout kColumn{
      1, 6, {{{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}}}};

  const float aspect = (float)grid.w / (float)grid.h;
  const Layout* layout = nullptr;
  if (aspect > 5.0f)
    layout = &kRow;
  else if (aspect < 0.25f)
    layout = &kColumn;
  else if (aspect > 1.0f)
    layout = &kHorizontalCross;
  else
    layout = &kVerticalCross;

  const int edge = std::min(grid.w / layout->cols, grid.h / layout->rows);
  if (edge <= 0) return {};
  std::array<Grid, 6> faces;
  for (int i = 0; i < 6; ++i)
    faces[i] = cut(grid, layout->at[i][0], layout->at[i][1], edge);
  return fromEquirect(gridImage(facesToEquirect(faces, edge * 4)));
}

sk_sp<SkImage> EnvironmentMap::image(float roughness) const {
  if (!m_state) return nullptr;
  const sk_sp<SkImage>& base = m_state->base;
  roughness = std::clamp(roughness, 0.0f, 1.0f);
  const int bucket = (int)std::lround(roughness * 8.0f);
  if (bucket == 0) return base;
  {
    const std::lock_guard<std::mutex> held(m_state->lock);
    if (auto it = m_state->blurs.find(bucket); it != m_state->blurs.end())
      return it->second;
  }
  const int w = base->width(), h = base->height();
  const SkImageInfo info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  std::vector<float> pixels((size_t)w * h * 4);
  const SkPixmap pixmap(info, pixels.data(), (size_t)w * 4 * sizeof(float));
  if (!base->readPixels(nullptr, pixmap, 0, 0)) return base;
  // Box radius from the bucket: three passes triple the effective
  // spread, so keep the per-pass radius modest.
  const int radius =
      std::max(1, (int)std::lround(std::pow((float)bucket / 8.0f, 1.5f) *
                                   (float)w * 0.045f));
  boxBlurF32(pixels, w, h, radius);
  sk_sp<SkImage> blurred = SkImages::RasterFromPixmapCopy(pixmap);
  if (!blurred) return base;
  const std::lock_guard<std::mutex> held(m_state->lock);
  m_state->blurs[bucket] = blurred;
  return blurred;
}

Texture EnvironmentMap::texture(float roughness) const {
  // Repeat in u so azimuth wraps seamlessly; clamp at the poles.
  return Texture::of(image(roughness))
      .tile(SkTileMode::kRepeat, SkTileMode::kClamp);
}

SkISize EnvironmentMap::size() const {
  return m_state && m_state->base ? m_state->base->dimensions()
                                  : SkISize::MakeEmpty();
}

int EnvironmentMap::prefilterSize() const {
  if (m_prefilter > 0) return m_prefilter;
  // Level 8 of the chain is one 256th of level 0, and a level under four
  // texels wide stops being a panorama, so the default bound is what
  // leaves the last level readable without prefiltering a 4K sky nine
  // times.
  const int w = size().width();
  return w > 0 ? std::clamp(w, 1 << (kLevels - 1), 1024) : 0;
}

EnvironmentMap EnvironmentMap::withPrefilterSize(int width) const {
  EnvironmentMap out = *this;
  out.m_prefilter = std::max(0, width);
  return out;
}

EnvironmentMap EnvironmentMap::withGround(SkColor4f color) const {
  if (!m_state) return {};
  Grid grid = readGrid(m_state->base);
  if (grid.empty()) return *this;
  for (int y = 0; y < grid.h; ++y) {
    const float v = ((float)y + 0.5f) / (float)grid.h;
    // A hard horizon would survive every blur as a seam, so the ground
    // meets the sky over a band a few degrees wide.
    const float t = std::clamp((v - 0.5f) / 0.03f, 0.0f, 1.0f);
    const float k = t * t * (3.0f - 2.0f * t);
    if (k <= 0) continue;
    for (int x = 0; x < grid.w; ++x) {
      float* px = grid.at(x, y);
      for (int c = 0; c < 4; ++c) px[c] += ((&color.fR)[c] - px[c]) * k;
    }
  }
  return fromEquirect(gridImage(grid)).withPrefilterSize(m_prefilter);
}

std::vector<sk_sp<SkImage>> EnvironmentMap::chain() const {
  if (!m_state) return {};
  const int top = prefilterSize();
  {
    const std::lock_guard<std::mutex> held(m_state->lock);
    if (auto it = m_state->chains.find(top); it != m_state->chains.end())
      return it->second;
  }
  std::vector<sk_sp<SkImage>> levels;
  levels.reserve(kLevels);
  for (int level = 0; level < kLevels; ++level) {
    const int w = std::max(top >> level, 2);
    const int h = std::max(w / 2, 1);
    const Grid src = readGrid(image((float)level / (float)(kLevels - 1)));
    if (src.empty()) return {};
    levels.push_back(
        gridImage(src.w == w && src.h == h ? src : resample(src, w, h)));
  }
  const std::lock_guard<std::mutex> held(m_state->lock);
  m_state->chains[top] = levels;
  return levels;
}

sk_sp<SkImage> EnvironmentMap::irradiance() const {
  if (!m_state) return nullptr;
  {
    const std::lock_guard<std::mutex> held(m_state->lock);
    if (m_state->cosine) return m_state->cosine;
  }
  // The convolution is quadratic in texel count, so the panorama is read
  // down to a size where a cosine lobe — the widest filter there is —
  // cannot tell the difference.
  constexpr int kSrcW = 64, kSrcH = 32;
  constexpr int kOutW = 32, kOutH = 16;
  const Grid src = resample(readGrid(m_state->base), kSrcW, kSrcH);
  if (src.empty()) return nullptr;

  // Every source texel's direction and solid angle, once.
  struct Sample {
    SkV3 dir;
    float weight;
    float rgb[3];
  };
  std::vector<Sample> samples;
  samples.reserve((size_t)kSrcW * kSrcH);
  for (int y = 0; y < kSrcH; ++y) {
    const float v = ((float)y + 0.5f) / (float)kSrcH;
    const float solid = std::sin(v * kPi);
    for (int x = 0; x < kSrcW; ++x) {
      const float u = ((float)x + 0.5f) / (float)kSrcW;
      const float* px = src.at(x, y);
      samples.push_back(
          {equirectDirection({u, v}), solid, {px[0], px[1], px[2]}});
    }
  }

  Grid out;
  out.w = kOutW;
  out.h = kOutH;
  out.px.assign((size_t)kOutW * kOutH * 4, 0.0f);
  for (int y = 0; y < kOutH; ++y) {
    const float v = ((float)y + 0.5f) / (float)kOutH;
    for (int x = 0; x < kOutW; ++x) {
      const float u = ((float)x + 0.5f) / (float)kOutW;
      const SkV3 n = equirectDirection({u, v});
      float acc[3] = {0, 0, 0};
      float total = 0;
      for (const Sample& s : samples) {
        const float cosine = n.x * s.dir.x + n.y * s.dir.y + n.z * s.dir.z;
        if (cosine <= 0) continue;
        const float k = cosine * s.weight;
        for (int c = 0; c < 3; ++c) acc[c] += s.rgb[c] * k;
        total += k;
      }
      float* dst = out.at(x, y);
      // Normalised by the same weights: the cosine-weighted MEAN of the
      // panorama, which is irradiance over pi — the number a Lambertian
      // body multiplies its albedo by, and for a panorama of one colour
      // that colour exactly.
      for (int c = 0; c < 3; ++c) dst[c] = total > 0 ? acc[c] / total : 0;
      dst[3] = 1;
    }
  }
  sk_sp<SkImage> made = gridImage(out);
  const std::lock_guard<std::mutex> held(m_state->lock);
  m_state->cosine = made;
  return made;
}

SkColor4f EnvironmentMap::average() const {
  if (!m_state) return {0, 0, 0, 0};
  {
    const std::lock_guard<std::mutex> held(m_state->lock);
    if (m_state->meanDone) return m_state->mean;
  }
  const Grid src = resample(readGrid(m_state->base), 64, 32);
  if (src.empty()) return {0, 0, 0, 0};
  double acc[3] = {0, 0, 0};
  double total = 0;
  for (int y = 0; y < src.h; ++y) {
    // A texel row near a pole covers less sky than one at the equator.
    const double solid = std::sin((((double)y + 0.5) / src.h) * kPi);
    for (int x = 0; x < src.w; ++x) {
      const float* px = src.at(x, y);
      for (int c = 0; c < 3; ++c) acc[c] += px[c] * solid;
      total += solid;
    }
  }
  const SkColor4f mean{(float)(acc[0] / total), (float)(acc[1] / total),
                       (float)(acc[2] / total), 1};
  const std::lock_guard<std::mutex> held(m_state->lock);
  m_state->mean = mean;
  m_state->meanDone = true;
  return mean;
}

}  // namespace sigil::material
