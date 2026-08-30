#pragma once

/** @file
 * SigilCompose's comparable type erasure — Erased, the one shape every
 * seam value on a description takes: a set of operations behind an
 * abstract interface, held with the value that implements them so two
 * descriptions can ask whether they carry the same one.
 */

#include <any>
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace sigil::compose {

/** A VALUE that answers an interface.
 *
 *  `Ops` is an abstract class — the operations a phase of the kernel calls
 *  through. A model is any class deriving from it; constructing an Erased
 *  from one copies the model into shared immutable state, so a description
 *  carrying the value costs a pointer and a copy-on-write node copy is a
 *  refcount bump.
 *
 *  Equality is the reconciler's question, and it is answered the way
 *  `Shape` answers it: copies of ONE value (shared state) are equal; two
 *  separately-constructed values are equal when they hold the same model
 *  type and that type's `==` says so; a model with no `==` is the escape
 *  hatch and compares equal to nothing but its own copies. An empty Erased
 *  holds no operations and answers false to `bool`. */
template <typename Ops>
class Erased {
 public:
  Erased() = default;

  /** A comparable model: its type and its value take part in equality. */
  template <typename M>
    requires(std::derived_from<std::remove_cvref_t<M>, Ops> &&
             std::equality_comparable<std::remove_cvref_t<M>> &&
             !std::same_as<std::remove_cvref_t<M>, Erased>)
  Erased(M model) {  // NOLINT: implicit by design (a seam value IS the model)
    using Model = std::remove_cvref_t<M>;
    State state;
    state.held = model;
    state.equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const Model&>(a) == std::any_cast<const Model&>(b);
    };
    state.ops = std::make_shared<const Model>(std::move(model));
    m_state = std::make_shared<const State>(std::move(state));
  }

  /** The escape hatch: a model with no `==`. Identity only. */
  template <typename M>
    requires(std::derived_from<std::remove_cvref_t<M>, Ops> &&
             !std::equality_comparable<std::remove_cvref_t<M>> &&
             !std::same_as<std::remove_cvref_t<M>, Erased>)
  explicit Erased(M model) {
    using Model = std::remove_cvref_t<M>;
    State state;
    state.ops = std::make_shared<const Model>(std::move(model));
    m_state = std::make_shared<const State>(std::move(state));
  }

  explicit operator bool() const { return m_state && (bool)m_state->ops; }
  const Ops* operator->() const {
    return m_state ? m_state->ops.get() : nullptr;
  }
  const Ops& operator*() const { return *m_state->ops; }
  /** The operations, or null when empty. */
  const Ops* get() const { return m_state ? m_state->ops.get() : nullptr; }
  /** Does this value take part in structural equality? (False for the
   *  escape hatch and for an empty value.) */
  bool comparable() const { return m_state && (bool)m_state->equals; }

  /** Shared state is equal; comparable models of one type compare their
   *  values; anything else is conservative. */
  bool operator==(const Erased& o) const {
    if (m_state == o.m_state) return true;
    if (!m_state || !o.m_state) return false;
    if (!m_state->equals || !o.m_state->equals) return false;
    return m_state->held.type() == o.m_state->held.type() &&
           m_state->equals(m_state->held, o.m_state->held);
  }

 private:
  struct State {
    std::any held;
    bool (*equals)(const std::any&, const std::any&) = nullptr;
    std::shared_ptr<const Ops> ops;
  };
  std::shared_ptr<const State> m_state;
};

}  // namespace sigil::compose
