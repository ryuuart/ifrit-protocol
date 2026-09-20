#pragma once

/** @file
 * @ingroup core-reconcile
 *
 * An inherited value, read where a component is described: environment::Provide
 * binds a value for a describe scope, environment::inherited reads it, and
 * environment::capture is what a memo takes where it is written, so that its
 * environment is part of its key and environment::Restore can re-establish it
 * around the deferred describe.
 */

#include <sigilcore/compute/Hash.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::core {

// ---------------------------------------------------------------------------
// environment — an INHERITED VALUE, read where a component is described
//
// A describe phase is an ordinary C++ call tree, so the describe-time
// call stack IS the description tree, and the C++ answer to "inherit
// down a call stack" is dynamic scope: a `Provide` binds for its scope
// and anything described inside it reads the value back.
//
// An inherited value is read DURING DESCRIBE and lands in the reading
// node's own description, so the prune is already an exact dependency
// tracker and no phase learns a new concept.

/** THE INHERITED-VALUE CHANNEL: a value bound for a scope of the
 *  description tree and read by anything described inside it, without
 *  being threaded through every call between. Bindings are keyed by C++
 *  TYPE, so this is a transport channel and not a design-token
 *  vocabulary — the key a component uses is its own properties type.
 *  What is read is recorded, so a node that read a binding re-describes
 *  when that binding changes and one that did not is left alone. */
namespace environment {

/** One ambient binding, type-erased. `type` is a per-T number so no RTTI
 *  is needed and the type test is an integer compare. Two entries are
 *  equal when they bind the same type to values equal by that type's
 *  own `operator==`; the same holder short-circuits. */
struct Entry {
  std::uint64_t type = 0;
  std::shared_ptr<const void> value;
  bool (*equal)(const void*, const void*) = nullptr;

  bool operator==(const Entry& o) const {
    if (type != o.type) return false;
    if (value == o.value) return true;  // the same binding object
    return equal && value && o.value && equal(value.get(), o.value.get());
  }
};

/** A captured ambient stack, innermost LAST. Compares by value — the same
 *  bindings, in the same order, each equal by its own `operator==` —
 *  which is what makes a memo's environment part of its key. Empty is
 *  the overwhelmingly common case and costs one empty vector: the
 *  feature is free unused. */
using Snapshot = std::vector<Entry>;

}  // namespace environment

namespace detail {

/** The live describe-time stack. Thread-local: a describe runs on whatever
 *  thread the host calls on. */
environment::Snapshot& environmentStack();

/** THE IDENTITY OF AN INHERITED TYPE, as a number the compiler derives
 *  from the type's own spelling.
 *
 *  IT CANNOT BE AN ADDRESS. A dynamically loaded image compiled with
 *  hidden visibility gets its own copy of every inline the loader of it
 *  also has, including the static inside one — so a value bound in the
 *  loaded image and read in the loader would carry two different
 *  addresses for one type and never match. The compiler's own spelling
 *  of the type is the same string in both images, so it is hashed at
 *  compile time and the number is the key. */
template <class T>
constexpr std::uint64_t environmentTypeTag() {
  return hash::fnv1a(hash::kFnvOffset, std::string_view{__PRETTY_FUNCTION__});
}

}  // namespace detail

namespace environment {

/** The bindings in scope right now, copied — what a memo captures where
 *  it is WRITTEN, because by the time the reconciler decides whether to
 *  run the deferred describe the author's scope is gone. */
Snapshot capture();

/** Identity of the active Restore on this thread, or zero outside one.
 *  Each restoration has a distinct identity even for the same snapshot;
 *  leaving it reinstates the enclosing identity. Scope adapters can use this
 *  to require that a binding closes in the environment where it was opened. */
[[nodiscard]] std::uint64_t restoreIdentity() noexcept;

/** Re-establishes a captured stack around a DEFERRED describe (the memo
 *  invoke). Swaps rather than pushes: a deferred call must see exactly
 *  what its author's scope had, not that stack plus whatever the current
 *  reconcile walk happens to sit inside. */
class Restore {
 public:
  explicit Restore(const Snapshot& snapshot);
  ~Restore();
  Restore(const Restore&) = delete;
  Restore& operator=(const Restore&) = delete;

 private:
  Snapshot m_saved;
  std::uint64_t m_savedIdentity = 0;
};

}  // namespace environment

namespace environment {

/** Bind `value` for every component described while this object lives.
 *  RAII and LIFO; an inner `Provide<T>` shadows an outer one, and other
 *  types are unaffected. Not copyable or movable — it is a scope. */
template <class T>
class Provide {
 public:
  explicit Provide(T value) {
    static_assert(std::is_copy_constructible_v<T>,
                  "an inherited value is a value");
    auto held = std::make_shared<const T>(std::move(value));
    m_self = held.get();
    detail::environmentStack().push_back(
        Entry{detail::environmentTypeTag<T>(),
              std::shared_ptr<const void>(std::move(held)),
              [](const void* a, const void* b) {
                return *static_cast<const T*>(a) == *static_cast<const T*>(b);
              }});
    m_depth = detail::environmentStack().size();
  }
  /** UNBINDS THIS SCOPE'S BINDING AND NO OTHER. The well-nested path is
   *  a compare and a pop_back, allocation-free.
   *  @trap Destroying providers out of LIFO order is misuse: the entry
   *  is found by the held value's identity and removed, since an
   *  unconditional pop would unbind a SIBLING that is still alive, and
   *  the misuse warns unconditionally and with no switch. */
  ~Provide() {
    Snapshot& stack = detail::environmentStack();
    if (stack.size() == m_depth && stack.back().value.get() == m_self) {
      stack.pop_back();
      return;
    }
    std::fputs(
        "[core] environment::Provide destroyed out of order — scopes must nest "
        "LIFO; removing only this scope's own binding\n",
        stderr);
    for (size_t i = stack.size(); i-- > 0;)
      if (stack[i].value.get() == m_self) {
        stack.erase(stack.begin() + (std::ptrdiff_t)i);
        return;
      }
  }
  Provide(const Provide&) = delete;
  Provide& operator=(const Provide&) = delete;

 private:
  size_t m_depth = 0;
  const void* m_self = nullptr;  // identity of the entry this scope pushed
};

/** The nearest enclosing binding of `T`, or nullptr when nothing bound
 *  one — which is a component's cue to use its own default. Valid until
 *  the binding's scope ends (i.e. for the rest of the describe call that
 *  read it). */
template <class T>
const T* inherited() {
  const Snapshot& stack = detail::environmentStack();
  const std::uint64_t tag = detail::environmentTypeTag<T>();
  for (size_t i = stack.size(); i-- > 0;)
    if (stack[i].type == tag)
      return static_cast<const T*>(stack[i].value.get());
  return nullptr;
}

/** The inherited value, or `fallback` — the one-liner spelling for a
 *  component that has a sensible default of its own. */
template <class T>
T inheritedOr(const T& fallback) {
  const T* found = inherited<T>();
  return found ? *found : fallback;
}

/** Is a binding of `T` in scope? For a component that must behave
 *  differently rather than just fall back to a default. */
template <class T>
bool bound() {
  return inherited<T>() != nullptr;
}

}  // namespace environment

}  // namespace sigil::core
