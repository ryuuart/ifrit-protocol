#pragma once

/** @file
 * Binding animation: the engine and the readings that take an
 * animatable number, colour or fill, a tween, a staggered value, an
 * easing or a transition from the shapes Python spells them as.
 */

#include <include/core/SkColor.h>
#include <pybind11/pybind11.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/time/Duration.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Transition.h>
#include <sigilmotion/values/Tween.h>

#include <functional>
#include <memory>
#include <thread>

/** THE LINE A COLOUR ANIMATED FROM PYTHON TAKES: Python's colour
 *  animatable holds Skia's four floats, and its line between two of them
 *  is Material's, so the engine moves it exactly as it moves a
 *  `material::Color`. Declared beside the type's own namespace, where the
 *  engine's `interpolate()` finds it. */
inline SkColor4f interpolate(const SkColor4f& start, const SkColor4f& end,
                             float amount) {
  const sigil::material::Color mixed = sigil::material::interpolate(
      {start.fR, start.fG, start.fB, start.fA},
      {end.fR, end.fG, end.fB, end.fA}, amount);
  return {mixed.r, mixed.g, mixed.b, mixed.a};
}

namespace sigil::python {

class CallbackLifetime;

/** An engine of Python's own, or checked access to the one a host is
 *  lending. The native calls are the same in both cases; what differs
 *  is who moves the engine's clock.
 *
 *  An engine is not thread-safe, so an owned handle refuses a thread
 *  other than the one that made it, and a borrowed one refuses through
 *  the access it was given, which also reports a session that has
 *  closed.
 *
 *  Nothing an engine is handed is held by a bare pointer: a live value
 *  shares its cell with every copy, and a playback is a shared state.
 *  What remains is the Python callables, which a borrowed handle
 *  retains against the host lifetime it names, and an owned one leaves
 *  to ordinary shared ownership released when the native holder lets
 *  go. */
class EngineHandle {
 public:
  /** An engine of this handle's own, moved from Python. */
  explicit EngineHandle(motion::EngineOptions options = {});
  /** A handle onto the host's engine: @p access reaches it and throws
   *  when the session is gone, and @p callbacks is the lifetime Python
   *  callables are retained against. */
  EngineHandle(std::function<motion::Engine&()> access,
               CallbackLifetime* callbacks);
  /** The engine itself: the one this handle owns, or the host's through
   *  the access it was given. Throws on a thread other than the one
   *  that made an owned handle, and on a session that has closed. */
  motion::Engine& get() const;
  /** Whether this handle owns its engine, and so may move its clock. */
  bool owned() const { return m_owner != nullptr; }
  /** The engine on the same terms as `get`, for a call only the owner
   *  of an engine makes. Throws @p refusal on a borrowed handle. */
  motion::Engine& ownedEngine(const char* refusal) const;
  /** The lifetime Python callables given to this engine are retained
   *  against, or null on an owned handle, which has no host. */
  CallbackLifetime* callbacks() const { return m_callbacks; }

 private:
  std::shared_ptr<motion::Engine> m_owner;
  std::function<motion::Engine&()> m_access;
  CallbackLifetime* m_callbacks = nullptr;
  std::thread::id m_thread;
};

/** An animatable number read from @p value: an animatable, a tween,
 *  or a plain number that stands still. */
motion::Animatable<float> motionAnimatable(pybind11::handle value);
/** An animatable colour read from @p value, on the same terms. */
motion::Animatable<SkColor4f> motionInk(pybind11::handle value);
/** An animatable fill read from @p value, on the same terms; a colour
 *  animatable or tween is read as the fill of that colour. */
motion::Animatable<compose::Fill> motionFill(pybind11::handle value);
/** An easing read from @p value: a named curve, a curve value, or a
 *  callable that shapes a fraction. None is the default ease-out. */
motion::Easing motionEase(pybind11::handle value);
/** @p curve as Python reads it back: the `Easing` class, callable and
 *  comparable under the rule two held curves compare by. */
pybind11::object easingReading(const motion::Easing& curve);
/** A transition read from @p value: a transition, or a number of
 *  seconds for a transition of that duration. None is the default. */
motion::Transition motionTransition(pybind11::handle value);
/** A tween of a number read from @p value, which must be one. */
motion::Tween<float> motionTween(pybind11::handle value);

/** A number that may differ per child, read from @p value: a plain
 *  number or a `Staggered`. */
motion::Staggered<float> staggeredNumber(pybind11::handle value);
/** A length of time that may differ per child, read from @p value: a
 *  number of seconds, a `datetime.timedelta`, or a `Staggered` whose
 *  values are seconds. */
motion::Staggered<motion::Duration> staggeredDuration(pybind11::handle value);
/** @p value as Python reads it back: the plain number when it is the
 *  same for every child, the `Staggered` otherwise. */
pybind11::object staggeredReading(const motion::Staggered<float>& value);
/** @p value as Python reads it back, in seconds. */
pybind11::object staggeredReading(const motion::Staggered<motion::Duration>& value);

}  // namespace sigil::python
