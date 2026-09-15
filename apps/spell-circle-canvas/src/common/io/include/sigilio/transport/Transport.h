#pragma once

/** @file
 * The transports a hub opens FEEDS through: a URI scheme, and the socket
 * behind it. Registering one teaches a hub that scheme; a feed the hub
 * is then asked for on it binds or connects a socket of its own, and
 * every message that socket receives arrives in that feed.
 *
 * One of them carries no socket at all: a shm:// feed reads a region of
 * memory another process on this machine has mapped, which is the same
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

/** Installs the UDP transport on @p hub for the schemes "udp" and
 *  "osc". A URI of the form udp://:PORT listens on every interface, IPv4
 *  and IPv6 alike, PORT 0 meaning any free port; udp://HOST:PORT (HOST a
 *  name, an IPv4 address, or a bracketed IPv6 address) opens a socket
 *  that sends to that peer and receives whatever comes back to it. Every
 *  socket the transport opens runs on one thread of its own that lives
 *  as long as the hub holds the transport.
 *
 *  A listening socket holds no one peer, so nothing goes out of it by
 *  itself; what it can do is answer ONE sender, by the address that
 *  sender's datagram arrived from.
 *
 *  osc:// is that same socket under another name: an osc://:9000 feed is
 *  a UDP socket whose messages are OSC packets. A feed keeps the scheme
 *  it was opened with — in its uri(), in the local address() it reports
 *  and in the sender every arrival names — so a reader picks the
 *  decoding off the URI rather than out of the bytes. */
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

/** Installs every transport this feature carries: UDP, the OSC name over
 *  it, WebSocket at both ends — the listener first, and the client in
 *  front of it — and the shared memory reader. */
void registerTransports(Hub& hub);

}  // namespace sigil::io
