#include "UdpReceiver.h"

#include <array>
#include <asio.hpp>
#include <atomic>
#include <thread>
#include <tuple>
#include <utility>

namespace spellcircle {

namespace {

/** "ip:port", with v4-mapped IPv6 senders presented as plain IPv4 —
 *  ::ffff:127.0.0.1 reads as 127.0.0.1. */
std::string formatSource(const asio::ip::udp::endpoint& endpoint) {
  asio::ip::address address = endpoint.address();
  if (address.is_v6()) {
    const asio::ip::address_v6 v6 = address.to_v6();
    if (v6.is_v4_mapped())
      address = asio::ip::make_address_v4(asio::ip::v4_mapped, v6);
  }
  return address.to_string() + ":" + std::to_string(endpoint.port());
}

/** Whether a receive failure describes one datagram rather than the socket.
 *  A UDP socket reports an ICMP rejection provoked by an earlier send as an
 *  error on the next receive, and a datagram that does not fit the buffer
 *  reports as a message-size error; the socket is still bound and the next
 *  datagram still arrives, so the loop re-arms on these. Every other error
 *  is a property of the socket itself and would be handed straight back on
 *  the next receive, so re-arming on it spins the I/O thread. */
bool describesOneDatagram(const std::error_code& error) {
  return error == asio::error::connection_refused ||
         error == asio::error::connection_reset ||
         error == asio::error::host_unreachable ||
         error == asio::error::network_unreachable ||
         error == asio::error::network_reset ||
         error == asio::error::message_size ||
         error == asio::error::timed_out || error == asio::error::interrupted ||
         error == asio::error::would_block || error == asio::error::try_again;
}

}  // namespace

/** One bind's worth of state: context, socket, and the thread running the
 *  receive loop. Recreated wholesale per start() — cheaper to reason about
 *  than restarting a shared io_context, and rebinds are user-interaction
 *  rate. */
struct UdpReceiver::Session {
  asio::io_context context;
  asio::ip::udp::socket socket{context};
  asio::ip::udp::endpoint sender;
  std::array<std::uint8_t, 65536> buffer{};
  DatagramHandler handler;
  std::thread thread;
  // Written by the I/O thread when the loop ends on a socket failure, read
  // by the owner: the message is stored before `stopped` is released, so a
  // reader that sees the flag sees the whole message.
  std::string failure;
  std::atomic<bool> stopped{false};

  ~Session() {
    // Closing from the I/O thread's own executor avoids racing the
    // in-flight async_receive_from; run() returns once the abort lands.
    if (thread.joinable()) {
      asio::post(context, [this] {
        std::error_code ignored;
        std::ignore = socket.close(ignored);
      });
      thread.join();
    }
  }

  void receiveNext() {
    socket.async_receive_from(
        asio::buffer(buffer), sender,
        [this](std::error_code error, std::size_t received) {
          if (error == asio::error::operation_aborted || !socket.is_open())
            return;  // teardown
          if (error && !describesOneDatagram(error)) {
            // The socket, not the datagram, is broken. Re-arming would hand
            // the same failure back immediately and burn the I/O thread on a
            // condition that cannot clear, so the loop ends here and leaves
            // the reason for failure() to report; only a fresh start() binds
            // again.
            failure = error.message();
            stopped.store(true, std::memory_order_release);
            return;
          }
          if (!error && handler)
            handler(std::vector<std::uint8_t>(buffer.data(),
                                              buffer.data() + received),
                    formatSource(sender));
          // A failure that concerned only this datagram leaves the socket
          // bound, so the loop re-arms for the next one.
          receiveNext();
        });
  }
};

UdpReceiver::UdpReceiver() = default;

UdpReceiver::~UdpReceiver() { stop(); }

std::string UdpReceiver::start(std::uint16_t port, DatagramHandler handler) {
  stop();

  auto session = std::make_unique<Session>();
  session->handler = std::move(handler);

  std::error_code error;
  asio::ip::udp::socket& socket = session->socket;
  std::ignore = socket.open(asio::ip::udp::v6(), error);
  if (error) return "Socket creation failed — " + error.message();

  // Dual-stack: accept IPv4 senders as v4-mapped addresses, like
  // QUdpSocket bound to QHostAddress::Any. Best effort — some stacks
  // reject the option and are dual-stack by default.
  std::error_code optionError;
  std::ignore = socket.set_option(asio::ip::v6_only(false), optionError);
  std::ignore = socket.set_option(asio::ip::udp::socket::reuse_address(true),
                                  optionError);

  std::ignore = socket.bind(
      asio::ip::udp::endpoint(asio::ip::address_v6::any(), port), error);
  if (error)
    return "Bind failed on :" + std::to_string(port) + " — " + error.message();

  session->receiveNext();
  session->thread = std::thread([raw = session.get()] { raw->context.run(); });

  m_session = std::move(session);
  return {};
}

void UdpReceiver::stop() { m_session.reset(); }

bool UdpReceiver::listening() const {
  return m_session && !m_session->stopped.load(std::memory_order_acquire);
}

std::string UdpReceiver::failure() const {
  if (!m_session || !m_session->stopped.load(std::memory_order_acquire))
    return {};
  return m_session->failure;
}

}  // namespace spellcircle
