#include "sigilshape/Points.h"

#include "sigilshape/detail/Hash.h"
#include "sigilshape/detail/VecMath.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkPaint.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace sigil::shape {

using detail::basisFor;
using detail::normalized;
using glm::cross;

namespace {

/** The shared hash driven as a PRNG: advance the carried state, mix a
 *  copy of it. Same bits the local copy produced (detail/Hash.h). */
uint32_t pcg(uint32_t &state) {
  state = detail::pcgAdvance(state);
  return detail::pcgMix(state);
}

float rand01(uint32_t &state) {
  return (float)pcg(state) / (float)0xFFFFFFFFu;
}

float valueNoise3(glm::vec3 p, uint32_t seed) {
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

/** The pad values Cloud::append fills missing lanes with, BY NAME —
 *  the lane conventions, not one blanket default: a missing "size"
 *  means scale 1 (not invisible instances), a missing "Tex" the
 *  identity uv window, a missing "uv" zeros; other scalars pad 0,
 *  other colors white, vectors {0, 0, 1}. */
float scalarDefault(std::string_view name) {
  return name == "size" ? 1.0f : 0.0f;
}

glm::vec4 colorDefault(std::string_view name) {
  if (name == "Tex")
    return {0, 0, 1, 1};
  if (name == "uv")
    return {0, 0, 0, 0};
  return {1, 1, 1, 1};
}

} // namespace

std::vector<float> &Cloud::scalar(const std::string &name, float fill) {
  std::vector<float> &lane = scalars[name];
  lane.resize(positions.size(), fill);
  return lane;
}

std::vector<glm::vec3> &Cloud::vector(const std::string &name,
                                      glm::vec3 fill) {
  std::vector<glm::vec3> &lane = vectors[name];
  lane.resize(positions.size(), fill);
  return lane;
}

std::vector<glm::vec4> &Cloud::color(const std::string &name,
                                     glm::vec4 fill) {
  std::vector<glm::vec4> &lane = colors[name];
  lane.resize(positions.size(), fill);
  return lane;
}

const std::vector<float> *Cloud::scalarIf(std::string_view name) const {
  auto it = scalars.find(name);
  return it == scalars.end() ? nullptr : &it->second;
}
const std::vector<glm::vec3> *Cloud::vectorIf(std::string_view name) const {
  auto it = vectors.find(name);
  return it == vectors.end() ? nullptr : &it->second;
}
const std::vector<glm::vec4> *Cloud::colorIf(std::string_view name) const {
  auto it = colors.find(name);
  return it == colors.end() ? nullptr : &it->second;
}

void Cloud::append(const Cloud &other) {
  const size_t oldSize = positions.size();
  const size_t newSize = oldSize + other.positions.size();
  positions.insert(positions.end(), other.positions.begin(),
                   other.positions.end());
  // Union of lanes on both sides, a missing side padded with the lane
  // NAME's conventional default (scalarDefault/colorDefault above) —
  // the same convention on the pad-ours and pad-theirs paths.
  for (auto &[name, lane] : scalars)
    lane.resize(newSize, scalarDefault(name));
  for (const auto &[name, lane] : other.scalars) {
    std::vector<float> &mine = scalars[name];
    mine.resize(oldSize, scalarDefault(name));
    mine.insert(mine.end(), lane.begin(), lane.end());
    mine.resize(newSize, scalarDefault(name));
  }
  for (auto &[name, lane] : vectors)
    lane.resize(newSize, glm::vec3{0, 0, 1});
  for (const auto &[name, lane] : other.vectors) {
    std::vector<glm::vec3> &mine = vectors[name];
    mine.resize(oldSize, glm::vec3{0, 0, 1});
    mine.insert(mine.end(), lane.begin(), lane.end());
    mine.resize(newSize, glm::vec3{0, 0, 1});
  }
  for (auto &[name, lane] : colors)
    lane.resize(newSize, colorDefault(name));
  for (const auto &[name, lane] : other.colors) {
    std::vector<glm::vec4> &mine = colors[name];
    mine.resize(oldSize, colorDefault(name));
    mine.insert(mine.end(), lane.begin(), lane.end());
    mine.resize(newSize, colorDefault(name));
  }
}

namespace points {

Cloud onSpline(const Spline3 &spline, int count, glm::vec3 up) {
  Cloud out;
  const std::vector<Frame3> rail = curves::frames(spline, count, up);
  out.positions.reserve(rail.size());
  std::vector<float> t;
  std::vector<glm::vec3> tangent, normal, binormal;
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

Cloud grid(glm::vec3 origin, glm::vec3 du, glm::vec3 dv, int nu,
           int nv) {
  Cloud out;
  nu = std::max(nu, 1);
  nv = std::max(nv, 1);
  const glm::vec3 n = normalized(cross(du, dv));
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

Cloud ring(glm::vec3 center, float radius, int count, glm::vec3 axis) {
  Cloud out;
  count = std::max(count, 1);
  glm::vec3 x, y, z;
  basisFor(axis,
           std::abs(axis.y) > 0.9f ? glm::vec3{1, 0, 0}
                                   : glm::vec3{0, 1, 0},
           &x, &y, &z);
  out.positions.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    const float a = (float)i / (float)count * 2.0f * (float)M_PI;
    out.positions.push_back(center + x * (radius * std::cos(a)) +
                            y * (radius * std::sin(a)));
  }
  std::vector<float> &t = out.scalar("t");
  std::vector<glm::vec3> &normal = out.vector("normal");
  for (int i = 0; i < count; ++i) {
    t[(size_t)i] = (float)i / (float)count;
    normal[(size_t)i] =
        normalized(out.positions[(size_t)i] - center);
  }
  return out;
}

Cloud scatterBox(glm::vec3 lo, glm::vec3 hi, int count, uint32_t seed) {
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
    const glm::vec3 &a = mesh.positions[mesh.indices[t * 3]];
    const glm::vec3 &b = mesh.positions[mesh.indices[t * 3 + 1]];
    const glm::vec3 &c = mesh.positions[mesh.indices[t * 3 + 2]];
    cumulative[t + 1] =
        cumulative[t] + 0.5 * (double)glm::length(cross(b - a, c - a));
  }
  const double total = cumulative.back();
  if (total <= 0)
    return out;

  uint32_t state = seed * 2654435761u + 17u;
  out.positions.reserve((size_t)count);
  std::vector<glm::vec3> pickedNormals;
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
    const glm::vec3 &a = mesh.positions[i0];
    const glm::vec3 &b = mesh.positions[i1];
    const glm::vec3 &c = mesh.positions[i2];
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
  for (glm::vec3 &p : cloud.positions)
    p += glm::vec3{(rand01(state) * 2 - 1) * amplitude,
                   (rand01(state) * 2 - 1) * amplitude,
                   (rand01(state) * 2 - 1) * amplitude};
}

void displaceNoise(Cloud &cloud, float amplitude, float frequency,
                   uint32_t seed) {
  for (glm::vec3 &p : cloud.positions) {
    const glm::vec3 q = p * frequency;
    p += glm::vec3{
        valueNoise3(q, seed) * amplitude,
        valueNoise3(q + glm::vec3{31.7f, 0, 0}, seed) * amplitude,
        valueNoise3(q + glm::vec3{0, 47.3f, 0}, seed) * amplitude};
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
  const std::vector<glm::vec4> *tintLane =
      options.tintLane.empty() ? nullptr : cloud.colorIf(options.tintLane);
  const std::vector<glm::vec3> *orientLane =
      options.orientLane.empty() ? nullptr
                                 : cloud.vectorIf(options.orientLane);
  const bool tinted = tintLane != nullptr || !stamp.colors.empty();

  for (size_t i = 0; i < n; ++i) {
    const float s =
        options.scale *
        (scaleLane && i < scaleLane->size() ? (*scaleLane)[i] : 1.0f);
    glm::vec3 bx{1, 0, 0}, by{0, 1, 0}, bz{0, 0, 1};
    if (orientLane && i < orientLane->size())
      basisFor((*orientLane)[i], options.up, &bx, &by, &bz);
    const glm::vec3 origin = cloud.positions[i];
    const uint32_t base = (uint32_t)out.positions.size();
    for (size_t v = 0; v < stampVerts; ++v) {
      const glm::vec3 &p = stamp.positions[v];
      out.positions.push_back(origin + (bx * p.x + by * p.y + bz * p.z) * s);
      if (v < stamp.normals.size()) {
        const glm::vec3 &nrm = stamp.normals[v];
        out.normals.push_back(bx * nrm.x + by * nrm.y + bz * nrm.z);
      }
      if (v < stamp.uvs.size())
        out.uvs.push_back(stamp.uvs[v]);
      if (tinted) {
        glm::vec4 tint = tintLane && i < tintLane->size()
                             ? (*tintLane)[i]
                             : glm::vec4{1, 1, 1, 1};
        if (v < stamp.colors.size())
          tint *= stamp.colors[v];
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

void promoteToPrims(Mesh &mesh, const Cloud &cloud,
                    std::string_view cloudLane,
                    const std::string &primLane) {
  const size_t points = cloud.size();
  const size_t tris = mesh.triangleCount();
  if (points == 0 || tris == 0 || tris % points != 0)
    return;
  const size_t perPoint = tris / points;

  // "Id" is the reserved source: the owning point's own index, the
  // only per-piece identity this layer needs (a stamp instance is a
  // run of triangles sharing an Id value, not a separate container).
  const bool wantId = cloudLane == "Id";
  const std::vector<float> *scalars =
      wantId ? nullptr : cloud.scalarIf(cloudLane);
  const std::vector<glm::vec3> *vectors =
      wantId ? nullptr : cloud.vectorIf(cloudLane);
  const std::vector<glm::vec4> *colors =
      wantId ? nullptr : cloud.colorIf(cloudLane);
  if (!wantId && !scalars && !vectors && !colors)
    return;

  std::vector<glm::vec4> &lane = mesh.prim(primLane);
  lane.assign(tris, glm::vec4{0, 0, 0, 0});
  for (size_t i = 0; i < points; ++i) {
    glm::vec4 value{0, 0, 0, 0};
    if (wantId) {
      value = {(float)i, 0, 0, 0};
    } else if (scalars) {
      if (i >= scalars->size())
        continue;
      const float s = (*scalars)[i];
      value = {s, s, s, s};
    } else if (vectors) {
      if (i >= vectors->size())
        continue;
      const glm::vec3 &v = (*vectors)[i];
      value = {v.x, v.y, v.z, 0};
    } else {
      if (i >= colors->size())
        continue;
      value = (*colors)[i];
    }
    for (size_t k = 0; k < perPoint; ++k)
      lane[i * perPoint + k] = value;
  }
}

void drawBillboards(SkCanvas &canvas, const Cloud &cloud,
                    const space::Camera &camera, SkSize viewport,
                    const BillboardStyle &style) {
  const size_t n = cloud.size();
  if (n == 0)
    return;
  const glm::mat4 vp = camera.viewProjection(viewport);
  const glm::mat4 view = camera.view();

  struct Splat {
    SkPoint screen;
    float px;   // half-size in pixels
    float depth;
    glm::vec4 tint;
  };
  std::vector<Splat> splats;
  splats.reserve(n);

  const std::vector<float> *sizeLane =
      style.sizeLane.empty() ? nullptr : cloud.scalarIf(style.sizeLane);
  const std::vector<glm::vec4> *tintLane =
      style.tintLane.empty() ? nullptr : cloud.colorIf(style.tintLane);

  // Pixels per world unit at distance d: focal / d * (h/2).
  const float focal =
      1.0f / std::tan(camera.fovYDeg * (float)M_PI / 360.0f);
  const float halfH = viewport.height() * 0.5f;

  for (size_t i = 0; i < n; ++i) {
    const glm::vec3 &p = cloud.positions[i];
    const glm::vec4 clip = vp * glm::vec4{p, 1};
    if (clip.w <= 1e-4f)
      continue;
    const glm::vec4 eye = view * glm::vec4{p, 1};
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
    glm::vec4 tint = style.tint;
    if (tintLane && i < tintLane->size())
      tint *= (*tintLane)[i];
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
    const SkColor4f tintColor = {splat.tint.r, splat.tint.g,
                                 splat.tint.b, splat.tint.a};
    paint.setColorFilter(SkColorFilters::Blend(tintColor.toSkColor(),
                                               SkBlendMode::kModulate));
    const SkRect dst = SkRect::MakeXYWH(splat.screen.fX - splat.px,
                                        splat.screen.fY - splat.px,
                                        splat.px * 2, splat.px * 2);
    canvas.drawImageRect(sprite, dst, sampling, &paint);
  }
}

} // namespace points

} // namespace sigil::shape
