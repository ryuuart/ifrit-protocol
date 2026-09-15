#pragma once

/** @file
 * The transports a hub opens FEEDS through: a URI scheme, and the socket
 * behind it. Registering one teaches a hub that scheme; a feed the hub
 * is then asked for on it binds or connects a socket of its own, and
 * every message that socket receives arrives in that feed.
 *
 * Three of them carry no socket at all: a shm:// feed reads a region of
 * memory another process on this machine has mapped, a midi:// feed is
 * a port on the controller standing beside the screen, and a serial://
 * feed is the device file a board on a cable is plugged into — the same
 * door with the network taken out from under it.
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

/** Installs the UDP transport on @p hub for the schemes "udp", "osc"
 *  and "artnet". A URI of the form udp://:PORT listens on every
 *  interface, IPv4 and IPv6 alike, PORT 0 meaning any free port;
 *  udp://HOST:PORT (HOST a name, an IPv4 address, or a bracketed IPv6
 *  address) opens a socket that sends to that peer and receives whatever
 *  comes back to it. Every socket the transport opens runs on one thread
 *  of its own that lives as long as the hub holds the transport.
 *
 *  A listening socket holds no one peer, so nothing goes out of it by
 *  itself; what it can do is answer ONE sender, by the address that
 *  sender's datagram arrived from.
 *
 *  osc:// is that same socket under another name: an osc://:9000 feed is
 *  a UDP socket whose messages are OSC packets. artnet:// is that same
 *  socket again, for the datagrams a lighting desk sends: an
 *  artnet://:6454 feed listens for them and an artnet://HOST:6454 feed
 *  is a desk to send them to. A feed keeps the scheme it was opened with
 *  — in its uri(), in the local address() it reports and in the sender
 *  every arrival names — so a reader picks the decoding off the URI
 *  rather than out of the bytes. */
void registerUdp(Hub& hub);

/** Installs the WebSocket transport on @p hub for the scheme "ws". A URI
 *  of the form ws://:PORT/PATH listens on every interface for peers
 *  reaching that path, PORT 0 meaning any free port and an omitted PATH
 *  meaning "/". Every text or binary message from any peer arrives in
 *  the feed naming the peer it came from, and send() goes out to every
 *  peer on that path at once, while one named peer is answered alone.
 *  Each listening feed runs on a thread of its own that stands until the
 *  feed is closed.
 *
 *  A URI'S QUERY MAY NAME WHERE ITS PAGES STAND —
 *  ws://:PORT/PATH?pages=URI — and the same port then answers HTTP GET
 *  out of that directory, so what a peer loads and the socket it opens
 *  back are one address. "/" and "/index.html" are the directory's
 *  index.html; any other path is the file of that name beneath the
 *  directory, typed by its extension; a path naming no file there, one
 *  climbing out through "..", and every request to a listener whose URI
 *  named no pages at all, are answered 404. The URI is resolved through
 *  @p hub's mount table as the feed opens — a URI that resolves to no
 *  directory opens nothing and leaves the reason on the feed — and what
 *  the listener keeps afterwards is the directory itself, read again
 *  per request, so a page edited on disk is the page the next reload is
 *  served. The path peers reach is the PATH alone: a query is the
 *  listener's own arrangement and stands in neither the address the
 *  feed reports nor the one an arrival names.
 *
 *  This registration LISTENS: the sockets underneath carry no client and
 *  are built without TLS, so a URI naming a host to call opens nothing
 *  here and leaves the reason on the feed, and there is no "wss" it
 *  could hold a port for. registerWebSocketClient() is what takes those
 *  URIs, and it stands in front of this. */
void registerWebSocket(Hub& hub);

/** Installs the WebSocket CLIENT on @p hub for the schemes "ws" and
 *  "wss", in front of whatever was registered for them: one scheme, and
 *  the shape of the URI is what says which end of it a feed is. A URI of
 *  the form ws://HOST:PORT/PATH (HOST a name, an IPv4 address, or a
 *  bracketed IPv6 address) calls that server, and wss://HOST:PORT/PATH
 *  calls it over TLS; a URI naming no host is a port to hold and goes to
 *  the transport this one was installed over, so registerWebSocket()
 *  comes first and a scheme with no listener behind it refuses such a
 *  URI with the reason.
 *
 *  Every message the server sends arrives whole, however many frames it
 *  was split into, naming the server as its sender; send() writes one
 *  whole message back to it, a client having the one peer it dialled,
 *  which is also the address() it reports. Each feed runs one session on
 *  a thread of its own, and the handshake runs there too: a feed is
 *  answered before its server is reached, error() carries the reason
 *  when it cannot be, and a send before then goes nowhere and says so.
 *  A server that ends the session closes the feed. */
void registerWebSocketClient(Hub& hub);

/** Installs the SHARED MEMORY reader on @p hub for the scheme "shm". A
 *  URI of the form shm://NAME maps the shared memory object of that
 *  name and delivers what the one writer of that region puts there, so
 *  a message crosses from another process on this machine through
 *  memory both of them have mapped, with no socket beneath it and no
 *  kernel on the way.
 *
 *  THE READER NEVER WRITES AND NEVER WAITS. The region is mapped
 *  read-only, and it carries a number that counts the messages written
 *  into it: odd while one is being written and even once it is whole.
 *  So a read that brackets its copy between two equal even numbers took
 *  a message nobody was writing over, and one that does not is dropped
 *  and read again at the next look. A region no writer has made, and
 *  one whose first bytes are not this layout's, opens nothing and
 *  leaves the reason on the feed.
 *
 *  NOTHING PUSHES, SO THE FEED LOOKS: shm://NAME?rate=HERTZ reads the
 *  region a whole number of times a second, 120 times where the URI
 *  names no rate, and every message the writer leaves standing between
 *  two looks is one arrival. Every arrival names the region as its
 *  sender, spelled shm://NAME as the address() the feed reports is —
 *  the rate being the reader's own arrangement and no part of what the
 *  region is called. A reader has no way back to a writer through the
 *  region, so send() and sendTo() are false on such a feed: a scene
 *  that must answer holds another door for that. The looks of every
 *  feed opened through one registration run on one thread, made when
 *  the first of them opens. */
void registerSharedMemory(Hub& hub);

/** THE OTHER END OF A shm:// REGION: the one process that puts the
 *  messages in it.
 *
 *  Construction makes the shared memory object named @p name, replacing
 *  whatever stood under that name, large enough to hold @p capacity
 *  bytes of payload under the fixed-size header the layout opens with.
 *  ONE WRITER PER REGION: a region carries one count of its messages
 *  and not one per writer, so two writers on a name would write over
 *  each other. Destruction unmaps the region and takes the name back,
 *  after which a reader opening that name finds nothing and says so,
 *  while a reader that already mapped it goes on reading the message it
 *  holds.
 *
 *  A WRITER STANDS BEFORE ITS READERS. A reader maps what is there when
 *  it opens, so a region made after the feed was opened is a region
 *  that feed never sees.
 *
 *  A writer in another language needs nothing of this class: the layout
 *  and the count are the whole of what the two ends share. */
class SharedMemoryWriter {
 public:
  SharedMemoryWriter(std::string_view name, size_t capacity);
  ~SharedMemoryWriter();

  SharedMemoryWriter(const SharedMemoryWriter&) = delete;
  SharedMemoryWriter& operator=(const SharedMemoryWriter&) = delete;

  /** Whether the region stands. False when the object could not be made
   *  or mapped, and every write() is false from then on. */
  bool open() const { return m_region != nullptr; }

  /** Puts @p bytes in the region as the newest message: the count goes
   *  odd, the size and the bytes are written, and the count goes even
   *  again, so a reader either takes the whole message or sees that one
   *  was being written and looks again. False when the region does not
   *  stand and when @p bytes are more than the capacity it was made
   *  with — a message is written whole or not at all. The same bytes
   *  written twice are two messages: what makes a message new is the
   *  count and not what it says. */
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

/** Installs the MIDI transport on @p hub for the scheme "midi". A URI of
 *  the form midi://in/NAME opens the first input port whose own name
 *  holds NAME, letter for letter with case left out of it, and
 *  midi://out/NAME the first output port that does; NAME left out
 *  altogether takes the first port there is, which is the one
 *  controller on a desk that has one. A PORT IS NAMED AND NEVER
 *  NUMBERED here, because which port a machine calls its second depends
 *  on what else was plugged in this morning. A name nobody answers to
 *  opens nothing and leaves on the feed both the name that was looked
 *  for and the ports that do exist.
 *
 *  midi://in/virtual:NAME and midi://out/virtual:NAME MAKE a port of
 *  that name instead of looking for one, so other software on this
 *  machine can reach the scene as it reaches a controller. A system
 *  that does not offer ports made rather than found opens nothing and
 *  says so.
 *
 *  AN INPUT DELIVERS EVERY MESSAGE THE WIRE CARRIES, one arrival per
 *  message, status byte first and the bytes exactly as they arrived —
 *  system exclusive included, since that is what a controller answers a
 *  question with. The two a scene cannot use are left out: a clock
 *  beats twenty-four times a quarter note and a sensing byte arrives
 *  several times a second whether or not anybody played anything, so a
 *  feed taking both would be a feed of heartbeat with the performance
 *  somewhere inside it. Every arrival names the port as its sender,
 *  spelled midi://in/NAME with the port's whole name, exactly as the
 *  address() such a feed reports is; an output reports midi://out/NAME
 *  the same way.
 *  An input is ONE WAY — what comes back down a cable is the other
 *  cable, which is a door of its own — so send() and Feed::sendTo() are
 *  both false on one, while an output's send() writes the bytes as one
 *  message and is false where the driver refused it.
 *
 *  NO THREAD IS STARTED. The driver runs a callback of its own on every
 *  arriving message, which is the thread a message is delivered from,
 *  and an output is written on the thread that asked. */
void registerMidi(Hub& hub);

/** Installs the SERIAL transport on @p hub for the scheme "serial". A
 *  URI of the form serial://DEVICE?baud=RATE opens the device file of
 *  that path — whole and absolute, as in
 *  serial:///dev/tty.usbmodem1101?baud=115200 — at that rate. THE RATE
 *  IS REQUIRED and there is none to fall back on: two ends that
 *  disagree about it read each other as noise, so a URI carrying none
 *  opens nothing and says so. The rest of the wire's settings stand in
 *  the same query and have the defaults a board is wired for —
 *  bits=5|6|7|8 (8), parity=none|odd|even (none), stop=1|2 (1),
 *  flow=none|software|hardware (none) — and a port that will not take
 *  one of them is a door that does not open, since a port read at a
 *  setting nobody asked for answers bytes that are not the ones on the
 *  wire.
 *
 *  A MESSAGE IS A LINE, BOTH WAYS. What reaches a serial port is a run
 *  of bytes with no message boundary in it, so the boundary is the one
 *  the sender writes: every arrival is the bytes up to a newline, with
 *  a carriage return before it left off and a blank line delivered to
 *  nobody, and half a line is no arrival at all until the rest of it
 *  comes. A FEED THAT OPENS ONTO A WIRE ALREADY IN MID-LINE takes the
 *  tail of that line as its first arrival, there being nothing in the
 *  bytes that says where the line began. A run of 64 KiB with no
 *  newline among them is handed over as
 *  the line it stands as and the reading begins again, so a sender that
 *  frames nothing still reaches a reader. send() writes the bytes and a
 *  newline after them, which is where the reader on the board stops.
 *
 *  A CABLE HOLDS ONE PEER: send() reaches what is at the other end and
 *  there is no sender to pick out by name, so Feed::sendTo() is false
 *  on such a feed. Every arrival names the port as its sender, spelled
 *  serial://DEVICE with the settings left off — what the board IS, and
 *  not how this end was told to read it — exactly as the address() the
 *  feed reports is. The ports every registration opens share ONE
 *  thread, made when the first of them opens, so a hub taught the
 *  scheme and never asked for a port starts nothing. */
void registerSerial(Hub& hub);

/** Installs the gRPC transport on @p hub for the scheme "grpc". ONE
 *  SCHEME, TWO SHAPES, and the shape of the URI is what says which end
 *  a feed is. A URI of the form grpc://:PORT/Service/Method holds that
 *  port and serves that one method, PORT 0 meaning any free port;
 *  grpc://HOST:PORT/Service/Method (HOST a name, an IPv4 address, or a
 *  bracketed IPv6 address) calls that method there. BOTH ENDS NAME THE
 *  METHOD, so a URI carrying a service and nothing after it opens
 *  nothing and leaves the reason on the feed.
 *
 *  THE METHOD IS SCHEMA-AGNOSTIC: it carries bytes in both directions
 *  and nothing here parses one, so a feed's buffers, its JSON and
 *  whatever else a sender writes cross it with no generated stub in the
 *  transport. What a message MEANS is the business of the library that
 *  owns the format, exactly as on every other door.
 *
 *  A SERVER'S PEER IS A CALL AND NOT A CALLER. Every call that arrives
 *  is a stream of its own, one caller may hold several at once, and
 *  each message written on one is an arrival naming that call —
 *  grpc://ADDRESS#NUMBER, the number counting the calls that feed has
 *  taken. Feed::sendTo() writes on the call it names, send() writes on
 *  every call standing and is false where none is, and a caller that
 *  ends its half of the stream ends that peer. The address() such a
 *  feed reports is grpc://[::]:PORT/Service/Method, every interface of
 *  both families being one dual-stack listener.
 *
 *  A CLIENT HOLDS THE ONE CALL IT OPENED. send() writes one message on
 *  it, every message the server writes is an arrival naming the URI
 *  that was called — which is also the address() such a feed reports —
 *  and Feed::sendTo() is false on it, a client having the one peer it
 *  called. The server ending the call closes the feed, and a server
 *  that goes away mid-conversation fails it with the sentence saying
 *  the call ENDED — which a call that never reached a server at all
 *  does not say, the two being worth telling apart by reading the
 *  reason. A channel that has not connected within ten seconds fails
 *  the feed that way too, and a refused connection says it at once
 *  instead of waiting the bound out.
 *
 *  NO TLS AT EITHER END in this cut, so both a server and a call stand
 *  on a machine or a network somebody already trusts; a "grpcs" scheme
 *  carrying credentials is the step after this one and is not
 *  registered.
 *
 *  NO THREAD IS STARTED FOR A FEED. gRPC runs threads of its own and
 *  calls back onto them, so a server's callers and a client's stream
 *  are carried without this library holding a loop, an executor or a
 *  completion queue of its own. */
void registerGrpc(Hub& hub);

/** Installs the WEBRTC transport on @p hub for the scheme "webrtc". A
 *  URI of the form webrtc://ROOM?signal=URI holds one conversation,
 *  named ROOM, between this end and however many peers take it up, and
 *  every message crosses STRAIGHT between them: two ends that have
 *  found each other speak over no server, which is what lets a phone on
 *  a mobile network reach a scene behind a router.
 *
 *  NEITHER END CAN DIAL THE OTHER, so they are introduced. `signal`
 *  names the ws:// or wss:// URI that introduction crosses, opened on
 *  @p hub as any other feed is, and THE SHAPE OF THAT URI SAYS WHICH
 *  END THIS ONE IS: ws://:PORT/PATH is a port to hold, so this feed
 *  WAITS to be taken up and answers whoever offers; ws://HOST:PORT/PATH
 *  is a server to call, so this feed TAKES A ROOM UP and offers into
 *  it. What crosses that door is JSON text —
 *  {"kind":"offer","room":…,"sdp":…}, the same with "answer", and
 *  {"kind":"candidate","room":…,"candidate":…,"mid":…} — each naming
 *  its room, so one socket carries as many conversations as there are
 *  rooms on it and a door reads only its own. The signalling feed is
 *  the transport's own to drain: a reader that drains it too takes
 *  introductions away from the doors on it. ?ice=stun:HOST:PORT names a
 *  server asked what address this machine has to the world, may be
 *  given more than once, and is needed for two ends on different
 *  networks and for neither of two on one.
 *
 *  THE FRAME CARRIES THE INTRODUCTION. Hub::dispatch() is what reads
 *  the signalling door and answers it, so a handshake takes a few
 *  frames and a host that never dispatches never finishes one. What
 *  arrives on a channel is not frame-paced: it is delivered the moment
 *  it lands.
 *
 *  ONE PEER PER CONNECTION, on a channel named "feed". Every message
 *  arriving on one is an arrival naming that peer —
 *  webrtc://ROOM#NUMBER, the number counting the peers this feed has
 *  taken — send() writes on every channel standing open, and
 *  Feed::sendTo() writes on the one it names. A channel that closes
 *  ends its peer, and closing the feed ends every connection and lets
 *  the signalling door go with it. The address() such a feed reports is
 *  webrtc://ROOM: the signal is this door's own arrangement and no part
 *  of what the conversation is called.
 *
 *  NO THREAD IS STARTED FOR A FEED. The library underneath runs threads
 *  of its own and calls back onto them, so the routes, the encryption
 *  and the stream are carried without this library holding a loop or an
 *  executor of its own. */
void registerWebRtc(Hub& hub);

/** Installs every transport this feature carries: UDP, the OSC name over
 *  it, WebSocket at both ends — the listener first, and the client in
 *  front of it — the shared memory reader, MIDI, the serial port, gRPC
 *  at both ends, and WebRTC over a signalling door of the same hub. */
void registerTransports(Hub& hub);

}  // namespace sigil::io
