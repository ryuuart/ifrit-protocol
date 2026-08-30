/** @file
 * The registry: every study, in the order a sweep walks them.
 */

#include <vector>

#include "Studies.h"

namespace sigil::world::testing {

std::span<const Study> registry() {
  static const std::vector<Study> studies = {firstLight()};
  return studies;
}

}  // namespace sigil::world::testing
