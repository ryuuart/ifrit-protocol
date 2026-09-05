/** @file
 * The built-in executor and the Runtime value that carries it: a point
 * set laid out as named float4 lanes, every operator evaluated over
 * them in chain order, and the result poured into a Cloud under the
 * conventional lane names.
 *
 * WHERE THE ARITHMETIC IS. An operator whose body is a pure function of
 * one point is not written here: it is written once in the kernel this
 * feature compiles, and this executor calls that kernel's own generated
 * C++. What is left in this file is what no per-point kernel can be —
 * the generators, the neighbourhood pass, the permutation, the primitive
 * class, and the operators whose definition calls for a library sine.
 */

#include <sigilcore/compute/Noise.h>
#include <sigilcore/schedule/Parallel.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>

#include "sigilgeometry/mesh/pop/Kernel.h"
#include "sigilgeometry/mesh/pop/Pop.h"

namespace sigil::geometry::mesh {

// The tier's other features this file stands on, pulled in so the code
// below reads as one vocabulary.
using curve::Spline3;

namespace {

/** How many points one worker takes at a time when a pass is divided.
 *  A pass over lanes is a handful of arithmetic operations per point, so
 *  handing a chunk to a worker is only worth it for many thousands of
 *  them; below that the whole range stays on the calling thread. A cook
 *  divides its own passes at the grain its executor was given, and this
 *  is what the built-in one carries. */
constexpr size_t kLaneGrain = 4096;

float wrap01(float t) { return t - std::floor(t); }

/** The attribute store: every attribute is a named float4 lane —
 *  builtins ("P", "T", "Dir", "Scale", "Color", "Tex") and customs
 *  alike. Customs spring into being on first touch. */
struct Attrs {
  size_t count = 0;
  pop::Lanes lanes;

  std::vector<glm::vec4>& ensure(const std::string& name) {
    auto [it, inserted] = lanes.try_emplace(name);
    if (inserted) it->second.assign(count, pop::laneFill(name));
    return it->second;
  }
  glm::vec4 load(const std::string& name, size_t i) { return ensure(name)[i]; }
  void store(const std::string& name, size_t i, glm::vec4 v) {
    ensure(name)[i] = v;
  }
  glm::vec3 p3(size_t i) {
    const glm::vec4 v = load("P", i);
    return {v.x, v.y, v.z};
  }
};

/** ONE OPERATOR RUN THROUGH ITS KERNEL, on this tier: the lanes the
 *  dispatch named, created if this is where they first appear, and the
 *  kernel called over them. A role the operator does not read is handed
 *  the destination, which the kernel never looks at. */
void runKernel(Attrs& attrs, const kernel::Dispatch& work) {
  glm::vec4* const dst = attrs.ensure(work.dst).data();
  const auto lane = [&](const std::string& name) -> glm::vec4* {
    return name.empty() ? dst : attrs.ensure(name).data();
  };
  // Read in this order and not inside the call: a lane created here can
  // be the one another role names, and every one of them must exist
  // before any of their addresses is handed over.
  glm::vec4* const a = lane(work.a);
  glm::vec4* const b = lane(work.b);
  glm::vec4* const c = lane(work.c);
  glm::vec4* const mask = lane(work.mask);
  kernel::run(work, dst, a, b, c, mask);
}

}  // namespace

glm::vec3 pop::noiseField(glm::vec3 p, float freq, float seed) {
  const float sx = std::sin(p.y * freq * 6.1f + seed) +
                   0.5f * std::sin(p.z * freq * 11.3f + seed * 1.7f);
  const float sy = std::sin(p.z * freq * 5.3f + seed * 2.1f) +
                   0.5f * std::sin(p.x * freq * 9.7f + seed);
  const float sz = std::sin(p.x * freq * 7.9f + seed * 1.3f) +
                   0.5f * std::sin(p.y * freq * 8.3f + seed * 2.6f);
  return glm::vec3{sx, sy, sz} * 0.6667f;
}

glm::vec4 pop::laneFill(std::string_view name) {
  if (name == "Scale" || name == "Color") return {1, 1, 1, 1};
  if (name == "Tex") return {0, 0, 1, 1};
  if (name == "Dir") return {0, 0, 1, 0};
  return {0, 0, 0, 0};
}

std::string_view pop::attrFor(std::string_view lane) {
  if (lane == "t") return "T";
  if (lane == "size") return "Scale";
  if (lane == "dir" || lane == "normal") return "Dir";
  if (lane == "tint") return "Color";
  return lane;
}

std::string_view pop::cloudLaneFor(std::string_view attr) {
  if (attr == "T") return "t";
  if (attr == "Scale") return "size";
  if (attr == "Dir") return "dir";
  if (attr == "Color") return "tint";
  return attr;
}

void pop::seedAttrs(const Cloud& cloud, pop::Lanes& lanes) {
  const size_t n = cloud.size();
  const auto lane = [&](const std::string& name, glm::vec4 fill) -> auto& {
    auto [it, inserted] = lanes.try_emplace(name);
    if (inserted || it->second.size() != n) it->second.assign(n, fill);
    return it->second;
  };
  std::vector<glm::vec4>& P = lane("P", {0, 0, 0, 0});
  core::schedule::parallelFor(n, kLaneGrain, [&](size_t first, size_t last) {
    for (size_t i = first; i < last; ++i)
      P[i] = {cloud.positions[i].x, cloud.positions[i].y, cloud.positions[i].z,
              0};
  });
  lane("T", {0, 0, 0, 0});
  lane("Dir", {0, 0, 1, 0});
  lane("Scale", {1, 1, 1, 1});
  lane("Color", {1, 1, 1, 1});
  lane("Tex", {0, 0, 1, 1});
  for (const auto& [name, values] : cloud.scalars) {
    if (values.size() != n) continue;
    const std::string target(attrFor(name));
    std::vector<glm::vec4>& out = lane(target, {0, 0, 0, 0});
    core::schedule::parallelFor(n, kLaneGrain, [&](size_t first, size_t last) {
      for (size_t i = first; i < last; ++i)
        out[i] = target == "Scale"
                     ? glm::vec4{values[i], values[i], values[i], values[i]}
                     : glm::vec4{values[i], 0, 0, 0};
    });
  }
  for (const auto& [name, values] : cloud.vectors) {
    if (values.size() != n) continue;
    // "dir" is the cook's own export; "normal" is what generators and
    // importers write. The table maps either onto Dir, so "dir" has to
    // win where both exist.
    const bool skip = name == "normal" && cloud.vectorIf("dir");
    const std::string target(skip ? std::string_view(name) : attrFor(name));
    std::vector<glm::vec4>& out = lane(target, {0, 0, 1, 0});
    core::schedule::parallelFor(n, kLaneGrain, [&](size_t first, size_t last) {
      for (size_t i = first; i < last; ++i)
        out[i] = {values[i].x, values[i].y, values[i].z, 0};
    });
  }
  for (const auto& [name, values] : cloud.colors) {
    if (values.size() != n) continue;
    const std::string target(attrFor(name));
    lane(target, {1, 1, 1, 1}) = values;
  }
}

std::vector<std::string> pop::seedCustomNames(const Cloud& cloud) {
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

void pop::deformFrame(const pop::Deform& op, glm::vec3* axis,
                      glm::vec3* direction, glm::vec3* side) {
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

size_t pop::seedLanes(const pop::Chain& chain, pop::Lanes* lanes) {
  if (!lanes || chain.empty()) return 0;
  const auto* scatter = std::get_if<pop::SplineScatter>(&chain.front());
  const auto* surface = std::get_if<pop::MeshScatter>(&chain.front());
  const auto* given = std::get_if<pop::PointSet>(&chain.front());
  if (scatter && (scatter->loop.size() < 3 || scatter->count < 1)) return 0;
  if (surface && (surface->mesh.indices.empty() || surface->count < 1))
    return 0;
  if (given && given->cloud.positions.empty()) return 0;
  if (!scatter && !surface && !given) return 0;

  const size_t count = (size_t)(scatter   ? scatter->count
                                : surface ? surface->count
                                          : (int)given->cloud.size());
  const auto lane = [&](const std::string& name) -> auto& {
    auto [it, inserted] = lanes->try_emplace(name);
    if (inserted) it->second.assign(count, pop::laneFill(name));
    return it->second;
  };
  std::vector<glm::vec4>& laneP = lane("P");
  std::vector<glm::vec4>& laneT = lane("T");
  std::vector<glm::vec4>& laneDir = lane("Dir");
  lane("Scale");
  lane("Color");

  if (given) pop::seedAttrs(given->cloud, *lanes);

  if (surface) {
    const Cloud seeds =
        points::onMesh(surface->mesh, (int)count, surface->seed);
    const std::vector<glm::vec3>* normals = seeds.vectorIf("normal");
    const size_t copied = std::min(count, seeds.size());
    core::schedule::parallelFor(
        copied, kLaneGrain, [&](size_t first, size_t last) {
          for (size_t i = first; i < last; ++i) {
            const glm::vec3& p = seeds.positions[i];
            laneP[i] = {p.x, p.y, p.z, 0};
            laneT[i] = {((float)i + 0.5f) / (float)count, 0, 0, 0};
            if (normals) {
              const glm::vec3& n = (*normals)[i];
              laneDir[i] = {n.x, n.y, n.z, 0};
            }
          }
        });
  }

  if (scatter) {
    Spline3 spline;
    spline.points = scatter->loop;
    spline.closed = true;
    core::schedule::parallelFor(
        count, kLaneGrain, [&](size_t first, size_t last) {
          for (size_t i = first; i < last; ++i) {
            const uint32_t seed = scatter->seed;
            const float u0 = ((float)i + 0.5f) / (float)count;
            const float t =
                scatter->head - scatter->span + scatter->span * u0 +
                (core::noise::pcgUnit((uint32_t)i * 3u + seed) - 0.5f) *
                    (scatter->span / (float)count) * 4.0f;
            const glm::vec3 p = spline.position(wrap01(t));
            glm::vec3 tangent = spline.position(wrap01(t + 0.002f)) -
                                spline.position(wrap01(t - 0.002f));
            const float len = glm::length(tangent);
            tangent = len > 1e-6f ? tangent * (1.0f / len) : glm::vec3{1, 0, 0};
            glm::vec3 n0 = std::abs(tangent.y) < 0.9f
                               ? glm::cross(tangent, {0, 1, 0})
                               : glm::cross(tangent, {1, 0, 0});
            n0 = n0 * (1.0f / glm::length(n0));
            const glm::vec3 b0 = glm::cross(tangent, n0);
            const float ang =
                core::noise::pcgUnit((uint32_t)i * 7u + seed + 2u) * 6.2831853f;
            const float rad =
                std::sqrt(core::noise::pcgUnit((uint32_t)i * 5u + seed + 3u)) *
                scatter->radius;
            const glm::vec3 placed =
                p + (n0 * std::cos(ang) + b0 * std::sin(ang)) * rad;
            laneP[i] = {placed.x, placed.y, placed.z, 0};
            laneT[i] = {u0, 0, 0, 0};
            laneDir[i] = {tangent.x, tangent.y, tangent.z, 0};
          }
        });
  }
  return count;
}

Cloud pop::exportLanes(const pop::Lanes& lanes, size_t count) {
  Cloud out;
  if (count == 0) return out;
  const auto find = [&](const char* name) -> const std::vector<glm::vec4>* {
    const auto it = lanes.find(std::string_view(name));
    return it == lanes.end() ? nullptr : &it->second;
  };
  const std::vector<glm::vec4>* P = find("P");
  const std::vector<glm::vec4>* T = find("T");
  const std::vector<glm::vec4>* Dir = find("Dir");
  const std::vector<glm::vec4>* Scale = find("Scale");
  const std::vector<glm::vec4>* Color = find("Color");
  if (!P || !T || !Dir || !Scale || !Color) return out;

  out.positions.resize(count);
  std::vector<float>& t = out.scalar(std::string(cloudLaneFor("T")));
  std::vector<glm::vec3>& dir = out.vector(std::string(cloudLaneFor("Dir")));
  std::vector<float>& size = out.scalar(std::string(cloudLaneFor("Scale")), 1);
  std::vector<glm::vec4>& tint = out.color(std::string(cloudLaneFor("Color")));
  core::schedule::parallelFor(count, kLaneGrain,
                              [&](size_t first, size_t last) {
                                for (size_t i = first; i < last; ++i) {
                                  const glm::vec4 p = (*P)[i];
                                  out.positions[i] = {p.x, p.y, p.z};
                                  t[i] = (*T)[i].x;
                                  const glm::vec4 d = (*Dir)[i];
                                  dir[i] = {d.x, d.y, d.z};
                                  size[i] = (*Scale)[i].x;
                                  tint[i] = (*Color)[i];
                                }
                              });
  for (const auto& [name, lane] : lanes) {
    if (name == "P" || name == "T" || name == "Dir" || name == "Scale" ||
        name == "Color")
      continue;
    std::vector<glm::vec4>& exported = out.color(name);
    for (size_t i = 0; i < count; ++i) exported[i] = lane[i];
  }
  return out;
}

namespace {

/** The built-in executor's body. @p grain is how many points one worker
 *  takes at a time; it changes nothing about the cloud, only how the
 *  passes are divided. */
Cloud cookOnCpu(const pop::Chain& chain, size_t grain) {
  Attrs attrs;
  attrs.count = pop::seedLanes(chain, &attrs.lanes);
  if (attrs.count == 0) return {};
  // Not const: the SET class changes how many points there are, and
  // every operator after one of those addresses the new count.
  size_t count = attrs.count;

  for (size_t opIndex = 1; opIndex < chain.size(); ++opIndex) {
    // THE KERNEL FIRST. An operator that has one is arithmetic this file
    // does not hold a second copy of, and the dispatch is the same
    // description a device executor is handed.
    kernel::Dispatch work;
    if (kernel::describe(chain[opIndex], count, &work)) {
      runKernel(attrs, work);
      continue;
    }
    std::visit(
        [&](const auto& op) {
          using T = std::decay_t<decltype(op)>;
          if constexpr (std::is_same_v<T, pop::SplineScatter> ||
                        std::is_same_v<T, pop::MeshScatter> ||
                        std::is_same_v<T, pop::PointSet> ||
                        std::is_same_v<T, pop::Promote>) {
            // Generators only lead a chain and are ignored mid-chain.
            // Promote is the PRIMITIVE class: nothing to do on the point
            // sink — a Cloud has no primitives. cookMesh() reads these
            // ops back off the chain once the stamps exist.
          } else if constexpr (std::is_same_v<T, pop::Relax>) {
            // Neighborhood op: double-buffered, read-old/write-new — the
            // shape a parallel pass would have too, and the reason there
            // is no kernel for it: a point reads two it does not own, so
            // one lane cannot be both what is read and what is written.
            // The mask blends the relaxed value against the old one
            // BEFORE the scratch write, so a masked point's neighbours
            // still see its old value this pass.
            std::vector<glm::vec4>& values = attrs.ensure(op.lane.name);
            const std::vector<glm::vec4>* mask =
                op.mask.empty() ? nullptr : &attrs.ensure(op.mask);
            for (int pass = 0; pass < op.iterations; ++pass) {
              std::vector<glm::vec4> next(count);
              core::schedule::parallelFor(
                  count, grain, [&](size_t first, size_t last) {
                    for (size_t i = first; i < last; ++i) {
                      const size_t a = i == 0 ? 0 : i - 1;
                      const size_t b = i + 1 < count ? i + 1 : i;
                      const glm::vec4 mid = (values[a] + values[b]) * 0.5f;
                      const glm::vec4 v = values[i];
                      const glm::vec4 relaxed = v + (mid - v) * op.strength;
                      float m = 1.0f;
                      if (mask) {
                        const float raw = (*mask)[i].x;
                        m = raw < 0.0f ? 0.0f : (raw > 1.0f ? 1.0f : raw);
                      }
                      next[i] = m >= 1.0f ? relaxed : v + (relaxed - v) * m;
                    }
                  });
              values.swap(next);
            }
          } else if constexpr (std::is_same_v<T, pop::Sort>) {
            // The permutation class: EVERY lane travels with its
            // point, so the store stays coherent and only the order
            // changes. Stable, so equal keys keep their cooked order
            // and a re-cook is deterministic. Keys are read (and the
            // source lane thereby created) BEFORE the lanes are
            // permuted, so the map is not grown mid-walk.
            std::vector<float> keys(count);
            const std::vector<glm::vec4>& values = attrs.ensure(op.by.name);
            core::schedule::parallelFor(
                count, grain, [&](size_t first, size_t last) {
                  for (size_t i = first; i < last; ++i) {
                    const glm::vec4 v = values[i];
                    const float k = v.x * op.weights.x + v.y * op.weights.y +
                                    v.z * op.weights.z + v.w * op.weights.w;
                    // A NaN key would break the comparator's strict weak
                    // ordering outright (UB in stable_sort), so it sorts as
                    // zero rather than corrupting the whole permutation.
                    keys[i] = std::isfinite(k) ? k : 0.0f;
                  }
                });
            std::vector<uint32_t> order(count);
            std::iota(order.begin(), order.end(), 0u);
            std::stable_sort(
                order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
                  return op.descending ? keys[a] > keys[b] : keys[a] < keys[b];
                });
            std::vector<glm::vec4> next(count);
            for (auto& [name, lane] : attrs.lanes) {
              core::schedule::parallelFor(
                  count, grain, [&](size_t first, size_t last) {
                    for (size_t i = first; i < last; ++i)
                      next[i] = lane[order[i]];
                  });
              lane = next;
            }
          } else if constexpr (std::is_same_v<T, pop::Delete>) {
            // The SET class, and the only operator that changes the
            // count. Every lane is compacted through one permutation,
            // so the store stays coherent and nothing but its length
            // moves. An unnamed mask deletes nothing: an operator that
            // emptied the set by omission is not one anybody wants.
            if (op.mask.empty()) return;
            const std::vector<glm::vec4>& mask = attrs.ensure(op.mask);
            std::vector<uint32_t> kept;
            kept.reserve(count);
            for (size_t i = 0; i < count; ++i) {
              const bool named = mask[i].x >= op.threshold;
              if (named == op.keep) kept.push_back((uint32_t)i);
            }
            for (auto& [name, lane] : attrs.lanes) {
              std::vector<glm::vec4> next(kept.size());
              core::schedule::parallelFor(
                  kept.size(), grain, [&](size_t first, size_t last) {
                    for (size_t i = first; i < last; ++i)
                      next[i] = lane[kept[i]];
                  });
              lane = std::move(next);
            }
            count = kept.size();
            attrs.count = count;
          } else if constexpr (std::is_same_v<T, pop::Deform>) {
            // The frame: a unit axis, plus for Bend a unit direction
            // made perpendicular to it. Degenerate inputs (a zero
            // axis, a direction parallel to the axis) fall back the
            // same way wherever this is evaluated.
            glm::vec3 axis, dir, side;
            pop::deformFrame(op, &axis, &dir, &side);
            const float span = op.high - op.low;
            const float rad = op.amount * 3.14159265f / 180.0f;
            std::vector<glm::vec4>& values = attrs.ensure(op.lane.name);
            const std::vector<glm::vec4>* mask =
                op.mask.empty() ? nullptr : &attrs.ensure(op.mask);
            core::schedule::parallelFor(
                count, grain, [&](size_t first, size_t last) {
                  for (size_t i = first; i < last; ++i) {
                    const glm::vec4 v = values[i];
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
                    const glm::vec4 result{out.x, out.y, out.z, v.w};
                    float m = mask ? (*mask)[i].x : 1.0f;
                    m = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
                    values[i] = m >= 1.0f ? result : v + (result - v) * m;
                  }
                });
          } else if constexpr (std::is_same_v<T, pop::Noise>) {
            // A field of library sines. There is no kernel for it: a
            // polynomial sine is a different function from a library
            // one, not a rounding of it, so a kernel would change what
            // this operator MEANS rather than where it runs.
            std::vector<glm::vec4>& values = attrs.ensure(op.lane.name);
            const std::vector<glm::vec4>* mask =
                op.mask.empty() ? nullptr : &attrs.ensure(op.mask);
            core::schedule::parallelFor(
                count, grain, [&](size_t first, size_t last) {
                  for (size_t i = first; i < last; ++i) {
                    const glm::vec4 old = values[i];
                    const glm::vec3 dd =
                        pop::noiseField({old.x, old.y, old.z}, op.frequency,
                                        op.seed) *
                        op.amplitude;
                    glm::vec4 v = old;
                    v.x += dd.x;
                    v.y += dd.y;
                    v.z += dd.z;
                    float m = mask ? (*mask)[i].x : 1.0f;
                    m = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
                    values[i] = m >= 1.0f ? v : old + (v - old) * m;
                  }
                });
          }
        },
        chain[opIndex]);
  }

  return pop::exportLanes(attrs.lanes, count);
}

/** The built-in executor: every operator on the CPU, and the cloud
 *  every other executor is measured against. */
struct CpuExecutor : pop::Executor {
  // The grain is the whole of its state, so two executors given the same
  // one are the same value and two default cooks compare equal. (A
  // defaulted comparison cannot say so — the abstract base it derives
  // from has none.)
  CpuExecutor() = default;
  explicit CpuExecutor(size_t itemGrain) : grain(itemGrain) {}

  size_t grain = kLaneGrain;
  bool operator==(const CpuExecutor& other) const {
    return grain == other.grain;
  }

  std::string name() const override { return "cpu"; }
  // The reference runs the whole vocabulary; there is no operator for
  // it to decline, because it is what "supported" is defined against.
  bool supports(const pop::Op&) const override { return true; }
  Cloud cook(const pop::Chain& chain) const override {
    return cookOnCpu(chain, grain);
  }
};

}  // namespace

pop::Runtime pop::Runtime::cpu() {
  static const pop::Runtime kCpu{CpuExecutor{}};
  return kCpu;
}

pop::Runtime pop::Runtime::cpu(size_t itemGrain) {
  return pop::Runtime{CpuExecutor{itemGrain}};
}

Cloud pop::cook(const pop::Chain& chain, const pop::Runtime& runtime) {
  if (!runtime) return {};
  // Asked HERE rather than left to the executor, so every runtime gets
  // the same guarantee and the same message. A chain short one operator
  // cooks a cloud that looks right and is not the described one, which
  // is the one failure a caller cannot see.
  for (const pop::Op& op : chain)
    if (!runtime->supports(op))
      throw std::runtime_error("the \"" + runtime->name() +
                               "\" pop runtime cannot run the \"" +
                               std::string(pop::opName(op)) + "\" operator");
  return runtime->cook(chain);
}

}  // namespace sigil::geometry::mesh
