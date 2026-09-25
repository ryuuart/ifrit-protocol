#pragma once

/** @file
 * @ingroup io-hub
 * A FEED: a resource that keeps arriving. One door, keyed by URI, with a
 * transport on one side and readers on the other; this is the readers'
 * side, and the transport's is the `Inlet` it is handed. A reader on any thread
 * either takes the newest message whole through latest() or drains in
 * order the ones it has not seen through receive(), and neither ever
 * waits for one. A feed holds the
 * last `FeedPolicy::capacity` messages for receive(), and dropped() counts
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
#include <utility>
#include <vector>

#include "sigilio/advanced/Transport.h"
#include "sigilio/source/Source.h"

namespace sigil::io {

class Feed;
class RecordingWriter;

namespace detail {
/** The writer one record() call opened, shared by the feed that
 *  appends to it and the Recording that stops it. */
struct RecordingSlot;
}  // namespace detail

/** ONE MESSAGE OFF A FEED: the payload, who sent it, and when it came.
 *  A feed stamps its revision and the moment it was received as it
 *  takes it; a message made by hand carries what it was made with. */
struct Message {
  Message() = default;
  /** A message carrying @p payload from @p sender, arrived @p arrivedAt
   *  after its feed opened — how a recording lists one — and numbered
   *  @p revision. */
  explicit Message(std::shared_ptr<const Bytes> payload,
                   std::string sender = {},
                   std::chrono::duration<double> arrivedAt = {},
                   uint64_t revision = 0)
      : payload(std::move(payload)),
        m_sender(std::move(sender)),
        m_arrivedAt(arrivedAt),
        m_revision(revision) {}

  /** The bytes as they arrived; never null on a message a feed took. */
  std::shared_ptr<const Bytes> payload;

  /** The address the message came from, spelled the way a URI of that
   *  scheme is, `udp://127.0.0.1:52341`; empty for a recording, and for
   *  a transport that has no way of knowing. */
  const std::string& sender() const { return m_sender; }

  /** When it arrived, counted from when its feed opened; on a replayed
   *  recording, the time the recording carries. This is what a
   *  recording stores. */
  std::chrono::duration<double> arrivedAt() const { return m_arrivedAt; }

  /** When it arrived on the steady clock: a live message's receive
   *  time; a replayed one's recorded spacing laid from the first advance
   *  that moved the recording. The clock's origin on a message no feed
   *  took. */
  std::chrono::steady_clock::time_point receivedAt() const {
    return m_receivedAt;
  }

  /** 1 for the first message on a feed, counting up from there. */
  uint64_t revision() const { return m_revision; }

 private:
  friend class Feed;

  std::string m_sender;
  std::chrono::duration<double> m_arrivedAt{};
  std::chrono::steady_clock::time_point m_receivedAt{};
  uint64_t m_revision = 0;
};

/** WHETHER A FEED'S DOOR STANDS. */
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

/** A FEED'S STATE AS ONE COMPARABLE VALUE: what `Feed::state()` answers,
 *  read out together, so a reader that keeps the value it last showed
 *  describes again exactly when `state() != shown`. */
struct FeedState {
  ReadyState readiness = ReadyState::Connecting;
  /** How many messages have arrived; 0 before the first. */
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

/** WHERE A SEND GOES. */
struct SendOptions {
  /** ONE peer, named the way a message's `sender()` is; empty sends to
   *  the door's own peer, or to every peer a listening door holds. */
  std::string to;
};

/** HOW MUCH A FEED HOLDS FOR ITS READERS. It stands beside the class it
 *  belongs to rather than inside it because a member initializer of a
 *  nested class is not in hand until the class around it closes, and
 *  the constructor below takes this one as its default. */
struct FeedPolicy {
  /** Messages kept for receive() before the oldest is dropped. */
  size_t capacity = 256;
};

/** A FEED BEING WRITTEN DOWN: what `Feed::record()` hands back. Every
 *  message on the feed goes to the file until this handle stops it —
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
  /** A door named @p uri under @p policy. Nothing is opened here: a
   *  hub hands it a transport or a recording, and a test puts messages
   *  on it through `testing::inletOf()`. */
  Feed(std::string uri, FeedPolicy policy = {});
  /** Closes the opened end. */
  ~Feed();

  Feed(const Feed&) = delete;
  Feed& operator=(const Feed&) = delete;

  /** No more messages are taken. What was received stays readable, and
   *  the opened end's close runs once. */
  void close();

  /** THE NEWEST MESSAGE WHOLE: its revision, when it arrived, its
   *  payload and who sent it, read out together so they are one
   *  message's; nothing before the first. It is latched rather than
   *  queued, so draining through receive() leaves it standing. */
  std::optional<Message> latest() const;

  /** The next message this reader has not taken, in order; nothing when
   *  none is waiting. Never waits for one. */
  std::optional<Message> receive();

  /** WHERE THE DOOR STANDS, in one value read out together: whether it
   *  is open, how many messages have come and how many fell off the
   *  front, the local end it bound and what went wrong. Two reads
   *  compare equal exactly when nothing a reader shows has moved. */
  FeedState state() const;

  /** The URI this feed was opened on, as it was written. */
  const std::string& uri() const { return m_uri; }

  /** Sends @p payload back through the opened end — to the door's own
   *  peer, or, with `.to`, to ONE sender named the way a message's
   *  `sender()` is, `udp://127.0.0.1:52341`, which is how a door that
   *  holds no peer of its own answers the one that wrote to it. False
   *  when the way is one-way for that purpose, when the feed is closed,
   *  and when no transport opened it. */
  bool send(const Bytes& payload, const SendOptions& options = {}) const;

  /** THE PEERS ATTACHED NOW, each named the way a message's `sender()`
   *  spells it, which is how a door that holds many learns that one has
   *  left: the name is gone from here. Empty when the feed is closed,
   *  when no transport opened it, and when the transport holds no peers
   *  of its own — a datagram socket has senders and never attached
   *  peers. */
  std::vector<std::string> peers() const;

  /** Appends every message from now on to the file at @p path in the
   *  recording format, so this feed can be played back later through
   *  `Hub::replay()`, until the handle this returns stops it. A feed
   *  writes one recording at a time: a second call stops the first.
   *  @trap A file that cannot be opened hands back a handle that has
   *  already stopped, and `state().error` says why. */
  [[nodiscard]] Recording record(std::filesystem::path path);

 private:
  friend class Inlet;
  friend class Hub;

  /** Takes one message, stamped with the seconds since the feed was
   *  made and naming the address it came from, or nobody. Any thread. */
  void deliver(Bytes payload, std::string sender);

  /** The same, with a recording's own time instead of the clock's, and
   *  no sender. */
  void deliver(Bytes payload, std::chrono::duration<double> arrivedAt);

  /** Says what went wrong, which state() answers from then on; an empty
   *  reason takes off what stood there. The feed stays open. */
  void fail(std::string why);

  /** Takes the end its transport opened. Once: a second end, and one
   *  handed to a feed that is already closed, is closed rather than
   *  kept. An end that stands is kept with whatever the transport has
   *  said about it by then.
   *  @trap AN END WITH NOTHING IN IT, HANDED TO A FEED CARRYING A
   *  REASON, IS NO END: the feed stays unopened with that reason
   *  standing, so the next ask for its URI opens it again into this
   *  same feed. */
  void open(OpenedFeed end);

  /** Reads @p recording instead of a transport: advance() is then what
   *  delivers, and the recording is the door this feed opened. */
  void replay(std::vector<Message> recording);

  /** Moves a replayed recording's time to @p seconds on the caller's
   *  clock. The first call fixes the origin, so a recording starts when
   *  its feed is first advanced; every recorded message due by then is
   *  delivered in order with its recorded time, and the feed closes
   *  after the last one. A live feed ignores it. */
  void advance(double seconds);

  /** Stamps and queues one message with the lock already held. Every
   *  path that takes a message — the transport's and the recording's —
   *  runs through here, so a revision is never handed out twice and
   *  the file a recording writes carries the feed's own order. */
  void deliverLocked(std::shared_ptr<const Bytes> payload,
                     std::chrono::duration<double> arrivedAt,
                     std::chrono::steady_clock::time_point receivedAt,
                     std::string sender);

  /** Lays @p arrivedAt onto the steady clock: from the first advance
   *  that moved a replayed recording, else from when the feed was made.
   *  Negative, nonfinite or unrepresentable times map to that origin. */
  std::chrono::steady_clock::time_point receivedAtLocked(
      std::chrono::duration<double> arrivedAt) const;

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
   *  queue a message and for the append that records one, never across
   *  a call into the transport: a reader waits for another reader, and
   *  never for a socket. */
  mutable std::mutex m_mutex;
  std::deque<Message> m_messages;
  /** The last message, kept whole: who sent the newest message and
   *  which one it is are readable without draining the queue another
   *  reader is taking messages off. */
  std::optional<Message> m_latest;
  uint64_t m_revision = 0;
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
  std::vector<Message> m_recording;
  size_t m_replayed = 0;
  bool m_replaying = false;
  std::optional<double> m_origin;
  std::optional<std::chrono::steady_clock::time_point> m_replayOrigin;
};

}  // namespace sigil::io
