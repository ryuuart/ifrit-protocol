#include "UdpReceiver.h"

#include <array>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <mutex>
#include <utility>

namespace spellcircle {
namespace {

using boost::asio::ip::udp;
using boost::system::error_code;

std::string formatSource(const udp::endpoint& endpoint) {
  auto address = endpoint.address();
  if (address.is_v6() && address.to_v6().is_v4_mapped())
    address = boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped,
                                               address.to_v6());
  const std::string host =
      address.is_v6() ? "[" + address.to_string() + "]" : address.to_string();
  return host + ":" + std::to_string(endpoint.port());
}

bool describesOneDatagram(const error_code& error) {
  using namespace boost::asio::error;
  return error == connection_refused || error == connection_reset ||
         error == host_unreachable || error == network_unreachable ||
         error == network_reset || error == message_size ||
         error == timed_out || error == interrupted || error == would_block ||
         error == try_again;
}

}  // namespace

struct UdpReceiver::State : std::enable_shared_from_this<State> {
  struct Session {
    explicit Session(const boost::asio::any_io_executor& executor,
                     DatagramHandler datagram, StatusHandler status)
        : socket(executor),
          datagram(std::move(datagram)),
          status(std::move(status)) {}

    udp::socket socket;
    udp::endpoint sender;
    std::array<std::uint8_t, 65536> buffer{};
    DatagramHandler datagram;
    StatusHandler status;
    std::uint16_t port = 0;
  };

  explicit State(boost::asio::any_io_executor executor)
      : strand(boost::asio::make_strand(std::move(executor))) {}

  // Socket state belongs to the strand. The gate also synchronizes controls
  // with callback entry, so stop need not wait for a running event loop.
  // Recursion permits a handler to stop or destroy its own receiver.
  boost::asio::strand<boost::asio::any_io_executor> strand;
  std::recursive_mutex gate;
  std::uint64_t revision = 0;
  bool accepting = false;
  std::shared_ptr<Session> current;

  bool active(std::uint64_t binding) const {
    return accepting && revision == binding;
  }

  void closeCurrent() {
    if (!current) return;
    error_code ignored;
    current->socket.close(ignored);
    current.reset();
  }

  void fail(const std::shared_ptr<Session>& session, const error_code& error) {
    accepting = false;
    closeCurrent();
    if (session->status) session->status({session->port, error});
  }

  void receive(const std::shared_ptr<Session>& session, std::uint64_t binding) {
    session->socket.async_receive_from(
        boost::asio::buffer(session->buffer), session->sender,
        [self = shared_from_this(), session, binding](error_code error,
                                                      std::size_t count) {
          const auto receivedAt = std::chrono::steady_clock::now();
          std::lock_guard lock(self->gate);
          if (!self->active(binding)) return;
          if (error) {
            if (!describesOneDatagram(error)) {
              self->fail(session, error);
              return;
            }
          } else {
            // Rearm before entering user code so a caught callback exception
            // does not silently strand an otherwise live binding.
            Datagram packet{
                {session->buffer.begin(), session->buffer.begin() + count},
                formatSource(session->sender),
                receivedAt};
            self->receive(session, binding);
            if (session->datagram) session->datagram(std::move(packet));
            return;
          }
          self->receive(session, binding);
        });
  }

  void begin(std::uint64_t binding, std::uint16_t port,
             DatagramHandler datagram, StatusHandler status) {
    std::lock_guard lock(gate);
    if (!active(binding)) return;
    closeCurrent();
    const auto session = std::make_shared<Session>(strand, std::move(datagram),
                                                   std::move(status));
    current = session;
    session->port = port;
    error_code error;
    session->socket.open(udp::v6(), error);
    if (!error)
      session->socket.set_option(boost::asio::ip::v6_only(false), error);
    if (!error) session->socket.bind(udp::endpoint(udp::v6(), port), error);
    if (!error) session->port = session->socket.local_endpoint(error).port();
    if (error) {
      fail(session, error);
      return;
    }
    receive(session, binding);
    if (session->status) session->status({session->port, {}});
  }
};

UdpReceiver::UdpReceiver(boost::asio::any_io_executor executor)
    : m_state(std::make_shared<State>(std::move(executor))) {}

UdpReceiver::~UdpReceiver() { stop(); }

void UdpReceiver::start(std::uint16_t port, DatagramHandler datagram,
                        StatusHandler status) {
  const auto state = m_state;
  std::lock_guard lock(state->gate);
  const auto binding = ++state->revision;
  state->accepting = true;
  boost::asio::post(
      state->strand, [state, binding, port, datagram = std::move(datagram),
                      status = std::move(status)]() mutable {
        state->begin(binding, port, std::move(datagram), std::move(status));
      });
}

void UdpReceiver::stop() {
  const auto state = m_state;
  std::lock_guard lock(state->gate);
  if (!state->accepting) return;
  state->accepting = false;
  const auto binding = ++state->revision;
  boost::asio::post(state->strand, [state, binding] {
    std::lock_guard lock(state->gate);
    if (state->revision == binding) state->closeCurrent();
  });
}

}  // namespace spellcircle
