/** @file
 * The QUIC transport: the port a feed's URI holds or the host it calls,
 * the connections that cross it, the stream each message is, and the
 * datagrams a door sends instead when its URI asks for them.
 *
 * ONE CONNECTION CARRIES EVERY MESSAGE, AND A MESSAGE IS ONE STREAM. A
 * send opens a unidirectional stream, writes the bytes and ends it; the
 * arrival is delivered when the end of that stream arrives. So a message
 * keeps its boundary, which a byte stream has none of, and no message
 * waits behind another: a stream that lost a packet holds up itself and
 * nothing else. The price is that two messages sent one after the other
 * may land in the other order, each stream being carried on its own.
 *
 * A DATAGRAM IS THE OTHER WAY ACROSS THE SAME CONNECTION. `datagrams=1`
 * in a URI's query sends every message as a QUIC datagram instead:
 * unreliable, unordered, and no larger than what one packet on the path
 * carries, so a send of more than that answers false. Datagrams are
 * always TAKEN, whatever a door sends, so a peer that writes one reaches
 * a door whose own URI asked for nothing.
 *
 * TLS IS NOT OPTIONAL. A QUIC connection is encrypted or it is not a
 * connection, so a feed that holds a port names the certificate and the
 * private key it answers with, and a feed that calls one either trusts
 * what it is shown or says plainly that it will not check. Both ends
 * name the same protocol, and an end speaking another is refused at the
 * handshake rather than left to send bytes nobody reads.
 *
 * NO THREAD IS STARTED FOR A FEED. The library underneath runs workers
 * of its own and calls back onto them, so the packets, the encryption,
 * the streams and the timers are carried without this library holding a
 * loop, an executor or a socket. The one thread this file ever makes is
 * the one a door let go from inside such a callback hands its shutdown
 * to, below.
 */

#include <msquic.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "QuicAddress.h"
#include "QuicLibrary.h"
#include "QuicPeer.h"
#include "QuicSession.h"
#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {
namespace quic {
namespace {

/** THE TRANSPORT'S END OF ONE FEED: the listener holding a port, or the
 *  one connection a call opened, and the session both of them deliver
 *  through.
 *
 *  One listener per feed. Closing a feed then gives back a port of its
 *  own and ends connections of its own, and reaches no other feed. */
struct Door {
  ~Door() { close(); }

  void close();
  bool send(const Bytes& message);
  bool sendTo(std::string_view to, const Bytes& message);

  /** Every connection standing now, for a send that reaches all of
   *  them. */
  std::vector<std::shared_ptr<Peer>> standing() const;

  std::shared_ptr<Session> session = std::make_shared<Session>();
  HQUIC listener = nullptr;
  /** The one connection a call holds; null on a door that holds a
   *  port. */
  std::shared_ptr<Peer> dialled;
  /** Raised before the shutdown begins, so a send racing it stops rather
   *  than handing bytes to a door that is ending. */
  std::atomic<bool> closed{false};
};

std::vector<std::shared_ptr<Peer>> Door::standing() const {
  std::vector<std::shared_ptr<Peer>> reached;
  const std::lock_guard<std::mutex> lock(session->gate);
  reached.reserve(session->peers.size());
  for (const auto& [named, peer] : session->peers) reached.push_back(peer);
  return reached;
}

void Door::close() {
  if (closed.exchange(true)) return;
  auto shut = [session = session, held = listener, called = dialled] {
    const Library& lib = library();
    if (held) {
      // In that order: the listener takes no more connections, and then
      // the port comes back, which waits for it to stop indicating.
      lib.api->ListenerStop(held);
      lib.api->ListenerClose(held);
    }
    if (called) {
      called->givenUp();
      called->shutdown();
    }
    std::vector<std::shared_ptr<Peer>> reached;
    {
      const std::lock_guard<std::mutex> lock(session->gate);
      reached.reserve(session->peers.size());
      for (const auto& [named, peer] : session->peers) reached.push_back(peer);
    }
    // Outside the table: ending one connection must not hold the table
    // every other connection is found through.
    for (const std::shared_ptr<Peer>& peer : reached) peer->shutdown();
    // Last, and here rather than wherever the session's last holder
    // happens to let go, which may be a callback of the library's.
    session->giveBack();
  };
  listener = nullptr;
  dialled.reset();
  if (insideCallback) {
    // Giving a listener back waits for its callbacks to end, so it
    // cannot be waited for from inside one. The port comes back a moment
    // after the last holder let go rather than within it.
    std::thread(std::move(shut)).detach();
    return;
  }
  shut();
}

bool Door::send(const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  if (dialled) return dialled->write(message);
  // The connections are taken out from under the lock and written to
  // outside it: a write on one peer must not hold the table every other
  // peer is found through.
  bool went = false;
  for (const std::shared_ptr<Peer>& peer : standing())
    if (peer->write(message)) went = true;
  // A door that holds its own connections can say a broadcast reached
  // none of them, which is what "nobody is listening" looks like from
  // here.
  return went;
}

bool Door::sendTo(std::string_view to, const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  std::shared_ptr<Peer> peer;
  {
    const std::lock_guard<std::mutex> lock(session->gate);
    const auto found = session->peers.find(std::string(to));
    if (found == session->peers.end()) return false;
    peer = found->second;
  }
  return peer->write(message);
}

/** A feed whose transport could not open: the reason stands on the feed,
 *  and there is no door to close or to send through. */
OpenedFeed refuse(const std::weak_ptr<Feed>& into, std::string why) {
  if (const std::shared_ptr<Feed> feed = into.lock())
    feed->fail(std::move(why));
  return {};
}

/** Opens one feed's listener: the port the URI names, answered for with
 *  the certificate and key it names beside it.
 *
 *  The bind is done here rather than left to the background, so a feed
 *  that could not take its port says so by the time it is answered. */
OpenedFeed hold(const Hub& hub, std::string_view uri, const Address& place,
                const std::weak_ptr<Feed>& into) {
  // A QUIC PORT ANSWERS FOR ITSELF OR IT ANSWERS NOBODY: there is no
  // unencrypted form of this door to fall back to.
  if (place.certificate.empty() || place.key.empty())
    return refuse(into, std::string(uri) +
                            " names no certificate: a feed holds a port at "
                            "quic://:port?cert=<file>&key=<file>, a quic "
                            "connection being encrypted or nothing at all");
  // WHERE THE PAIR STANDS IS RESOLVED NOW, through the mount table, and
  // what the listener keeps from here on is what the library read out of
  // the two files: a feed may outlive the hub that opened it.
  const std::filesystem::path certificate = fileAt(hub, place.certificate);
  if (certificate.empty())
    return refuse(into, place.certificate +
                            " is no file: a listener's certificate stands at a "
                            "path, or at a URI the hub resolves to one");
  const std::filesystem::path key = fileAt(hub, place.key);
  if (key.empty())
    return refuse(into, place.key +
                            " is no file: a listener's key stands at a path, "
                            "or at a URI the hub resolves to one");

  const std::string certificateName = certificate.string();
  const std::string keyName = key.string();
  QUIC_CERTIFICATE_FILE pair{};
  pair.CertificateFile = certificateName.c_str();
  pair.PrivateKeyFile = keyName.c_str();
  QUIC_CREDENTIAL_CONFIG credential{};
  credential.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
  credential.Flags = QUIC_CREDENTIAL_FLAG_NONE;
  credential.CertificateFile = &pair;

  const auto door = std::make_shared<Door>();
  door->session->feed = into;
  door->session->datagrams = place.datagrams;
  QUIC_STATUS why = QUIC_STATUS_SUCCESS;
  door->session->configuration = configurationFor(credential, why);
  if (!door->session->configuration)
    return refuse(
        into, "could not listen on " + std::string(uri) + ": " + reasonOf(why));

  const Library& lib = library();
  why = lib.api->ListenerOpen(lib.registration, &onListener,
                              door->session.get(), &door->listener);
  if (QUIC_FAILED(why))
    return refuse(
        into, "could not listen on " + std::string(uri) + ": " + reasonOf(why));

  // Every interface of both families is one dual-stack socket, which is
  // an address with no family named in it.
  QUIC_ADDR asked{};
  QuicAddrSetFamily(&asked, QUIC_ADDRESS_FAMILY_UNSPEC);
  QuicAddrSetPort(&asked, portOf(place));
  const QUIC_BUFFER named = protocol();
  why = lib.api->ListenerStart(door->listener, &named, 1, &asked);
  if (QUIC_FAILED(why))
    return refuse(
        into, "could not listen on " + std::string(uri) + ": " + reasonOf(why));

  // THE PORT THE SYSTEM CHOSE is read back, which is what a URI naming
  // port 0 was asking for.
  QUIC_ADDR took{};
  uint32_t size = sizeof(took);
  if (QUIC_FAILED(lib.api->GetParam(
          door->listener, QUIC_PARAM_LISTENER_LOCAL_ADDRESS, &size, &took)))
    return refuse(into, "could not listen on " + std::string(uri) +
                            ": the port it took could not be read back");

  OpenedFeed opened;
  opened.address = "quic://[::]:" + std::to_string(QuicAddrGetPort(&took));
  opened.close = [door] { door->close(); };
  opened.send = [door](const Bytes& message) { return door->send(message); };
  opened.sendTo = [door](std::string_view to, const Bytes& message) {
    return door->sendTo(to, message);
  };
  return opened;
}

/** Opens one feed's call: one connection to the host and port the URI
 *  names.
 *
 *  The handshake is left to the library rather than waited for here,
 *  because an end that answers slowly, or is not there at all, would
 *  otherwise hold whoever asked for the feed for as long as reaching it
 *  takes. What it decided reaches the feed either way: as arrivals, or
 *  as the sentence error() answers. */
OpenedFeed reach(const Address& place, const std::weak_ptr<Feed>& into) {
  const Library& lib = library();
  QUIC_CREDENTIAL_CONFIG credential{};
  credential.Type = QUIC_CREDENTIAL_TYPE_NONE;
  credential.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
  // A SELF-SIGNED CERTIFICATE ON A STAGE IS THE ORDINARY CASE, and
  // nobody signs for it, so a URI may say plainly that this end will not
  // check what it is shown. What that gives up is the certainty that the
  // machine answering is the one the URI named; what stays is that
  // everything crossing the connection is encrypted all the same.
  if (place.insecure)
    credential.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

  const auto door = std::make_shared<Door>();
  door->session->feed = into;
  door->session->datagrams = place.datagrams;
  // The address a CALL reports and every arrival on it names: the
  // authority the URI wrote, the query being this end's own arrangement.
  door->session->called = "quic://" + authorityOf(place);
  const std::string called = door->session->called;
  QUIC_STATUS why = QUIC_STATUS_SUCCESS;
  door->session->configuration = configurationFor(credential, why);
  if (!door->session->configuration)
    return refuse(into, "could not reach " + called + ": " + reasonOf(why));

  const auto peer =
      std::make_shared<Peer>(door->session, called, /*dialled=*/true);
  HQUIC connection = nullptr;
  why = lib.api->ConnectionOpen(lib.registration, &onConnection, peer.get(),
                                &connection);
  if (QUIC_FAILED(why))
    return refuse(into, "could not reach " + called + ": " + reasonOf(why));
  peer->adopt(connection, peer);
  why = lib.api->ConnectionStart(connection, door->session->configuration,
                                 QUIC_ADDRESS_FAMILY_UNSPEC, place.host.c_str(),
                                 portOf(place));
  if (QUIC_FAILED(why)) {
    peer->discard();
    return refuse(into, "could not reach " + called + ": " + reasonOf(why));
  }
  door->dialled = peer;

  OpenedFeed opened;
  // The address a call has is the end it reached: the port its own socket
  // took is the system's to choose and nothing anybody could reach it at.
  opened.address = called;
  opened.close = [door] { door->close(); };
  opened.send = [door](const Bytes& message) { return door->send(message); };
  return opened;
}

/** ONE SCHEME, TWO SHAPES, split by the shape of the URI: a URI naming a
 *  host is an end to call, and a URI naming none is a port to hold. */
OpenedFeed openFeed(const Hub& hub, std::string_view uri,
                    const std::weak_ptr<Feed>& into) {
  const std::optional<Address> address = parseAddress(uri);
  if (!address)
    return refuse(into,
                  std::string(uri) +
                      " is not a quic address: a feed holds a port at "
                      "quic://:port?cert=<file>&key=<file> and calls one at "
                      "quic://host:port");
  if (!library().api)
    return refuse(into, std::string(uri) +
                            " opens nothing: this machine has no quic library");
  if (address->host.empty()) return hold(hub, uri, *address, into);
  return reach(*address, into);
}

}  // namespace
}  // namespace quic

void registerQuic(Hub& hub) {
  // The certificate and key a URI's query names are resolved through
  // this hub as the feed opens. The reference is read there and nowhere
  // else: a transport is registered ON a hub and is held by it, so the
  // hub stands for every call made through this, while the feed that
  // call answers may outlive it — which is why what the listener keeps
  // is what the library read out of the two files, and not a way back
  // here.
  hub.setFeedTransport("quic",
                       [&hub](std::string_view uri, std::weak_ptr<Feed> into) {
                         return quic::openFeed(hub, uri, into);
                       });
}

}  // namespace sigil::io
