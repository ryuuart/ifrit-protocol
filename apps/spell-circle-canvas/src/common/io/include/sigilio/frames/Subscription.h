#pragma once

/** @file
 * @ingroup io-frames
 * The door another application's frames arrive by: the seam a host holds
 * a publication over, and the one factory that answers with whatever
 * this build can subscribe through.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilio/source/State.h"

namespace sigil::io::frames {

/** One discoverable native frame source. Names are scoped by application. */
struct Publication {
  std::string name;
  std::string application;
};

/** Current directory snapshot. The native main event loop receives
 *  announcements; this call never waits. Unsupported platforms return empty. */
std::vector<Publication> publications();

namespace detail {
/** WHAT A BACKEND IMPLEMENTS behind a `Subscription` handle. */
class SubscriptionEnd {
 public:
  virtual ~SubscriptionEnd() = default;

  /** THE NEWEST PUBLISHED FRAME, or null before the first one arrives —
   *  and the ask that OPENS onto a publication that has appeared.
   *  @trap BORROWED UNTIL THE NEXT CALL: the previous frame is let go
   *  whenever another is asked for, so a caller keeping one past that
   *  takes its own reference, which wrapping it as an image does. */
  virtual void* latest() = 0;

  /** Where the publication stands, in the value a feed answers:
   *  `ReadyState::Open` while frames can still arrive — the publication
   *  answered AND the directory still knows of it, because a publisher
   *  that retires tells its subscribers and one that was killed tells
   *  nobody — and `ReadyState::Connecting` while the name is waited for.
   *  `FeedState::revision` counts the frames that have arrived, across a
   *  publisher that stopped and came back, because a publication that
   *  came back is the same publication: it is what a frame rate is read
   *  from and what says a frame is NEW. */
  [[nodiscard]] virtual FeedState state() const = 0;

  /** The name this subscription follows, as it was asked for. */
  [[nodiscard]] virtual std::string_view name() const = 0;

  /** The application the standing publication is drawn in, as the
   *  directory named it; empty until one stands. */
  [[nodiscard]] virtual std::string_view publishingApplication() const = 0;
};

}  // namespace detail

/** A PUBLICATION, HELD BY ITS NAME: the end that receives the frames
 * another application on this machine offers. THE FRAME IS THE GRAPHICS
 * API'S OWN, as an opaque pointer — on Metal an `id<MTLTexture>` — with
 * alpha premultiplied as the publisher drew it and ITS FIRST ROW AT THE
 * IMAGE'S BOTTOM, which is the way round the surface a publication is
 * carried on is written and read. Nothing is copied on the way in, and a
 * turn is a copy, so what arrives is the surface itself: a caller
 * drawing it in a space whose first row is the top turns it over as it
 * draws. THE NAME IS
 * WHAT IS HELD, not the process behind it: a publisher that stops and
 * starts is followed, and a name nothing publishes yet is waited for.
 * A copyable handle: the subscription ends when the last handle onto it
 * goes, and a handle made empty receives nothing.
 * @trap Asking for the latest frame is also what OPENS onto a
 * publication that has appeared, so a host asks every frame. */
class Subscription {
 public:
  /** A handle onto no subscription. */
  Subscription() = default;
  /** A handle onto @p end, which a backend made. */
  explicit Subscription(std::shared_ptr<detail::SubscriptionEnd> end)
      : m_end(std::move(end)) {}

  /** THE NEWEST PUBLISHED FRAME, or null before the first one arrives —
   *  and the ask that OPENS onto a publication that has appeared.
   *  @trap BORROWED UNTIL THE NEXT CALL: the previous frame is let go
   *  whenever another is asked for, so a caller keeping one past that
   *  takes its own reference, which wrapping it as an image does. */
  void* latest() const { return m_end ? m_end->latest() : nullptr; }

  /** Where the publication stands, in the value a feed answers:
   *  `ReadyState::Open` while frames can still arrive, and a revision
   *  that counts them across a publisher that stopped and came back. A
   *  handle onto nothing answers `ReadyState::Closed`. */
  [[nodiscard]] FeedState state() const {
    if (m_end) return m_end->state();
    FeedState nothing;
    nothing.readiness = ReadyState::Closed;
    return nothing;
  }

  /** The name this subscription follows, as it was asked for. */
  [[nodiscard]] std::string_view name() const {
    return m_end ? m_end->name() : std::string_view();
  }

  /** The application the standing publication is drawn in, as the
   *  directory named it; empty until one stands. */
  [[nodiscard]] std::string_view publishingApplication() const {
    return m_end ? m_end->publishingApplication() : std::string_view();
  }

  /** Whether this handle holds a subscription. */
  explicit operator bool() const { return m_end != nullptr; }

 private:
  std::shared_ptr<detail::SubscriptionEnd> m_end;
};

/** A subscription to @p name on @p metalDevice — and, where @p application
 * is not empty, only that application's. @p metalDevice is an
 * `id<MTLDevice>` as an opaque pointer, the caller staying its owner. A
 * name nothing publishes yet is NOT a refusal: the subscription waits.
 * @trap AN EMPTY HANDLE IS AN ORDINARY ANSWER — no protocol, no device, an
 * empty name — and means a run that receives nothing, never another way
 * to. */
Subscription subscribe(std::string name,
                                        std::string application,
                                        void* metalDevice);

/** THE METAL DEVICE THIS MACHINE DRAWS ON, as an `id<MTLDevice>` bridged
 * to `void*` — what a caller that holds no device of its own subscribes
 * on, and the one a window drawing through Metal stands on unless it
 * asked for another. The platform owns it; nothing here releases it.
 * Null off macOS and on a machine with no Metal device. */
void* defaultMetalDevice();

}  // namespace sigil::io::frames
