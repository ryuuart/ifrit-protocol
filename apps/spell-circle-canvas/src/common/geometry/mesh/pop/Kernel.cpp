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

#include <sigilslang/Pop.spv.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "Parallel.h"
#include "sigilgeometry/mesh/pop/Spirv.h"

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
  Args args;
  Buffer dst;
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

/** A colour or a vector field as four floats. */
glm::vec4 asVec4(const glm::vec3& v, float w) { return {v.x, v.y, v.z, w}; }

}  // namespace

bool has(const pop::Op& op) {
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
      op);
}

bool describe(const pop::Op& op, size_t count, Dispatch* out) {
  if (!has(op) || !out) return false;
  Dispatch work;
  work.args.code.x = (uint32_t)op.index();
  work.args.code.y = (uint32_t)count;

  std::visit(
      [&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, pop::Jitter>) {
          work.dst = work.a = value.lane.name;
          work.mask = value.mask;
          work.args.code.z = value.seed;
          work.args.a = {value.amplitude, 0, 0, 0};
        } else if constexpr (std::is_same_v<T, pop::Ramp>) {
          work.dst = value.lane.name;
          work.a = "T";
          work.mask = value.mask;
          work.args.a = value.from;
          work.args.b = value.to;
        } else if constexpr (std::is_same_v<T, pop::Vary>) {
          work.dst = value.lane.name;
          work.mask = value.mask;
          work.args.code.z = value.seed;
          work.args.a = {value.base, value.spread, 0, 0};
        } else if constexpr (std::is_same_v<T, pop::LookAt>) {
          // Dir is read for its .w and written; P is where the point
          // stands. Two roles, one of them the destination itself.
          work.dst = work.b = "Dir";
          work.a = "P";
          work.mask = value.mask;
          work.args.a = asVec4(value.target, 0);
        } else if constexpr (std::is_same_v<T, pop::Math>) {
          work.dst = work.a = value.lane.name;
          work.mask = value.mask;
          work.args.a = value.mul;
          work.args.b = value.add;
        } else if constexpr (std::is_same_v<T, pop::Fill>) {
          work.dst = value.attr.name;
          work.mask = value.mask;
          work.args.a = value.value;
        } else if constexpr (std::is_same_v<T, pop::Atlas>) {
          // Clamped HERE and nowhere else, so the kernel divides by a
          // number that cannot be zero however the description was
          // written.
          work.dst = "Tex";
          work.mask = value.mask;
          work.args.code.z = value.seed;
          work.args.nums.x = (uint32_t)std::max(value.cols, 1);
          work.args.nums.y = (uint32_t)std::max(value.rows, 1);
        } else if constexpr (std::is_same_v<T, pop::Lookup>) {
          work.dst = value.to.name;
          work.a = value.from.name;
          work.mask = value.mask;
          work.table = value.stops;
          work.args.a = value.weights;
          work.args.b = {value.low, value.high, 0, 0};
          work.args.nums.x = (uint32_t)value.stops.size();
        } else if constexpr (std::is_same_v<T, pop::Select>) {
          // The lane a selection writes IS what a mask is made of, so a
          // selector takes none of its own.
          work.dst = value.to;
          work.a = value.from.name;
          work.args.a = asVec4(value.center, 0);
          work.args.b = asVec4(value.size, 0);
          work.args.c = {value.feather, 0, 0, 0};
          work.args.nums.x = (uint32_t)value.shape;
          work.args.nums.y = (uint32_t)value.combine;
          work.args.nums.z = value.invert ? 1u : 0u;
        } else if constexpr (std::is_same_v<T, pop::Affine>) {
          work.dst = work.a = value.lane.name;
          work.mask = value.mask;
          work.args.nums.x = value.direction ? 1u : 0u;
          work.args.m0 = value.matrix[0];
          work.args.m1 = value.matrix[1];
          work.args.m2 = value.matrix[2];
          work.args.m3 = value.matrix[3];
        } else if constexpr (std::is_same_v<T, pop::Peak>) {
          work.dst = value.lane.name;
          work.a = value.along.name;
          work.mask = value.mask;
          work.args.a = {value.distance, 0, 0, 0};
        } else if constexpr (std::is_same_v<T, pop::Normal>) {
          work.dst = work.a = value.lane.name;
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
          work.args.a = asVec4(value.center, value.sense);
          work.args.b = asVec4(fallback, 0);
        } else if constexpr (std::is_same_v<T, pop::Mix>) {
          work.dst = value.to.name;
          work.a = value.a.name;
          work.b = value.b.name;
          work.c = value.factorLane;
          work.mask = value.mask;
          work.args.a = {value.factor, 0, 0, 0};
          work.args.nums.x = value.factorLane.empty() ? 0u : 1u;
        }
      },
      op);

  if (!work.mask.empty()) work.args.code.w |= kMasked;
  *out = std::move(work);
  return true;
}

void run(const Dispatch& dispatch, glm::vec4* dst, glm::vec4* a, glm::vec4* b,
         glm::vec4* c, glm::vec4* mask) {
  const size_t count = dispatch.args.code.y;
  if (count == 0 || !dst) return;
  Globals globals;
  globals.args = dispatch.args;
  globals.dst = {dst, count};
  globals.a = {a ? a : dst, count};
  globals.b = {b ? b : dst, count};
  globals.c = {c ? c : dst, count};
  globals.mask = {mask ? mask : dst, count};
  // The kernel writes through `dst` alone, so the table it only reads
  // may be handed over as it stands.
  globals.table = {const_cast<glm::vec4*>(dispatch.table.data()),
                   dispatch.table.size()};

  const uint32_t groupCount = (uint32_t)((count + kGroupSize - 1) / kGroupSize);
  parallel::groups(groupCount, [&](uint32_t first, uint32_t last) {
    VaryingInput varying{{first, 0, 0}, {last, 1, 1}};
    sigilPopKernel(&varying, nullptr, &globals);
  });
}

std::span<const uint32_t> spirv() {
  // Decorated HERE, beside the kernel, rather than by whichever runtime
  // dispatches it: a module that means one thing on one device and
  // another on the next is not a single source, and a second backend
  // would have to remember to do this.
  static const std::vector<uint32_t> module = noContraction(
      {slangmodule::Pop::kSpirv,
       sizeof(slangmodule::Pop::kSpirv) / sizeof(slangmodule::Pop::kSpirv[0])});
  return {module.data(), module.size()};
}

}  // namespace sigil::geometry::mesh::kernel
