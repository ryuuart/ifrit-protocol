#pragma once

/** @file
 * How a frames URI names its protocol, its publication and the graphics
 * API that carries it — the one reading `hub.publish()` and
 * `hub.subscribe()` share.
 */

#include <sigilio/frames/Frame.h>
#include <sigilio/frames/Subscription.h>

#include <optional>
#include <string_view>

namespace sigil::io::frames::detail {

/** What a frames URI and its device resolve to. */
struct Carrier {
  GraphicsApi api = GraphicsApi::Default;
  std::string_view name;
  void* device = nullptr;
};

/** `syphon://NAME` is carried over Metal and `spout://NAME` over
 *  Direct3D11; @p device's API, when it names one, must be that one, and
 *  a null handle is this machine's default Metal device. Nothing for any
 *  other scheme, an empty name, a mismatched API or no device. */
inline std::optional<Carrier> carrierOf(std::string_view uri,
                                        const Device& device) {
  constexpr std::string_view separator = "://";
  const size_t split = uri.find(separator);
  if (split == std::string_view::npos) return std::nullopt;
  const std::string_view scheme = uri.substr(0, split);
  Carrier carrier;
  carrier.name = uri.substr(split + separator.size());
  if (scheme == "syphon")
    carrier.api = GraphicsApi::Metal;
  else if (scheme == "spout")
    carrier.api = GraphicsApi::Direct3D11;
  else
    return std::nullopt;
  if (carrier.name.empty()) return std::nullopt;
  if (device.api != GraphicsApi::Default && device.api != carrier.api)
    return std::nullopt;
  carrier.device = device.handle;
  if (!carrier.device && carrier.api == GraphicsApi::Metal)
    carrier.device = defaultMetalDevice();
  if (!carrier.device) return std::nullopt;
  return carrier;
}

}  // namespace sigil::io::frames::detail
