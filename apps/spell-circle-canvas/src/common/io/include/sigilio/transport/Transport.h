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
 *  peer on that path at once. Each listening feed runs on a thread of
 *  its own that stands until the feed is closed.
 *
 *  Listening only: the library underneath carries no client, so a URI
 *  that names a host opens nothing and leaves the reason on the feed.
 *  There is no "wss" for the same kind of reason — the sockets
 *  underneath are built without TLS. */
void registerWebSocket(Hub& hub);

/** Installs every transport this feature carries: UDP, the OSC name over
 *  it, and WebSocket. */
void registerTransports(Hub& hub);

}  // namespace sigil::io
