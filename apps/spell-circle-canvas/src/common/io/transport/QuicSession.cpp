#include "QuicSession.h"

#include <memory>
#include <string>
#include <utility>

#include "QuicLibrary.h"
#include "sigilio/hub/Feed.h"

namespace sigil::io::quic {

void Session::giveBack() {
  if (!configuration) return;
  library().api->ConfigurationClose(configuration);
  configuration = nullptr;
}

void deliver(const Session& session, const std::string& from, Bytes message) {
  if (const std::shared_ptr<Feed> feed = session.feed.lock())
    feed->deliver(std::move(message), from);
}

}  // namespace sigil::io::quic
