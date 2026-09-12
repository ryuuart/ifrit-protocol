#pragma once

/** @file
 * The built-in executor's own store, the grain it divides a pass at, and
 * the operators that read points a point does not own. Private to the
 * translation units the executor is written across; nothing outside them
 * includes it.
 */

#include <cstddef>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/pop/Runtime.h"

namespace sigil::geometry::mesh {
namespace {

/** How many points one worker takes at a time when a pass is divided.
 *  A pass over lanes is a handful of arithmetic operations per point, so
 *  handing a chunk to a worker is only worth it for many thousands of
 *  them; below that the whole range stays on the calling thread. A cook
 *  divides its own passes at the grain its executor was given, and this
 *  is what the built-in one carries. */
constexpr size_t kLaneGrain = 4096;

}  // namespace

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

/** THE OPERATORS THAT READ POINTS A POINT DOES NOT OWN. None of them has
 *  a kernel, because a kernel is a pure function of one point: these
 *  four each look at their neighbours — in chain order, in space, or in
 *  another cloud entirely — so one lane cannot be both what is read and
 *  what is written. @p count is how many points the store holds and
 *  @p grain how many one worker takes at a time. */
void runSmooth(Attrs& attributes, const pop::Smooth& operation, size_t count,
               size_t grain);
void runRelax(Attrs& attributes, const pop::Relax& operation, size_t count);
void runCluster(Attrs& attributes, const pop::Cluster& operation, size_t count,
                size_t grain);
void runTransfer(Attrs& attributes, const pop::Transfer& operation,
                 size_t count);

}  // namespace sigil::geometry::mesh
