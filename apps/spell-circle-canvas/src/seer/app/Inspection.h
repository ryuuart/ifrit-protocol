#pragma once

/** @file
 * Seer's side of the protocol: the endpoint its window mounts on loopback
 * by default, answering what Seer is.
 *
 * Nothing of the protocol is spelled here: this header is read by Qt's
 * translation units, where `emit` is a macro, and the protocol's own
 * headers name a member so.
 */

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace seer {

/** AN ENDPOINT ANSWERING `host` on loopback at @p port — any free one for
 *  0 — with its address written under @p stateRoot. Seer holds no
 *  sketch session and no clock, so every other domain answers
 *  `notMounted`. Nothing moves but by `dispatch()`, which the window's
 *  event loop calls; with no client attached a dispatch runs no
 *  handler. */
class Inspection {
 public:
  Inspection(uint16_t port, std::filesystem::path stateRoot);
  ~Inspection();

  Inspection(const Inspection&) = delete;
  Inspection& operator=(const Inspection&) = delete;

  /** Answers whatever the clients attached have asked since the last. */
  void dispatch();

  /** Whether the endpoint holds a port. */
  [[nodiscard]] bool listening() const;

 private:
  /** The hub, the dispatcher and the endpoint. */
  struct State;
  std::unique_ptr<State> m_state;
};

}  // namespace seer
