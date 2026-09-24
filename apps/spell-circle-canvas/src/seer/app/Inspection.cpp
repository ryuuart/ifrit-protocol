/** @file
 * Seer's endpoint: the host domain on loopback.
 */

#include "Inspection.h"

#include <sigilio/hub/Hub.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilprotocol/endpoint/Endpoint.h>

#include <cstdio>
#include <utility>

namespace seer {

struct Inspection::State {
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

Inspection::Inspection(uint16_t port, std::filesystem::path stateRoot)
    : m_state(std::make_unique<State>()) {
  sigil::protocol::Program program;
  program.name = "Seer";
  program.version = SIGIL_SEER_VERSION;
  program.stateRoot = std::move(stateRoot);
  m_state->dispatcher =
      std::make_unique<sigil::protocol::Dispatcher>(std::move(program));
  sigil::protocol::EndpointPolicy policy;
  policy.port = port;
  m_state->endpoint = std::make_unique<sigil::protocol::Endpoint>(
      m_state->hub, *m_state->dispatcher, policy);
  if (m_state->endpoint->listening())
    std::fprintf(stderr, "[seer] protocol at %s\n",
                 m_state->endpoint->address().c_str());
  else
    std::fprintf(stderr, "[seer] %s\n", m_state->endpoint->error().c_str());
}

Inspection::~Inspection() = default;

void Inspection::dispatch() { m_state->hub.dispatch(); }

bool Inspection::listening() const { return m_state->endpoint->listening(); }

}  // namespace seer
