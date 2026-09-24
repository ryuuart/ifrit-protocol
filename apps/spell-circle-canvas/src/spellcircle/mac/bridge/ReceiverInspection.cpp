// The receiver's endpoint: the host domain on loopback, where asked.

#include "ReceiverInspection.h"

#include <sigilio/hub/Hub.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilprotocol/endpoint/Endpoint.h>

#include <charconv>
#include <string_view>
#include <system_error>
#include <utility>

namespace spellcircle {

std::optional<InspectionRequest> inspectionRequested(
    const std::vector<std::string>& arguments) {
  std::optional<InspectionRequest> request;
  std::filesystem::path state;
  for (size_t index = 0; index < arguments.size(); ++index) {
    const std::string_view argument = arguments[index];
    if (argument == "--state" && index + 1 < arguments.size()) {
      state = arguments[++index];
    } else if (argument == "--inspect") {
      request.emplace();
    } else if (argument.starts_with("--inspect=")) {
      const std::string_view port = argument.substr(10);
      unsigned value = 0;
      const auto [end, error] =
          std::from_chars(port.data(), port.data() + port.size(), value);
      if (port.empty() || error != std::errc{} ||
          end != port.data() + port.size() || value > 65535)
        continue;
      request.emplace();
      request->port = static_cast<uint16_t>(value);
    }
  }
  if (request) request->stateRoot = state;
  return request;
}

struct ReceiverInspection::State {
  sigil::io::Hub hub;
  // Made in this order and let go in the reverse, so the endpoint goes
  // first and the hub it leases last.
  std::unique_ptr<sigil::protocol::Dispatcher> dispatcher;
  std::unique_ptr<sigil::protocol::Endpoint> endpoint;

  ~State() {
    endpoint.reset();
    dispatcher.reset();
  }
};

ReceiverInspection::ReceiverInspection(const InspectionRequest& request,
                                       std::filesystem::path stateRoot)
    : m_state(std::make_unique<State>()) {
  std::error_code error;
  std::filesystem::create_directories(stateRoot, error);
  sigil::protocol::Program program;
  program.name = "SpellCircle";
  program.version = SPELLCIRCLE_VERSION;
  program.stateRoot = std::move(stateRoot);
  m_state->dispatcher =
      std::make_unique<sigil::protocol::Dispatcher>(std::move(program));
  sigil::protocol::EndpointPolicy policy;
  policy.port = request.port;
  m_state->endpoint = std::make_unique<sigil::protocol::Endpoint>(
      m_state->hub, *m_state->dispatcher, policy);
}

ReceiverInspection::~ReceiverInspection() = default;

void ReceiverInspection::dispatch() { m_state->hub.dispatch(); }

bool ReceiverInspection::listening() const {
  return m_state->endpoint->listening();
}

std::string ReceiverInspection::address() const {
  return listening() ? m_state->endpoint->address()
                     : m_state->endpoint->error();
}

sigil::protocol::Dispatcher& ReceiverInspection::dispatcher() {
  return *m_state->dispatcher;
}

}  // namespace spellcircle
