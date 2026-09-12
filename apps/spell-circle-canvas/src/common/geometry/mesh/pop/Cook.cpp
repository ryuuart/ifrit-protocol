/** @file
 * The built-in executor and the Runtime value that carries it: the
 * operators evaluated over the store in chain order, and the door that
 * refuses a chain the runtime it was handed cannot run.
 *
 * WHERE THE ARITHMETIC IS. An operator whose body is a pure function of
 * one point is not written here: it is written once in the kernel this
 * feature compiles, and this executor calls that kernel's own generated
 * C++. What is left in this file is what no per-point kernel can be —
 * the permutation, the set class, the space deformers, and the operator
 * whose definition calls for a library sine. The four that read points
 * they do not own stand in Neighbourhood.cpp, and the seed and the
 * export in Lanes.cpp.
 */

#include <sigilcore/schedule/Parallel.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <numeric>
#include <stdexcept>
#include <string>

#include "CookInternal.h"
#include "sigilgeometry/mesh/pop/Kernel.h"
#include "sigilgeometry/mesh/pop/Pop.h"

namespace sigil::geometry::mesh {

namespace {

/** ONE OPERATOR RUN THROUGH ITS KERNEL, on this tier: the lanes the
 *  dispatch named, created if this is where they first appear, and the
 *  kernel called over them. A role the operator does not read is handed
 *  the destination, which the kernel never looks at. */
void runKernel(Attrs& attributes, const kernel::OperationDispatch& work) {
  glm::vec4* const destination = attributes.ensure(work.destination).data();
  const auto lane = [&](const std::string& name) -> glm::vec4* {
    return name.empty() ? destination : attributes.ensure(name).data();
  };
  // Read in this order and not inside the call: a lane created here can
  // be the one another role names, and every one of them must exist
  // before any of their addresses is handed over.
  glm::vec4* const a = lane(work.a);
  glm::vec4* const b = lane(work.b);
  glm::vec4* const c = lane(work.c);
  glm::vec4* const mask = lane(work.mask);
  kernel::run(work, destination, a, b, c, mask);
}

/** The frame a Deform runs in: its axis normalized, its bend direction
 *  made perpendicular to that axis and normalized (a direction parallel
 *  to the axis, or zero, falls back to a fixed perpendicular), and
 *  side = axis x direction. `Deform` has no kernel and every device
 *  executor declines it, so the frame is this cook's alone. */
void deformFrame(const pop::Deform& operation, glm::vec3* axis,
                 glm::vec3* direction, glm::vec3* side) {
  glm::vec3 a = operation.axis;
  const float al = glm::length(a);
  a = al > 1e-6f ? a / al : glm::vec3{0, 1, 0};
  glm::vec3 d = operation.direction - a * glm::dot(operation.direction, a);
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

namespace {

/** The built-in executor's body. @p grain is how many points one worker
 *  takes at a time; it changes nothing about the cloud, only how the
 *  passes are divided. */
Cloud cookOnCpu(const pop::Chain& chain, size_t grain) {
  Attrs attributes;
  attributes.count = pop::seedLanes(chain, &attributes.lanes);
  if (attributes.count == 0) return {};
  // Not const: the SET class changes how many points there are, and
  // every operator after one of those addresses the new count.
  size_t count = attributes.count;

  for (size_t opIndex = 1; opIndex < chain.size(); ++opIndex) {
    // THE KERNEL FIRST. An operator that has one is arithmetic this file
    // does not hold a second copy of, and the dispatch is the same
    // description a device executor is handed.
    kernel::OperationDispatch work;
    if (kernel::describe(chain[opIndex], count, &work)) {
      runKernel(attributes, work);
      continue;
    }
    std::visit(
        [&](const auto& operation) {
          using T = std::decay_t<decltype(operation)>;
          if constexpr (std::is_same_v<T, pop::SplineScatter> ||
                        std::is_same_v<T, pop::MeshScatter> ||
                        std::is_same_v<T, pop::PointSet> ||
                        std::is_same_v<T, pop::Promote>) {
            // Generators only lead a chain and are ignored mid-chain.
            // Promote is the PRIMITIVE class: nothing to do on the point
            // sink — a Cloud has no primitives. cookMesh() reads these
            // operations back off the chain once the stamps exist.
          } else if constexpr (std::is_same_v<T, pop::Smooth>) {
            runSmooth(attributes, operation, count, grain);
          } else if constexpr (std::is_same_v<T, pop::Relax>) {
            runRelax(attributes, operation, count);
          } else if constexpr (std::is_same_v<T, pop::Cluster>) {
            runCluster(attributes, operation, count, grain);
          } else if constexpr (std::is_same_v<T, pop::Transfer>) {
            runTransfer(attributes, operation, count);
          } else if constexpr (std::is_same_v<T, pop::Sort>) {
            // The permutation class: EVERY lane travels with its
            // point, so the store stays coherent and only the order
            // changes. Stable, so equal keys keep their cooked order
            // and a re-cook is deterministic. Keys are read (and the
            // source lane thereby created) BEFORE the lanes are
            // permuted, so the map is not grown mid-walk.
            std::vector<float> keys(count);
            const std::vector<glm::vec4>& values =
                attributes.ensure(operation.by.name);
            core::schedule::parallelFor(
                count, grain, [&](size_t first, size_t last) {
                  for (size_t i = first; i < last; ++i) {
                    const glm::vec4 v = values[i];
                    const float k =
                        v.x * operation.weights.x + v.y * operation.weights.y +
                        v.z * operation.weights.z + v.w * operation.weights.w;
                    // A NaN key would break the comparator's strict weak
                    // ordering outright (UB in stable_sort), so it sorts as
                    // zero rather than corrupting the whole permutation.
                    keys[i] = std::isfinite(k) ? k : 0.0f;
                  }
                });
            std::vector<uint32_t> order(count);
            std::iota(order.begin(), order.end(), 0u);
            std::stable_sort(order.begin(), order.end(),
                             [&](uint32_t a, uint32_t b) {
                               return operation.descending ? keys[a] > keys[b]
                                                           : keys[a] < keys[b];
                             });
            std::vector<glm::vec4> next(count);
            for (auto& [name, lane] : attributes.lanes) {
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
            if (operation.mask.empty()) return;
            const std::vector<glm::vec4>& mask =
                attributes.ensure(operation.mask);
            std::vector<uint32_t> kept;
            kept.reserve(count);
            for (size_t i = 0; i < count; ++i) {
              const bool named = mask[i].x >= operation.threshold;
              if (named == operation.keep) kept.push_back((uint32_t)i);
            }
            for (auto& [name, lane] : attributes.lanes) {
              std::vector<glm::vec4> next(kept.size());
              core::schedule::parallelFor(
                  kept.size(), grain, [&](size_t first, size_t last) {
                    for (size_t i = first; i < last; ++i)
                      next[i] = lane[kept[i]];
                  });
              lane = std::move(next);
            }
            count = kept.size();
            attributes.count = count;
          } else if constexpr (std::is_same_v<T, pop::Deform>) {
            // The frame: a unit axis, plus for Bend a unit direction
            // made perpendicular to it. Degenerate inputs (a zero
            // axis, a direction parallel to the axis) fall back the
            // same way wherever this is evaluated.
            glm::vec3 axis, dir, side;
            deformFrame(operation, &axis, &dir, &side);
            const float span = operation.high - operation.low;
            const float rad = operation.amount * 3.14159265f / 180.0f;
            std::vector<glm::vec4>& values =
                attributes.ensure(operation.lane.name);
            const std::vector<glm::vec4>* mask =
                operation.mask.empty() ? nullptr
                                       : &attributes.ensure(operation.mask);
            core::schedule::parallelFor(
                count, grain, [&](size_t first, size_t last) {
                  for (size_t i = first; i < last; ++i) {
                    const glm::vec4 v = values[i];
                    const glm::vec3 p =
                        glm::vec3{v.x, v.y, v.z} - operation.origin;
                    const float h = glm::dot(p, axis);
                    const glm::vec3 perp = p - axis * h;
                    float u = span != 0.0f ? (h - operation.low) / span
                                           : (h >= operation.low ? 1.0f : 0.0f);
                    u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
                    glm::vec3 out;
                    if (operation.kind == pop::Deform::Kind::Twist) {
                      const float ang = rad * u;
                      const float c = std::cos(ang), sn = std::sin(ang);
                      // Rodrigues about the unit axis; perp is already
                      // perpendicular so the parallel term is zero.
                      out = axis * h + perp * c + glm::cross(axis, perp) * sn;
                    } else if (operation.kind == pop::Deform::Kind::Taper) {
                      out = axis * h +
                            perp * (1.0f + (operation.amount - 1.0f) * u);
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
                            h < operation.low
                                ? operation.low
                                : (h > operation.high ? operation.high : h);
                        const float theta = (hb - operation.low) / R;
                        const float c = std::cos(theta), sn = std::sin(theta);
                        // Arc centre sits at +R along dir from (low). A
                        // point at height hb and offset x lands at
                        //   along axis: low + (R - x) * sin(theta)
                        //   along dir:  R - (R - x) * cos(theta)
                        const float extra = h - hb;  // rigid overhang
                        const float hOut =
                            operation.low + (R - x) * sn + extra * c;
                        const float xOut = R - (R - x) * c + extra * sn;
                        out = axis * hOut + dir * xOut + side * y;
                      }
                    }
                    out += operation.origin;
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
            std::vector<glm::vec4>& values =
                attributes.ensure(operation.lane.name);
            const std::vector<glm::vec4>* mask =
                operation.mask.empty() ? nullptr
                                       : &attributes.ensure(operation.mask);
            core::schedule::parallelFor(
                count, grain, [&](size_t first, size_t last) {
                  for (size_t i = first; i < last; ++i) {
                    const glm::vec4 old = values[i];
                    const glm::vec3 dd =
                        pop::noiseField({old.x, old.y, old.z},
                                        operation.frequency, operation.seed) *
                        operation.amplitude;
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

  return pop::exportLanes(attributes.lanes, count);
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
  bool supports(const pop::Operation&) const override { return true; }
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
  for (const pop::Operation& operation : chain)
    if (!runtime->supports(operation))
      throw std::runtime_error(
          "the \"" + runtime->name() + "\" pop runtime cannot run the \"" +
          std::string(pop::operationName(operation)) + "\" operator");
  return runtime->cook(chain);
}

}  // namespace sigil::geometry::mesh
