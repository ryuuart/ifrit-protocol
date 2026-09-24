#pragma once

/** @file
 * @ingroup protocol-runtime
 * THE ENDPOINT: a host's dispatcher behind a socket. One of SigilData's
 * connections on SigilIO's ws:// listener, on loopback, its address
 * written under the program's state root before the first frame, the
 * definition served beside it, and every command answered on the frame
 * thread inside the hub's dispatch, so no agent races the paint. The
 * clients it attaches stand on the dispatcher beside those attached in
 * process.
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
  /** Peers beyond this machine to admit, each a bare IP address. With
   *  none the endpoint holds loopback alone and cannot be reached from
   *  another machine; with any it holds every interface and admits
   *  loopback and those, refusing every other peer before it becomes a
   *  client. One that is no IP address opens nothing, and error() names
   *  it. */
  std::vector<std::string> statedPeers;
};

/** ONE ENDPOINT: every client on a socket attached to one dispatcher,
 *  one session per peer, cleared when the peer leaves.
 *  @trap Nothing moves but by the hub's dispatch: a host that never
 *  dispatches answers no command, and one that dispatches with no
 *  client attached runs no handler at all.
 *  @trap MADE ON A HUB AND A DISPATCHER, IT IS LET GO BEFORE EITHER: it
 *  holds a lease on the hub and answers through the dispatcher. Letting
 *  it go detaches its clients, which runs every onDetach() listener, so
 *  the agents mounted on that dispatcher still stand then too. */
class Endpoint {
 public:
  /** Listens on @p hub as @p policy says, attaching every peer to
   *  @p dispatcher, whose program's state root is required: the address
   *  file and the served definition are written under it before this
   *  returns. Teaches @p hub the websocket listener where it knows no
   *  ws:// transport. Where it cannot listen, error() says why and
   *  nothing is written. */
  Endpoint(io::Hub& hub, Dispatcher& dispatcher, EndpointPolicy policy = {});
  /** Lets every client it attached go, telling each that enabled `host`
   *  that the endpoint is closing, and takes back the address file it
   *  wrote. The telling is sent before the socket closes, and a peer
   *  that reads it hears it; one whose socket is already full may
   *  not. */
  ~Endpoint();

  Endpoint(const Endpoint&) = delete;
  Endpoint& operator=(const Endpoint&) = delete;

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

 private:
  /** The socket's side: the connection, the peers attached and their
   *  sessions, and the lease that notices a peer leaving. */
  struct Socket;
  std::unique_ptr<Socket> m_socket;
};

}  // namespace sigil::protocol
