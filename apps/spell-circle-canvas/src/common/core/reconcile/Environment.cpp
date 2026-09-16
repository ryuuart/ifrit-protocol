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
namespace {

thread_local std::uint64_t currentRestore = 0;
thread_local std::uint64_t nextRestore = 0;

}  // namespace

Snapshot capture() { return detail::environmentStack(); }

std::uint64_t restoreIdentity() noexcept { return currentRestore; }

Restore::Restore(const Snapshot& snapshot) {
  Snapshot next = snapshot;  // copied first: `snapshot` may alias the stack
  m_saved = std::move(detail::environmentStack());
  detail::environmentStack() = std::move(next);
  m_savedIdentity = currentRestore;
  currentRestore = ++nextRestore;
}

Restore::~Restore() {
  detail::environmentStack() = std::move(m_saved);
  currentRestore = m_savedIdentity;
}

}  // namespace environment

}  // namespace sigil::core
