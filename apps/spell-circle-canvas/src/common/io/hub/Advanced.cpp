/** @file
 * The control `sigilio/advanced/` declares over a hub: each free
 * function reaches the hub's own member through the one door the hub
 * grants, so the hub's page names none of them.
 */

#include <utility>

#include "sigilio/advanced/Decoding.h"
#include "sigilio/advanced/Feeds.h"
#include "sigilio/advanced/Network.h"
#include "sigilio/advanced/Places.h"
#include "sigilio/advanced/Residency.h"
#include "sigilio/advanced/Time.h"
#include "sigilio/advanced/Transport.h"
#include "sigilio/hub/Hub.h"

namespace sigil::io {

struct detail::HubAccess {
  static void mount(Hub& hub, std::string prefix,
                    std::filesystem::path directory) {
    hub.mount(std::move(prefix), std::move(directory));
  }
  static std::filesystem::path resolve(const Hub& hub, std::string_view uri) {
    return hub.resolve(uri);
  }
  static std::vector<std::string> select(const Hub& hub,
                                         std::string_view selector) {
    return hub.select(selector);
  }
  static bool poll(Hub& hub) { return hub.poll(); }
  static size_t preload(Hub& hub, std::span<const std::string_view> uris) {
    return hub.preload(uris);
  }
  static size_t preload(Hub& hub, std::span<const std::string> uris) {
    return hub.preload(uris);
  }
  static size_t preload(Hub& hub, std::string_view selector) {
    return hub.preload(selector);
  }
  static ResourceLease retain(Hub& hub) { return hub.retain(); }
  static ResourceLease retain(Hub& hub, std::string_view selector) {
    return hub.retain(selector);
  }
  static ResourceLease retain(Hub& hub,
                              std::span<const std::string_view> selectors) {
    return hub.retain(selectors);
  }
  static size_t discardUnretained(Hub& hub) { return hub.discardUnretained(); }
  static void advance(Hub& hub) { hub.advance(); }
  static void advance(Hub& hub, std::chrono::duration<double> time) {
    hub.advance(time);
  }
  static Lease onAdvance(Hub& hub, Lease::Callback callback) {
    return hub.onAdvance(std::move(callback));
  }
  static std::vector<Feed> feeds(const Hub& hub) { return hub.feeds(); }
  static void registerTransport(Hub& hub, std::string scheme,
                                Transport transport) {
    hub.setFeedTransport(std::move(scheme), std::move(transport));
  }
  static Transport transport(const Hub& hub, std::string_view scheme) {
    return hub.feedTransport(scheme);
  }
  static void setNetworkCacheDirectory(Hub& hub,
                                       std::filesystem::path directory) {
    hub.setNetworkCacheDirectory(std::move(directory));
  }
  static void setNetworkPolicy(Hub& hub, NetworkPolicy policy) {
    hub.setNetworkPolicy(policy);
  }
  static void setNetworkTransport(Hub& hub, NetworkTransport transport) {
    hub.setNetworkTransport(std::move(transport));
  }
  static void setDecoder(Hub& hub, std::type_index type,
                         detail::Redecode decode, detail::Configure configure) {
    hub.setDecoder(type, std::move(decode), std::move(configure));
  }
  static std::shared_ptr<const Bytes> probeRead(const Hub& hub,
                                                std::string_view uri,
                                                ResourceInfo& info) {
    return hub.probeFetch(uri, info);
  }
};

using detail::HubAccess;

void mount(Hub& hub, std::string prefix, std::filesystem::path directory) {
  HubAccess::mount(hub, std::move(prefix), std::move(directory));
}
std::filesystem::path resolve(const Hub& hub, std::string_view uri) {
  return HubAccess::resolve(hub, uri);
}
std::vector<std::string> select(const Hub& hub, std::string_view selector) {
  return HubAccess::select(hub, selector);
}
bool poll(Hub& hub) { return HubAccess::poll(hub); }

size_t preload(Hub& hub, std::span<const std::string_view> uris) {
  return HubAccess::preload(hub, uris);
}
size_t preload(Hub& hub, std::span<const std::string> uris) {
  return HubAccess::preload(hub, uris);
}
size_t preload(Hub& hub, std::string_view selector) {
  return HubAccess::preload(hub, selector);
}
ResourceLease retain(Hub& hub) { return HubAccess::retain(hub); }
ResourceLease retain(Hub& hub, std::string_view selector) {
  return HubAccess::retain(hub, selector);
}
ResourceLease retain(Hub& hub, std::span<const std::string_view> selectors) {
  return HubAccess::retain(hub, selectors);
}
ResourceLease retain(Hub& hub,
                     std::initializer_list<std::string_view> selectors) {
  return HubAccess::retain(
      hub, std::span<const std::string_view>(selectors.begin(),
                                             selectors.size()));
}
size_t discardUnretained(Hub& hub) { return HubAccess::discardUnretained(hub); }

void advance(Hub& hub) { HubAccess::advance(hub); }
void advance(Hub& hub, std::chrono::duration<double> time) {
  HubAccess::advance(hub, time);
}
Lease onAdvance(Hub& hub, Lease::Callback callback) {
  return HubAccess::onAdvance(hub, std::move(callback));
}

std::vector<Feed> feeds(const Hub& hub) { return HubAccess::feeds(hub); }

void registerTransport(Hub& hub, std::string scheme, Transport transport) {
  HubAccess::registerTransport(hub, std::move(scheme), std::move(transport));
}
Transport transport(const Hub& hub, std::string_view scheme) {
  return HubAccess::transport(hub, scheme);
}

void setNetworkCacheDirectory(Hub& hub, std::filesystem::path directory) {
  HubAccess::setNetworkCacheDirectory(hub, std::move(directory));
}
void setNetworkPolicy(Hub& hub, NetworkPolicy policy) {
  HubAccess::setNetworkPolicy(hub, policy);
}
void setNetworkTransport(Hub& hub, NetworkTransport transport) {
  HubAccess::setNetworkTransport(hub, std::move(transport));
}

void detail::setDecoder(Hub& hub, std::type_index type, Redecode decode,
                        Configure configure) {
  HubAccess::setDecoder(hub, type, std::move(decode), std::move(configure));
}
std::shared_ptr<const Bytes> detail::probeRead(const Hub& hub,
                                               std::string_view uri,
                                               ResourceInfo& info) {
  return HubAccess::probeRead(hub, uri, info);
}

}  // namespace sigil::io
