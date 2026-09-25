#pragma once

/** @file
 * @ingroup motion-values
 *
 * `Animatable<T>`, the one noun for a value that can change over time:
 * what every property verb takes, and what `animatable()` and `bind()`
 * hand back. It holds exactly one of a constant, a described motion, a
 * live value somebody writes, or a live value followed through a
 * `Binding` — the fat forms behind one out-of-line block so the slot
 * itself stays small — and compares under the rule an identity prune
 * reads two slots by.
 */

#include <sigilcore/comparable/Fields.h>
#include <sigilmotion/bind/Binding.h>

#include <cstdint>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include "sigilmotion/values/Tween.h"

namespace sigil::motion {

namespace detail {
/** THE CELL a live value is: the number, and whether a motion is writing
 *  it. Every copy of a live `Animatable` shares one, which is what makes a
 *  live value live — the sketch writes it, the engine writes it, and every
 *  property holding a copy reads the same number. */
template <typename T>
struct Cell {
  T value{};
  /** Which motion may write the cell: a motion started on it takes the
   *  next number, and one holding an older number has been replaced and
   *  writes nothing more. */
  uint32_t writer = 0;
  /** A motion is writing the cell. */
  bool moving = false;
  /** The motion writing it, while one is — what a blended change rides
   *  on top of. */
  std::weak_ptr<void> motion;
};
}  // namespace detail

/**
 * A VALUE THAT CAN CHANGE OVER TIME — the one type every property that can
 * move takes, and the one a sketch holds for a number it drives.
 *
 *     Animatable<float> opacity = 1.0f;                       // a constant
 *     Animatable<float> fade = animate({.from = 0.0f, .to = 1.0f});  // a described motion
 *     Animatable<float> wave = animatable(0.0f);              // a live value you write
 *     wave = std::sin(seconds);
 *     Animatable<float> tilt = bind(wave, {.to = {-8, 8}});   // a live value, shaped
 *
 * A constant is animatable too: the name says what the slot CAN hold, not
 * what it is doing.
 *
 * A LIVE value is shared, not copied: every copy of one reads and writes
 * the same cell, so handing `wave` to a property connects the property to
 * the number rather than taking a snapshot of it, and the cell lives for
 * as long as any copy does. Assigning a number to a live value writes the
 * cell; assigning one to any other form makes it that constant.
 *
 * Stored compactly rather than as a variant: the described and shaped
 * forms are fat while most properties on most objects are plain
 * constants, so every fat form shares one out-of-line block and the slot
 * itself stays small. Consumers that carry many of these per object depend
 * on that; do not inline the payload.
 */
template <typename T>
class Animatable {
 public:
  /** Which form holds. The numbering is part of the public behaviour — a
   *  shaped live value sorts after a bare one rather than taking its place
   *  — so append new forms at the end and never renumber. */
  enum class Form : uint8_t { Constant, Described, Live, Bound };

  Animatable() = default;
  Animatable(T value) : m_constant(std::move(value)) {}
  Animatable(Tween<T> described) : m_form(Form::Described) {
    extra().described = std::move(described);
  }
  Animatable(const Animatable& other) { *this = other; }
  Animatable(Animatable&&) noexcept = default;
  Animatable& operator=(const Animatable& other) {
    if (this == &other) return *this;
    m_form = other.m_form;
    m_constant = other.m_constant;
    m_extra = other.m_extra ? std::make_unique<Extra>(*other.m_extra) : nullptr;
    return *this;
  }
  Animatable& operator=(Animatable&&) noexcept = default;
  /** A live value takes the number — every copy reads it from here on; any
   *  other form becomes that constant. */
  Animatable& operator=(T value) {
    if (m_form == Form::Live) {
      m_extra->cell->value = std::move(value);
    } else {
      m_form = Form::Constant;
      m_constant = std::move(value);
      m_extra.reset();
    }
    return *this;
  }

  /** THE VALUE NOW: a live value's number, a shaped one's source run
   *  through its stages, a constant, or a described motion's settled
   *  value — where it rests when it is not moving. */
  [[nodiscard]] T value() const {
    switch (m_form) {
      case Form::Constant:
        return m_constant;
      case Form::Described:
        return m_extra->described.rest();
      case Form::Live:
        return m_extra->cell->value;
      case Form::Bound:
        if constexpr (std::is_same_v<T, float>)
          return m_extra->binding.apply(m_extra->source->value());
        else
          return m_constant;
    }
    return m_constant;
  }

  /** WHETHER THE VALUE IS DECLARED TO MOVE: a live value always is — the
   *  hand that writes it can stop at any moment and nothing here can see
   *  when — and a shaped one is when its source is. A constant and a
   *  described motion are not; a described motion moves only once a host
   *  runs it, and the host holds that motion. */
  [[nodiscard]] bool isRunning() const {
    return m_form == Form::Live ||
           (m_form == Form::Bound && m_extra->source->isRunning());
  }

  // ── the forms, read back: for the reconciler that runs them and the
  //    prune that compares them ─────────────────────────────────────────

  [[nodiscard]] Form form() const { return m_form; }
  /** The constant, when that is the form held. */
  [[nodiscard]] const T* constant() const {
    return m_form == Form::Constant ? &m_constant : nullptr;
  }
  /** The described motion, when that is the form held. */
  [[nodiscard]] const Tween<T>* described() const {
    return m_form == Form::Described ? &m_extra->described : nullptr;
  }
  /** The stages a shaped value runs its source through. */
  [[nodiscard]] const Binding* binding() const {
    return m_form == Form::Bound ? &m_extra->binding : nullptr;
  }
  /** The value a shaped value follows. */
  [[nodiscard]] const Animatable<float>* source() const {
    return m_form == Form::Bound ? m_extra->source.get() : nullptr;
  }
  /** WHICH live cell this value reads — its own, or, for a shaped value,
   *  the one its source reads — and null for a constant or a described
   *  motion. Two live values are the same value exactly when this is. */
  [[nodiscard]] const void* identity() const {
    if (m_form == Form::Live) return m_extra->cell.get();
    if (m_form == Form::Bound) return m_extra->source->identity();
    return nullptr;
  }
  /** The cell itself, for the engine that writes it; null unless live. */
  [[nodiscard]] const std::shared_ptr<detail::Cell<T>>& cell() const {
    static const std::shared_ptr<detail::Cell<T>> kNone;
    return m_form == Form::Live ? m_extra->cell : kNone;
  }

  template <typename U>
  friend Animatable<U> animatable(U initial);
  friend Animatable<float> bind(const Animatable<float>& source,
                                Binding stages);

 private:
  /** The out-of-line block for the fat forms. They are mutually
   *  exclusive, so one pointer carries all of them and a slot holding a
   *  constant allocates nothing at all. */
  struct Extra {
    Tween<T> described{};
    std::shared_ptr<detail::Cell<T>> cell;
    Binding binding{};
    std::shared_ptr<const Animatable<float>> source;
  };
  Extra& extra() {
    if (!m_extra) m_extra = std::make_unique<Extra>();
    return *m_extra;
  }

  Form m_form = Form::Constant;
  T m_constant{};
  std::unique_ptr<Extra> m_extra;
};

/** A LIVE VALUE, starting at @p initial: write it (`wave = std::sin(t)`),
 *  hand it to a property, and the property follows. Every copy shares the
 *  one cell. */
template <typename T>
Animatable<T> animatable(T initial) {
  Animatable<T> live;
  live.m_form = Animatable<T>::Form::Live;
  live.extra().cell = std::make_shared<detail::Cell<T>>();
  live.m_extra->cell->value = std::move(initial);
  return live;
}

/** FOLLOW A LIVE NUMBER, shaped on its way: the result reads @p source
 *  through @p stages whenever it is read, so it is exactly as live as the
 *  source is and costs nothing until somebody reads it.
 *
 *      .rotateY(bind(flip, {.to = {0, 360}}))
 *      .opacity(bind(seconds, {.from = {0, 1.06f},
 *                              .envelope = envelope::square(0.58f),
 *                              .to = {0.1f, 1.0f}}))
 *
 *  A shaped value can itself be followed, and the stages compose: the
 *  result of `bind(bind(x, a), b)` reads `b` applied to `a` applied to
 *  `x`. */
inline Animatable<float> bind(const Animatable<float>& source,
                              Binding stages = {}) {
  Animatable<float> bound;
  bound.m_form = Animatable<float>::Form::Bound;
  bound.extra().binding = std::move(stages);
  bound.m_extra->source = std::make_shared<const Animatable<float>>(source);
  return bound;
}

namespace detail {
/** THE FIELD PIN on a tween: a structured binding names every field, so a
 *  field added to or taken from `Tween` fails to compile here until it is
 *  ruled on in `tweenEqual()` — which `propertyEqual()` below reads a
 *  described motion by — and named in this list. A staggered field
 *  converts from anything its value does, which a counted pin cannot see
 *  past, so this one is spelled out. */
inline auto fields(Tween<float>& tween) {
  auto& [from, to, keyframes, duration, delay, ease, loop, alternate,
         composition] = tween;
  return std::tie(from, to, keyframes, duration, delay, ease, loop, alternate,
                  composition);
}
}  // namespace detail

/** TWO SLOTS ARE EQUAL when they take the same form and that form's
 *  contents are equal: a constant by `==`, a described motion by
 *  `tweenEqual`, a live value by the CELL it reads — its
 *  identity, never the number behind it — and a shaped value by its
 *  source's identity and its stages. A live value therefore never compares
 *  equal to a different one, and a slot that is moving is never pruned
 *  into a slot that is moving to something else. */
template <typename T>
bool propertyEqual(const Animatable<T>& left, const Animatable<T>& right) {
  using Form = typename Animatable<T>::Form;
  if (left.form() != right.form()) return false;
  switch (left.form()) {
    case Form::Constant:
      return *left.constant() == *right.constant();
    case Form::Described:
      return tweenEqual(*left.described(), *right.described());
    case Form::Live:
      return left.identity() == right.identity();
    case Form::Bound:
      return propertyEqual(*left.source(), *right.source()) &&
             *left.binding() == *right.binding();
  }
  return false;
}

/** `propertyEqual` under the operator, so a description struct holding an
 *  animatable slot keeps its `= default` equality and cannot acquire a
 *  second, weaker rule by accident. ONE body: this IS `propertyEqual`. */
template <typename T>
bool operator==(const Animatable<T>& left, const Animatable<T>& right) {
  return propertyEqual(left, right);
}

inline Animatable<float> animate(Tween<float> tween) {
  return Animatable<float>(std::move(tween));
}

template <typename T>
Animatable<T> animate(Tween<T> tween) {
  return Animatable<T>(std::move(tween));
}

}  // namespace sigil::motion
