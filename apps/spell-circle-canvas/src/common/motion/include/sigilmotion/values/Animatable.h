#pragma once

/** @file
 * Animatable<T>, the property slot that holds exactly one of a
 * constant, a constant with its own transition, a live Output, or an
 * Output shaped through a bound chain — the fat forms behind one
 * out-of-line block so the slot itself stays small — and the comparator
 * an identity prune reads two slots through.
 */

#include <choreograph/Choreograph.h>
#include <sigilcore/comparable/Fields.h>

#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>

#include "sigilmotion/bind/Bound.h"
#include "sigilmotion/values/Keyframes.h"
#include "sigilmotion/values/Transition.h"

namespace sigil::motion {

/**
 * An ANIMATABLE property value — the slot type behind every property
 * that can move. It holds exactly one of: a constant, a constant with
 * its own transition, a live Choreograph binding, or that binding shaped
 * through `bind()`. A constant is animatable too; the name says what the
 * slot CAN hold, not what it is doing.
 *
 * The caller owns any bound `Output` and the clock that steps it. A slot
 * outliving the Output it points at dangles.
 *
 * Stored compactly rather than as a variant: `Transitioned<T>` is the fat
 * form — spec, entrance value and waypoint list — while most properties
 * on most objects are plain constants, so both fat forms share one
 * out-of-line block and the slot itself stays small. Consumers that carry
 * many of these per object depend on that; do not inline the payload.
 */
template <typename T>
class Animatable {
 public:
  Animatable() = default;
  Animatable(T v) : m_plain(std::move(v)) {}
  Animatable(Transitioned<T> t) : m_kind(Kind::kAnim) {
    extra().anim = std::move(t);
  }
  Animatable(const choreograph::Output<T>* bound)
      : m_kind(Kind::kBound), m_bound(bound) {}
  /** bind(&out).…  — a shaped binding. Float properties only; the extra
   *  block is the same one the transitioned form allocates, so this adds
   *  nothing to sizeof(Animatable) and nothing to a slot that never uses
   *  it. */
  Animatable(const Bound& b) : m_kind(Kind::kBoundMapped) {
    m_bound = b.value().source;
    extra().bound = b.value();
  }
  Animatable(const Animatable& other) { *this = other; }
  Animatable(Animatable&&) noexcept = default;
  Animatable& operator=(const Animatable& other) {
    if (this == &other) return *this;
    m_kind = other.m_kind;
    m_plain = other.m_plain;
    m_bound = other.m_bound;
    m_extra = other.m_extra ? std::make_unique<Extra>(*other.m_extra) : nullptr;
    return *this;
  }
  Animatable& operator=(Animatable&&) noexcept = default;

  /** Which form holds: 0 plain, 1 transitioned, 2 bound, 3 shaped
   *  binding.
   *
   *  A stable discriminant and nothing more: any consumer comparing two
   *  animatable values needs one. The numbering is part of the public
   *  behaviour — a shaped binding sorts AFTER a bare one rather than
   *  taking its place — so append new forms at the end and never
   *  renumber. */
  int index() const { return (int)m_kind; }
  const T* plain() const { return m_kind == Kind::kPlain ? &m_plain : nullptr; }
  const Transitioned<T>* transitioned() const {
    return m_kind == Kind::kAnim ? &m_extra->anim : nullptr;
  }
  /** The bound Output, shaped or not, so a consumer asking only "is this
   *  driven live?" reads one accessor and does not have to know which of
   *  the two bound forms it holds. */
  const choreograph::Output<T>* binding() const {
    return m_kind == Kind::kBound || m_kind == Kind::kBoundMapped ? m_bound
                                                                  : nullptr;
  }
  /** The shaping, if this binding has any. */
  const BoundFloat* boundMap() const {
    return m_kind == Kind::kBoundMapped ? &m_extra->bound : nullptr;
  }

 private:
  enum class Kind : uint8_t { kPlain, kAnim, kBound, kBoundMapped };
  /** The out-of-line block for the two FAT forms. They are mutually
   *  exclusive, so one pointer carries both and a slot holding neither
   *  allocates nothing at all. */
  struct Extra {
    Transitioned<T> anim{};
    BoundFloat bound{};
  };
  Extra& extra() {
    if (!m_extra) m_extra = std::make_unique<Extra>();
    return *m_extra;
  }

  Kind m_kind = Kind::kPlain;
  T m_plain{};
  const choreograph::Output<T>* m_bound = nullptr;
  std::unique_ptr<Extra> m_extra;
};

namespace detail {
/** A transitioned value decomposed member by member, for a comparator
 *  that wants to WALK it rather than name each field one at a time. */
template <typename T>
auto fields(Transitioned<T>& v) {
  auto& [value, spec, from, waypoints] = v;
  return std::tie(value, spec, from, waypoints);
}
}  // namespace detail

static_assert(core::kFieldCount<Transitioned<float>> == 4,
              "Transitioned gained or lost a field — rule on it in "
              "propEqual() below, then bump this count.");
/** Two animatable slots are equal when they take the same form and that
 *  form's contents are equal: a plain value by `==`, a transitioned value
 *  by target, origin, waypoints and spec, a shaped binding by
 *  `boundMapEqual`, and a bare binding by the Output's identity — the
 *  pointer, not the number behind it. A LIVE binding therefore never
 *  compares equal to a different Output, and a slot that is moving is
 *  never pruned into a slot that is moving to something else. */
template <typename T>
bool propEqual(const Animatable<T>& a, const Animatable<T>& b) {
  if (a.index() != b.index()) return false;
  if (const T* plainA = a.plain()) return *plainA == *b.plain();
  if (const Transitioned<T>* trA = a.transitioned()) {
    const Transitioned<T>* trB = b.transitioned();
    return trA->value == trB->value && trA->from == trB->from &&
           trA->waypoints == trB->waypoints &&
           transitionEqual(trA->spec, trB->spec);
  }
  if (const BoundFloat* mapA = a.boundMap())
    return boundMapEqual(*mapA, *b.boundMap());
  return a.binding() == b.binding();
}

/** `propEqual` under the operator, so a description struct holding an
 *  animatable slot keeps its `= default` equality and cannot acquire a
 *  second, weaker rule by accident. ONE body: this IS `propEqual`. */
template <typename T>
bool operator==(const Animatable<T>& a, const Animatable<T>& b) {
  return propEqual(a, b);
}

}  // namespace sigil::motion
