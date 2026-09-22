/** @file
 * The stamping operator: one element made per node stating the lane,
 * attached to that node.
 */

#include "sigilcompose/kit/Stamp.h"

namespace sigil::compose::stamp {

void ByLane::add(Scope& scope) const {
  if (!make) return;
  for (const Scope::Node* node : scope.having(lane)) {
    Element made = make(*node);
    made.key(node->key + "-stamp");
    node->attach(std::move(made));
  }
}

}  // namespace sigil::compose::stamp
