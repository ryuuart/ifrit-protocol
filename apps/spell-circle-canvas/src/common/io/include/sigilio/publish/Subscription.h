#pragma once

/** @file
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

/**
 * A PUBLICATION, HELD BY ITS NAME. Another application on this machine
 * offers its frames under a name; this is the end that receives them, so
 * a host may wear somebody else's picture the way it wears one of its
 * own.
 *
 * THE FRAME IS THE GRAPHICS API'S OWN, as an opaque pointer: on Metal an
 * `id<MTLTexture>`, the same handle the publisher hands over. Nothing
 * here is Skia's and nothing is a toolkit's, so what receives a frame is
 * plain C++ and the header compiles wherever this repository compiles.
 *
 * THE NAME IS WHAT IS HELD, not the process behind it. A publisher that
 * stops and starts is followed rather than lost, and a name nothing
 * publishes yet is waited for: asking for the newest frame is also what
 * OPENS onto a publication that has appeared, so a host that asks every
 * frame — which is what a host does — needs nothing else to reconnect.
 *
 * WHAT ARRIVES is the frame as the publisher drew it: its rows in the
 * order its texture holds them, the first of them the top of the
 * picture, and its alpha premultiplied the way that canvas wrote it.
 * Nothing here turns the picture over or divides the alpha out.
 */
class Subscription {
 public:
  virtual ~Subscription() = default;

  /** THE NEWEST PUBLISHED FRAME, or null before the first one arrives
   *  — and the ask that OPENS onto a publication that has appeared,
   *  which is why an answer of nothing is worth asking for.
   *
   *  It is BORROWED UNTIL THE NEXT CALL: this subscription holds the
   *  reference and lets the previous frame go whenever it is asked for
   *  another, so a caller that must keep one past that call takes its
   *  own reference — which is what wrapping it as an image does. */
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

/**
 * A subscription to @p name on @p mtlDevice — the publication that
 * announced itself under that name and, where @p application is not
 * empty, only that application's.
 *
 * @p mtlDevice is an `id<MTLDevice>` as an opaque pointer, the device
 * the frames will be made textures on; the caller stays its owner, and a
 * caller that holds no device of its own subscribes on
 * `defaultMetalDevice()`.
 *
 * NULL IS AN ORDINARY ANSWER: this build subscribes over no protocol, or
 * there is no device to receive on, or the name is empty and nothing
 * could be found under it. A caller treats a null subscription as a run
 * that receives nothing and says so, never as a reason to receive
 * another way.
 *
 * A NAME NOTHING PUBLISHES YET IS NOT A REFUSAL. The subscription
 * stands, waiting, and opens onto the publication when it appears — so a
 * host subscribes once and the order the two applications were started
 * in stops mattering.
 */
std::unique_ptr<Subscription> subscribe(std::string name,
                                        std::string application,
                                        void* mtlDevice);

/**
 * THE METAL DEVICE THIS MACHINE DRAWS ON, as an `id<MTLDevice>` bridged
 * to `void*` — what a caller that holds no device of its own subscribes
 * on. The platform owns it; nothing here releases it. Null off macOS and
 * on a machine with no Metal device.
 *
 * It is the device a window drawing through Metal stands on where that
 * window asked for no other, which is what makes a received frame a
 * texture that same drawing can sample: a texture belongs to the device
 * it was made on and to no other.
 */
void* defaultMetalDevice();

}  // namespace sigil::io::publish
