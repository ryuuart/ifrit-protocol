#pragma once

/** @file
 * Binding animation: the clock and the schedules, and the readings
 * that take an animatable number, colour, fill, easing or transition
 * from the shapes Python spells them as.
 */

#include <include/core/SkColor.h>
#include <pybind11/pybind11.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmotion/values/Animatable.h>

#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace sigil::motion {
class Ticker;
}

namespace sigil::python {

class CallbackLifetime;

/** A ticker of Python's own, or checked access to the one a host is
 *  lending. The native calls are the same in both cases; what differs
 *  is who owns the stepping and who holds what the ticker was handed.
 *
 *  A ticker is not thread-safe, so every handle refuses a thread other
 *  than the one that made it, and a borrowed one refuses again through
 *  the access it was given once the host's session has closed.
 *
 *  An owned handle holds the outputs and statuses a ticker call was
 *  given, because nothing else would; a borrowed one hands them to the
 *  session, whose lifetime they belong to rather than an escaped
 *  Python wrapper's. Callables go the same way: a borrowed handle
 *  names the host lifetime they are retained against, and an owned one
 *  names none, which leaves them ordinary shared ownership released
 *  when the native holder lets go. */
class TickerHandle {
 public:
  /** A ticker of this handle's own, stepped from Python. */
  TickerHandle();
  /** A handle onto the host's ticker: @p access reaches it and throws
   *  when the session is gone, @p retain gives the session something
   *  the ticker was handed to hold, and @p callbacks is the lifetime
   *  Python callables are retained against. */
  TickerHandle(std::function<motion::Ticker&()> access,
               std::function<void(std::shared_ptr<const void>)> retain,
               CallbackLifetime* callbacks);
  /** The ticker itself: the one this handle owns, or the host's through
   *  the access it was given. Throws on a thread other than the one
   *  that made the handle, and on a session that has closed. */
  motion::Ticker& get() const;
  /** Whether this handle owns its ticker, and so may step it. */
  bool owned() const { return m_owner != nullptr; }
  /** Holds @p value for as long as the ticker can read it: the handle
   *  itself when it owns the ticker, the host's session otherwise. */
  void retain(std::shared_ptr<const void> value) const;
  /** The lifetime Python callables given to this ticker are retained
   *  against, or null on an owned handle, which has no host. */
  CallbackLifetime* callbacks() const { return m_callbacks; }

 private:
  std::shared_ptr<motion::Ticker> m_owner;
  std::shared_ptr<std::vector<std::shared_ptr<const void>>> m_held;
  std::function<motion::Ticker&()> m_access;
  std::function<void(std::shared_ptr<const void>)> m_retain;
  CallbackLifetime* m_callbacks = nullptr;
  std::thread::id m_thread;
};

/** The master timeline of a ticker, reached through the same access the
 *  ticker's own handle was made with. The timeline lives inside the
 *  ticker, so a Python wrapper that outlives its session refuses rather
 *  than reading storage that has gone. */
class TimelineHandle {
 public:
  /** The timeline of the ticker @p ticker reaches. */
  explicit TimelineHandle(TickerHandle ticker);
  /** The timeline itself, on the same terms as `TickerHandle::get`. */
  choreograph::Timeline& get() const;
  /** The ticker this timeline belongs to, for whatever the timeline is
   *  handed and the ticker has to hold. */
  const TickerHandle& ticker() const { return m_ticker; }

 private:
  TickerHandle m_ticker;
};

/** An animatable number read from @p value: an output, a binding or a
 *  transitioned value, or a plain number that stands still. */
motion::Animatable<float> motionAnimatable(pybind11::handle value);
/** An animatable colour read from @p value, on the same terms. */
motion::Animatable<SkColor4f> motionInk(pybind11::handle value);
/** An animatable fill read from @p value, on the same terms. */
motion::Animatable<compose::Fill> motionFill(pybind11::handle value);
/** An easing read from @p value: a named curve, a curve value, or a
 *  callable that shapes a fraction. None is the default ease-out. */
choreograph::EaseFn motionEase(pybind11::handle value);
/** A transition read from @p value; None is the default transition. */
motion::Transition motionTransition(pybind11::handle value);

}  // namespace sigil::python
