/** @file
 * THE QUIC LIBRARY A DOOR OF THIS SCHEME STANDS ON: its function table
 * and registration, the arrangement every connection of a feed is made
 * under, what it says when something goes wrong, and the flag that tells
 * a thread it is inside one of the library's own callbacks.
 */

#pragma once

#include <msquic.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace sigil::io::quic {

/** THE PROTOCOL BOTH ENDS NAME. A QUIC handshake carries the names of
 *  the protocols each end speaks and fails when they share none, so this
 *  is what keeps a feed's door from being taken up by software that
 *  speaks something else over the same port. */
constexpr std::string_view kProtocol = "sigil-feed/1";

/** The largest message a peer may send. A feed's message is one whole
 *  thing rather than a stream, so a stream that grows past this is
 *  abandoned instead of the door holding a growing fragment for it. */
constexpr size_t kMessageCeiling = 16u * 1024u * 1024u;

/** How long a call is given to reach the end it named before the feed is
 *  told it was not reached. It is the handshake's own bound, and what it
 *  answers for is a host that TAKES THE CONNECTION NOWHERE: a machine
 *  that answers at once that nothing is listening on that port ends the
 *  call there and never waits this out, while without a bound the silent
 *  case would leave a feed waiting for as long as anybody held it. */
constexpr uint32_t kReachWithinMs = 10000;

/** How long a connection with nothing crossing it stands, and how often
 *  nothing is said on it. A door is open for as long as a scene holds
 *  it, so the quiet is kept alive on purpose; an end that goes away
 *  rather than falling quiet is noticed within the first of these. */
constexpr uint32_t kIdleMs = 30000;
constexpr uint32_t kKeepAliveMs = 10000;

/** How many of a peer's messages may be in flight at once. Every message
 *  is a stream, so a peer may open this many before it waits for the
 *  ones it opened to finish. */
constexpr uint16_t kMessagesInFlight = 1024;

/** What a door says as it ends a connection or a stream: nothing in
 *  particular. The number crosses the wire, so the two ends would have
 *  to agree on what it means, and there is nothing here for them to
 *  agree about. */
constexpr QUIC_UINT62 kNothingToSay = 0;

/** THE LIBRARY UNDERNEATH: its function table, and the registration its
 *  workers run under. Both are made on the first quic:// feed and stand
 *  for the life of the process.
 *
 *  GIVING EITHER BACK WAITS FOR EVERY WORKER TO END, and a worker cannot
 *  wait for itself — while the last holder of a feed may well be a
 *  delivery running on one of those workers. So what a door owns and
 *  gives back is a listener, a connection and a stream, each of which
 *  can be given back from where it is reached, and the library beneath
 *  them stays. */
struct Library {
  const QUIC_API_TABLE* api = nullptr;
  HQUIC registration = nullptr;
};

const Library& library();

/** The protocol as the handshake carries it. A function-local string
 *  rather than a literal because the call takes a writable buffer, and
 *  nothing here writes through it. */
QUIC_BUFFER protocol();

/** What went wrong, in words rather than in a number. The ones named
 *  here are the ones a reader of a feed's error can act on; anything
 *  else is handed over as the library's own number, which says more than
 *  a sentence that fits everything. */
std::string reasonOf(QUIC_STATUS status);

/** HOW EVERY CONNECTION OF EVERY FEED IS ARRANGED. The quiet is kept
 *  alive rather than let go, a peer may hold as many messages in flight
 *  as there are streams for, and a datagram is always taken — a sender
 *  that writes one reaches a door whose own URI asked for nothing. */
QUIC_SETTINGS feedSettings();

/** The arrangement one door's connections stand on, with @p credential
 *  loaded into it; null where it could not be made, and @p why is then
 *  the library's word for that. */
HQUIC configurationFor(const QUIC_CREDENTIAL_CONFIG& credential,
                       QUIC_STATUS& why);

/** Whether this thread is inside one of the library's own callbacks.
 *
 *  A listener is given back by waiting for it to stop indicating, and a
 *  callback cannot wait for itself. The last holder of a feed can be the
 *  very delivery arriving on it, so letting that delivery go is one of
 *  the ways a door is closed — and a door closed that way hands its
 *  shutdown to a thread nothing is waiting for. */
extern thread_local bool insideCallback;

/** Raises that flag for as long as one callback runs. */
struct InCallback {
  InCallback() : before(insideCallback) { insideCallback = true; }
  ~InCallback() { insideCallback = before; }

  InCallback(const InCallback&) = delete;
  InCallback& operator=(const InCallback&) = delete;

  bool before;
};

}  // namespace sigil::io::quic
