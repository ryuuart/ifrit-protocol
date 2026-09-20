#pragma once

/** @file
 * @ingroup io-transport
 * The transports a hub opens FEEDS through: a URI scheme, and the socket
 * behind it. Registering one teaches a hub that scheme; a feed the hub
 * is then asked for on it binds or connects a socket of its own, and
 * every message that socket receives arrives in that feed. Three of them
 * carry no socket at all — shm://, midi:// and serial://.
 *
 * What a message MEANS is not decided here: a feed answers bytes, and
 * the library that owns the format decodes them.
 */

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace sigil::io {

class Hub;

/** Installs the UDP transport on @p hub for the schemes "udp", "osc" and
 *  "artnet". udp://:PORT listens on every interface, IPv4 and IPv6 alike,
 *  PORT 0 meaning any free port; udp://HOST:PORT sends to that peer and
 *  receives whatever comes back to it. One thread per socket, living as
 *  long as the hub holds the transport.
 *  @trap A listening socket holds no one peer, so send() goes nowhere by
 *  itself; what it can answer is the ONE sender an arrival names,
 *  through Feed::sendTo(). */
void registerUdp(Hub& hub);

/** Installs the WebSocket LISTENER on @p hub for the scheme "ws".
 *  ws://:PORT/PATH holds that path on every interface, PORT 0 meaning any
 *  free port and an omitted PATH meaning "/"; a ?pages=URI query answers
 *  HTTP GET out of that directory on the same port. send() reaches every
 *  peer at once and Feed::sendTo() one, on a thread per listening feed.
 *  @trap There is no client here and no "wss" to hold a port for: a URI
 *  naming a host to CALL opens nothing, and registerWebSocketClient() is
 *  what takes those. */
void registerWebSocket(Hub& hub);

/** Installs the WebSocket CLIENT on @p hub for the schemes "ws" and
 *  "wss", IN FRONT of whatever was registered for them — the shape of
 *  the URI is what says which end a feed is. ws://HOST:PORT/PATH calls
 *  that server and wss://HOST:PORT/PATH calls it over TLS, while a URI
 *  naming no host is a port to hold and goes to the transport this one
 *  was installed over. One session per feed, on a thread of its own.
 *  @trap A feed is answered before its server is reached: error() carries
 *  the reason when it cannot be, and a send before then goes nowhere and
 *  says so. */
void registerWebSocketClient(Hub& hub);

/** Installs the SHARED MEMORY reader on @p hub for the scheme "shm".
 *  shm://NAME maps the region of that name READ-ONLY and delivers what
 *  its one writer puts there, with no socket beneath it and no kernel on
 *  the way. Nothing pushes, so the feed LOOKS: ?rate=HERTZ a whole number
 *  of times a second, 120 where the URI names none, on one thread for
 *  every feed of a registration.
 *  @trap A name nobody has made a region under is a door onto NOTHING
 *  rather than one that failed — no error(), and delivery the moment a
 *  writer makes one.
 *  @silent send() and Feed::sendTo(): a reader has no way back to a
 *  writer through a region. */
void registerSharedMemory(Hub& hub);

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

/** Installs the MIDI transport on @p hub for the scheme "midi".
 *  midi://in/NAME opens the first input port whose own name holds NAME,
 *  case left out of it, midi://out/NAME the first output that does, and
 *  NAME left out takes the first port there is; virtual:NAME MAKES a port
 *  of that name instead of looking for one. No thread is started: the
 *  driver's own callback is the thread a message arrives on.
 *  @trap A PORT IS NAMED AND NEVER NUMBERED, because which port a machine
 *  calls its second depends on what else was plugged in this morning.
 *  @silent send() and Feed::sendTo() on an INPUT: what comes back down a
 *  cable is the other cable. */
void registerMidi(Hub& hub);

/** Installs the SERIAL transport on @p hub for the scheme "serial".
 *  serial://DEVICE?baud=RATE opens that device file, named whole and
 *  absolute, at that rate; the rest of the wire stands in the same query
 *  — bits=5|6|7|8 (8), parity=none|odd|even (none), stop=1|2 (1),
 *  flow=none|software|hardware (none). A MESSAGE IS A LINE both ways. One
 *  thread per registration, made when its first port opens.
 *  @trap THE RATE IS REQUIRED and there is none to fall back on: two ends
 *  that disagree about it read each other as noise, so a URI carrying
 *  none opens nothing.
 *  @silent Feed::sendTo(): a cable holds one peer, so there is no sender
 *  to pick out by name. */
void registerSerial(Hub& hub);

/** Installs the gRPC transport on @p hub for the scheme "grpc". ONE
 *  SCHEME, TWO SHAPES, and the shape of the URI says which end a feed is:
 *  grpc://:PORT/Service/Method holds that port and serves that one
 *  method, grpc://HOST:PORT/Service/Method calls it there. The method is
 *  SCHEMA-AGNOSTIC — bytes both ways, nothing parsed here. No TLS at
 *  either end, and no thread of this library's.
 *  @trap BOTH ENDS NAME THE METHOD: a URI carrying a service and nothing
 *  after it opens nothing and leaves the reason on the feed. */
void registerGrpc(Hub& hub);

/** Installs the QUIC transport on @p hub for the scheme "quic". ONE
 *  SCHEME, TWO SHAPES: quic://:PORT?cert=FILE&key=FILE holds that port —
 *  a connection is encrypted or it is not a connection, so the pair is
 *  required — and quic://HOST:PORT calls that end, ?insecure=1 checking
 *  no certificate. A message is one unidirectional stream, or one
 *  unreliable datagram under ?datagrams=1. No thread of this library's.
 *  @trap Two messages sent one after the other may land in the OTHER
 *  order, each stream waiting only on itself. */
void registerQuic(Hub& hub);

/** Installs the WEBRTC transport on @p hub for the scheme "webrtc".
 *  webrtc://ROOM?signal=URI holds one conversation, named ROOM, and every
 *  message crosses STRAIGHT between the ends. Neither end can dial the
 *  other, so `signal` names the ws:// or wss:// door the introduction
 *  crosses, and the shape of THAT URI says which end this one is: a port
 *  to hold waits, a server to call takes the room up. ?ice=stun:HOST:PORT
 *  may be given more than once.
 *  @trap Hub::dispatch() is what reads the signalling door and answers
 *  it, so a handshake takes a few frames and a host that never dispatches
 *  never finishes one. */
void registerWebRtc(Hub& hub);

/** Installs every transport this feature carries: UDP, the OSC name over
 *  it, WebSocket at both ends — the listener first, and the client in
 *  front of it — the shared memory reader, MIDI, the serial port, gRPC
 *  at both ends, QUIC at both ends, and WebRTC over a signalling door of
 *  the same hub. */
void registerTransports(Hub& hub);

}  // namespace sigil::io
