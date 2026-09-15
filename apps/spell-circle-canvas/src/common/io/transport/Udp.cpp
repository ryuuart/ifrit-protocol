/** @file
 * The UDP transport: the address a feed's URI names, the dual-stack
 * listener or the connected sender opened for it, the receive loop that
 * hands every datagram to the feed it was opened for, and the one
 * datagram a listener writes back to a sender it named.
 *
 * Three schemes stand on it. udp:// is the socket by its own name, and
 * osc:// and artnet:// are that same socket opened for datagrams that
 * are OSC packets and for datagrams that are a lighting desk's
 * universes: the scheme a feed was opened with is the scheme every
 * address it reports is spelled with, so a reader takes the decoding
 * off the URI and this file carries no opinion about what a datagram
 * holds.
 */

#include <array>
#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "IoThread.h"
#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {
namespace {

using boost::asio::ip::udp;
using boost::system::error_code;

/** No UDP datagram carries more than this, so one that arrives is never
 *  read in halves. */
constexpr size_t kDatagramCeiling = 65536;

/** Errors that describe ONE datagram rather than the socket: a peer that
 *  answered an earlier send with a refusal, a route that was gone for a
 *  moment, a payload larger than the buffer, a wait the system
 *  interrupted. The binding is still good, so the loop re-arms and the
 *  next datagram arrives as if nothing had happened. */
bool describesOneDatagram(const error_code& error) {
  using namespace boost::asio::error;
  return error == connection_refused || error == connection_reset ||
         error == host_unreachable || error == network_unreachable ||
         error == network_reset || error == message_size ||
         error == timed_out || error == interrupted || error == would_block ||
         error == try_again;
}

/** WHAT A DATAGRAM URI NAMES: the peer to speak to, or no host at all,
 *  which is a port to listen on. */
struct Address {
  std::string host;
  std::uint16_t port = 0;
};

/** The address @p uri names under @p scheme, or nothing when it names
 *  none: everything after the scheme is a host and a port, the host
 *  bracketed when it is an IPv6 literal and left out altogether to
 *  listen on every interface. A port is decimal digits and nothing else,
 *  so a path or a query behind it is not an address this transport can
 *  open. */
std::optional<Address> parseAddress(std::string_view uri,
                                    std::string_view scheme) {
  if (!uri.starts_with(scheme)) return std::nullopt;
  std::string_view rest = uri.substr(scheme.size());
  if (!rest.starts_with("://")) return std::nullopt;
  rest = rest.substr(3);
  Address address;
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

/** An endpoint spelled the way a URI of @p scheme is,
 *  "udp://127.0.0.1:52341". An IPv6 address is bracketed, so what
 *  follows the last colon is always the port. */
std::string printedAddress(std::string_view scheme,
                           const udp::endpoint& endpoint) {
  std::ostringstream printed;
  printed.imbue(std::locale::classic());
  printed << scheme << "://";
  const boost::asio::ip::address host = endpoint.address();
  if (host.is_v6()) {
    const boost::asio::ip::address_v6 six = host.to_v6();
    // An IPv4 peer that reached a dual-stack socket arrives as its
    // address mapped into IPv6, and is named by the IPv4 address it can
    // be written back to rather than by the mapping.
    if (six.is_v4_mapped())
      printed << boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped,
                                                  six);
    else
      printed << "[" << six << "]";
  } else {
    printed << host;
  }
  printed << ":" << endpoint.port();
  return printed.str();
}

/** The local end @p socket holds, spelled the way the URI that opened it
 *  is; empty when the socket cannot say. */
std::string localAddress(std::string_view scheme, const udp::socket& socket) {
  error_code error;
  const udp::endpoint local = socket.local_endpoint(error);
  if (error) return {};
  return printedAddress(scheme, local);
}

/** THE TRANSPORT'S END OF ONE FEED: the socket, the buffer a datagram
 *  lands in, and the feed the datagram goes to.
 *
 *  Every callback holds this, so a feed let go while a receive is in
 *  flight leaves it standing until that callback returns. The feed
 *  itself is held weakly: when it cannot be locked there is nobody left
 *  to deliver to, and the loop ends there. */
struct Door : std::enable_shared_from_this<Door> {
  Door(std::shared_ptr<detail::IoThread> thread, std::string scheme,
       std::weak_ptr<Feed> feed)
      : io(std::move(thread)),
        strand(boost::asio::make_strand(io->context())),
        socket(strand),
        scheme(std::move(scheme)),
        feed(std::move(feed)) {}

  /** Arms one receive, which arms the next. */
  void receive();
  void close();
  bool send(const Bytes& datagram);
  bool sendTo(std::string_view to, const Bytes& datagram);

  /** Held, not borrowed: the context has to outlive the socket standing
   *  on it. */
  std::shared_ptr<detail::IoThread> io;
  boost::asio::strand<boost::asio::io_context::executor_type> strand;
  udp::socket socket;
  /** The name this socket was opened under, which every address it
   *  reports is spelled with. */
  std::string scheme;
  std::weak_ptr<Feed> feed;
  udp::endpoint sender;
  std::array<std::byte, kDatagramCeiling> buffer{};
  /** Raised before the close is posted, so a callback the strand has
   *  already entered stops instead of re-arming a socket on its way
   *  out. */
  std::atomic<bool> closed{false};
};

void Door::receive() {
  socket.async_receive_from(
      boost::asio::buffer(buffer), sender,
      [self = shared_from_this()](const error_code& error, size_t count) {
        if (self->closed.load(std::memory_order_acquire)) return;
        const std::shared_ptr<Feed> feed = self->feed.lock();
        if (!feed) return;
        if (error) {
          // One datagram's error leaves the binding good. Anything else
          // ends this socket, and the feed is told in the system's own
          // words why nothing more will arrive.
          if (!describesOneDatagram(error)) {
            feed->fail(error.message());
            return;
          }
          self->receive();
          return;
        }
        Bytes datagram;
        datagram.bytes.assign(self->buffer.begin(),
                              self->buffer.begin() + count);
        // The sender is read here because the next receive writes over
        // it: the endpoint is one member, and it belongs to whichever
        // datagram last landed in the buffer.
        std::string from = printedAddress(self->scheme, self->sender);
        // Re-armed before the delivery: the bytes are already out of the
        // buffer, and the time a consumer takes over them is not time
        // the socket spends unable to receive.
        self->receive();
        feed->deliver(std::move(datagram), std::move(from));
      });
}

void Door::close() {
  closed.store(true, std::memory_order_release);
  boost::asio::post(strand, [self = shared_from_this()] {
    error_code ignored;
    self->socket.close(ignored);
  });
}

bool Door::send(const Bytes& datagram) {
  if (closed.load(std::memory_order_acquire)) return false;
  // The socket belongs to the strand, so the bytes travel there in a
  // copy of their own and the caller's Bytes are its own again as soon
  // as this returns.
  boost::asio::post(
      strand, [self = shared_from_this(), payload = datagram.bytes]() mutable {
        if (self->closed.load(std::memory_order_acquire)) return;
        // A datagram the system refuses is one datagram and not the socket:
        // UDP promises no delivery, and the next send is as good as this
        // one was.
        error_code ignored;
        self->socket.send(boost::asio::buffer(payload), 0, ignored);
      });
  return true;
}

bool Door::sendTo(std::string_view to, const Bytes& datagram) {
  if (closed.load(std::memory_order_acquire)) return false;
  const std::optional<Address> address = parseAddress(to, scheme);
  if (!address) return false;
  error_code reading;
  const boost::asio::ip::address peer =
      boost::asio::ip::make_address(address->host, reading);
  // A sender is named by the literal address its datagram arrived from,
  // so answering one is arithmetic and never a lookup: a host name here
  // is nobody to answer rather than a wait on a resolver.
  if (reading) return false;
  boost::asio::post(
      strand, [self = shared_from_this(), peer, port = address->port,
               payload = datagram.bytes]() mutable {
        if (self->closed.load(std::memory_order_acquire)) return;
        error_code local;
        const udp::endpoint bound = self->socket.local_endpoint(local);
        boost::asio::ip::address host = peer;
        // A listener is one dual-stack v6 socket, and an IPv4 peer is
        // written back to as the mapping such a socket reads one as.
        if (!local && bound.protocol() == udp::v6() && host.is_v4())
          host = boost::asio::ip::make_address_v6(boost::asio::ip::v4_mapped,
                                                  host.to_v4());
        // A datagram the system refuses is one datagram and not the socket,
        // exactly as the peer's own send beside this one is.
        error_code ignored;
        self->socket.send_to(boost::asio::buffer(payload),
                             udp::endpoint(host, port), 0, ignored);
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

/** Opens one feed's socket: a dual-stack listener when the URI names no
 *  host, a socket connected to the peer when it names one.
 *
 *  A named host is resolved here rather than in the background, so a
 *  feed that cannot reach its peer says so by the time it is answered. */
OpenedFeed openFeed(const std::shared_ptr<detail::IoThread>& io,
                    std::string_view scheme, std::string_view uri,
                    const std::weak_ptr<Feed>& into) {
  const std::string name(scheme);
  const std::optional<Address> address = parseAddress(uri, scheme);
  if (!address)
    return refuse(into, std::string(uri) + " is not a " + name +
                            " address: a feed is opened on " + name +
                            "://host:port, and on " + name +
                            "://:port to listen");

  const auto door = std::make_shared<Door>(io, name, into);
  error_code error;
  bool sends = false;
  if (address->host.empty()) {
    // One socket for both families: a v6 socket that is not v6-only
    // takes an IPv4 sender as an address mapped into v6, so a listener
    // holds one port rather than one port per family.
    door->socket.open(udp::v6(), error);
    if (!error) door->socket.set_option(boost::asio::ip::v6_only(false), error);
    if (!error)
      door->socket.bind(udp::endpoint(udp::v6(), address->port), error);
    if (error)
      return refuse(into, "could not listen on " + std::string(uri) + ": " +
                              error.message());
  } else {
    udp::resolver resolver(io->context());
    const udp::resolver::results_type peers =
        resolver.resolve(address->host, std::to_string(address->port), error);
    if (!error && peers.empty()) error = boost::asio::error::host_not_found;
    if (!error) {
      // A datagram socket with a peer: a send needs no address with it,
      // and what the system delivers back is that peer's alone.
      const udp::endpoint peer = peers.begin()->endpoint();
      door->socket.open(peer.protocol(), error);
      if (!error) door->socket.connect(peer, error);
    }
    if (error)
      return refuse(
          into, "could not reach " + std::string(uri) + ": " + error.message());
    sends = true;
  }

  OpenedFeed opened;
  opened.address = localAddress(name, door->socket);
  opened.close = [door] { door->close(); };
  // A listener answers whoever writes to it and has no one peer of its
  // own, so its way is one-way.
  if (sends)
    opened.send = [door](const Bytes& datagram) {
      return door->send(datagram);
    };
  else
    // It can still answer ONE sender: the address a datagram arrived
    // from is an address to write back to, which is the whole of the
    // way out of a door that holds no peer.
    opened.sendTo = [door](std::string_view to, const Bytes& datagram) {
      return door->sendTo(to, datagram);
    };

  // Armed last: the socket belongs to the strand from the first receive
  // on, so the local end it bound is read while this thread is still the
  // only one that can touch it.
  door->receive();
  return opened;
}

/** The one opener, under whichever scheme it was registered: the scheme
 *  travels with it, so the same socket answers udp:// and osc:// and
 *  each feed keeps the name it was opened with. */
FeedTransport datagramTransport(std::shared_ptr<detail::SharedIoThread> shared,
                                std::string scheme) {
  return [shared = std::move(shared), scheme = std::move(scheme)](
             std::string_view uri, std::weak_ptr<Feed> into) {
    return openFeed(shared->acquire(), scheme, uri, into);
  };
}

}  // namespace

void registerUdp(Hub& hub) {
  // All three names share one thread, because they are one socket: a hub
  // asked for none of the schemes still starts nothing.
  auto shared = std::make_shared<detail::SharedIoThread>();
  hub.setFeedTransport("udp", datagramTransport(shared, "udp"));
  hub.setFeedTransport("osc", datagramTransport(shared, "osc"));
  // The lighting desks' datagrams, under the name their wire is called
  // by, on the port that wire holds: artnet://:6454 listens for them.
  hub.setFeedTransport("artnet", datagramTransport(shared, "artnet"));
}

void registerTransports(Hub& hub) {
  registerUdp(hub);
  // The listener first and the caller after it: the caller stands in
  // front of whatever the scheme holds and hands a URI with no host back
  // to it, so the order is what makes both forms open.
  registerWebSocket(hub);
  registerWebSocketClient(hub);
  registerSharedMemory(hub);
  registerMidi(hub);
  registerSerial(hub);
  registerGrpc(hub);
  registerQuic(hub);
  // After both ends of the websocket, because a webrtc feed opens its
  // signalling door through this same hub: whichever end of that door
  // its URI names has to be registered before one can be asked for.
  registerWebRtc(hub);
}

}  // namespace sigil::io
