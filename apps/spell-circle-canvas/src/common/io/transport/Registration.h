#pragma once

/** @file
 * One installer per transport, which registerTransports() calls for the
 * schemes it is asked for. Private to the transport feature and its own
 * cases: a consumer names schemes, never a transport's installer.
 */

namespace sigil::io {
class Hub;
}  // namespace sigil::io

namespace sigil::io::detail {

/** Installs the UDP transport on @p hub for the schemes "udp", "osc" and
 *  "artnet". udp://:PORT listens on every interface, IPv4 and IPv6 alike,
 *  PORT 0 meaning any free port; udp://HOST:PORT sends to that peer and
 *  receives whatever comes back to it. One thread per socket, living as
 *  long as the hub holds the transport.
 *  @trap A listening socket holds no one peer, so send() goes nowhere by
 *  itself; what it can answer is the ONE sender an arrival names,
 *  through Feed::send() with SendOptions::to. */
void registerUdp(Hub& hub);

/** Installs the WebSocket LISTENER on @p hub for the scheme "ws".
 *  ws://:PORT/PATH holds that path on every interface, PORT 0 meaning any
 *  free port and an omitted PATH meaning "/"; a ?pages=URI query answers
 *  HTTP GET out of that directory on the same port. send() reaches every
 *  peer at once and Feed::send() with SendOptions::to one, on a thread
 *  per listening feed.
 *  @trap There is no client here and no "wss" to hold a port for: a URI
 *  naming a host to CALL opens nothing, and the client in front of it is
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
 *  @silent send() and Feed::send() with SendOptions::to: a reader has no
 *  way back to a writer through a region. */
void registerSharedMemory(Hub& hub);

/** Installs the MIDI transport on @p hub for the scheme "midi".
 *  midi://in/NAME opens the first input port whose own name holds NAME,
 *  case left out of it, midi://out/NAME the first output that does, and
 *  NAME left out takes the first port there is; virtual:NAME MAKES a port
 *  of that name instead of looking for one. No thread is started: the
 *  driver's own callback is the thread a message arrives on.
 *  @trap A PORT IS NAMED AND NEVER NUMBERED, because which port a machine
 *  calls its second depends on what else was plugged in this morning.
 *  @silent send() and Feed::send() with SendOptions::to on an INPUT: what
 *  comes back down a cable is the other cable. */
void registerMidi(Hub& hub);

}  // namespace sigil::io::detail
