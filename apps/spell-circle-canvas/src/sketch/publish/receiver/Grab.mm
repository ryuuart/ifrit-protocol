// One frame out of a publication and into a file, with no window in the
// way.

#include "Grab.h"

#import <Metal/Metal.h>

#include "Capture.h"
#include "Feed.h"
#include "Servers.h"

#include <cstdio>

namespace receiver {

namespace {

/** How long a grab waits for its publication and its frames together. A
 *  subscriber is not seen by a publisher the instant it asks, and a
 *  publisher draws when it draws, so the budget covers the whole
 *  handshake and not one step of it. */
constexpr double kGrabSeconds = 5.0;

/** How many frames a grab waits for when it was not told. One new frame
 *  is the proof that what is being read was drawn after the subscription,
 *  rather than whatever the surface happened to hold. */
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

  Feed feed(device, arguments.server, arguments.app);
  if (!feed.open(budget)) {
    std::fprintf(stderr, "nothing is publishing under \"%s\" on this machine\n",
                 arguments.server.c_str());
    return 2;
  }

  while (feed.frames() < (unsigned long long)wanted) {
    if ([NSDate timeIntervalSinceReferenceDate] >= deadline) {
      std::fprintf(stderr, "%llu of %d frames arrived from \"%s\" in %.3g seconds\n", feed.frames(),
                   wanted, arguments.server.c_str(), budget);
      return 3;
    }
    if (!feed.standing()) {
      std::fprintf(stderr, "\"%s\" stopped publishing before a frame arrived\n",
                   arguments.server.c_str());
      return 3;
    }
    turnRunLoop(kSlice);
  }

  id<MTLTexture> frame = feed.newestFrame();
  if (!frame) {
    std::fprintf(stderr, "\"%s\" announced a frame it then had none of\n",
                 arguments.server.c_str());
    return 3;
  }
  if (!writeTexturePng(frame, queue, arguments.grabPath)) return 4;
  std::printf("wrote %s (%lux%lu)\n", arguments.grabPath.c_str(), (unsigned long)frame.width,
              (unsigned long)frame.height);
  return 0;
}

}  // namespace receiver
