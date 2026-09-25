#pragma once

/** @file
 * Seer's side of the protocol: the endpoint its window mounts on loopback
 * by default, answering what Seer is.
 *
 * The protocol's own types stand behind a pointer, so the window's
 * translation units that read this header compile none of its headers.
 */

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace sigil::protocol {
class Dispatcher;
}

namespace seer {

/** AN ENDPOINT ANSWERING `host` on loopback at @p port — any free one for
 *  0 — with its address written under @p stateRoot. Seer holds no
 *  sketch session and no clock, so every other domain answers
 *  `notMounted`. Nothing moves but by `advance()`, which the window's
 *  event loop calls; with no client attached an advance runs no
 *  handler. */
class Inspection {
 public:
  Inspection(uint16_t port, std::filesystem::path stateRoot);
  ~Inspection();

  Inspection(const Inspection&) = delete;
  Inspection& operator=(const Inspection&) = delete;

  /** Answers whatever the clients attached have asked since the last. */
  void advance();

  /** Whether the endpoint holds a port. */
  [[nodiscard]] bool listening() const;

  /** The dispatcher the endpoint answers through, which a client in the
   *  same process attaches to as a socket's does. */
  [[nodiscard]] sigil::protocol::Dispatcher& dispatcher();

 private:
  /** The hub, the dispatcher and the endpoint. */
  struct State;
  std::unique_ptr<State> m_state;
};

}  // namespace seer
