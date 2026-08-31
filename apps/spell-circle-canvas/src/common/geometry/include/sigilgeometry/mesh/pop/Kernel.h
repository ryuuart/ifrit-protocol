#pragma once

/** @file
 * THE POINT OPERATORS AS ONE PIECE OF ARITHMETIC, and the description a
 * dispatch of them is.
 *
 * An operator whose body is a pure function of one point is written once
 * — in Slang, beside this feature — and the build compiles that source
 * twice: to C++, which `run` below calls, and to SPIR-V, which a
 * runtime that owns a device dispatches. Neither side re-derives a
 * formula, which is what lets two tiers be held to bit identity rather
 * than to a tolerance.
 *
 * What is here is the seam between them: the argument block, which lane
 * fills each of the kernel's binding roles, and the two questions a
 * runtime asks — is there a kernel for this operator, and what does it
 * want bound.
 *
 * Not every operator has one. A generator seeds the lanes rather than
 * mapping over them; a neighbourhood operator reads points it does not
 * own; a permutation moves whole points rather than their values; a
 * primitive operator addresses triangles that do not exist yet; and an
 * operator whose definition calls for a library sine is not portable
 * arithmetic. `has()` is the honest answer for each.
 */

#include <cstdint>
#include <glm/vec4.hpp>
#include <span>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/pop/Pop.h"

namespace sigil::geometry::mesh::kernel {

/** ONE DISPATCH'S PARAMETERS, in the layout the kernel declares.
 *
 *  Every member is a four-component vector, so these bytes stand at the
 *  same offsets in a device's uniform buffer as they do here — there is
 *  no layout to report and none to guess. */
struct Args {
  /** x: which operator, by its index among the language's descriptions;
   *  y: how many points; z: the operator's seed; w: flags, bit 0 meaning
   *  a mask lane is bound. */
  glm::uvec4 code{0, 0, 0, 0};
  /** The operator's whole numbers, one meaning per operator. */
  glm::uvec4 nums{0, 0, 0, 0};
  /** …and its real ones. */
  glm::vec4 a{0, 0, 0, 0};
  glm::vec4 b{0, 0, 0, 0};
  glm::vec4 c{0, 0, 0, 0};
  /** An affine's four columns. */
  glm::vec4 m0{0, 0, 0, 0};
  glm::vec4 m1{0, 0, 0, 0};
  glm::vec4 m2{0, 0, 0, 0};
  glm::vec4 m3{0, 0, 0, 0};
};

/** The flag bits `Args::code.w` carries. */
enum : uint32_t {
  /** A mask lane is bound, so the operator's write is blended by it. */
  kMasked = 1u << 0u,
};

/** ONE OPERATOR, READY TO RUN: its arguments, the lane each binding role
 *  takes, and the table a lookup reads.
 *
 *  A role an operator does not read is left unnamed, and a runtime may
 *  bind anything to it — the kernel never reads it. Writing goes through
 *  `dst` alone. Where `dst` and a source name one lane, that is one lane
 *  read and written in place, which is what most filters are. */
struct Dispatch {
  Args args;
  std::string dst;
  std::string a;
  std::string b;
  std::string c;
  /** Empty when the operator takes every point in full. */
  std::string mask;
  /** A lookup's stops; empty for every other operator. */
  std::vector<glm::vec4> table;
};

/** Does @p op have a kernel? Every runtime asks this and no runtime
 *  keeps a list of its own, so an operator gains a kernel in one place
 *  and every runtime that dispatches them gains it at once. */
bool has(const pop::Op& op);

/** @p op over @p count points, as a dispatch. False — leaving @p out
 *  untouched — when there is no kernel for the operator. */
bool describe(const pop::Op& op, size_t count, Dispatch* out);

/** THE HOST RUN: the kernel's own generated C++ over the bound lanes.
 *
 *  Every pointer addresses at least `args.code.y` values. Only @p dst is
 *  written; a role @p dispatch left unnamed may be given any of the
 *  others, since nothing reads it. */
void run(const Dispatch& dispatch, glm::vec4* dst, glm::vec4* a, glm::vec4* b,
         glm::vec4* c, glm::vec4* mask);

/** THE KERNEL AS A DEVICE RUNS IT: the SPIR-V this build compiled from
 *  the same source `run` came out of, with one `NoContraction`
 *  decoration per arithmetic result.
 *
 *  The decoration is not optional and is not the emitter's: nothing in
 *  the words it produces tells a driver that a multiply and the add
 *  after it are two operations, so a driver is free to fuse them and
 *  round once where the source rounds twice. It is added here rather
 *  than by a runtime, so a second backend cannot forget it.
 *
 *  The words stand for the life of the process. */
std::span<const uint32_t> spirv();

}  // namespace sigil::geometry::mesh::kernel
