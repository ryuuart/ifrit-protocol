#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spellcircle {

/**
 * Dual-stack UDP datagram receiver on a dedicated I/O thread (standalone
 * ASIO). The one transport implementation shared by every receiver app —
 * the Qt NetworkManager and the native macOS SCKEngine both sit on top of
 * this instead of maintaining their own socket code.
 *
 * The handler is invoked on the internal I/O thread with the raw datagram
 * bytes and the sender formatted as "ip:port" (v4-mapped senders presented
 * as plain IPv4). Marshal to your UI/main thread before touching app state;
 * payload verification is intentionally left to the caller so this stays a
 * pure transport.
 *
 * start()/stop() are not thread-safe against each other — call them from
 * one owning thread (both apps drive them from their main threads).
 */
class UdpReceiver {
public:
  using DatagramHandler = std::function<void(std::vector<std::uint8_t> payload,
                                             std::string source)>;

  UdpReceiver();
  ~UdpReceiver();

  UdpReceiver(const UdpReceiver &) = delete;
  UdpReceiver &operator=(const UdpReceiver &) = delete;

  /**
   * Binds :port (IPv6 any, dual-stack — IPv4 senders arrive v4-mapped) and
   * starts the receive loop. Any previous binding is torn down first, so a
   * port change while listening rebinds in place — the semantics both apps
   * already expose. Returns an empty string on success, otherwise a
   * human-readable bind failure for status displays.
   */
  std::string start(std::uint16_t port, DatagramHandler handler);

  /** Closes the socket and joins the I/O thread. No handler invocations
   *  occur after stop() returns. Safe to call when not listening. */
  void stop();

  bool listening() const;

private:
  struct Session;
  std::unique_ptr<Session> m_session;
};

} // namespace spellcircle
