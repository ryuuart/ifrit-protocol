/** @file
 * The point kernel's SPIR-V, made to mean what its source says: the
 * words this build embedded, carrying one `NoContraction` decoration per
 * arithmetic result.
 *
 * The decoration is added HERE, beside the kernel, rather than by
 * whichever runtime dispatches it: a module that means one thing on one
 * device and another thing on the next is not a single source, and a
 * second backend would have to remember to do this. What it means, and
 * why nothing in the emitter's own output says it, is `mesh/Spirv.h`.
 */

#include "sigilgeometry/mesh/Spirv.h"

#include <sigilslang/Pop.spv.h>

#include <cstdint>
#include <vector>

#include "sigilgeometry/mesh/pop/Kernel.h"

namespace sigil::geometry::mesh::kernel {

std::span<const uint32_t> spirv() {
  static const std::vector<uint32_t> module = noContraction(
      {slangmodule::Pop::kSpirv,
       sizeof(slangmodule::Pop::kSpirv) / sizeof(slangmodule::Pop::kSpirv[0])});
  return {module.data(), module.size()};
}

}  // namespace sigil::geometry::mesh::kernel
