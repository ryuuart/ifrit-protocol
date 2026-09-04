/** @file
 * Who a target's pixels belong to. A geometry pass CLEARS its target
 * before it paints, so a second one over a resource that has already
 * been written does not stand over that picture — it destroys it, and
 * the frame keeps the second pass's bodies alone. Laying one picture
 * over another is what a post pass is for, so this is refused while the
 * plan is being read rather than discovered by looking at the result.
 */

#include <boost/container/flat_map.hpp>
#include <string>
#include <vector>

#include "Build.h"

namespace sigil::world::graph::detail {

std::string claims(std::span<const Pass> passes,
                   std::span<const size_t> order) {
  boost::container::flat_map<std::string, size_t> writtenBy;
  for (size_t index : order) {
    const Pass& pass = passes[index];
    for (const std::string& name : pass.writes()) {
      const auto [held, fresh] = writtenBy.emplace(name, index);
      if (fresh) continue;
      // A pass carrying a body runs INSTEAD of the stage and keeps only
      // the stage's declarations, so it clears nothing and what it makes
      // of what is already there is its own business. Every other stage
      // writes what it was given: a compute pass replaces a point set
      // and a post pass writes the result of reading its layers, and
      // neither is a picture quietly thrown away.
      if (pass.stage() == Stage::Geometry && !pass.body())
        return "the geometry pass \"" + pass.name() + "\" writes \"" + name +
               "\", which \"" + passes[held->second].name() +
               "\" has already written — and a geometry pass clears its "
               "target, so this erases that picture rather than standing "
               "over it. Lay one picture over another with a post pass, or "
               "give this pass a target of its own.";
      held->second = index;
    }
  }
  return {};
}

}  // namespace sigil::world::graph::detail
