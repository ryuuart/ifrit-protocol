#pragma once

/** @file
 * @ingroup protocol-runtime
 * THE ENDPOINT: the dispatcher behind a socket. One of SigilData's
 * connections on SigilIO's ws:// listener, on loopback, its address
 * written under the program's state root before the first frame, the
 * definition served beside it, and every command answered on the frame
 * thread inside the hub's dispatch, so no agent races the paint.
 */

#include <sigilprotocol/dispatch/Dispatcher.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::io {
// The endpoint opens its door on the host's hub; a consumer of this
// header names that hub through its own.
class Hub;
}  // namespace sigil::io

namespace sigil::protocol {

/** WHERE AN ENDPOINT LISTENS AND WHOM IT LETS IN. It stands beside the
 *  class rather than inside it so its member initializers are in hand
 *  where the constructor takes it as a default. */
struct EndpointPolicy {
  /** The port to hold; 0 for any free one, which the address file
   *  names. */
  uint16_t port = 0;
  /** Peers beyond this machine to admit, by IP address. With none the
   *  endpoint holds loopback alone and cannot be reached from another
   *  machine; with any it holds every interface and admits loopback and
   *  those, refusing every other peer before it becomes a client. */
  std::vector<std::string> statedPeers;
};

/** ONE ENDPOINT: a dispatcher every client on a socket attaches to, one
 *  session per peer, cleared when the peer leaves.
 *  @trap Nothing moves but by the hub's dispatch: a host that never
 *  dispatches answers no command, and one that dispatches with no
 *  client attached runs no handler at all. */
class Endpoint final : public Dispatcher {
 public:
  /** Listens on @p hub as @p policy says, for @p program, whose state
   *  root is required: the address file and the served definition are
   *  written under it before this returns. Teaches @p hub the websocket
   *  listener where it knows no ws:// transport. Where it cannot listen,
   *  error() says why and nothing is written. */
  Endpoint(io::Hub& hub, Program program, EndpointPolicy policy = {});
  /** Lets every client go, telling each that enabled `host` that the
   *  host is closing, and takes back the address file it wrote. */
  ~Endpoint() override;

  /** Whether a port is held. */
  bool listening() const;

  /** Why no port is held, in words that open with `endpoint:`; empty
   *  where one is. */
  const std::string& error() const;

  /** THE ADDRESS A CLIENT ON THIS MACHINE DIALS,
   *  `ws://127.0.0.1:52341/sigil`; empty where nothing is held. */
  const std::string& address() const;

  /** THE ADDRESS FILE, `<state>/protocol-address`: the address above and
   *  a line ending, written whole before the first frame, which a client
   *  started beside the program reads to learn the port it was given. */
  std::filesystem::path addressFile() const;

 protected:
  void deliver(const std::string& session, std::string_view method,
               std::string_view parameters) override;

 private:
  /** The socket's side: the connection, the peers attached and their
   *  sessions, and the lease that notices a peer leaving. */
  struct Socket;
  std::unique_ptr<Socket> m_socket;
};

}  // namespace sigil::protocol
