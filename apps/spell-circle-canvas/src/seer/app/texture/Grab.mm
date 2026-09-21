// One frame out of a publication and into a file, with no window in the
// way.

#include "Grab.h"

#import <Metal/Metal.h>

#include <sigilio/publish/Subscription.h>

#include "Capture.h"
#include "Servers.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace seer::texture {

namespace {

/** How long a grab waits for its publication and its frames together. A
 *  subscriber is not seen by a publisher the instant it asks, and a
 *  publisher draws when it draws, so the budget covers the whole
 *  handshake and not one step of it. */
constexpr double kGrabSeconds = 5.0;

/** A retained static frame satisfies the default capture. A larger count
 *  waits for subsequent publications after the subscription connects. */
constexpr int kGrabFrames = 1;

/** The slice the wait for frames is broken into. Frames are counted on
 *  Syphon's own thread, so this is only how often the count is looked
 *  at — and how often anything else another process has to say lands. */
constexpr double kSlice = 0.02;

}  // namespace

int runGrab(const Arguments &arguments) {
  const double budget = arguments.timeoutSeconds.value_or(kGrabSeconds);
  const int wanted = arguments.frames.value_or(kGrabFrames);
  const NSTimeInterval deadline = [NSDate timeIntervalSinceReferenceDate] + budget;

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  id<MTLCommandQueue> queue = [device newCommandQueue];
  if (!device || !queue) {
    std::fprintf(stderr, "this machine has no Metal device to receive on\n");
    return 4;
  }

  std::unique_ptr<sigil::io::publish::Subscription> subscription = sigil::io::publish::subscribe(
      arguments.texture, arguments.application, (__bridge void *)device);
  if (!subscription) {
    std::fprintf(stderr, "this build subscribes to nothing\n");
    return 4;
  }

  // ASKING FOR A FRAME IS WHAT OPENS onto the publication, so waiting for
  // one to answer is a wait spent asking.
  for (;;) {
    subscription->newestFrame();
    if (subscription->standing()) break;
    if ([NSDate timeIntervalSinceReferenceDate] >= deadline) {
      // The application is named back when one was asked for: a
      // publication of that name from somebody else is not the one that
      // was wanted, and a refusal that did not say so would read as
      // nobody publishing it.
      if (arguments.application.empty())
        std::fprintf(stderr, "nothing is publishing under \"%s\" on this machine\n",
                     arguments.texture.c_str());
      else
        std::fprintf(stderr, "%s is publishing nothing under \"%s\" on this machine\n",
                     arguments.application.c_str(), arguments.texture.c_str());
      return 2;
    }
    turnRunLoop(kSlice);
  }

  while (subscription->generation() < (uint64_t)wanted) {
    if ([NSDate timeIntervalSinceReferenceDate] >= deadline) {
      std::fprintf(stderr, "%llu of %d frames arrived from \"%s\" in %.3g seconds\n",
                   (unsigned long long)subscription->generation(), wanted,
                   arguments.texture.c_str(), budget);
      return 3;
    }
    if (!subscription->standing()) {
      std::fprintf(stderr, "\"%s\" stopped publishing before a frame arrived\n",
                   arguments.texture.c_str());
      return 3;
    }
    turnRunLoop(kSlice);
  }

  id<MTLTexture> frame = (__bridge id<MTLTexture>)subscription->newestFrame();
  if (!frame) {
    std::fprintf(stderr, "\"%s\" announced a frame it then had none of\n",
                 arguments.texture.c_str());
    return 3;
  }
  // A RECEIVED FRAME IS THE CARRIED SURFACE ITSELF, whose first row is
  // the bottom of the picture, so the file is written the other way up
  // from the rows it arrived in.
  if (!writeTexturePng(frame, queue, arguments.grabPath, Rows::BottomFirst))
    return 4;
  std::printf("wrote %s (%lux%lu)\n", arguments.grabPath.c_str(), (unsigned long)frame.width,
              (unsigned long)frame.height);
  return 0;
}

}  // namespace seer::texture
