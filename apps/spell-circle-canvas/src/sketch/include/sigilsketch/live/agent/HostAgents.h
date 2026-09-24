#pragma once

/** @file
 * @ingroup sketch-live
 *
 * The agents a sketch host mounts — the registry, the session and its
 * clock — made and wired in one place, so a host only mounts them.
 */

#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilsketch/core/agent/RegistryAgent.h>
#include <sigilsketch/live/agent/ClockAgent.h>
#include <sigilsketch/live/agent/SessionAgent.h>

namespace sigil::sketch {

/** EVERY AGENT A SKETCH HOST MOUNTS: the registry's, the session's and
 *  the clock's, each wired on the host's one dispatcher as it is made.
 *  Sketchbook, a test's harness and Python's in-process host each hold
 *  one, so the three answer the same commands the same way.
 *
 *  It is made after the dispatcher and let go before it, and after
 *  every client attached to it: the endpoint and the in-process clients
 *  are declared after it, so they go first. */
class HostAgents {
 public:
  /** The agents, mounted on @p dispatcher; the registry's catalog reads
   *  @p catalog, and the session builds its hosts as @p session says. */
  HostAgents(protocol::Dispatcher& dispatcher, SessionAgentOptions session,
             CatalogSources catalog = {});

  HostAgents(const HostAgents&) = delete;
  HostAgents& operator=(const HostAgents&) = delete;

  /** ONE TURN OF THE HOST'S LOOP: the session's, see
   *  `SessionAgent::frame`. */
  void frame() { m_session.frame(); }

  [[nodiscard]] RegistryAgent& registry() { return m_registry; }
  [[nodiscard]] SessionAgent& session() { return m_session; }
  [[nodiscard]] ClockAgent& clock() { return m_clock; }

 private:
  RegistryAgent m_registry;
  SessionAgent m_session;
  ClockAgent m_clock;
};

}  // namespace sigil::sketch
