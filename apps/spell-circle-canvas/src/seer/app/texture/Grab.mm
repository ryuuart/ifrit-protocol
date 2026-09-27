// One frame out of a publication and into a file, with no window in the
// way.

#include "Grab.h"

#import <Metal/Metal.h>

#include <include/core/SkImage.h>
#include <sigilio/frames/Subscription.h>
#include <sigilio/hub/Hub.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/core/Image.h>
#include <sigilmedia/image/Encode.h>

#include "Servers.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>

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
  if (!device) {
    std::fprintf(stderr, "this machine has no Metal device to receive on\n");
    return 4;
  }

  sigil::io::Hub hub;
  sigil::io::frames::Subscription subscription =
      hub.subscribe("syphon://" + arguments.texture,
                    {.application = arguments.application,
                     .device = {.handle = (__bridge void *)device}});
  if (!subscription) {
    std::fprintf(stderr, "this build subscribes to nothing\n");
    return 4;
  }

  // ASKING FOR A FRAME IS WHAT OPENS onto the publication, so waiting for
  // one to answer is a wait spent asking.
  for (;;) {
    subscription.latest();
    if (subscription.state().isOpen()) break;
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

  while (subscription.state().revision < (uint64_t)wanted) {
    if ([NSDate timeIntervalSinceReferenceDate] >= deadline) {
      std::fprintf(stderr, "%llu of %d frames arrived from \"%s\" in %.3g seconds\n",
                   (unsigned long long)subscription.state().revision, wanted,
                   arguments.texture.c_str(), budget);
      return 3;
    }
    if (!subscription.state().isOpen()) {
      std::fprintf(stderr, "\"%s\" stopped publishing before a frame arrived\n",
                   arguments.texture.c_str());
      return 3;
    }
    turnRunLoop(kSlice);
  }

  // THE SUBSCRIPTION IS A PICTURE SOURCE: its newest frame read back with
  // no recorder is the arrival in host memory, already turned the right
  // way up from the bottom-first surface it was carried on.
  const sk_sp<SkImage> picture =
      sigil::media::deviceImage(subscription.frameAt(), nullptr);
  if (!picture) {
    std::fprintf(stderr, "\"%s\" announced a frame it then had none of\n",
                 arguments.texture.c_str());
    return 3;
  }
  if (!hub.save(arguments.grabPath, sigil::media::Image::of(picture))) {
    std::fprintf(stderr, "%s could not be written\n", arguments.grabPath.c_str());
    return 4;
  }
  std::printf("wrote %s (%dx%d)\n", arguments.grabPath.c_str(), picture->width(),
              picture->height());
  return 0;
}

}  // namespace seer::texture
