#pragma once

/** @file
 * The door behind every `Feed` handle: the queue a transport delivers
 * into and readers drain, the latched newest message, the end a
 * transport opened, the file a recording is written to and the
 * recording played back instead of a transport. Private to the hub
 * feature, so a reader's header carries none of it.
 */

#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "sigilio/hub/Feed.h"

namespace sigil::io::detail {

/** THE DOOR EVERY HANDLE ONTO ONE FEED SHARES: bytes in from a
 *  transport or a recording, messages out to readers on any thread. The
 *  last handle to let go of it closes it. */
class FeedDoor {
 public:
  /** A door named @p uri under @p options. Nothing is opened here: a
   *  hub hands it a transport or a recording, and a test puts messages
   *  on it through `testing::inletOf()`. */
  FeedDoor(std::string uri, ListenOptions options);
  /** Closes the opened end. */
  ~FeedDoor();

  FeedDoor(const FeedDoor&) = delete;
  FeedDoor& operator=(const FeedDoor&) = delete;

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
  void open(TransportEnd end);

  /** Reads @p recording instead of a transport: advance() is then what
   *  delivers, and the recording is the door this feed opened. */
  void replay(std::vector<Message> recording);

  /** Moves a replayed recording's time to @p time on the caller's
   *  clock. The first call fixes the origin, so a recording starts when
   *  its feed is first advanced; every recorded message due by then is
   *  delivered in order with its recorded time, and the feed closes
   *  after the last one. A live feed ignores it. */
  void advance(std::chrono::duration<double> time);

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
  const ListenOptions m_options;
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
  TransportEnd m_openedEnd;
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
  std::optional<std::chrono::duration<double>> m_origin;
  std::optional<std::chrono::steady_clock::time_point> m_replayOrigin;
};

}  // namespace sigil::io::detail
