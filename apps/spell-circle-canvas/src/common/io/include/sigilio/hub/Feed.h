#pragma once

/** @file
 * @ingroup io-hub
 * A FEED: a resource that keeps arriving. One door, keyed by URI, with a
 * transport on one side and readers on the other. A reader on any thread
 * either takes the newest message whole through latest() or drains in
 * order the ones it has not seen through receive(), and neither ever
 * waits for one. A feed holds the
 * last `FeedPolicy::capacity` arrivals for receive(), and dropped() counts
 * what fell off the front. The same door plays a recording back, and
 * writes one for as long as the Recording its record() hands out lives.
 */

#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sigilio/source/Source.h"

namespace sigil::io {

class Feed;
class RecordingWriter;

namespace detail {
/** The writer one record() call opened, shared by the feed that
 *  appends to it and the Recording that stops it. */
struct RecordingSlot;
}  // namespace detail

/** ONE MESSAGE OFF A FEED. */
struct Arrival {
  /** 1 for the first arrival on a feed, counting up from there. */
  uint64_t generation = 0;
  /** Seconds: on a live feed, since the feed was made; on a replayed
   *  recording, the value the recording carries. */
  double at = 0;
  std::shared_ptr<const Bytes> bytes;
  /** The address the message came from, spelled the way a URI of that
   *  scheme is, `udp://127.0.0.1:52341`; empty for a recording, and for
   *  a transport that has no way of knowing. */
  std::string from;
};

/** WHAT A TRANSPORT HANDS BACK once it has opened a URI. */
struct OpenedFeed {
  /** Closes the transport's end. The feed calls it once, from close()
   *  or from its destructor. */
  std::function<void()> close;
  /** Sends through the same door; empty when the way is one-way. */
  std::function<bool(const Bytes&)> send;
  /** Sends to ONE sender, named the way an arrival's `from` spells it;
   *  empty when the transport cannot address one. */
  std::function<bool(std::string_view to, const Bytes&)> sendTo;
  /** The peers attached to the door now, each named the way an
   *  arrival's `from` spells it; empty when the transport holds no peers
   *  of its own. */
  std::function<std::vector<std::string>()> peers;
  /** The local end as the transport bound it, "udp://[::]:52341";
   *  empty when it has none. */
  std::string address;
};

/** HOW A SCHEME OPENS A DOOR: given the URI and the feed to deliver
 *  into, hands back the opened end. The transport keeps only the weak
 *  pointer and delivers through into.lock(), so a feed nobody holds is
 *  gone and its transport stops. */
using FeedTransport =
    std::function<OpenedFeed(std::string_view uri, std::weak_ptr<Feed> into)>;

/** HOW MUCH A FEED HOLDS FOR ITS READERS. It stands beside the class it
 *  belongs to rather than inside it because a member initializer of a
 *  nested class is not in hand until the class around it closes, and
 *  the constructor below takes this one as its default. */
struct FeedPolicy {
  /** Arrivals kept for receive() before the oldest is dropped. */
  size_t capacity = 256;
};

/** A FEED BEING WRITTEN DOWN: what `Feed::record()` hands back. Every
 *  arrival on the feed goes to the file until this handle stops it —
 *  through stop(), or by going out of scope — so a recording nobody
 *  holds is one that has ended. Move-only; a handle made empty, or
 *  moved from, stops nothing. */
class Recording {
 public:
  Recording() = default;
  /** Stops the recording this handle holds. */
  ~Recording();
  /** Takes over @p other's recording, leaving it holding nothing. */
  Recording(Recording&& other) noexcept;
  /** Stops this handle's recording and takes over @p other's. */
  Recording& operator=(Recording&& other) noexcept;
  Recording(const Recording&) = delete;
  Recording& operator=(const Recording&) = delete;

  /** Takes no more frames. What reached the file stays whole, and the
   *  file is closed before this returns. */
  void stop();

  /** Whether nothing is being written through this handle any more:
   *  stop() was called, the feed closed, the feed started another
   *  recording, or the file could not take a frame. */
  bool stopped() const;

 private:
  friend class Feed;
  explicit Recording(std::shared_ptr<detail::RecordingSlot> slot);

  std::shared_ptr<detail::RecordingSlot> m_slot;
};

/** A door that keeps delivering: bytes in from a transport or a
 *  recording, messages out to readers on any thread. */
class Feed {
 public:
  /** Maps an arrival from this feed onto the steady clock. Live arrivals
   *  retain their transport receive time; replay times are relative to the
   *  first advance() call, preserving recorded spacing across queued reads.
   *  Negative, nonfinite or unrepresentable times map to that clock origin. */
  std::chrono::steady_clock::time_point receivedAt(
      const Arrival& arrival) const;

  /** A door named @p uri under @p policy. Nothing is opened here: a
   *  transport or a recording is handed over afterwards. */
  Feed(std::string uri, FeedPolicy policy = {});
  /** Closes the opened end. */
  ~Feed();

  Feed(const Feed&) = delete;
  Feed& operator=(const Feed&) = delete;

  /** Takes one message, stamped with the seconds since the feed was
   *  made. Any thread. */
  void deliver(Bytes bytes);

  /** The same, naming the address the message came from — what a
   *  transport that knows its sender delivers through. */
  void deliver(Bytes bytes, std::string from);

  /** The same, with a recording's own time instead of the clock's. */
  void deliver(Bytes bytes, double at);

  /** Says what went wrong, which error() answers from then on; an empty
   *  reason is nothing wrong, and takes off what stood there. The feed
   *  stays open: a transport that lost one message still has a door.
   *  @trap A TRANSPORT THAT OPENED NOTHING SAYS SO HERE, and the feed is
   *  then one that was never OPENED rather than one with a door — which
   *  is what lets the next ask for its URI open it again. */
  void fail(std::string why);

  /** No more arrivals are taken. What was received stays readable, and
   *  the opened end's close runs once. */
  void close();

  /** THE NEWEST ARRIVAL WHOLE: the generation it came in as, the second
   *  it came in at, its bytes and the address it came from, read out
   *  together so they are one message's; nothing before the first
   *  arrival. It is latched rather than queued, so draining through
   *  receive() leaves it standing. */
  std::optional<Arrival> latest() const;

  /** How many messages have arrived; 0 before the first. */
  uint64_t generation() const;

  /** The next arrival this reader has not taken, in order; nothing when
   *  none is waiting. Never waits for one. */
  std::optional<Arrival> receive();

  /** Arrivals that fell off the front because the feed was full. */
  uint64_t dropped() const;

  /** Whether the door has been shut, after which nothing more
   *  arrives. */
  bool closed() const;

  /** Whether a door stands on this feed: an end its transport opened,
   *  or the recording it plays back instead of one. False before either
   *  is handed over and on a feed whose transport opened nothing, which
   *  carries the reason as its error(). */
  bool opened() const;

  /** What went wrong; empty when nothing did. */
  std::string error() const;

  /** The URI this feed was opened on, as it was written. */
  const std::string& uri() const { return m_uri; }

  /** The local end as the transport bound it, or empty. */
  std::string address() const;

  /** Sends back through the opened end. False when the way is one-way,
   *  when the feed is closed, and when no transport opened it. */
  bool send(const Bytes& bytes) const;

  /** Sends to ONE sender: @p to is an address spelled the way an
   *  arrival's `from` is, `udp://127.0.0.1:52341`, which is how a door
   *  that holds no peer of its own answers the one that wrote to it.
   *  False when the way is one-way for that purpose, when the feed is
   *  closed, and when no transport opened it. */
  bool sendTo(std::string_view to, const Bytes& bytes) const;

  /** THE PEERS ATTACHED NOW, each named the way an arrival's `from`
   *  spells it, which is how a door that holds many learns that one has
   *  left: the name is gone from here. Empty when the feed is closed,
   *  when no transport opened it, and when the transport holds no peers
   *  of its own — a datagram socket has senders and never attached
   *  peers. */
  std::vector<std::string> peers() const;

  /** Appends every arrival from now on to the file at @p path in the
   *  recording format, so this feed can be played back later through
   *  `Hub::replay()`, until the handle this returns stops it. A feed
   *  writes one recording at a time: a second call stops the first.
   *  @trap A file that cannot be opened hands back a handle that has
   *  already stopped, and error() says why. */
  [[nodiscard]] Recording record(std::filesystem::path path);

  /** Hands the feed the end its transport opened. Once: a second end,
   *  and one handed to a feed that is already closed, is closed rather
   *  than kept. An end that stands is kept with whatever the transport
   *  has said about it by then.
   *  @trap AN END WITH NOTHING IN IT, HANDED TO A FEED CARRYING A
   *  REASON, IS NO END: the feed stays unopened with that reason
   *  standing, so the next ask for its URI opens it again into this
   *  same feed. */
  void opened(OpenedFeed opened);

  /** This feed reads @p recording instead of a transport: advance() is
   *  then what delivers, and the recording is the door this feed
   *  opened. */
  void replay(std::vector<Arrival> recording);

  /** Moves a replayed recording's time to @p seconds on the caller's
   *  clock. The first call fixes the origin, so a recording starts when
   *  its feed is first advanced; every recorded arrival due by then is
   *  delivered in order with its recorded time, and the feed closes
   *  after the last one. A live feed ignores it. */
  void advance(double seconds);

 private:
  /** Stamps and queues one arrival with the lock already held. Every
   *  path that takes a message — the transport's and the recording's —
   *  runs through here, so a generation is never handed out twice and
   *  the file a recording writes carries the feed's own order. */
  void deliverLocked(std::shared_ptr<const Bytes> bytes, double at,
                     std::string from);

  /** Marks the feed closed and takes the opened end's close function
   *  out of it. The caller runs that function after releasing the lock:
   *  a transport shutting its end down may call back into the feed, and
   *  a lock held across that call would be taken twice by one thread. */
  std::function<void()> closeLocked();

  const std::string m_uri;
  const FeedPolicy m_policy;
  const std::chrono::steady_clock::time_point m_made =
      std::chrono::steady_clock::now();

  /** Guards every member below. It is held for the moves that stamp and
   *  queue an arrival and for the append that records one, never across
   *  a call into the transport: a reader waits for another reader, and
   *  never for a socket. */
  mutable std::mutex m_mutex;
  std::deque<Arrival> m_arrivals;
  /** The last arrival, kept whole: who sent the newest message and
   *  which one it is are readable without draining the queue another
   *  reader is taking messages off. */
  std::optional<Arrival> m_latest;
  uint64_t m_generation = 0;
  uint64_t m_dropped = 0;
  bool m_closed = false;
  std::string m_error;
  OpenedFeed m_openedEnd;
  bool m_wasOpened = false;
  /** The recording being written, shared with the Recording handle
   *  that stops it; null when nothing is being written. */
  std::shared_ptr<detail::RecordingSlot> m_recorder;

  /** A recording being played back: what it holds, how much of it has
   *  been delivered, and the time the first advance() fixed as its
   *  start. */
  std::vector<Arrival> m_recording;
  size_t m_replayed = 0;
  bool m_replaying = false;
  std::optional<double> m_origin;
  std::optional<std::chrono::steady_clock::time_point> m_replayOrigin;
};

}  // namespace sigil::io
