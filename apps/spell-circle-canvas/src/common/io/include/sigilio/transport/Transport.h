#pragma once

/** @file
 * @ingroup io-transport
 * The transports a hub opens FEEDS through: a URI scheme, and the socket
 * behind it. Registering them teaches a hub those schemes; a feed the
 * hub is then asked for on one binds or connects a socket of its own,
 * and every message that socket receives arrives in that feed. Three of
 * them carry no socket at all — shm://, midi:// and serial://.
 *
 * What a message MEANS is not decided here: a feed answers bytes, and
 * the library that owns the format decodes them.
 */

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::io {

class Hub;

/** Installs on @p hub the transports that answer @p schemes — every
 *  transport this feature carries when @p schemes is empty — so a feed
 *  asked for on one of those schemes binds, calls, maps or opens what
 *  its URI names. A transport answers several schemes and is installed
 *  under all of them when any one is named: "udp", "osc" and "artnet"
 *  are one socket; "ws" and "wss" are the listener with the client in
 *  front of it; "shm", "midi", "serial", "grpc", "quic" and "webrtc" are
 *  one transport each, and "webrtc" brings the websocket ends its
 *  introductions cross. Registering a transport again replaces it.
 *  @trap A scheme no transport here answers is passed over: a feed
 *  asked for on it is one whose error() says no transport is
 *  registered. */
void registerTransports(Hub& hub, const std::vector<std::string>& schemes = {});

namespace detail {
/** Registers every scheme this feature answers that @p hub has no
 *  transport for yet: what `Hub::listen()` runs on its first ask nothing
 *  answers, through the installer this feature hands the hub as the
 *  program starts. */
void installLinkedTransports(Hub& hub);
}  // namespace detail

/** THE OTHER END OF A shm:// REGION: the one process that puts the
 *  messages in it. Construction makes the shared memory object named
 *  @p name, replacing whatever stood under that name, large enough to
 *  hold @p capacity bytes of payload under the layout's fixed-size
 *  header; destruction unmaps it and takes the name back. Either end may
 *  start first, a reader holding the name rather than the memory.
 *  @trap ONE WRITER PER REGION: a region carries one count of its
 *  messages and not one per writer, so two writers on a name would write
 *  over each other. */
class SharedMemoryWriter {
 public:
  /** Makes or takes back the shared memory object called @p name, sized
   *  to hold a message of @p capacity bytes. */
  SharedMemoryWriter(std::string_view name, size_t capacity);
  ~SharedMemoryWriter();

  SharedMemoryWriter(const SharedMemoryWriter&) = delete;
  SharedMemoryWriter& operator=(const SharedMemoryWriter&) = delete;

  /** Whether the region stands. False when the object could not be made
   *  or mapped, and every write() is false from then on. */
  bool open() const { return m_region != nullptr; }

  /** Puts @p bytes in the region as the newest message, whole or not at
   *  all: false when the region does not stand and when @p bytes are more
   *  than the capacity it was made with.
   *  @trap The same bytes written twice are TWO messages: what makes a
   *  message new is the count and not what it says. */
  bool write(std::span<const std::byte> bytes);

 private:
  /** The mapping, how many bytes of it there are, and how many of them
   *  a message may take. A void pointer because what a region holds is
   *  this library's own business: a consumer of this header inherits no
   *  layout, no platform header and nothing to keep in step. */
  void* m_region = nullptr;
  size_t m_length = 0;
  size_t m_capacity = 0;
  /** The name to take back, empty once there is nothing to take. */
  std::string m_name;
};

}  // namespace sigil::io
