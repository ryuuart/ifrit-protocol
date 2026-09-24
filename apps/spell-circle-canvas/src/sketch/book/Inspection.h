#pragma once

/** @file
 * Sketchbook's side of the protocol: where it keeps the protocol's state,
 * and the endpoint a window or a sweep mounts on loopback.
 *
 * The protocol's own types stand behind a pointer, so the window's
 * translation units that read this header compile none of its headers.
 */

#include <sigilsketch/core/Catalog.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sigil::protocol {
class Dispatcher;
}

/** WHERE SKETCHBOOK KEEPS THE PROTOCOL'S STATE — the address file and the
 *  stills a client asks for: the run's `--state` root, or the platform's
 *  own location for this application when none was named. Empty where
 *  neither can be made. */
std::filesystem::path inspectionStateRoot();

/** One session a run holds open, as `host.describe` lists it. */
struct OpenSession {
  /** The registry name or the path it was opened from. */
  std::string sketch;
  /** The runtime it draws through. */
  std::string kind;
  float width = 0;
  float height = 0;
  /** The moment it declared; negative where it declared none. */
  double moment = -1;
};

/** AN ENDPOINT ANSWERING `host` AND `registry` on loopback at @p port —
 *  any free one for 0 — for a run whose frames are its own: the window's
 *  render thread draws them, or a sweep's walk does, so the session and
 *  clock domains are not mounted and answer `notMounted`. The sessions
 *  `host.describe` lists are what @p sessions reads, where it is given.
 *
 *  Nothing moves but by `dispatch()`, which the run calls where it may
 *  answer; with no client attached a dispatch runs no handler, so the run
 *  draws exactly what it draws without one. */
class Inspection {
 public:
  using Sessions = std::function<std::vector<OpenSession>()>;

  Inspection(uint16_t port, sigil::sketch::CatalogSources catalog,
             Sessions sessions = {});
  ~Inspection();

  Inspection(const Inspection&) = delete;
  Inspection& operator=(const Inspection&) = delete;

  /** Answers whatever the clients attached have asked since the last. */
  void dispatch();

  /** Whether the endpoint holds a port. */
  [[nodiscard]] bool listening() const;

  /** The dispatcher the endpoint answers through, which a client in the
   *  same process attaches to as a socket's does. */
  [[nodiscard]] sigil::protocol::Dispatcher& dispatcher();

 private:
  /** The hub, the dispatcher, the registry's agent and the endpoint. */
  struct State;
  std::unique_ptr<State> m_state;
};
