/** @file
 * The one place that knows what this build can publish over. Everything
 * else in this repository holds a `Publisher` or holds nothing.
 */

#include <sigilio/frames/Publisher.h>
#include <sigilio/frames/Subscription.h>
#include <sigilio/hub/Hub.h>

#include <string>
#include <utility>

#include "Protocol.h"

#if defined(__APPLE__)
#include "SyphonPublisher.h"
#elif defined(SIGIL_FRAMES_SPOUT)
#include "SpoutPublisher.h"
#endif

namespace sigil::io {

frames::Publisher Hub::publish(std::string_view uri,
                               const frames::PublishOptions& options) {
  const std::optional<frames::detail::Carrier> carrier =
      frames::detail::carrierOf(uri, options.device);
  if (!carrier) return {};
  std::string name(carrier->name);
#if defined(__APPLE__)
  if (carrier->api == frames::GraphicsApi::Metal)
    return frames::Publisher(
        frames::makeSyphonPublisher(std::move(name), carrier->device));
#elif defined(SIGIL_FRAMES_SPOUT)
  if (carrier->api == frames::GraphicsApi::Direct3D11)
    return frames::Publisher(
        frames::makeSpoutPublisher(std::move(name), carrier->device));
#endif
  return {};
}

}  // namespace sigil::io
