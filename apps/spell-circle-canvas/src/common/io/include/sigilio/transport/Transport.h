#pragma once

/** @file
 * The transports a hub opens FEEDS through: a URI scheme, and the socket
 * behind it. Registering one teaches a hub that scheme; a feed the hub
 * is then asked for on it binds or connects a socket of its own, and
 * every message that socket receives arrives in that feed.
 *
 * What a message MEANS is not decided here: a feed answers bytes, and
 * the library that owns the format decodes them.
 */

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

/** Installs every transport this feature carries: UDP, the OSC name over
 *  it, and WebSocket at both ends — the listener first, and the client
 *  in front of it. */
void registerTransports(Hub& hub);

}  // namespace sigil::io
