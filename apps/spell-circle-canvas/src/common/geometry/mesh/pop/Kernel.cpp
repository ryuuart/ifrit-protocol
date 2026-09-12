/** @file
 * The two ends of the kernel seam: an operator packed into the argument
 * block its kernel reads, and the kernel's own generated C++ called over
 * a set of lanes.
 *
 * NOTHING HERE COMPUTES AN OPERATOR. Every formula lives in the Slang
 * source the build compiles; what is written here is only which of the
 * operator's fields goes in which slot, which is a fact about the
 * description and not about the arithmetic.
 */

#include "sigilgeometry/mesh/pop/Kernel.h"

#include <sigilcore/schedule/Parallel.h>
#include <sigilslang/Pop.spv.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

/** THE KERNEL ITSELF, as the build's C++ emitter names it. Its two
 *  opaque parameters are the group range and the global bindings, whose
 *  layouts are mirrored below. */
extern "C" void sigilPopKernel(void* varying, void* entryPointParams,
                               void* globalParams);

namespace sigil::geometry::mesh::kernel {

namespace {

/** THE TWO PRELUDE TYPES the kernel's entry point takes.
 *
 *  The compiler's own C++ output declares these; they are mirrored here
 *  rather than reached for, because the generated file is a build
 *  artefact and a header of it is not one of its outputs. Both are fixed
 *  by the emitter: a group range as two triples, and a buffer as a
 *  pointer and a count. */
struct VaryingInput {
  uint32_t startGroup[3];
  uint32_t endGroup[3];
};

struct Buffer {
  void* data = nullptr;
  size_t count = 0;
};

/** The kernel's global parameters, member for member as it declares
 *  them: the argument block first, then one binding per role. */
struct Globals {
  OperationArguments arguments;
  Buffer destination;
  Buffer a;
  Buffer b;
  Buffer c;
  Buffer mask;
  Buffer table;
};

/** How many lanes one group of the kernel covers. It is the kernel's own
 *  `numthreads`, and the host walks whole groups the way a device
 *  dispatches them — the kernel drops the lanes past the count itself,
 *  so both ends run the same number of invocations. */
constexpr uint32_t kGroupSize = 64;
/** How many kernel groups one worker takes at a time. A group is already
 *  a run of lanes, so the run of groups only has to be long enough that
 *  entering the generated kernel is the small part of it. */
constexpr uint32_t kGroupsPerTask = 32;

/** A colour or a vector field as four floats. */
glm::vec4 asVec4(const glm::vec3& v, float w) { return {v.x, v.y, v.z, w}; }

}  // namespace

bool has(const pop::Operation& operation) {
  return std::visit(
      [](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        return std::is_same_v<T, pop::Jitter> || std::is_same_v<T, pop::Ramp> ||
               std::is_same_v<T, pop::Vary> || std::is_same_v<T, pop::LookAt> ||
               std::is_same_v<T, pop::Math> || std::is_same_v<T, pop::Fill> ||
               std::is_same_v<T, pop::Atlas> ||
               std::is_same_v<T, pop::Lookup> ||
               std::is_same_v<T, pop::Select> ||
               std::is_same_v<T, pop::Affine> || std::is_same_v<T, pop::Peak> ||
               std::is_same_v<T, pop::Mix> || std::is_same_v<T, pop::Normal>;
      },
      operation);
}

bool describe(const pop::Operation& operation, size_t count,
              OperationDispatch* out) {
  if (!has(operation) || !out) return false;
  OperationDispatch work;
  work.arguments.code.x = (uint32_t)operation.index();
  work.arguments.code.y = (uint32_t)count;

  std::visit(
      [&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, pop::Jitter>) {
          work.destination = work.a = value.lane.name;
          work.mask = value.mask;
          work.arguments.code.z = value.seed;
          work.arguments.a = {value.amplitude, 0, 0, 0};
        } else if constexpr (std::is_same_v<T, pop::Ramp>) {
          work.destination = value.lane.name;
          work.a = "T";
          work.mask = value.mask;
          work.arguments.a = value.from;
          work.arguments.b = value.to;
        } else if constexpr (std::is_same_v<T, pop::Vary>) {
          work.destination = value.lane.name;
          work.mask = value.mask;
          work.arguments.code.z = value.seed;
          work.arguments.a = {value.base, value.spread, 0, 0};
        } else if constexpr (std::is_same_v<T, pop::LookAt>) {
          // Dir is read for its .w and written; P is where the point
          // stands. Two roles, one of them the destination itself.
          work.destination = work.b = "Dir";
          work.a = "P";
          work.mask = value.mask;
          work.arguments.a = asVec4(value.target, 0);
        } else if constexpr (std::is_same_v<T, pop::Math>) {
          work.destination = work.a = value.lane.name;
          work.mask = value.mask;
          work.arguments.a = value.multiplier;
          work.arguments.b = value.add;
        } else if constexpr (std::is_same_v<T, pop::Fill>) {
          work.destination = value.attribute.name;
          work.mask = value.mask;
          work.arguments.a = value.value;
        } else if constexpr (std::is_same_v<T, pop::Atlas>) {
          // Clamped HERE and nowhere else, so the kernel divides by a
          // number that cannot be zero however the description was
          // written.
          work.destination = "Tex";
          work.mask = value.mask;
          work.arguments.code.z = value.seed;
          work.arguments.integers.x = (uint32_t)std::max(value.columns, 1);
          work.arguments.integers.y = (uint32_t)std::max(value.rows, 1);
        } else if constexpr (std::is_same_v<T, pop::Lookup>) {
          work.destination = value.to.name;
          work.a = value.from.name;
          work.mask = value.mask;
          work.table = value.stops;
          work.arguments.a = value.weights;
          work.arguments.b = {value.low, value.high, 0, 0};
          work.arguments.integers.x = (uint32_t)value.stops.size();
        } else if constexpr (std::is_same_v<T, pop::Select>) {
          // The lane a selection writes IS what a mask is made of, so a
          // selector takes none of its own.
          work.destination = value.to;
          work.a = value.from.name;
          work.arguments.a = asVec4(value.center, 0);
          work.arguments.b = asVec4(value.size, 0);
          work.arguments.c = {value.feather, 0, 0, 0};
          work.arguments.integers.x = (uint32_t)value.shape;
          work.arguments.integers.y = (uint32_t)value.combine;
          work.arguments.integers.z = value.invert ? 1u : 0u;
        } else if constexpr (std::is_same_v<T, pop::Affine>) {
          work.destination = work.a = value.lane.name;
          work.mask = value.mask;
          work.arguments.integers.x = value.direction ? 1u : 0u;
          work.arguments.m0 = value.matrix[0];
          work.arguments.m1 = value.matrix[1];
          work.arguments.m2 = value.matrix[2];
          work.arguments.m3 = value.matrix[3];
        } else if constexpr (std::is_same_v<T, pop::Peak>) {
          work.destination = value.lane.name;
          work.a = value.along.name;
          work.mask = value.mask;
          work.arguments.a = {value.distance, 0, 0, 0};
        } else if constexpr (std::is_same_v<T, pop::Normal>) {
          work.destination = work.a = value.lane.name;
          work.b = value.from.name;
          work.mask = value.mask;
          // NORMALIZED HERE and nowhere else, so the kernel divides by a
          // length it never has to check and a degenerate fallback lands
          // on one answer rather than on whatever the caller wrote.
          glm::vec3 fallback = value.fallback;
          const float length =
              std::sqrt(fallback.x * fallback.x + fallback.y * fallback.y +
                        fallback.z * fallback.z);
          fallback = length > 1e-6f ? fallback / length : glm::vec3{0, 0, 1};
          work.arguments.a = asVec4(value.center, value.sense);
          work.arguments.b = asVec4(fallback, 0);
        } else if constexpr (std::is_same_v<T, pop::Mix>) {
          work.destination = value.to.name;
          work.a = value.a.name;
          work.b = value.b.name;
          work.c = value.factorLane;
          work.mask = value.mask;
          work.arguments.a = {value.factor, 0, 0, 0};
          work.arguments.integers.x = value.factorLane.empty() ? 0u : 1u;
        }
      },
      operation);

  if (!work.mask.empty()) work.arguments.code.w |= kMasked;
  *out = std::move(work);
  return true;
}

void run(const OperationDispatch& dispatch, glm::vec4* destination,
         glm::vec4* a, glm::vec4* b, glm::vec4* c, glm::vec4* mask) {
  const size_t count = dispatch.arguments.code.y;
  if (count == 0 || !destination) return;
  Globals globals;
  globals.arguments = dispatch.arguments;
  globals.destination = {destination, count};
  globals.a = {a ? a : destination, count};
  globals.b = {b ? b : destination, count};
  globals.c = {c ? c : destination, count};
  globals.mask = {mask ? mask : destination, count};
  // The kernel writes through `destination` alone, so the table it only reads
  // may be handed over as it stands.
  globals.table = {const_cast<glm::vec4*>(dispatch.table.data()),
                   dispatch.table.size()};

  const uint32_t groupCount = (uint32_t)((count + kGroupSize - 1) / kGroupSize);
  // A worker takes a run of groups rather than one, so it enters the
  // generated kernel once per run; the kernel owns the group size and
  // clips its last group against the lane count either way.
  core::schedule::parallelFor(
      groupCount, kGroupsPerTask, [&](uint32_t first, uint32_t last) {
        VaryingInput varying{{first, 0, 0}, {last, 1, 1}};
        sigilPopKernel(&varying, nullptr, &globals);
      });
}

std::span<const uint32_t> operationSpirv() { return slangmodule::Pop::kSpirv; }

}  // namespace sigil::geometry::mesh::kernel
