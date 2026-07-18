#include "UdpReceiver.h"

#include <asio.hpp>

#include <array>
#include <thread>
#include <utility>

namespace spellcircle {

namespace {

/** "ip:port", with v4-mapped IPv6 senders presented as plain IPv4 —
 *  ::ffff:127.0.0.1 reads as 127.0.0.1, like both apps always displayed. */
std::string formatSource(const asio::ip::udp::endpoint &endpoint) {
  asio::ip::address address = endpoint.address();
  if (address.is_v6()) {
    const asio::ip::address_v6 v6 = address.to_v6();
    if (v6.is_v4_mapped())
      address = asio::ip::make_address_v4(asio::ip::v4_mapped, v6);
  }
  return address.to_string() + ":" + std::to_string(endpoint.port());
}

} // namespace

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

  ~Session() {
    // Closing from the I/O thread's own executor avoids racing the
    // in-flight async_receive_from; run() returns once the abort lands.
    if (thread.joinable()) {
      asio::post(context, [this] {
        std::error_code ignored;
        socket.close(ignored);
      });
      thread.join();
    }
  }

  void receiveNext() {
    socket.async_receive_from(
        asio::buffer(buffer), sender,
        [this](std::error_code error, std::size_t received) {
          if (error == asio::error::operation_aborted || !socket.is_open())
            return; // teardown
          if (!error && handler)
            handler(std::vector<std::uint8_t>(buffer.data(),
                                              buffer.data() + received),
                    formatSource(sender));
          // Transient per-datagram errors (e.g. ICMP-reflected
          // ECONNREFUSED) must not kill the loop.
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
  asio::ip::udp::socket &socket = session->socket;
  socket.open(asio::ip::udp::v6(), error);
  if (error)
    return "Socket creation failed — " + error.message();

  // Dual-stack: accept IPv4 senders as v4-mapped addresses, like
  // QUdpSocket bound to QHostAddress::Any. Best effort — some stacks
  // reject the option and are dual-stack by default.
  std::error_code optionError;
  socket.set_option(asio::ip::v6_only(false), optionError);
  socket.set_option(asio::ip::udp::socket::reuse_address(true), optionError);

  socket.bind(asio::ip::udp::endpoint(asio::ip::address_v6::any(), port),
              error);
  if (error)
    return "Bind failed on :" + std::to_string(port) + " — " + error.message();

  session->receiveNext();
  session->thread = std::thread([raw = session.get()] { raw->context.run(); });

  m_session = std::move(session);
  return {};
}

void UdpReceiver::stop() { m_session.reset(); }

bool UdpReceiver::listening() const { return m_session != nullptr; }

} // namespace spellcircle
