/** @file
 * The registry, session and clock agents, made and mounted together.
 */

#include "sigilsketch/live/agent/HostAgents.h"

#include <sigilprotocol/clock/ClockAgent.h>
#include <sigilprotocol/registry/RegistryAgent.h>
#include <sigilprotocol/session/SessionAgent.h>

#include <utility>

namespace sigil::sketch {

HostAgents::HostAgents(protocol::Dispatcher& dispatcher,
                       SessionAgentOptions session, CatalogSources catalog)
    : m_registry(std::move(catalog)),
      m_session(dispatcher, std::move(session)),
      m_clock(dispatcher, m_session) {
  protocol::registry::wire(
      dispatcher, static_cast<protocol::registry::RegistryAgent&>(m_registry));
  protocol::session::wire(
      dispatcher, static_cast<protocol::session::SessionAgent&>(m_session));
  protocol::clock::wire(dispatcher,
                        static_cast<protocol::clock::ClockAgent&>(m_clock));
}

}  // namespace sigil::sketch
