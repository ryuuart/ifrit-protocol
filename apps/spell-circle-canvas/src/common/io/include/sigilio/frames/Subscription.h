#pragma once

/** @file
 * @ingroup io-frames
 * The door another application's frames arrive by: the handle
 * `hub.subscribe()` answers, and the seam a backend implements behind it.
 */

#include <sigilmedia/core/Frame.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilio/frames/Frame.h"
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

  /** THE NEWEST PUBLISHED FRAME, or nothing before the first one arrives
   *  — and the ask that OPENS onto a publication that has appeared.
   *  @trap BORROWED UNTIL THE NEXT CALL: the previous frame is let go
   *  whenever another is asked for, so a caller keeping one past that
   *  takes its own reference, which wrapping it as an image does. */
  virtual std::optional<Frame> latest() = 0;

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

/** What every copy of one handle shares: the picture the newest arrival
 *  was presented as, so asking again before another arrives answers the
 *  same one. */
struct Presentation;

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
 *
 * IT IS A PICTURE SOURCE besides: `frameAt()` answers the newest frame as
 * a `media::Frame` standing on the device it arrived on, which
 * `media::deviceImage()` binds for a recorder the right way up — or
 * reads back and turns over with no recorder — so a subscription goes
 * wherever a `media::PixelSource` does: `material::Texture(subscription)`.
 * @trap Asking for the latest frame is also what OPENS onto a
 * publication that has appeared, so a host asks every frame. */
class Subscription {
 public:
  /** A handle onto no subscription. */
  Subscription() = default;
  /** A handle onto @p end, which a backend made. */
  explicit Subscription(std::shared_ptr<detail::SubscriptionEnd> end);

  /** THE NEWEST PUBLISHED FRAME — its texture and size, with no command
   *  buffer — or nothing before the first one arrives, and the ask that
   *  OPENS onto a publication that has appeared.
   *  @trap BORROWED UNTIL THE NEXT CALL: the previous frame is let go
   *  whenever another is asked for, so a caller keeping one past that
   *  takes its own reference, which wrapping it as an image does. */
  std::optional<Frame> latest() const {
    return m_end ? m_end->latest() : std::nullopt;
  }

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

  /** THE NEWEST FRAME AS A PICTURE: the arrived texture as a device
   *  frame whose binding wraps it for a recorder, or reads it back, the
   *  right way up; the same frame until another arrives, and an empty
   *  one before the first. @p time is not read: a publication answers
   *  what has most recently arrived. Asking is also what opens onto a
   *  publication that has appeared. */
  media::Frame frameAt(std::chrono::duration<double> time = {}) const;
  /** The frames that have arrived, which is what says a frame is new. */
  [[nodiscard]] uint64_t revision() const { return state().revision; }
  /** Whether frames can keep arriving: a held subscription always can,
   *  a publisher that stops being waited for until it comes back. */
  [[nodiscard]] bool isRunning() const { return m_end != nullptr; }
  /** Two handles onto one subscription are one source. */
  bool operator==(const Subscription& other) const {
    return m_end == other.m_end;
  }

 private:
  std::shared_ptr<detail::SubscriptionEnd> m_end;
  std::shared_ptr<detail::Presentation> m_presentation;
};

/** THE METAL DEVICE THIS MACHINE DRAWS ON, as an `id<MTLDevice>` bridged
 * to `void*` — what a caller that holds no device of its own subscribes
 * on, and the one a window drawing through Metal stands on unless it
 * asked for another. The platform owns it; nothing here releases it.
 * Null off macOS and on a machine with no Metal device. */
void* defaultMetalDevice();

}  // namespace sigil::io::frames
