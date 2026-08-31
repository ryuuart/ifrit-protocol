#pragma once

/** @file
 * Animatable<T>, the property slot that holds exactly one of a
 * constant, a constant with its own transition, a live Output, or an
 * Output shaped through a bound chain — the fat forms behind one
 * out-of-line block so the slot itself stays small.
 */

#include <choreograph/Choreograph.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "sigilmotion/bind/Bound.h"
#include "sigilmotion/values/Keyframes.h"

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

}  // namespace sigil::motion
