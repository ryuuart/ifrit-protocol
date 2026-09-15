#pragma once

/** @file
 * The transports a hub opens FEEDS through: a URI scheme, and the socket
 * behind it. Registering one teaches a hub that scheme; a feed the hub
 * is then asked for on it binds or connects a socket of its own, and
 * every datagram that socket receives arrives in that feed.
 *
 * What a datagram MEANS is not decided here: a feed answers bytes, and
 * the library that owns the format decodes them.
 */

namespace sigil::io {

class Hub;

/** Installs the UDP transport on @p hub for the scheme "udp". A URI of
 *  the form udp://:PORT listens on every interface, IPv4 and IPv6 alike,
 *  PORT 0 meaning any free port; udp://HOST:PORT (HOST a name, an IPv4
 *  address, or a bracketed IPv6 address) opens a socket that sends to
 *  that peer and receives whatever comes back to it. Every socket the
 *  transport opens runs on one thread of its own that lives as long as
 *  the hub holds the transport. */
void registerUdp(Hub& hub);

/** Installs every transport this feature carries; UDP today. */
void registerTransports(Hub& hub);

}  // namespace sigil::io
