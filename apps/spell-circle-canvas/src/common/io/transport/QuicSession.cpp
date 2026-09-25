#include "QuicSession.h"

#include <memory>
#include <string>
#include <utility>

#include "QuicLibrary.h"

namespace sigil::io::quic {

void Session::giveBack() {
  if (!configuration) return;
  library().api->ConfigurationClose(configuration);
  configuration = nullptr;
}

void deliver(const Session& session, const std::string& from, Bytes message) {
  session.inlet.deliver(std::move(message), from);
}

}  // namespace sigil::io::quic
