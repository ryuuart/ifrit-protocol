/** @file
 * The barriers, as a plan rather than as an API call: between two
 * consecutive touches of one resource where either of them writes, and
 * between the resources that were handed one surface. The CPU executor
 * performs the steps in order and needs none of them; the plan is built
 * and checked all the same, because an ordering that only states its
 * hazards where a device is present states them where they cannot be
 * tested.
 */

#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <boost/container/map.hpp>
#include <string>
#include <vector>

#include "Build.h"

namespace sigil::world::graph::detail {

namespace {

struct Touch {
  size_t step = 0;
  Access access = Access::Read;
};

}  // namespace

void barriers(std::span<const PassWork> steps,
              std::span<const Resource> resources, std::vector<Barrier>& into) {
  into.clear();
  boost::container::map<std::string, std::vector<Touch>> byName;
  for (size_t step = 0; step < steps.size(); ++step) {
    const Touches touches = touchesOf(steps[step]);
    for (const std::string& name : touches.writes)
      byName[name].push_back({step, Access::Write});
    for (const std::string& name : touches.reads) {
      std::vector<Touch>& list = byName[name];
      // A step that both reads and writes one resource touches it once,
      // as a write: that is the stronger claim and the one a hazard is
      // stated against.
      if (!list.empty() && list.back().step == step) continue;
      list.push_back({step, Access::Read});
    }
  }

  for (const auto& [name, touches] : byName) {
    for (size_t i = 1; i < touches.size(); ++i) {
      if (touches[i - 1].access == Access::Read &&
          touches[i].access == Access::Read)
        continue;
      into.push_back(Barrier{.resource = name,
                             .after = touches[i - 1].step,
                             .before = touches[i].step,
                             .from = touches[i - 1].access,
                             .to = touches[i].access});
    }
  }

  // …and where one surface serves two resources, the second's first
  // write waits on the first's last reader.
  boost::container::flat_map<int, std::vector<const Resource*>> bySlot;
  for (const Resource& resource : resources)
    if (resource.slot >= 0) bySlot[resource.slot].push_back(&resource);
  for (auto& [slot, sharing] : bySlot) {
    std::stable_sort(sharing.begin(), sharing.end(),
                     [](const Resource* a, const Resource* b) {
                       return a->first < b->first;
                     });
    for (size_t i = 1; i < sharing.size(); ++i)
      into.push_back(Barrier{.resource = sharing[i]->name,
                             .after = sharing[i - 1]->last,
                             .before = sharing[i]->first,
                             .from = Access::Read,
                             .to = Access::Write});
  }

  std::stable_sort(into.begin(), into.end(),
                   [](const Barrier& a, const Barrier& b) {
                     if (a.before != b.before) return a.before < b.before;
                     return a.after < b.after;
                   });
}

}  // namespace sigil::world::graph::detail
