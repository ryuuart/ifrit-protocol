#pragma once

/** @file
 * SigilCompose env — an inherited value, read where a component is
 * composed: env::Provide binds a value for a describe scope,
 * env::inherited reads it, and the detail:: snapshot types are what a memo
 * captures so that its environment is part of its key.
 */

#include <include/core/SkTypes.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::compose {

// ---------------------------------------------------------------------------
// env — an INHERITED VALUE, read where a component is composed
//
// SwiftUI's Environment and React's context, for a library whose describe
// phase is an ordinary C++ call tree. Passing `const Theme&` is idiomatic
// for your OWN components; what has no answer without this is the
// library's own — a `feed::`, a decoration nested four levels down —
// each of which must otherwise be handed its colours by whoever composes
// it, so a theme change is a mechanical edit at every call site.
//
// THE ONE THING TO UNDERSTAND. Describe here is EAGER and BOTTOM-UP:
// `box().child(panel())` calls `panel()` before the box exists, and every
// component is a plain function whose arguments are evaluated inside the
// enclosing scope. So the describe-time call stack IS the element tree,
// and the C++ answer to "inherit down a call stack" is dynamic scope:
//
//     env::Provide<Palette> theme(dark);      // binds for this scope
//     return box().child(panel());            // panel() reads it
//
//     // …four levels down, in a component that was never handed it:
//     const Palette *p = env::inherited<Palette>();
//
// WHY THIS DOES NOT COST THE PRUNE. An inherited value is read DURING
// DESCRIBE and lands in the reading node's own props, so the reconciler's
// property comparison is already an exact dependency tracker: a node whose
// props came out identical prunes, whether or not it read the environment,
// and a node whose colour actually moved re-patches. The element tree the
// Composer sees is environment-INDEPENDENT — the value is baked in by then
// — so no kernel phase learns a new concept and nothing invalidates a
// subtree wholesale.
//
// The alternative shape, resolving a theme at PAINT through bound Outputs,
// trades the wrong way: it makes every themed node permanently volatile
// and therefore uncacheable, paying per frame forever to save work on the
// rare frame where the theme actually changes.
//
// THE ONE PLACE THE KERNEL HAD TO LEARN IT is `memo`, the only site in
// the library where a component function runs AFTER the author's scope
// has ended. A memo therefore captures the ambient stack at construction,
// compares it alongside its props, and re-establishes it around the
// deferred call — so `memo` stays a pure function of (props, environment)
// and cannot serve a stale theme. Everything else that takes a callable
// (a `ContourWalk` stamp, a `custom()` program) runs at derive or paint
// time with NO scope: capture what such a lambda needs by value at the
// call site, which is where the scope still exists.
//
// REQUIREMENTS ON AN INHERITED TYPE, and what its equality means: it is
// copyable and equality-comparable, and two values are equal exactly when
// describing anything under them yields props that compare equal. The
// comparison is therefore structural and exact — SkColor4f bitwise,
// typefaces by pointer — never perceptual and never epsilon'd, because
// the consumer of the answer is the prune.
//
// MATERIALISE DERIVED VALUES INTO THE TYPE. A theme that carries a
// `std::function` derivation rule instead of the colours it produces is
// incomparable, so it never compares equal to itself and every memo below
// it becomes a permanent miss. Run the function once and store the
// results.
//
// There is deliberately NO library-wide `Theme` type. Bindings are keyed
// by C++ TYPE, and the key a library component uses is its own existing
// props type (`feed::TextOptions`), so this is a transport channel rather
// than a design-token vocabulary.

namespace detail {

/** One ambient binding, type-erased. `type` is a per-T address so no RTTI
 *  is needed and the type test is a pointer compare. */
struct EnvEntry {
  const void* type = nullptr;
  std::shared_ptr<const void> value;
  bool (*equal)(const void*, const void*) = nullptr;
};

/** A captured ambient stack, innermost LAST. Empty is the overwhelmingly
 *  common case and costs one empty vector — the feature is free unused. */
using EnvSnapshot = std::vector<EnvEntry>;

/** The live describe-time stack. Thread-local: compose is a guest and
 *  describes on whatever thread the host calls on. */
EnvSnapshot& envStack();

/** Value equality over two captured stacks: same bindings, in the same
 *  order, each equal by its own `operator==`. Identical holders short-
 *  circuit. This is what makes a memo's environment part of its key. */
bool envEqual(const EnvSnapshot& a, const EnvSnapshot& b);

/** Re-establishes a captured stack around a DEFERRED describe (the memo
 *  invoke). Swaps rather than pushes: a deferred call must see exactly
 *  what its author's scope had, not that stack plus whatever the current
 *  reconcile walk happens to sit inside. */
class EnvRestore {
 public:
  explicit EnvRestore(const EnvSnapshot& snapshot);
  ~EnvRestore();
  EnvRestore(const EnvRestore&) = delete;
  EnvRestore& operator=(const EnvRestore&) = delete;

 private:
  EnvSnapshot m_saved;
};

template <class T>
const void* envTypeTag() {
  static const char tag = 0;
  return &tag;
}

}  // namespace detail

namespace env {

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
    detail::envStack().push_back(detail::EnvEntry{
        detail::envTypeTag<T>(), std::shared_ptr<const void>(std::move(held)),
        [](const void* a, const void* b) {
          return *static_cast<const T*>(a) == *static_cast<const T*>(b);
        }});
    m_depth = detail::envStack().size();
  }
  /** Unbinds THIS scope's binding and no other. Destroying providers out
   *  of LIFO order is misuse; when it happens, the destructor locates its
   *  own entry by the held value's identity and removes exactly that one
   *  — an unconditional pop would unbind a SIBLING that is still alive.
   *  The misuse warns; the well-nested path stays a compare and a
   *  pop_back, allocation-free. */
  ~Provide() {
    detail::EnvSnapshot& stack = detail::envStack();
    if (stack.size() == m_depth && stack.back().value.get() == m_self) {
      stack.pop_back();
      return;
    }
    SkDebugf(
        "[compose] env::Provide destroyed out of order — scopes must "
        "nest LIFO; removing only this scope's own binding\n");
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
 *  one — which is a component's cue to use its own default, exactly like
 *  a React context's default value. Valid until the binding's scope ends
 *  (i.e. for the rest of the describe call that read it). */
template <class T>
const T* inherited() {
  const detail::EnvSnapshot& stack = detail::envStack();
  const void* tag = detail::envTypeTag<T>();
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
  return found ? *found : std::move(fallback);
}

/** Is a binding of `T` in scope? For a component that must behave
 *  differently rather than just fall back to a default. */
template <class T>
bool bound() {
  return inherited<T>() != nullptr;
}

}  // namespace env

}  // namespace sigil::compose
