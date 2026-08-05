#include "sigilshape/Pop.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

#include "sigilshape/Geometry.h"
#include "sigilshape/detail/Hash.h"

namespace sigil::shape {

namespace {

/** The GPU kernels' hash, bit for bit (see world's pop kernels) —
 *  detail::pcgHash IS that hash; only the float squeeze (24 mantissa
 *  bits over 2^24) lives here, because it is what the kernels do. */
float hash1(uint32_t x) {
  return (float)(detail::pcgHash(x) & 0x00FFFFFFu) / 16777216.0f;
}

glm::vec3 drift(glm::vec3 p, float freq, float seed) {
  const float sx = std::sin(p.y * freq * 6.1f + seed) +
                   0.5f * std::sin(p.z * freq * 11.3f + seed * 1.7f);
  const float sy = std::sin(p.z * freq * 5.3f + seed * 2.1f) +
                   0.5f * std::sin(p.x * freq * 9.7f + seed);
  const float sz = std::sin(p.x * freq * 7.9f + seed * 1.3f) +
                   0.5f * std::sin(p.y * freq * 8.3f + seed * 2.6f);
  return glm::vec3{sx, sy, sz} * 0.6667f;
}

float wrap01(float t) { return t - std::floor(t); }

/** The attribute store: every attribute is a named float4 lane —
 *  builtins ("P", "T", "Dir", "Scale", "Color", "Tex") and customs
 *  alike. Customs spring into being on first touch. */
struct Attrs {
  size_t count = 0;
  std::map<std::string, std::vector<glm::vec4>, std::less<>> lanes;

  std::vector<glm::vec4>& ensure(const std::string& name, glm::vec4 fill) {
    auto [it, inserted] = lanes.try_emplace(name);
    if (inserted) it->second.assign(count, fill);
    return it->second;
  }
  glm::vec4 defaultFor(const std::string& name) const {
    if (name == "Scale" || name == "Color") return {1, 1, 1, 1};
    if (name == "Tex") return {0, 0, 1, 1};
    if (name == "Dir") return {0, 0, 1, 0};
    return {0, 0, 0, 0};
  }
  glm::vec4 load(const std::string& name, size_t i) {
    return ensure(name, defaultFor(name))[i];
  }
  void store(const std::string& name, size_t i, glm::vec4 v) {
    ensure(name, defaultFor(name))[i] = v;
  }
  glm::vec3 p3(size_t i) {
    const glm::vec4 v = load("P", i);
    return {v.x, v.y, v.z};
  }
};

}  // namespace

namespace popops {

Cloud cook(const pop::Chain& chain) {
  Cloud out;
  if (chain.empty()) return out;
  const auto* scatter = std::get_if<pop::SplineScatter>(&chain.front());
  const auto* surface = std::get_if<pop::MeshScatter>(&chain.front());
  if (scatter && (scatter->loop.size() < 3 || scatter->count < 1)) return out;
  if (surface && (surface->mesh.indices.empty() || surface->count < 1))
    return out;
  if (!scatter && !surface) return out;

  const size_t count = (size_t)(scatter ? scatter->count : surface->count);
  Attrs attrs;
  attrs.count = count;
  std::vector<glm::vec4>& laneP = attrs.ensure("P", {0, 0, 0, 0});
  std::vector<glm::vec4>& laneT = attrs.ensure("T", {0, 0, 0, 0});
  std::vector<glm::vec4>& laneDir = attrs.ensure("Dir", {0, 0, 1, 0});
  attrs.ensure("Scale", {1, 1, 1, 1});
  attrs.ensure("Color", {1, 1, 1, 1});

  if (surface) {
    const Cloud seeds =
        points::onMesh(surface->mesh, (int)count, surface->seed);
    const std::vector<glm::vec3>* normals = seeds.vectorIf("normal");
    for (size_t i = 0; i < count && i < seeds.size(); ++i) {
      const glm::vec3& p = seeds.positions[i];
      laneP[i] = {p.x, p.y, p.z, 0};
      laneT[i] = {((float)i + 0.5f) / (float)count, 0, 0, 0};
      if (normals) {
        const glm::vec3& n = (*normals)[i];
        laneDir[i] = {n.x, n.y, n.z, 0};
      }
    }
  }

  Spline3 spline;
  if (scatter) {
    spline.points = scatter->loop;
    spline.closed = true;
  }
  for (size_t i = 0; scatter && i < count; ++i) {
    const uint32_t seed = scatter->seed;
    const float u0 = ((float)i + 0.5f) / (float)count;
    const float t = scatter->head - scatter->span + scatter->span * u0 +
                    (hash1((uint32_t)i * 3u + seed) - 0.5f) *
                        (scatter->span / (float)count) * 4.0f;
    const glm::vec3 p = spline.position(wrap01(t));
    glm::vec3 tangent = spline.position(wrap01(t + 0.002f)) -
                        spline.position(wrap01(t - 0.002f));
    const float len = glm::length(tangent);
    tangent = len > 1e-6f ? tangent * (1.0f / len) : glm::vec3{1, 0, 0};
    glm::vec3 n0 = std::abs(tangent.y) < 0.9f ? glm::cross(tangent, {0, 1, 0})
                                              : glm::cross(tangent, {1, 0, 0});
    n0 = n0 * (1.0f / glm::length(n0));
    const glm::vec3 b0 = glm::cross(tangent, n0);
    const float ang = hash1((uint32_t)i * 7u + seed + 2u) * 6.2831853f;
    const float rad =
        std::sqrt(hash1((uint32_t)i * 5u + seed + 3u)) * scatter->radius;
    const glm::vec3 placed =
        p + (n0 * std::cos(ang) + b0 * std::sin(ang)) * rad;
    laneP[i] = {placed.x, placed.y, placed.z, 0};
    laneT[i] = {u0, 0, 0, 0};
    laneDir[i] = {tangent.x, tangent.y, tangent.z, 0};
  }

  for (size_t opIndex = 1; opIndex < chain.size(); ++opIndex) {
    std::visit(
        [&](const auto& op) {
          using T = std::decay_t<decltype(op)>;
          if constexpr (std::is_same_v<T, pop::SplineScatter> ||
                        std::is_same_v<T, pop::MeshScatter>) {
            // generators only lead a chain; ignore mid-chain
          } else if constexpr (std::is_same_v<T, pop::Promote>) {
            // The PRIMITIVE class: nothing to do on the point sink —
            // a Cloud has no primitives. cookMesh() reads these ops
            // back off the chain once the stamps exist.
          } else if constexpr (std::is_same_v<T, pop::Relax>) {
            // Neighborhood op: double-buffered, read-old/write-new —
            // the same shape the GPU's parallel pass has.
            for (int pass = 0; pass < op.iterations; ++pass) {
              std::vector<glm::vec4> next(count);
              for (size_t i = 0; i < count; ++i) {
                const size_t a = i == 0 ? 0 : i - 1;
                const size_t b = i + 1 < count ? i + 1 : i;
                const glm::vec4 mid = (attrs.load(op.lane.name, a) +
                                       attrs.load(op.lane.name, b)) *
                                      0.5f;
                const glm::vec4 v = attrs.load(op.lane.name, i);
                next[i] = v + (mid - v) * op.strength;
              }
              for (size_t i = 0; i < count; ++i)
                attrs.store(op.lane.name, i, next[i]);
            }
          } else if constexpr (std::is_same_v<T, pop::Lookup>) {
            // Per-point remap through a stop table. Written with the
            // same associations the kernel uses (the key's four
            // products summed left to right, the lerp as
            // a + (b - a) * f) — CSLookup is this function.
            const int n = (int)op.stops.size();
            if (n == 0) return;  // an empty table is a no-op on both executors
            const float denom = op.high - op.low;
            for (size_t i = 0; i < count; ++i) {
              const glm::vec4 s = attrs.load(op.from.name, i);
              const float key = s.x * op.weights.x + s.y * op.weights.y +
                                s.z * op.weights.z + s.w * op.weights.w;
              float u = denom != 0.0f ? (key - op.low) / denom : 0.0f;
              u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
              const float x = u * (float)(n - 1);
              int i0 = (int)std::floor(x);
              i0 = i0 < 0 ? 0 : (i0 > n - 1 ? n - 1 : i0);
              const int i1 = i0 + 1 < n ? i0 + 1 : n - 1;
              const float f = x - (float)i0;
              attrs.store(op.to.name, i,
                          op.stops[i0] + (op.stops[i1] - op.stops[i0]) * f);
            }
          } else if constexpr (std::is_same_v<T, pop::Sort>) {
            // The permutation class: EVERY lane travels with its
            // point, so the store stays coherent and only the order
            // changes. Stable, so equal keys keep their cooked order
            // and a re-cook is deterministic. Keys are read (and the
            // source lane thereby created) BEFORE the lanes are
            // permuted, so the map is not grown mid-walk.
            std::vector<float> keys(count);
            for (size_t i = 0; i < count; ++i) {
              const glm::vec4 v = attrs.load(op.by.name, i);
              const float k = v.x * op.weights.x + v.y * op.weights.y +
                              v.z * op.weights.z + v.w * op.weights.w;
              // A NaN key would break the comparator's strict weak
              // ordering outright (UB in stable_sort), so it sorts as
              // zero rather than corrupting the whole permutation.
              keys[i] = std::isfinite(k) ? k : 0.0f;
            }
            std::vector<uint32_t> order(count);
            std::iota(order.begin(), order.end(), 0u);
            std::stable_sort(
                order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
                  return op.descending ? keys[a] > keys[b] : keys[a] < keys[b];
                });
            std::vector<glm::vec4> next(count);
            for (auto& [name, lane] : attrs.lanes) {
              for (size_t i = 0; i < count; ++i) next[i] = lane[order[i]];
              lane = next;
            }
          } else if constexpr (std::is_same_v<T, pop::Set>) {
            for (size_t i = 0; i < count; ++i)
              attrs.store(op.attr.name, i, op.value);
          } else if constexpr (std::is_same_v<T, pop::Atlas>) {
            // Clamp once and use throughout: raw op.cols in the cell
            // remap is a division by zero on Atlas{0, ...} (the GPU
            // clamps). Formulas otherwise identical — GPU parity.
            const int cols = std::max(op.cols, 1);
            const int rows = std::max(op.rows, 1);
            const float du = 1.0f / (float)cols;
            const float dv = 1.0f / (float)rows;
            const int cells = std::max(cols * rows, 1);
            for (size_t i = 0; i < count; ++i) {
              const int cell = std::min(
                  (int)(hash1((uint32_t)i * 13u + op.seed) * (float)cells),
                  cells - 1);
              attrs.store("Tex", i,
                          {(float)(cell % cols) * du, (float)(cell / cols) * dv,
                           du, dv});
            }
          } else {
            for (size_t i = 0; i < count; ++i) {
              if constexpr (std::is_same_v<T, pop::Jitter>) {
                glm::vec4 v = attrs.load(op.lane.name, i);
                v.x += (hash1((uint32_t)i * 3u + op.seed) - 0.5f) * 2.0f *
                       op.amplitude;
                v.y += (hash1((uint32_t)i * 5u + op.seed + 1u) - 0.5f) * 2.0f *
                       op.amplitude;
                v.z += (hash1((uint32_t)i * 7u + op.seed + 2u) - 0.5f) * 2.0f *
                       op.amplitude;
                attrs.store(op.lane.name, i, v);
              } else if constexpr (std::is_same_v<T, pop::Noise>) {
                const glm::vec3 dd =
                    drift(attrs.p3(i), op.frequency, op.seed) * op.amplitude;
                glm::vec4 v = attrs.load(op.lane.name, i);
                v.x += dd.x;
                v.y += dd.y;
                v.z += dd.z;
                attrs.store(op.lane.name, i, v);
              } else if constexpr (std::is_same_v<T, pop::Ramp>) {
                const float t = attrs.load("T", i).x;
                attrs.store(op.lane.name, i, op.from + (op.to - op.from) * t);
              } else if constexpr (std::is_same_v<T, pop::Vary>) {
                const float v =
                    op.base *
                    (1.0f +
                     op.spread *
                         (hash1((uint32_t)i * 11u + op.seed) * 2.0f - 1.0f));
                attrs.store(op.lane.name, i, {v, v, v, v});
              } else if constexpr (std::is_same_v<T, pop::LookAt>) {
                glm::vec3 d = op.target - attrs.p3(i);
                const float len = glm::length(d);
                const glm::vec3 dir =
                    len > 1e-6f ? d * (1.0f / len) : glm::vec3{0, 0, 1};
                const float w = attrs.load("Dir", i).w;
                attrs.store("Dir", i, {dir.x, dir.y, dir.z, w});
              } else if constexpr (std::is_same_v<T, pop::Math>) {
                const glm::vec4 v = attrs.load(op.lane.name, i);
                attrs.store(op.lane.name, i, v * op.mul + op.add);
              }
            }
          }
        },
        chain[opIndex]);
  }

  // Export: builtins to the conventional Cloud lanes; everything
  // else (customs + "Tex") as four-wide color lanes under their own
  // names — nothing an op wrote is unreachable downstream.
  out.positions.resize(count);
  std::vector<float>& t = out.scalar("t");
  std::vector<glm::vec3>& dir = out.vector("dir");
  std::vector<float>& size = out.scalar("size", 1);
  std::vector<glm::vec4>& tint = out.color("tint");
  for (size_t i = 0; i < count; ++i) {
    const glm::vec4 p = attrs.lanes["P"][i];
    out.positions[i] = {p.x, p.y, p.z};
    t[i] = attrs.lanes["T"][i].x;
    const glm::vec4 d = attrs.lanes["Dir"][i];
    dir[i] = {d.x, d.y, d.z};
    size[i] = attrs.lanes["Scale"][i].x;
    tint[i] = attrs.lanes["Color"][i];
  }
  for (const auto& [name, lane] : attrs.lanes) {
    if (name == "P" || name == "T" || name == "Dir" || name == "Scale" ||
        name == "Color")
      continue;
    std::vector<glm::vec4>& exported = out.color(name);
    for (size_t i = 0; i < count; ++i) exported[i] = lane[i];
  }
  return out;
}

namespace {

/** pop's attribute names -> the Cloud lane cook() exported them under.
 *  The builtins land on the conventional lowercase lanes; "Tex" and
 *  every custom keep their own name. One table, so the prim class
 *  addresses attributes with exactly the same spelling the point class
 *  does. "Id" is reserved and handled by promoteToPrims. */
std::string cloudLaneFor(const std::string& attr) {
  if (attr == "T") return "t";
  if (attr == "Dir") return "dir";
  if (attr == "Scale") return "size";
  if (attr == "Color") return "tint";
  return attr;  // "P" has no lane; "Tex" and customs keep their names
}

}  // namespace

Mesh cookMesh(const pop::Chain& chain, const Mesh& stamp) {
  const Cloud cloud = cook(chain);
  points::InstanceOptions options;
  options.orientLane = "dir";
  options.scaleLane = "size";
  options.tintLane = "tint";
  Mesh out = points::instance(cloud, stamp, options);
  // The texture hint: "Tex" = {uOff, vOff, uScale, vScale} per point
  // remaps each stamped point's uv block — atlas selection, sprite
  // variety, per-point texture windows.
  if (const std::vector<glm::vec4>* tex = cloud.colorIf("Tex")) {
    const size_t stampVerts = stamp.vertexCount();
    // A uv-less stamp instances with an EMPTY uv lane; only remap
    // when the instanced uvs actually cover every stamped vertex.
    if (out.uvs.size() == cloud.size() * stampVerts) {
      for (size_t point = 0; point < cloud.size(); ++point) {
        const glm::vec4& cell = (*tex)[point];
        for (size_t v = 0; v < stampVerts; ++v) {
          glm::vec2& uv = out.uvs[point * stampVerts + v];
          uv = {cell.x + uv.x * cell.z, cell.y + uv.y * cell.w};
        }
      }
    }
  }
  // The PRIMITIVE class: every Promote op bakes a point lane onto the
  // stamped triangles. Each point owns stamp.triangleCount() of them,
  // which is exactly the run points::promoteToPrims addresses.
  for (const pop::Op& op : chain)
    if (const auto* promote = std::get_if<pop::Promote>(&op))
      points::promoteToPrims(
          out, cloud,
          promote->from.name == "Id" ? "Id" : cloudLaneFor(promote->from.name),
          promote->to.empty() ? promote->from.name : promote->to);
  return out;
}

namespace {

Spline3 pathThrough(const pop::Chain& chain, bool closed) {
  Spline3 path;
  path.points = cook(chain).positions;
  path.closed = closed;
  return path;
}

}  // namespace

Mesh cookTube(const pop::Chain& chain, float radius, int sides,
              const SweepSinkOptions& options) {
  const Spline3 path = pathThrough(chain, options.closed);
  if (path.points.size() < 2) return {};
  return curves::tube(
      path, {.radius = radius, .segments = options.segments, .sides = sides});
}

Mesh cookRibbon(const pop::Chain& chain, float width,
                const SweepSinkOptions& options) {
  const Spline3 path = pathThrough(chain, options.closed);
  if (path.points.size() < 2) return {};
  return curves::ribbon(path, {.width = width, .segments = options.segments});
}

Mesh cookSweep(const pop::Chain& chain, const SkPath& profile,
               const SweepSinkOptions& options) {
  const Spline3 path = pathThrough(chain, options.closed);
  if (path.points.size() < 2) return {};
  const std::vector<Polyline> contours = flatten(profile, 0.4f);
  if (contours.empty() || contours[0].points.size() < 3) return {};
  const std::vector<SkPoint>& ring = contours[0].points;
  const std::vector<Frame3> rail =
      curves::frames(path, std::max(options.segments, 2), {0, 1, 0});

  Mesh out;
  const uint32_t n = (uint32_t)ring.size();
  for (const Frame3& f : rail)
    for (uint32_t i = 0; i < n; ++i) {
      const SkPoint p = ring[i];
      // Profile is authored y-down (SkPath space); the frame's normal
      // is its "up", so y flips — same convention extrude() uses.
      out.positions.push_back(f.position + f.binormal * p.fX - f.normal * p.fY);
      out.uvs.push_back({(float)i / (float)n, f.t});
    }
  for (uint32_t s = 0; s + 1 < (uint32_t)rail.size(); ++s)
    for (uint32_t i = 0; i < n; ++i) {
      const uint32_t j = (i + 1) % n;
      const uint32_t a = s * n + i, b = s * n + j;
      const uint32_t c = (s + 1) * n + i, d = (s + 1) * n + j;
      out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
    }
  out.computeNormals();
  return out;
}

}  // namespace popops

}  // namespace sigil::shape
