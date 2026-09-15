/** @file
 * The WebSocket transport: the port and the path a feed's URI names, the
 * listener holding them, the loop that listener runs on, and the peers
 * whose messages arrive in the feed and whom one send reaches all at
 * once — or, named one by one, alone.
 *
 * Listening only. The library underneath carries a websocket client as a
 * name and an empty body, so a URI naming a host to reach opens nothing
 * and says so; a peer connects inward, and the feed is the far end of
 * however many of them are attached.
 */

#include <libusockets.h>
#include <uwebsockets/App.h>

#include <atomic>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <future>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {
namespace {

/** The largest message a peer may send. A feed's message is one whole
 *  thing rather than a stream, so a peer that announces more than this
 *  is closed instead of the listener holding a growing fragment for it.
 */
constexpr unsigned int kMessageCeiling = 16u * 1024u * 1024u;

/** WHAT A ws:// URI NAMES: the port to listen on, the path peers reach
 *  it at, and the host that makes it a peer to call rather than a door
 *  to hold. */
struct Address {
  std::string host;
  std::uint16_t port = 0;
  std::string path = "/";
};

/** The address @p uri names, or nothing when it names none: a host and a
 *  port, then a path. The host is bracketed when it is an IPv6 literal
 *  and left out altogether to listen on every interface; the port is
 *  decimal digits and nothing else; an omitted path is the root. */
std::optional<Address> parseAddress(std::string_view uri) {
  constexpr std::string_view kScheme = "ws://";
  if (!uri.starts_with(kScheme)) return std::nullopt;
  std::string_view rest = uri.substr(kScheme.size());
  Address address;
  // The path is taken off the back first, so what remains is an
  // authority the same shape as any other and the slashes inside a path
  // are never mistaken for one.
  if (const size_t slash = rest.find('/'); slash != std::string_view::npos) {
    address.path = std::string(rest.substr(slash));
    rest = rest.substr(0, slash);
  }
  if (rest.starts_with('[')) {
    const size_t bracket = rest.find(']');
    if (bracket == std::string_view::npos || bracket == 1) return std::nullopt;
    address.host = std::string(rest.substr(1, bracket - 1));
    rest = rest.substr(bracket + 1);
  } else {
    // An unbracketed host ends at the first colon, which leaves an IPv6
    // literal written without its brackets to fail on its own digits.
    const size_t colon = rest.find(':');
    if (colon == std::string_view::npos) return std::nullopt;
    address.host = std::string(rest.substr(0, colon));
    rest = rest.substr(colon);
  }
  if (!rest.starts_with(':')) return std::nullopt;
  const std::string_view digits = rest.substr(1);
  unsigned int port = 0;
  const char* const end = digits.data() + digits.size();
  const std::from_chars_result read = std::from_chars(digits.data(), end, port);
  if (read.ec != std::errc() || read.ptr != end || port > 65535)
    return std::nullopt;
  address.port = static_cast<std::uint16_t>(port);
  return address;
}

/** One peer's address, spelled the way a URI of @p scheme is,
 *  "ws://127.0.0.1:52341"; empty when @p binary is neither four bytes
 *  nor sixteen. An IPv6 address is bracketed, so what follows the last
 *  colon is always the port. It is the same spelling the datagram
 *  transport beside this one gives an endpoint, so one reader reads the
 *  sender of an arrival off either.
 *
 *  workaround: the library underneath prints every address it holds as
 *  eight hexadecimal groups, which is neither the shortened form an IPv6
 *  address is written in nor anything that could be written back to.
 *  Its raw bytes are taken instead and printed here. */
std::string peerAddress(std::string_view scheme, std::string_view binary,
                        unsigned int port) {
  std::ostringstream printed;
  printed.imbue(std::locale::classic());
  printed << scheme << "://";
  if (binary.size() == 4) {
    boost::asio::ip::address_v4::bytes_type four{};
    std::memcpy(four.data(), binary.data(), four.size());
    printed << boost::asio::ip::address_v4(four);
  } else if (binary.size() == 16) {
    boost::asio::ip::address_v6::bytes_type sixteen{};
    std::memcpy(sixteen.data(), binary.data(), sixteen.size());
    const boost::asio::ip::address_v6 six(sixteen);
    // An IPv4 peer that reached a dual-stack listener arrives as its
    // address mapped into IPv6, and is named by the IPv4 address it can
    // be written back to rather than by the mapping.
    if (six.is_v4_mapped())
      printed << boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped,
                                                  six);
    else
      printed << "[" << six << "]";
  } else {
    return {};
  }
  printed << ":" << port;
  return printed.str();
}

/** WHAT ONE PEER CARRIES: the address it is named by. It is written
 *  once, when the peer arrives, so the sender every message of its
 *  names, the key it is found under to be answered alone, and the key
 *  its leaving clears are one string and cannot drift apart. The topic
 *  it subscribes to is the path it reached, and what it sends goes to
 *  the feed. */
struct Peer {
  std::string address;
};

/** WHAT THE LISTENER'S THREAD AND ITS CALLBACKS SHARE: the feed every
 *  peer delivers into, the topic a send goes out on, and the app and the
 *  loop the peers live on.
 *
 *  Held by the thread and by the door alike, so a callback still running
 *  when the feed is let go keeps everything it reads. The feed itself is
 *  held weakly: when it cannot be locked there is nobody left to deliver
 *  to, and the message is dropped there.
 *
 *  Only the loop's own thread reads or writes `app`, `listening` and
 *  `peers`; a door on another thread reaches them by deferring onto
 *  `loop`, which is published once the listener is bound and never
 *  changes after. */
struct Session {
  std::weak_ptr<Feed> feed;
  std::string scheme;
  std::string topic;
  uWS::App* app = nullptr;
  us_listen_socket_t* listening = nullptr;
  std::atomic<uWS::Loop*> loop{nullptr};
  /** EVERY PEER ATTACHED, under the address its messages are named by,
   *  which is what an answer to one of them is found through. A peer
   *  puts itself in as it arrives and takes itself out as it leaves, so
   *  nothing here outlives the socket it points at. */
  std::unordered_map<std::string, uWS::WebSocket<false, true, Peer>*> peers;
};

/** What a listener's thread answers with once it has tried to bind: the
 *  port it took, or the sentence saying why it took none. */
struct Bound {
  std::uint16_t port = 0;
  std::string failure;
};

/** Holds one listener until its loop is asked to stop, answering @p
 *  place's path for whoever connects.
 *
 *  The app is made, run and destroyed here because uWebSockets keeps one
 *  loop per thread and reaches it from wherever an app is touched: an
 *  app made on another thread would run against a loop that is not its
 *  own. */
void hold(const std::shared_ptr<Session>& session, const Address& place,
          std::promise<Bound> answer) {
  uWS::App app;
  session->app = &app;
  Bound bound;
  if (app.constructorFailed()) {
    bound.failure = "a websocket listener could not be made";
  } else {
    uWS::App::WebSocketBehavior<Peer> behavior;
    behavior.maxPayloadLength = kMessageCeiling;
    behavior.open = [session](auto* peer) {
      // Every peer on the path subscribes to that path, which is what
      // makes a broadcast one publish rather than a walk over a list
      // this file would have to keep. Answering ONE peer is the other
      // half, and that one is found by name, so the address is read
      // here — where the socket is certainly still open — and kept.
      peer->subscribe(session->topic);
      std::string named = peerAddress(session->scheme, peer->getRemoteAddress(),
                                      peer->getRemotePort());
      if (named.empty()) return;
      peer->getUserData()->address = named;
      session->peers[std::move(named)] = peer;
    };
    behavior.message = [session](auto* peer, std::string_view message,
                                 uWS::OpCode) {
      const std::shared_ptr<Feed> feed = session->feed.lock();
      if (!feed) return;
      const auto* const first =
          reinterpret_cast<const std::byte*>(message.data());
      Bytes payload;
      payload.bytes.assign(first, first + message.size());
      feed->deliver(std::move(payload), peer->getUserData()->address);
    };
    behavior.close = [session](auto* peer, int, std::string_view) {
      // A peer that has left is nobody to answer, and the entry that
      // would answer it points at a socket about to be freed.
      session->peers.erase(peer->getUserData()->address);
    };
    app.ws<Peer>(place.path, std::move(behavior));
    app.listen(place.port, [&session](us_listen_socket_t* token) {
      session->listening = token;
    });
    if (!session->listening) {
      // A refused bind comes back as no listen socket and nothing else,
      // so what the feed is told is what is known: the port was not
      // taken, whether because somebody holds it or because this process
      // may not have it.
      bound.failure = "the port could not be taken";
    } else {
      bound.port = static_cast<std::uint16_t>(us_socket_local_port(
          0, reinterpret_cast<us_socket_t*>(session->listening)));
      session->loop.store(uWS::Loop::get(), std::memory_order_release);
    }
  }

  const bool listening = bound.failure.empty();
  answer.set_value(std::move(bound));
  // Answered before the run, so a feed is opened as soon as its port is
  // taken rather than when the first peer arrives.
  if (listening) app.run();
  session->app = nullptr;
}

/** THE TRANSPORT'S END OF ONE FEED: the thread its listener runs on and
 *  everything that thread shares.
 *
 *  One app per feed, and one thread per app. Closing a feed then closes
 *  an app of its own — its listen socket gives the port back, its peers
 *  end — and reaches no other feed. One app shared by several paths
 *  would have to outlive whichever of its feeds closed first, and could
 *  give no port back at all while another feed still stood on it. */
struct Door {
  ~Door() { close(); }

  void close();
  bool send(const Bytes& message);
  bool sendTo(std::string_view to, const Bytes& message);

  std::shared_ptr<Session> session = std::make_shared<Session>();
  std::thread thread;
  /** Raised before the close is deferred, so a send racing it stops
   *  rather than handing bytes to a loop that is ending. */
  std::atomic<bool> closed{false};
};

void Door::close() {
  if (closed.exchange(true)) return;
  if (uWS::Loop* const loop = session->loop.load(std::memory_order_acquire))
    loop->defer([session = session] {
      // Everything the loop still polls has to go before its run
      // returns: the listen socket is what holds the port, and the app
      // is what holds the peers still attached to it.
      if (session->listening) {
        us_listen_socket_close(0, session->listening);
        session->listening = nullptr;
      }
      if (session->app) session->app->close();
    });
  if (!thread.joinable()) return;
  // A message in flight holds the feed while it delivers, so the last
  // holder of a feed can be the listener's own thread. It cannot wait
  // for itself: the close is already queued, and the loop drains it and
  // ends the thread on the way out of the callback this is inside.
  if (thread.get_id() == std::this_thread::get_id()) {
    thread.detach();
    return;
  }
  thread.join();
}

bool Door::send(const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  uWS::Loop* const loop = session->loop.load(std::memory_order_acquire);
  if (!loop) return false;
  // The app belongs to its loop, so the bytes travel there in a copy of
  // their own and the caller's Bytes are its own again as soon as this
  // returns. A feed carries bytes rather than text, so what goes out is
  // a binary frame.
  loop->defer([session = session, payload = message.bytes] {
    if (!session->app) return;
    session->app->publish(
        session->topic,
        std::string_view(reinterpret_cast<const char*>(payload.data()),
                         payload.size()),
        uWS::OpCode::BINARY);
  });
  return true;
}

bool Door::sendTo(std::string_view to, const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  uWS::Loop* const loop = session->loop.load(std::memory_order_acquire);
  if (!loop) return false;
  // The peer is looked up on the loop that owns it, so the answer is
  // true when it was posted rather than when it was written: a peer
  // that left in between is gone by the time the loop reaches this, and
  // its message stops here.
  loop->defer(
      [session = session, named = std::string(to), payload = message.bytes] {
        const auto found = session->peers.find(named);
        if (found == session->peers.end()) return;
        found->second->send(
            std::string_view(reinterpret_cast<const char*>(payload.data()),
                             payload.size()),
            uWS::OpCode::BINARY);
      });
  return true;
}

/** A feed whose transport could not open: the reason stands on the feed,
 *  and there is no door to close or to send through. */
OpenedFeed refuse(const std::weak_ptr<Feed>& into, std::string why) {
  if (const std::shared_ptr<Feed> feed = into.lock())
    feed->fail(std::move(why));
  return {};
}

/** Opens one feed's listener: the port and path the URI names, held on a
 *  thread of its own.
 *
 *  The bind is waited for here rather than left to the background, so a
 *  feed that could not take its port says so by the time it is
 *  answered. */
OpenedFeed openFeed(std::string_view uri, const std::weak_ptr<Feed>& into) {
  const std::optional<Address> address = parseAddress(uri);
  if (!address)
    return refuse(into, std::string(uri) +
                            " is not a ws address: a feed listens on "
                            "ws://:port/path");
  if (!address->host.empty())
    return refuse(into, std::string(uri) +
                            " names a host to reach, and this transport only "
                            "listens: a feed opens on ws://:port/path and "
                            "peers connect to it");

  const auto door = std::make_shared<Door>();
  door->session->feed = into;
  door->session->scheme = "ws";
  door->session->topic = address->path;

  std::promise<Bound> answer;
  std::future<Bound> waiting = answer.get_future();
  door->thread = std::thread([session = door->session, place = *address,
                              answer = std::move(answer)]() mutable {
    hold(session, place, std::move(answer));
  });
  const Bound bound = waiting.get();
  // Nothing was bound and nothing is running, so letting the door go
  // here is what joins the thread that said so.
  if (!bound.failure.empty())
    return refuse(
        into, "could not listen on " + std::string(uri) + ": " + bound.failure);

  OpenedFeed opened;
  // Every interface of both families is one dual-stack socket, which is
  // a v6 address with nothing in it, and the path stands behind the port
  // exactly as the URI that opened it spells it.
  opened.address = "ws://[::]:" + std::to_string(bound.port) + address->path;
  opened.close = [door] { door->close(); };
  opened.send = [door](const Bytes& message) { return door->send(message); };
  opened.sendTo = [door](std::string_view to, const Bytes& message) {
    return door->sendTo(to, message);
  };
  return opened;
}

}  // namespace

void registerWebSocket(Hub& hub) {
  hub.setFeedTransport("ws",
                       [](std::string_view uri, std::weak_ptr<Feed> into) {
                         return openFeed(uri, into);
                       });
}

}  // namespace sigil::io
