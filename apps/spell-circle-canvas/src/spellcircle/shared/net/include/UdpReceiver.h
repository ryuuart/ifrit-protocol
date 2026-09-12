#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spellcircle {

/** One complete UDP payload, its sender, and the time its receive completed.
 *  IPv6 sources use bracketed addresses; mapped IPv4 sources use plain IPv4.
 *  Arrival time uses a monotonic clock so UI queue delays do not affect rates.
 */
struct Datagram {
  std::vector<std::uint8_t> payload;
  std::string source;
  std::chrono::steady_clock::time_point receivedAt;
};

/** Dual-stack UDP transport on an executor supplied by the host.
 *
 *  The host owns and runs the executor's context, which must outlive this
 *  receiver. The receiver serializes its socket operations and callbacks even
 *  when several threads run that context. It never runs, restarts, stops, or
 *  joins the context. Payload verification belongs to the scene consumer.
 *
 *  Controls may be called from any thread, including a callback. Stopping or
 *  rebinding retires the preceding binding immediately: it waits for an
 *  executing callback to return, unless called from that callback, and prevents
 *  subsequent callbacks from entering. Socket cancellation itself is queued;
 *  no control waits for the executor to run. Callbacks must not block waiting
 *  for a thread that is stopping or rebinding the receiver. Callbacks already
 *  forwarded to another event loop must be invalidated by that consumer.
 *
 *  Callback exceptions propagate through the host's context run function.
 *  Destruction has the same cancellation guarantees as stop(). */
class UdpReceiver {
 public:
  /** A successful bind supplies the actual port (including an allocated port
   *  when start was given zero). An error reports a failed bind or a terminal
   *  receive failure; that binding receives no further datagrams. */
  struct Status {
    std::uint16_t port = 0;
    boost::system::error_code error;
  };

  using DatagramHandler = std::function<void(Datagram)>;
  using StatusHandler = std::function<void(Status)>;

  explicit UdpReceiver(boost::asio::any_io_executor executor);
  ~UdpReceiver();
  UdpReceiver(const UdpReceiver&) = delete;
  UdpReceiver& operator=(const UdpReceiver&) = delete;

  /** Queues a new binding, closing the preceding socket before binding.
   *  Both handlers execute on the supplied executor. Status reports the bind
   *  outcome before any datagram. A stop or newer start may retire a queued
   *  binding before either handler executes. */
  void start(std::uint16_t port, DatagramHandler datagram,
             StatusHandler status);

  /** Retires the binding and queues socket closure. The host's other work
   *  remains running. It is safe to stop before the context starts running. */
  void stop();

 private:
  struct State;
  std::shared_ptr<State> m_state;
};

}  // namespace spellcircle
