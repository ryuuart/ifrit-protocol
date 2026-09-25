/** @file
 * The one registration a consumer makes: the schemes it names, read as
 * the transports that answer them, installed in the order one stands on
 * another.
 */

#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "Registration.h"
#include "sigilio/advanced/Transport.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {

void registerTransports(Hub& hub, const std::vector<std::string>& schemes) {
  const auto named = [&schemes](std::initializer_list<std::string_view>
                                    answered) {
    if (schemes.empty()) return true;
    return std::any_of(answered.begin(), answered.end(),
                       [&schemes](std::string_view scheme) {
                         return std::find(schemes.begin(), schemes.end(),
                                          scheme) != schemes.end();
                       });
  };
  const bool webRtc = named({"webrtc"});
  if (named({"udp", "osc", "artnet"})) detail::registerUdp(hub);
  // The listener first and the caller after it: the caller stands in
  // front of whatever the scheme holds and hands a URI with no host back
  // to it, so the order is what makes both forms open. The secure scheme
  // is emptied first, since nothing listens for it and the caller would
  // otherwise stand in front of the caller an earlier registration left
  // there. A webrtc feed opens its signalling door through this same hub,
  // so both ends of that door come with it.
  if (webRtc || named({"ws", "wss"})) {
    hub.setFeedTransport("wss", {});
    detail::registerWebSocket(hub);
    detail::registerWebSocketClient(hub);
  }
  if (named({"shm"})) detail::registerSharedMemory(hub);
  if (named({"midi"})) detail::registerMidi(hub);
  if (named({"serial"})) detail::registerSerial(hub);
  if (named({"grpc"})) detail::registerGrpc(hub);
  if (named({"quic"})) detail::registerQuic(hub);
  if (webRtc) detail::registerWebRtc(hub);
}

void detail::installLinkedTransports(Hub& hub) {
  // Only the schemes nothing answers yet: a transport a host set by hand
  // stands.
  std::vector<std::string> missing;
  for (const char* scheme : {"udp", "osc", "artnet", "ws", "wss", "shm", "midi",
                             "serial", "grpc", "quic", "webrtc"})
    if (!hub.feedTransport(scheme)) missing.emplace_back(scheme);
  if (!missing.empty()) registerTransports(hub, missing);
}

namespace {

// The same hand-over Linked.cpp makes, from inside the archive: a program
// that names registerTransports() anywhere links this member, and a
// library that links the transports privately carries no Linked.cpp of
// its own into the program above it.
const bool linked = [] {
  detail::setLinkedTransports(&detail::installLinkedTransports);
  return true;
}();

}  // namespace

}  // namespace sigil::io
