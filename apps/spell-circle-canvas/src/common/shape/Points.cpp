#include "sigilshape/Points.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkPaint.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace sigil::shape {

namespace {

SkV3 cross(SkV3 a, SkV3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
          a.x * b.y - a.y * b.x};
}

SkV3 normalized(SkV3 v, SkV3 fallback = {0, 0, 1}) {
  const float len = v.length();
  return len < 1e-12f ? fallback : v * (1.0f / len);
}

uint32_t pcg(uint32_t &state) {
  state = state * 747796405u + 2891336453u;
  uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

float rand01(uint32_t &state) {
  return (float)pcg(state) / (float)0xFFFFFFFFu;
}

float valueNoise3(SkV3 p, uint32_t seed) {
  auto hash = [seed](int x, int y, int z) {
    uint32_t h = seed + (uint32_t)x * 374761393u + (uint32_t)y * 668265263u +
                 (uint32_t)z * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)(h ^ (h >> 16)) / (float)0xFFFFFFFFu;
  };
  const int xi = (int)std::floor(p.x), yi = (int)std::floor(p.y),
            zi = (int)std::floor(p.z);
  const float xf = p.x - (float)xi, yf = p.y - (float)yi,
              zf = p.z - (float)zi;
  auto smooth = [](float t) { return t * t * (3 - 2 * t); };
  const float u = smooth(xf), v = smooth(yf), w = smooth(zf);
  float accum = 0;
  for (int dz = 0; dz <= 1; ++dz)
    for (int dy = 0; dy <= 1; ++dy)
      for (int dx = 0; dx <= 1; ++dx) {
        const float weight = (dx ? u : 1 - u) * (dy ? v : 1 - v) *
                             (dz ? w : 1 - w);
        accum += hash(xi + dx, yi + dy, zi + dz) * weight;
      }
  return accum * 2.0f - 1.0f;
}

/** Basis with +z along @p dir. */
void basisFor(SkV3 dir, SkV3 up, SkV3 *x, SkV3 *y, SkV3 *z) {
  *z = normalized(dir);
  SkV3 side = cross(up, *z);
  if (side.lengthSquared() < 1e-8f)
    side = cross(SkV3{1, 0, 0}, *z);
  *x = normalized(side);
  *y = cross(*z, *x);
}

} // namespace

std::vector<float> &Cloud::scalar(const std::string &name, float fill) {
  std::vector<float> &lane = scalars[name];
  lane.resize(positions.size(), fill);
  return lane;
}

std::vector<SkV3> &Cloud::vector(const std::string &name, SkV3 fill) {
  std::vector<SkV3> &lane = vectors[name];
  lane.resize(positions.size(), fill);
  return lane;
}

std::vector<SkColor4f> &Cloud::color(const std::string &name,
                                     SkColor4f fill) {
  std::vector<SkColor4f> &lane = colors[name];
  lane.resize(positions.size(), fill);
  return lane;
}

const std::vector<float> *Cloud::scalarIf(std::string_view name) const {
  auto it = scalars.find(name);
  return it == scalars.end() ? nullptr : &it->second;
}
const std::vector<SkV3> *Cloud::vectorIf(std::string_view name) const {
  auto it = vectors.find(name);
  return it == vectors.end() ? nullptr : &it->second;
}
const std::vector<SkColor4f> *Cloud::colorIf(std::string_view name) const {
  auto it = colors.find(name);
  return it == colors.end() ? nullptr : &it->second;
}

void Cloud::append(const Cloud &other) {
  const size_t oldSize = positions.size();
  const size_t newSize = oldSize + other.positions.size();
  positions.insert(positions.end(), other.positions.begin(),
                   other.positions.end());
  // Union of lanes on both sides, padded with defaults.
  for (auto &[name, lane] : scalars)
    lane.resize(newSize,
                0.0f); // pad ours; other's values overwrite below if present
  for (const auto &[name, lane] : other.scalars) {
    std::vector<float> &mine = scalars[name];
    mine.resize(oldSize, 0.0f);
    mine.insert(mine.end(), lane.begin(), lane.end());
    mine.resize(newSize, 0.0f);
  }
  for (auto &[name, lane] : vectors)
    lane.resize(newSize, SkV3{0, 0, 1});
  for (const auto &[name, lane] : other.vectors) {
    std::vector<SkV3> &mine = vectors[name];
    mine.resize(oldSize, SkV3{0, 0, 1});
    mine.insert(mine.end(), lane.begin(), lane.end());
    mine.resize(newSize, SkV3{0, 0, 1});
  }
  for (auto &[name, lane] : colors)
    lane.resize(newSize, SkColor4f{1, 1, 1, 1});
  for (const auto &[name, lane] : other.colors) {
    std::vector<SkColor4f> &mine = colors[name];
    mine.resize(oldSize, SkColor4f{1, 1, 1, 1});
    mine.insert(mine.end(), lane.begin(), lane.end());
    mine.resize(newSize, SkColor4f{1, 1, 1, 1});
  }
}

namespace points {

Cloud onSpline(const Spline3 &spline, int count, SkV3 up) {
  Cloud out;
  const std::vector<Frame3> rail = curves::frames(spline, count, up);
  out.positions.reserve(rail.size());
  std::vector<float> t;
  std::vector<SkV3> tangent, normal, binormal;
  for (const Frame3 &f : rail) {
    out.positions.push_back(f.position);
    t.push_back(f.t);
    tangent.push_back(f.tangent);
    normal.push_back(f.normal);
    binormal.push_back(f.binormal);
  }
  out.scalars["t"] = std::move(t);
  out.vectors["tangent"] = std::move(tangent);
  out.vectors["normal"] = std::move(normal);
  out.vectors["binormal"] = std::move(binormal);
  return out;
}

Cloud grid(SkV3 origin, SkV3 du, SkV3 dv, int nu, int nv) {
  Cloud out;
  nu = std::max(nu, 1);
  nv = std::max(nv, 1);
  const SkV3 n = normalized(cross(du, dv));
  const size_t total = (size_t)nu * (size_t)nv;
  out.positions.reserve(total);
  for (int j = 0; j < nv; ++j)
    for (int i = 0; i < nu; ++i) {
      const float fu = nu > 1 ? (float)i / (float)(nu - 1) : 0.0f;
      const float fv = nv > 1 ? (float)j / (float)(nv - 1) : 0.0f;
      out.positions.push_back(origin + du * fu + dv * fv);
    }
  std::vector<float> &t = out.scalar("t");
  for (size_t i = 0; i < total; ++i)
    t[i] = total > 1 ? (float)i / (float)(total - 1) : 0.0f;
  out.vector("normal", n);
  return out;
}

Cloud ring(SkV3 center, float radius, int count, SkV3 axis) {
  Cloud out;
  count = std::max(count, 1);
  SkV3 x, y, z;
  basisFor(axis, std::abs(axis.y) > 0.9f ? SkV3{1, 0, 0} : SkV3{0, 1, 0},
           &x, &y, &z);
  out.positions.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    const float a = (float)i / (float)count * 2.0f * (float)M_PI;
    out.positions.push_back(center + x * (radius * std::cos(a)) +
                            y * (radius * std::sin(a)));
  }
  std::vector<float> &t = out.scalar("t");
  std::vector<SkV3> &normal = out.vector("normal");
  for (int i = 0; i < count; ++i) {
    t[(size_t)i] = (float)i / (float)count;
    normal[(size_t)i] =
        normalized(out.positions[(size_t)i] - center);
  }
  return out;
}

Cloud scatterBox(SkV3 lo, SkV3 hi, int count, uint32_t seed) {
  Cloud out;
  count = std::max(count, 0);
  uint32_t state = seed * 2654435761u + 1u;
  out.positions.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    out.positions.push_back({lo.x + (hi.x - lo.x) * rand01(state),
                             lo.y + (hi.y - lo.y) * rand01(state),
                             lo.z + (hi.z - lo.z) * rand01(state)});
  }
  std::vector<float> &t = out.scalar("t");
  for (int i = 0; i < count; ++i)
    t[(size_t)i] = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
  return out;
}

Cloud onMesh(const Mesh &mesh, int count, uint32_t seed) {
  Cloud out;
  const size_t triangles = mesh.triangleCount();
  if (triangles == 0 || count <= 0)
    return out;

  // Cumulative area table for area-weighted triangle picks.
  std::vector<double> cumulative(triangles + 1, 0.0);
  for (size_t t = 0; t < triangles; ++t) {
    const SkV3 &a = mesh.positions[mesh.indices[t * 3]];
    const SkV3 &b = mesh.positions[mesh.indices[t * 3 + 1]];
    const SkV3 &c = mesh.positions[mesh.indices[t * 3 + 2]];
    cumulative[t + 1] =
        cumulative[t] + 0.5 * (double)cross(b - a, c - a).length();
  }
  const double total = cumulative.back();
  if (total <= 0)
    return out;

  uint32_t state = seed * 2654435761u + 17u;
  out.positions.reserve((size_t)count);
  std::vector<SkV3> pickedNormals;
  pickedNormals.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    const double target = (double)rand01(state) * total;
    const size_t t =
        (size_t)(std::upper_bound(cumulative.begin(), cumulative.end(),
                                  target) -
                 cumulative.begin()) -
        1;
    const size_t tri = std::min(t, triangles - 1);
    // Uniform barycentric.
    float u = rand01(state), v = rand01(state);
    if (u + v > 1) {
      u = 1 - u;
      v = 1 - v;
    }
    const uint32_t i0 = mesh.indices[tri * 3];
    const uint32_t i1 = mesh.indices[tri * 3 + 1];
    const uint32_t i2 = mesh.indices[tri * 3 + 2];
    const SkV3 &a = mesh.positions[i0];
    const SkV3 &b = mesh.positions[i1];
    const SkV3 &c = mesh.positions[i2];
    out.positions.push_back(a + (b - a) * u + (c - a) * v);
    if (mesh.normals.size() == mesh.positions.size()) {
      pickedNormals.push_back(normalized(
          mesh.normals[i0] * (1 - u - v) + mesh.normals[i1] * u +
          mesh.normals[i2] * v));
    } else {
      pickedNormals.push_back(normalized(cross(b - a, c - a)));
    }
  }
  out.vectors["normal"] = std::move(pickedNormals);
  std::vector<float> &t = out.scalar("t");
  for (int i = 0; i < count; ++i)
    t[(size_t)i] = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
  return out;
}

void jitter(Cloud &cloud, float amplitude, uint32_t seed) {
  uint32_t state = seed * 2654435761u + 101u;
  for (SkV3 &p : cloud.positions)
    p += {(rand01(state) * 2 - 1) * amplitude,
          (rand01(state) * 2 - 1) * amplitude,
          (rand01(state) * 2 - 1) * amplitude};
}

void displaceNoise(Cloud &cloud, float amplitude, float frequency,
                   uint32_t seed) {
  for (SkV3 &p : cloud.positions) {
    const SkV3 q = p * frequency;
    p += {valueNoise3(q, seed) * amplitude,
          valueNoise3(q + SkV3{31.7f, 0, 0}, seed) * amplitude,
          valueNoise3(q + SkV3{0, 47.3f, 0}, seed) * amplitude};
  }
}

Mesh instance(const Cloud &cloud, const Mesh &stamp,
              const InstanceOptions &options) {
  Mesh out;
  const size_t n = cloud.size();
  const size_t stampVerts = stamp.vertexCount();
  out.positions.reserve(n * stampVerts);
  out.normals.reserve(n * stampVerts);
  out.uvs.reserve(n * stampVerts);
  out.indices.reserve(n * stamp.indices.size());

  const std::vector<float> *scaleLane =
      options.scaleLane.empty() ? nullptr
                                : cloud.scalarIf(options.scaleLane);
  const std::vector<SkColor4f> *tintLane =
      options.tintLane.empty() ? nullptr : cloud.colorIf(options.tintLane);
  const std::vector<SkV3> *orientLane =
      options.orientLane.empty() ? nullptr
                                 : cloud.vectorIf(options.orientLane);
  const bool tinted = tintLane != nullptr || !stamp.colors.empty();

  for (size_t i = 0; i < n; ++i) {
    const float s =
        options.scale *
        (scaleLane && i < scaleLane->size() ? (*scaleLane)[i] : 1.0f);
    SkV3 bx{1, 0, 0}, by{0, 1, 0}, bz{0, 0, 1};
    if (orientLane && i < orientLane->size())
      basisFor((*orientLane)[i], options.up, &bx, &by, &bz);
    const SkV3 origin = cloud.positions[i];
    const uint32_t base = (uint32_t)out.positions.size();
    for (size_t v = 0; v < stampVerts; ++v) {
      const SkV3 &p = stamp.positions[v];
      out.positions.push_back(origin + (bx * p.x + by * p.y + bz * p.z) * s);
      if (v < stamp.normals.size()) {
        const SkV3 &nrm = stamp.normals[v];
        out.normals.push_back(bx * nrm.x + by * nrm.y + bz * nrm.z);
      }
      if (v < stamp.uvs.size())
        out.uvs.push_back(stamp.uvs[v]);
      if (tinted) {
        SkColor4f tint =
            tintLane && i < tintLane->size() ? (*tintLane)[i]
                                             : SkColor4f{1, 1, 1, 1};
        if (v < stamp.colors.size()) {
          tint.fR *= stamp.colors[v].fR;
          tint.fG *= stamp.colors[v].fG;
          tint.fB *= stamp.colors[v].fB;
          tint.fA *= stamp.colors[v].fA;
        }
        out.colors.push_back(tint);
      }
    }
    for (uint32_t idx : stamp.indices)
      out.indices.push_back(base + idx);
  }
  return out;
}

Mesh panels(const Cloud &cloud, float width, float height,
            const InstanceOptions &options) {
  return instance(cloud, mesh::quad(width, height), options);
}

void drawBillboards(SkCanvas &canvas, const Cloud &cloud,
                    const space::Camera &camera, SkSize viewport,
                    const BillboardStyle &style) {
  const size_t n = cloud.size();
  if (n == 0)
    return;
  const SkM44 vp = camera.viewProjection(viewport);
  const SkM44 view = camera.view();

  struct Splat {
    SkPoint screen;
    float px;   // half-size in pixels
    float depth;
    SkColor4f tint;
  };
  std::vector<Splat> splats;
  splats.reserve(n);

  const std::vector<float> *sizeLane =
      style.sizeLane.empty() ? nullptr : cloud.scalarIf(style.sizeLane);
  const std::vector<SkColor4f> *tintLane =
      style.tintLane.empty() ? nullptr : cloud.colorIf(style.tintLane);

  // Pixels per world unit at distance d: focal / d * (h/2).
  const float focal =
      1.0f / std::tan(camera.fovYDeg * (float)M_PI / 360.0f);
  const float halfH = viewport.height() * 0.5f;

  for (size_t i = 0; i < n; ++i) {
    const SkV3 &p = cloud.positions[i];
    const SkV4 clip = vp * SkV4{p.x, p.y, p.z, 1};
    if (clip.w <= 1e-4f)
      continue;
    const SkV4 eye = view * SkV4{p.x, p.y, p.z, 1};
    Splat splat;
    splat.screen = {clip.x / clip.w, clip.y / clip.w};
    splat.depth = eye.z;
    const float size =
        style.size *
        (sizeLane && i < sizeLane->size() ? (*sizeLane)[i] : 1.0f);
    splat.px = style.perspective
                   ? std::max(0.25f, size * 0.5f * focal * halfH /
                                         std::max(-eye.z, 1e-3f))
                   : size * 0.5f;
    SkColor4f tint = style.tint;
    if (tintLane && i < tintLane->size()) {
      const SkColor4f &c = (*tintLane)[i];
      tint = {tint.fR * c.fR, tint.fG * c.fG, tint.fB * c.fB,
              tint.fA * c.fA};
    }
    splat.tint = tint;
    splats.push_back(splat);
  }
  if (style.depthSort)
    std::sort(splats.begin(), splats.end(),
              [](const Splat &a, const Splat &b) {
                return a.depth < b.depth; // far first
              });

  // The default sprite: a CPU-baked soft white dot (quadratic falloff).
  static const sk_sp<SkImage> softDot = [] {
    constexpr int kSize = 64;
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(kSize, kSize));
    for (int y = 0; y < kSize; ++y) {
      uint32_t *row = bm.getAddr32(0, y);
      for (int x = 0; x < kSize; ++x) {
        const float dx = ((float)x + 0.5f) / kSize * 2 - 1;
        const float dy = ((float)y + 0.5f) / kSize * 2 - 1;
        const float r = std::sqrt(dx * dx + dy * dy);
        const float a =
            std::clamp(1.0f - r, 0.0f, 1.0f); // linear edge...
        const float soft = a * a * (3 - 2 * a); // ...smoothstepped
        const uint32_t v = (uint32_t)std::lround(soft * 255.0f);
        row[x] = (v << SK_A32_SHIFT) | (v << SK_R32_SHIFT) |
                 (v << SK_G32_SHIFT) | (v << SK_B32_SHIFT); // premul white
      }
    }
    bm.setImmutable();
    return bm.asImage();
  }();

  SkPaint paint;
  paint.setAntiAlias(true);
  if (style.additive)
    paint.setBlendMode(SkBlendMode::kPlus);
  const SkSamplingOptions sampling(SkFilterMode::kLinear,
                                   SkMipmapMode::kLinear);
  const sk_sp<SkImage> &sprite = style.sprite ? style.sprite : softDot;

  for (const Splat &splat : splats) {
    // Full-color tint via modulate — works for any sprite (a white
    // sprite tints exactly).
    paint.setColorFilter(SkColorFilters::Blend(splat.tint.toSkColor(),
                                               SkBlendMode::kModulate));
    const SkRect dst = SkRect::MakeXYWH(splat.screen.fX - splat.px,
                                        splat.screen.fY - splat.px,
                                        splat.px * 2, splat.px * 2);
    canvas.drawImageRect(sprite, dst, sampling, &paint);
  }
}

} // namespace points

} // namespace sigil::shape
