/** @file
 * The describe-time ambient stack behind `env::`: the thread-local stack,
 * the copy a memo captures, and the scope guard that swaps a captured
 * stack in around a deferred call.
 */

#include "sigilcore/reconcile/Env.h"

namespace sigil::core {

namespace detail {

env::Snapshot& envStack() {
  static thread_local env::Snapshot stack;
  return stack;
}

}  // namespace detail

namespace env {

Snapshot capture() { return detail::envStack(); }

Restore::Restore(const Snapshot& snapshot) {
  Snapshot next = snapshot;  // copied first: `snapshot` may alias the stack
  m_saved = std::move(detail::envStack());
  detail::envStack() = std::move(next);
}

Restore::~Restore() { detail::envStack() = std::move(m_saved); }

}  // namespace env

}  // namespace sigil::core
