/** @file
 * The one place that knows what this build can subscribe over.
 * Everything else in this repository holds a `Subscription` or holds
 * nothing.
 */

#include <sigilio/frames/Publisher.h>
#include <sigilio/frames/Subscription.h>
#include <sigilio/hub/Hub.h>

#include <string>
#include <utility>

#include "Protocol.h"

#if defined(__APPLE__)
#include "SyphonSubscription.h"
#endif

namespace sigil::io {

namespace frames {

std::vector<Publication> publications() {
#if defined(__APPLE__)
  return syphonPublications();
#else
  return {};
#endif
}

void* defaultMetalDevice() {
#if defined(__APPLE__)
  return metalDeviceOfThisMachine();
#else
  return {};
#endif
}

}  // namespace frames

frames::Subscription Hub::subscribe(std::string_view uri,
                                    const frames::SubscribeOptions& options) {
  const std::optional<frames::detail::Carrier> carrier =
      frames::detail::carrierOf(uri, options.device);
  if (!carrier) return {};
#if defined(__APPLE__)
  if (carrier->api == frames::GraphicsApi::Metal)
    return frames::Subscription(frames::makeSyphonSubscription(
        std::string(carrier->name), options.application, carrier->device));
#endif
  return {};
}

}  // namespace sigil::io
