/** @file
 * The plan, assembled: order, realise, live, alias, barrier. A failure
 * at any step leaves a plan carrying the error and no steps at all,
 * because half an order is worse than none.
 */

#include <sigilworld/graph/Plan.h>

#include <cstddef>
#include <string>
#include <vector>

#include "Build.h"

namespace sigil::world::graph {

int Plan::aliased() const {
  int count = 0;
  for (const Resource& resource : m_resources)
    if (resource.aliased) ++count;
  return count;
}

const Resource* Plan::resource(std::string_view name) const {
  for (const Resource& resource : m_resources)
    if (resource.name == name) return &resource;
  return nullptr;
}

Plan build(const Frame& frame) {
  Plan plan;
  const std::span<const Pass> passes = frame.passes();
  if (passes.empty()) return plan;

  std::vector<size_t> order;
  plan.m_error = detail::order(passes, order);
  if (!plan.m_error.empty()) return plan;

  plan.m_error = detail::realise(passes, order, plan.m_steps);
  if (!plan.m_error.empty()) {
    plan.m_steps.clear();
    return plan;
  }

  detail::lives(plan.m_steps, frame, plan.m_resources, plan.m_present,
                plan.m_kept);
  plan.m_surfaces = detail::alias(plan.m_resources);
  detail::barriers(plan.m_steps, plan.m_resources, plan.m_barriers);
  return plan;
}

}  // namespace sigil::world::graph
