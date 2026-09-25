#pragma once

// The receiver's side of the protocol: an endpoint it mounts only where
// its command line asked for one.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sigil::protocol {
class Dispatcher;
}

namespace spellcircle {

/** WHAT THE COMMAND LINE ASKED OF THE PROTOCOL: `--inspect` with an
 *  optional `=PORT`, and `--state` with the directory the address file is
 *  written under. Nothing where `--inspect` was not given. Arguments this
 *  reading does not know are left for whoever does. */
struct InspectionRequest {
  uint16_t port = 0;
  /** Empty for the platform's own location for the receiver. */
  std::filesystem::path stateRoot;
};
[[nodiscard]] std::optional<InspectionRequest> inspectionRequested(
    const std::vector<std::string>& arguments);

/** AN ENDPOINT ANSWERING `host` on loopback: the receiver holds no sketch
 *  session and no clock, so every other domain answers `notMounted`.
 *  Nothing moves but by `advance()`, which the receiver's main queue
 *  calls; with no client attached an advance runs no handler. */
class ReceiverInspection {
 public:
  /** Listens as @p request says, keeping state under @p stateRoot. */
  ReceiverInspection(const InspectionRequest& request,
                     std::filesystem::path stateRoot);
  ~ReceiverInspection();

  ReceiverInspection(const ReceiverInspection&) = delete;
  ReceiverInspection& operator=(const ReceiverInspection&) = delete;

  /** Answers whatever the clients attached have asked since the last. */
  void advance();

  /** Whether a port is held. */
  [[nodiscard]] bool listening() const;

  /** The address a client dials, or why nothing listens. */
  [[nodiscard]] std::string address() const;

  /** The dispatcher the endpoint answers through, which a client in the
   *  same process attaches to as a socket's does. */
  [[nodiscard]] sigil::protocol::Dispatcher& dispatcher();

 private:
  struct State;
  std::unique_ptr<State> m_state;
};

}  // namespace spellcircle
