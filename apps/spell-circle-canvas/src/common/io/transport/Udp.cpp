/** @file
 * The UDP transport: the address a feed's URI names, the dual-stack
 * listener or the connected sender opened for it, and the receive loop
 * that hands every datagram to the feed it was opened for.
 */

#include <array>
#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
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
#include <mutex>
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

/** WHAT A udp:// URI NAMES: the peer to speak to, or no host at all,
 *  which is a port to listen on. */
struct Address {
  std::string host;
  std::uint16_t port = 0;
};

/** The address @p uri names, or nothing when it names none: everything
 *  after the scheme is a host and a port, the host bracketed when it is
 *  an IPv6 literal and left out altogether to listen on every interface.
 *  A port is decimal digits and nothing else, so a path or a query
 *  behind it is not an address this transport can open. */
std::optional<Address> parseAddress(std::string_view uri) {
  constexpr std::string_view kScheme = "udp://";
  if (!uri.starts_with(kScheme)) return std::nullopt;
  std::string_view rest = uri.substr(kScheme.size());
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

/** The local end @p socket holds, spelled the way the URI that opened it
 *  is; empty when the socket cannot say. An IPv6 address is bracketed,
 *  so what follows the last colon is always the port. */
std::string localAddress(const udp::socket& socket) {
  error_code error;
  const udp::endpoint local = socket.local_endpoint(error);
  if (error) return {};
  std::ostringstream printed;
  printed.imbue(std::locale::classic());
  printed << "udp://" << local;
  return printed.str();
}

/** THE TRANSPORT'S END OF ONE FEED: the socket, the buffer a datagram
 *  lands in, and the feed the datagram goes to.
 *
 *  Every callback holds this, so a feed let go while a receive is in
 *  flight leaves it standing until that callback returns. The feed
 *  itself is held weakly: when it cannot be locked there is nobody left
 *  to deliver to, and the loop ends there. */
struct Door : std::enable_shared_from_this<Door> {
  Door(std::shared_ptr<detail::IoThread> thread, std::weak_ptr<Feed> feed)
      : io(std::move(thread)),
        strand(boost::asio::make_strand(io->context())),
        socket(strand),
        feed(std::move(feed)) {}

  /** Arms one receive, which arms the next. */
  void receive();
  void close();
  bool send(const Bytes& datagram);

  /** Held, not borrowed: the context has to outlive the socket standing
   *  on it. */
  std::shared_ptr<detail::IoThread> io;
  boost::asio::strand<boost::asio::io_context::executor_type> strand;
  udp::socket socket;
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
        // Re-armed before the delivery: the bytes are already out of the
        // buffer, and the time a consumer takes over them is not time
        // the socket spends unable to receive.
        self->receive();
        feed->deliver(std::move(datagram));
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
                    std::string_view uri, const std::weak_ptr<Feed>& into) {
  const std::optional<Address> address = parseAddress(uri);
  if (!address)
    return refuse(into, std::string(uri) +
                            " is not a udp address: a feed is opened on "
                            "udp://host:port, and on udp://:port to listen");

  const auto door = std::make_shared<Door>(io, into);
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
  opened.address = localAddress(door->socket);
  opened.close = [door] { door->close(); };
  // A listener answers whoever writes to it and has no one peer of its
  // own, so its way is one-way.
  if (sends)
    opened.send = [door](const Bytes& datagram) {
      return door->send(datagram);
    };

  // Armed last: the socket belongs to the strand from the first receive
  // on, so the local end it bound is read while this thread is still the
  // only one that can touch it.
  door->receive();
  return opened;
}

/** THE THREAD ONE REGISTRATION'S SOCKETS SHARE. It is made when the
 *  first feed opens, so a hub given the transport and never asked for a
 *  udp feed starts no thread, and it stands until the hub drops the
 *  transport or the last socket opened through it is gone, whichever is
 *  later. */
class SharedIoThread {
 public:
  std::shared_ptr<detail::IoThread> acquire() {
    const std::lock_guard<std::mutex> lock(m_gate);
    if (!m_thread) m_thread = std::make_shared<detail::IoThread>();
    return m_thread;
  }

 private:
  std::mutex m_gate;
  std::shared_ptr<detail::IoThread> m_thread;
};

}  // namespace

void registerUdp(Hub& hub) {
  hub.setFeedTransport("udp",
                       [shared = std::make_shared<SharedIoThread>()](
                           std::string_view uri, std::weak_ptr<Feed> into) {
                         return openFeed(shared->acquire(), uri, into);
                       });
}

void registerTransports(Hub& hub) { registerUdp(hub); }

}  // namespace sigil::io
