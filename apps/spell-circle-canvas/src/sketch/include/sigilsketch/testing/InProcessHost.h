#pragma once

/** @file
 * @ingroup sketch-testing
 *
 * A sketch host in the test's own process: one dispatcher with the
 * registry, session and clock agents mounted, one client attached in
 * process, and every command answered by turning the host's loop until
 * its answer arrives — what a test, a script and Python's in-process
 * route each drive a session through.
 */

#include <sigilprotocol/clock/ClockClient.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilprotocol/dispatch/InProcess.h>
#include <sigilprotocol/host/HostClient.h>
#include <sigilprotocol/registry/RegistryClient.h>
#include <sigilprotocol/session/SessionClient.h>
#include <sigilsketch/core/Catalog.h>
#include <sigilsketch/live/agent/HostAgents.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch::testing {

/** WHAT AN IN-PROCESS HOST IS MADE WITH. Only `stateDirectory` has no
 *  default: stills, planes and every cache a run keeps land under it. */
struct InProcessHostOptions {
  /** Where the host keeps its state, which `host.describe` names. */
  std::filesystem::path stateDirectory;
  /** The program `host.describe` names. */
  std::string program = "sketch testing";
  /** Its version. */
  std::string version;
  /** How the session agent builds its hosts. A null font context is the
   *  system's, made for this host alone. */
  SessionAgentOptions session;
  /** Where the registry agent's catalog reads its files. */
  CatalogSources catalog;
  /** How long a command is waited for — an open that compiles a file
   *  takes seconds — before it is answered `failed`. */
  std::chrono::milliseconds patience{120000};
};

/** A SKETCH HOST IN THIS PROCESS, spoken to exactly as one over a socket
 *  is: the same dispatcher, the same agents, the same envelope text.
 *
 *  Every verb sends one command through the generated client and turns
 *  the host's loop — `HostAgents::frame` — until the answer arrives, so
 *  an open that waits on a build is waited for; a command never answered
 *  within the patience is answered `failed` saying so. The raw form,
 *  `send`, takes and answers the socket's envelope text.
 *
 *  One thread: the one that made it. */
class InProcessHost {
 public:
  explicit InProcessHost(InProcessHostOptions options);
  ~InProcessHost();

  InProcessHost(const InProcessHost&) = delete;
  InProcessHost& operator=(const InProcessHost&) = delete;

  /** The first command a client sends: the version, the domains mounted,
   *  the clock's policy, the state root, the sessions open and the
   *  clients attached. */
  protocol::Answer<protocol::host::values::DescribeResult> describe();

  /** Opens @p sketch, a registry name or a path, and waits for it. */
  protocol::Answer<protocol::session::values::Summary> open(
      const std::string& sketch);

  /** Pins the density every raster a session bakes is formed at — zero
   *  for the one a plate of it is photographed at — from the first frame
   *  of the next open. */
  protocol::Answer<protocol::values::Empty> pinDensity(double density = 0.0);

  /** Sets how the clock moves, with a budget of clock seconds or none. */
  protocol::Answer<protocol::values::Empty> clock(
      protocol::clock::Policy policy,
      std::optional<double> budgetSeconds = std::nullopt);

  /** Steps @p seconds in whole frames of one over @p rate. */
  protocol::Answer<protocol::clock::values::StepResult> step(
      double seconds, double rate = 60.0);

  /** The clock as it stands. */
  protocol::Answer<protocol::clock::values::CurrentResult> current();

  /** A still at @p density pixels per canvas unit, written under the
   *  state root at @p path, or at a name the host picks. */
  protocol::Answer<protocol::session::values::StillResult> still(
      double density = 1.0, const std::string& path = {});

  /** ONE ENVELOPE'S TEXT, answered as envelope text: the socket's words
   *  in process. */
  std::string send(std::string_view envelope);

  /** Waits, turning the host's loop, until @p answered holds. */
  template <class Result>
  protocol::Answer<Result> wait(
      std::optional<protocol::Answer<Result>>& answered) {
    turnUntil([&] { return answered.has_value(); });
    if (!answered) return overdue();
    return std::move(*answered);
  }

  /** Turns the host's loop until @p done or the patience runs out;
   *  whether it was done. */
  bool turnUntil(const std::function<bool()>& done);

  /** One turn of the host's loop. */
  void frame() { m_agents->frame(); }

  /** WHAT A FAILING CASE PRINTS FIRST: `host.describe`'s answer, the
   *  clock as it stands and the path of the last still, each as the
   *  protocol spells it. */
  std::string readout();

  /** What a generated client speaks through, for the commands no verb
   *  above names. */
  [[nodiscard]] protocol::Caller caller() const { return m_client->caller(); }

  [[nodiscard]] const std::filesystem::path& stateDirectory() const {
    return m_options.stateDirectory;
  }
  [[nodiscard]] protocol::Dispatcher& dispatcher() { return *m_dispatcher; }
  [[nodiscard]] HostAgents& agents() { return *m_agents; }

 private:
  static protocol::values::Error overdue();

  InProcessHostOptions m_options;
  std::unique_ptr<weave::FontContext> m_fonts;
  // Declared in the order they are made; the destructor lets the agents
  // go first, then the client, then the dispatcher they stand on.
  std::unique_ptr<protocol::Dispatcher> m_dispatcher;
  std::unique_ptr<HostAgents> m_agents;
  std::unique_ptr<protocol::InProcess> m_client;
};

}  // namespace sigil::sketch::testing
