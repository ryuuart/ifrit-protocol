#pragma once

/** @file
 * @ingroup io-publish
 * The door another application's frames arrive by: the seam a host holds
 * a publication over, and the one factory that answers with whatever
 * this build can subscribe through.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::io::publish {

/** One discoverable native frame source. Names are scoped by application. */
struct Publication {
  std::string name;
  std::string application;
};

/** Current directory snapshot. The native main event loop receives
 *  announcements; this call never waits. Unsupported platforms return empty. */
std::vector<Publication> publications();

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
 * @trap Asking for the newest frame is also what OPENS onto a
 * publication that has appeared, so a host asks every frame. */
class Subscription {
 public:
  virtual ~Subscription() = default;

  /** THE NEWEST PUBLISHED FRAME, or null before the first one arrives —
   *  and the ask that OPENS onto a publication that has appeared.
   *  @trap BORROWED UNTIL THE NEXT CALL: the previous frame is let go
   *  whenever another is asked for, so a caller keeping one past that
   *  takes its own reference, which wrapping it as an image does. */
  virtual void* newestFrame() = 0;

  /** How many frames have arrived, counted across a publisher that
   *  stopped and came back, because a publication that came back is the
   *  same publication. It is what a frame rate is read from and what
   *  says a frame is NEW rather than whatever was already there. */
  [[nodiscard]] virtual uint64_t generation() const = 0;

  /** True while frames can still arrive: the publication answered AND
   *  the directory still knows of it. Both, because a publisher that
   *  retires tells its subscribers and a publisher that was killed tells
   *  nobody — the second is noticed by the publication going off the
   *  list rather than by the subscription being closed. */
  [[nodiscard]] virtual bool standing() const = 0;

  /** The name this subscription follows, as it was asked for. */
  [[nodiscard]] virtual std::string_view name() const = 0;

  /** The application the standing publication is drawn in, as the
   *  directory named it; empty until one stands. */
  [[nodiscard]] virtual std::string_view publishingApplication() const = 0;
};

/** A subscription to @p name on @p mtlDevice — and, where @p application
 * is not empty, only that application's. @p mtlDevice is an
 * `id<MTLDevice>` as an opaque pointer, the caller staying its owner. A
 * name nothing publishes yet is NOT a refusal: the subscription waits.
 * @trap NULL IS AN ORDINARY ANSWER — no protocol, no device, an empty
 * name — and means a run that receives nothing, never another way to. */
std::unique_ptr<Subscription> subscribe(std::string name,
                                        std::string application,
                                        void* mtlDevice);

/** THE METAL DEVICE THIS MACHINE DRAWS ON, as an `id<MTLDevice>` bridged
 * to `void*` — what a caller that holds no device of its own subscribes
 * on, and the one a window drawing through Metal stands on unless it
 * asked for another. The platform owns it; nothing here releases it.
 * Null off macOS and on a machine with no Metal device. */
void* defaultMetalDevice();

}  // namespace sigil::io::publish
