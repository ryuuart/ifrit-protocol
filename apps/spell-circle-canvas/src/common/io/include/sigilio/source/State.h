#pragma once

/** @file
 * @ingroup io-source
 * WHERE A DOOR STANDS, as one value: the readiness a feed and a frame
 * subscription share, with the counts and the words a reader shows.
 * Standard library only, so both ends of the library read one type.
 */

#include <cstdint>
#include <string>

namespace sigil::io {

/** WHETHER A DOOR STANDS: a feed's, or a frame subscription's. */
enum class ReadyState {
  /** No door yet: nothing is opened, or what was asked for could not be
   *  and the next ask for the URI tries again; `FeedState::error` says
   *  why when something stood in the way. */
  Connecting,
  /** A door stands — an end its transport opened, or the recording it
   *  plays back instead of one — and messages arrive through it. */
  Open,
  /** The door has been shut; nothing more arrives, and what was received
   *  stays readable. */
  Closed,
};

/** A DOOR'S STATE AS ONE COMPARABLE VALUE: what `Feed::state()` and a
 *  frame subscription's `state()` answer, read out together, so a reader
 *  that keeps the value it last showed describes again exactly when
 *  `state() != shown`. */
struct FeedState {
  ReadyState readiness = ReadyState::Connecting;
  /** How many messages — or frames — have arrived; 0 before the first. */
  uint64_t revision = 0;
  /** Messages that fell off the front because the feed was full. */
  uint64_t dropped = 0;
  /** The local end as the transport bound it, `udp://[::]:52341`; empty
   *  when it has none. */
  std::string localAddress;
  /** What went wrong; empty when nothing did. */
  std::string error;

  /** Whether messages arrive through a door that stands. */
  bool isOpen() const { return readiness == ReadyState::Open; }

  bool operator==(const FeedState&) const = default;
};

}  // namespace sigil::io
