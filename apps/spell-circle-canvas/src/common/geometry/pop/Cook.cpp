/** @file
 * The CPU reference executor: a point set laid out as named float4
 * lanes, every operator evaluated over them in chain order, and the
 * result poured into a Cloud under the conventional lane names. Its
 * formulas are the definition a GPU executor of the same chain must
 * reproduce bit for bit.
 */

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/pop/Pop.h"

namespace sigil::geometry {

namespace {

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

void seedAttrs(
    const Cloud& cloud,
    std::map<std::string, std::vector<glm::vec4>, std::less<>>& lanes) {
  const size_t n = cloud.size();
  const auto lane = [&](const std::string& name, glm::vec4 fill) -> auto& {
    auto [it, inserted] = lanes.try_emplace(name);
    if (inserted || it->second.size() != n) it->second.assign(n, fill);
    return it->second;
  };
  std::vector<glm::vec4>& P = lane("P", {0, 0, 0, 0});
  for (size_t i = 0; i < n; ++i)
    P[i] = {cloud.positions[i].x, cloud.positions[i].y, cloud.positions[i].z,
            0};
  lane("T", {0, 0, 0, 0});
  lane("Dir", {0, 0, 1, 0});
  lane("Scale", {1, 1, 1, 1});
  lane("Color", {1, 1, 1, 1});
  lane("Tex", {0, 0, 1, 1});
  for (const auto& [name, values] : cloud.scalars) {
    if (values.size() != n) continue;
    const std::string target = name == "t"      ? "T"
                               : name == "size" ? "Scale"
                                                : name;
    std::vector<glm::vec4>& out = lane(target, {0, 0, 0, 0});
    for (size_t i = 0; i < n; ++i)
      out[i] = target == "Scale"
                   ? glm::vec4{values[i], values[i], values[i], values[i]}
                   : glm::vec4{values[i], 0, 0, 0};
  }
  for (const auto& [name, values] : cloud.vectors) {
    if (values.size() != n) continue;
    // "dir" is the cook's own export; "normal" is what generators and
    // importers write. Either seeds Dir, "dir" winning when both exist.
    const bool isDir =
        name == "dir" || (name == "normal" && !cloud.vectorIf("dir"));
    const std::string target = isDir ? "Dir" : name;
    std::vector<glm::vec4>& out = lane(target, {0, 0, 1, 0});
    for (size_t i = 0; i < n; ++i)
      out[i] = {values[i].x, values[i].y, values[i].z, 0};
  }
  for (const auto& [name, values] : cloud.colors) {
    if (values.size() != n) continue;
    const std::string target = name == "tint" ? "Color" : name;
    lane(target, {1, 1, 1, 1}) = values;
  }
}

std::vector<std::string> seedCustomNames(const Cloud& cloud) {
  std::vector<std::string> names;
  const auto note = [&](const std::string& name) {
    if (pop::builtinIndex(name) >= 0) return;
    for (const std::string& existing : names)
      if (existing == name) return;
    names.push_back(name);
  };
  const size_t n = cloud.size();
  for (const auto& [name, values] : cloud.scalars)
    if (values.size() == n && name != "t" && name != "size") note(name);
  for (const auto& [name, values] : cloud.vectors)
    if (values.size() == n && name != "dir" &&
        (name != "normal" || cloud.vectorIf("dir")))
      note(name);
  for (const auto& [name, values] : cloud.colors)
    if (values.size() == n && name != "tint") note(name);
  return names;
}

void deformFrame(const pop::Deform& op, glm::vec3* axis, glm::vec3* direction,
                 glm::vec3* side) {
  glm::vec3 a = op.axis;
  const float al = glm::length(a);
  a = al > 1e-6f ? a / al : glm::vec3{0, 1, 0};
  glm::vec3 d = op.direction - a * glm::dot(op.direction, a);
  const float dl = glm::length(d);
  if (dl > 1e-6f) {
    d = d / dl;
  } else {
    d = std::abs(a.y) < 0.9f ? glm::cross(a, {0, 1, 0})
                             : glm::cross(a, {1, 0, 0});
    d = d / glm::length(d);
  }
  *axis = a;
  *direction = d;
  *side = glm::cross(a, d);
}

Cloud cook(const pop::Chain& chain) {
  Cloud out;
  if (chain.empty()) return out;
  const auto* scatter = std::get_if<pop::SplineScatter>(&chain.front());
  const auto* surface = std::get_if<pop::MeshScatter>(&chain.front());
  const auto* given = std::get_if<pop::PointSet>(&chain.front());
  if (scatter && (scatter->loop.size() < 3 || scatter->count < 1)) return out;
  if (surface && (surface->mesh.indices.empty() || surface->count < 1))
    return out;
  if (given && given->cloud.positions.empty()) return out;
  if (!scatter && !surface && !given) return out;

  const size_t count = (size_t)(scatter   ? scatter->count
                                : surface ? surface->count
                                          : (int)given->cloud.size());
  Attrs attrs;
  attrs.count = count;
  std::vector<glm::vec4>& laneP = attrs.ensure("P", {0, 0, 0, 0});
  std::vector<glm::vec4>& laneT = attrs.ensure("T", {0, 0, 0, 0});
  std::vector<glm::vec4>& laneDir = attrs.ensure("Dir", {0, 0, 1, 0});
  attrs.ensure("Scale", {1, 1, 1, 1});
  attrs.ensure("Color", {1, 1, 1, 1});

  if (given) popops::seedAttrs(given->cloud, attrs.lanes);

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
                    (noise::pcgUnit((uint32_t)i * 3u + seed) - 0.5f) *
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
    const float ang = noise::pcgUnit((uint32_t)i * 7u + seed + 2u) * 6.2831853f;
    const float rad = std::sqrt(noise::pcgUnit((uint32_t)i * 5u + seed + 3u)) *
                      scatter->radius;
    const glm::vec3 placed =
        p + (n0 * std::cos(ang) + b0 * std::sin(ang)) * rad;
    laneP[i] = {placed.x, placed.y, placed.z, 0};
    laneT[i] = {u0, 0, 0, 0};
    laneDir[i] = {tangent.x, tangent.y, tangent.z, 0};
  }

  // The mask read every filter shares: a lane's .x clamped to [0, 1],
  // 1 for the empty name. Written as one expression here and one in
  // the kernels — old + (new - old) * m, with m == 1 storing new
  // outright so an unmasked write is bit-identical to a direct one.
  const auto maskAt = [&](const std::string& mask, size_t i) -> float {
    if (mask.empty()) return 1.0f;
    const float m = attrs.load(mask, i).x;
    return m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
  };
  const auto storeMasked = [&](const std::string& lane, size_t i,
                               const std::string& mask, glm::vec4 v) {
    const float m = maskAt(mask, i);
    if (m >= 1.0f) {
      attrs.store(lane, i, v);
      return;
    }
    const glm::vec4 old = attrs.load(lane, i);
    attrs.store(lane, i, old + (v - old) * m);
  };

  for (size_t opIndex = 1; opIndex < chain.size(); ++opIndex) {
    std::visit(
        [&](const auto& op) {
          using T = std::decay_t<decltype(op)>;
          if constexpr (std::is_same_v<T, pop::SplineScatter> ||
                        std::is_same_v<T, pop::MeshScatter> ||
                        std::is_same_v<T, pop::PointSet>) {
            // generators only lead a chain; ignore mid-chain
          } else if constexpr (std::is_same_v<T, pop::Promote>) {
            // The PRIMITIVE class: nothing to do on the point sink —
            // a Cloud has no primitives. cookMesh() reads these ops
            // back off the chain once the stamps exist.
          } else if constexpr (std::is_same_v<T, pop::Relax>) {
            // Neighborhood op: double-buffered, read-old/write-new —
            // the same shape the GPU's parallel pass has. The mask
            // blends the relaxed value against the old one BEFORE the
            // scratch write, so a masked point's neighbours still see
            // its old value this pass, exactly like the kernel.
            for (int pass = 0; pass < op.iterations; ++pass) {
              std::vector<glm::vec4> next(count);
              for (size_t i = 0; i < count; ++i) {
                const size_t a = i == 0 ? 0 : i - 1;
                const size_t b = i + 1 < count ? i + 1 : i;
                const glm::vec4 mid = (attrs.load(op.lane.name, a) +
                                       attrs.load(op.lane.name, b)) *
                                      0.5f;
                const glm::vec4 v = attrs.load(op.lane.name, i);
                const glm::vec4 relaxed = v + (mid - v) * op.strength;
                const float m = maskAt(op.mask, i);
                next[i] = m >= 1.0f ? relaxed : v + (relaxed - v) * m;
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
              storeMasked(op.to.name, i, op.mask,
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
          } else if constexpr (std::is_same_v<T, pop::Fill>) {
            for (size_t i = 0; i < count; ++i)
              storeMasked(op.attr.name, i, op.mask, op.value);
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
              const int cell =
                  std::min((int)(noise::pcgUnit((uint32_t)i * 13u + op.seed) *
                                 (float)cells),
                           cells - 1);
              storeMasked("Tex", i, op.mask,
                          {(float)(cell % cols) * du, (float)(cell / cols) * dv,
                           du, dv});
            }
          } else if constexpr (std::is_same_v<T, pop::Select>) {
            // The selector: distance in the region's own units, a
            // smoothstep band across the outer `feather` fraction, an
            // optional flip, then combined into the lane. The
            // smoothstep is spelled out (t * t * (3 - 2t)) so both
            // executors run the same arithmetic.
            const glm::vec3 size = {op.size.x != 0.0f ? op.size.x : 1e-6f,
                                    op.size.y != 0.0f ? op.size.y : 1e-6f,
                                    op.size.z != 0.0f ? op.size.z : 1e-6f};
            const float f = op.feather < 0.0f
                                ? 0.0f
                                : (op.feather > 1.0f ? 1.0f : op.feather);
            for (size_t i = 0; i < count; ++i) {
              const glm::vec4 s = attrs.load(op.from.name, i);
              const glm::vec3 q = {(s.x - op.center.x) / size.x,
                                   (s.y - op.center.y) / size.y,
                                   (s.z - op.center.z) / size.z};
              const float d =
                  op.shape == pop::Select::Shape::Sphere
                      ? std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z)
                      : std::max(std::abs(q.x),
                                 std::max(std::abs(q.y), std::abs(q.z)));
              float inside;
              if (f <= 0.0f) {
                inside = d <= 1.0f ? 1.0f : 0.0f;
              } else {
                float t = (d - (1.0f - f)) / f;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                inside = 1.0f - t * t * (3.0f - 2.0f * t);
              }
              if (op.invert) inside = 1.0f - inside;
              const glm::vec4 old = attrs.load(op.to, i);
              float v = inside;
              switch (op.combine) {
                case pop::Select::Combine::Replace:
                  break;
                case pop::Select::Combine::Union:
                  v = std::max(old.x, inside);
                  break;
                case pop::Select::Combine::Intersect:
                  v = std::min(old.x, inside);
                  break;
                case pop::Select::Combine::Subtract:
                  v = old.x * (1.0f - inside);
                  break;
              }
              attrs.store(op.to, i, {v, v, v, v});
            }
          } else if constexpr (std::is_same_v<T, pop::Affine>) {
            for (size_t i = 0; i < count; ++i) {
              const glm::vec4 v = attrs.load(op.lane.name, i);
              glm::vec4 r;
              if (op.direction) {
                const glm::vec4 t = op.matrix * glm::vec4{v.x, v.y, v.z, 0.0f};
                const float len = std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
                r = len > 1e-6f
                        ? glm::vec4{t.x / len, t.y / len, t.z / len, v.w}
                        : glm::vec4{v.x, v.y, v.z, v.w};
              } else {
                const glm::vec4 t = op.matrix * glm::vec4{v.x, v.y, v.z, 1.0f};
                r = {t.x, t.y, t.z, v.w};
              }
              storeMasked(op.lane.name, i, op.mask, r);
            }
          } else if constexpr (std::is_same_v<T, pop::Peak>) {
            for (size_t i = 0; i < count; ++i) {
              const glm::vec4 a = attrs.load(op.along.name, i);
              const float len = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
              glm::vec4 v = attrs.load(op.lane.name, i);
              if (len > 1e-6f) {
                const float k = op.distance / len;
                v.x += a.x * k;
                v.y += a.y * k;
                v.z += a.z * k;
              }
              storeMasked(op.lane.name, i, op.mask, v);
            }
          } else if constexpr (std::is_same_v<T, pop::Deform>) {
            // The frame: a unit axis, plus for Bend a unit direction
            // made perpendicular to it. Degenerate inputs (a zero
            // axis, a direction parallel to the axis) fall back the
            // same way on both executors.
            glm::vec3 axis, dir, side;
            deformFrame(op, &axis, &dir, &side);
            const float span = op.high - op.low;
            const float rad = op.amount * 3.14159265f / 180.0f;
            for (size_t i = 0; i < count; ++i) {
              const glm::vec4 v = attrs.load(op.lane.name, i);
              const glm::vec3 p = glm::vec3{v.x, v.y, v.z} - op.origin;
              const float h = glm::dot(p, axis);
              const glm::vec3 perp = p - axis * h;
              float u = span != 0.0f ? (h - op.low) / span
                                     : (h >= op.low ? 1.0f : 0.0f);
              u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
              glm::vec3 out;
              if (op.kind == pop::Deform::Kind::Twist) {
                const float ang = rad * u;
                const float c = std::cos(ang), sn = std::sin(ang);
                // Rodrigues about the unit axis; perp is already
                // perpendicular so the parallel term is zero.
                out = axis * h + perp * c + glm::cross(axis, perp) * sn;
              } else if (op.kind == pop::Deform::Kind::Taper) {
                out = axis * h + perp * (1.0f + (op.amount - 1.0f) * u);
              } else {
                // Bend: the band becomes an arc of `rad` radians and
                // length `span`, curving toward dir. Below the band
                // nothing moves; on it, the axis coordinate walks the
                // arc; above it, the point rides the arc's end
                // tangent. The x offset toward dir bends with the
                // arc (points on the outside stretch, inside
                // compress); the offset along side is carried over.
                const float x = glm::dot(perp, dir);
                const float y = glm::dot(perp, side);
                if (rad == 0.0f || span == 0.0f) {
                  out = p;
                } else {
                  const float R = span / rad;
                  const float hb =
                      h < op.low ? op.low : (h > op.high ? op.high : h);
                  const float theta = (hb - op.low) / R;
                  const float c = std::cos(theta), sn = std::sin(theta);
                  // Arc centre sits at +R along dir from (low). A
                  // point at height hb and offset x lands at
                  //   along axis: low + (R - x) * sin(theta)
                  //   along dir:  R - (R - x) * cos(theta)
                  const float extra = h - hb;  // rigid overhang
                  const float hOut = op.low + (R - x) * sn + extra * c;
                  const float xOut = R - (R - x) * c + extra * sn;
                  out = axis * hOut + dir * xOut + side * y;
                }
              }
              out += op.origin;
              storeMasked(op.lane.name, i, op.mask, {out.x, out.y, out.z, v.w});
            }
          } else if constexpr (std::is_same_v<T, pop::Mix>) {
            for (size_t i = 0; i < count; ++i) {
              const glm::vec4 a = attrs.load(op.a.name, i);
              const glm::vec4 b = attrs.load(op.b.name, i);
              const float f = op.factorLane.empty()
                                  ? op.factor
                                  : attrs.load(op.factorLane, i).x;
              storeMasked(op.to.name, i, op.mask, a + (b - a) * f);
            }
          } else {
            for (size_t i = 0; i < count; ++i) {
              if constexpr (std::is_same_v<T, pop::Jitter>) {
                glm::vec4 v = attrs.load(op.lane.name, i);
                v.x += (noise::pcgUnit((uint32_t)i * 3u + op.seed) - 0.5f) *
                       2.0f * op.amplitude;
                v.y +=
                    (noise::pcgUnit((uint32_t)i * 5u + op.seed + 1u) - 0.5f) *
                    2.0f * op.amplitude;
                v.z +=
                    (noise::pcgUnit((uint32_t)i * 7u + op.seed + 2u) - 0.5f) *
                    2.0f * op.amplitude;
                storeMasked(op.lane.name, i, op.mask, v);
              } else if constexpr (std::is_same_v<T, pop::Noise>) {
                const glm::vec3 dd =
                    drift(attrs.p3(i), op.frequency, op.seed) * op.amplitude;
                glm::vec4 v = attrs.load(op.lane.name, i);
                v.x += dd.x;
                v.y += dd.y;
                v.z += dd.z;
                storeMasked(op.lane.name, i, op.mask, v);
              } else if constexpr (std::is_same_v<T, pop::Ramp>) {
                const float t = attrs.load("T", i).x;
                storeMasked(op.lane.name, i, op.mask,
                            op.from + (op.to - op.from) * t);
              } else if constexpr (std::is_same_v<T, pop::Vary>) {
                const float v =
                    op.base *
                    (1.0f +
                     op.spread *
                         (noise::pcgUnit((uint32_t)i * 11u + op.seed) * 2.0f -
                          1.0f));
                storeMasked(op.lane.name, i, op.mask, {v, v, v, v});
              } else if constexpr (std::is_same_v<T, pop::LookAt>) {
                glm::vec3 d = op.target - attrs.p3(i);
                const float len = glm::length(d);
                const glm::vec3 dir =
                    len > 1e-6f ? d * (1.0f / len) : glm::vec3{0, 0, 1};
                const float w = attrs.load("Dir", i).w;
                storeMasked("Dir", i, op.mask, {dir.x, dir.y, dir.z, w});
              } else if constexpr (std::is_same_v<T, pop::Math>) {
                const glm::vec4 v = attrs.load(op.lane.name, i);
                storeMasked(op.lane.name, i, op.mask, v * op.mul + op.add);
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
}  // namespace popops

}  // namespace sigil::geometry
