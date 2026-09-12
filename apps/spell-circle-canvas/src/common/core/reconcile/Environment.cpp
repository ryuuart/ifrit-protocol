/** @file
 * The describe-time ambient stack behind `environment::`: the thread-local
 * stack, the copy a memo captures, and the scope guard that swaps a captured
 * stack in around a deferred call.
 */

#include "sigilcore/reconcile/Environment.h"

namespace sigil::core {

namespace detail {

environment::Snapshot& environmentStack() {
  static thread_local environment::Snapshot stack;
  return stack;
}

}  // namespace detail

namespace environment {

Snapshot capture() { return detail::environmentStack(); }

Restore::Restore(const Snapshot& snapshot) {
  Snapshot next = snapshot;  // copied first: `snapshot` may alias the stack
  m_saved = std::move(detail::environmentStack());
  detail::environmentStack() = std::move(next);
}

Restore::~Restore() { detail::environmentStack() = std::move(m_saved); }

}  // namespace environment

}  // namespace sigil::core
