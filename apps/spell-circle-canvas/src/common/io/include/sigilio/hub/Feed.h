#pragma once

/** @file
 * @ingroup io-hub
 * A FEED: a resource that keeps arriving. One door, keyed by URI, with a
 * transport on one side and readers on the other; this is the readers'
 * side, a copyable handle, and the transport's is the `Inlet` it is
 * handed. A reader on any thread either takes the newest message whole
 * through latest() or drains in order the ones it has not seen through
 * receive(), and neither ever waits for one. A feed holds the last
 * `ListenOptions::capacity` messages for receive(), and its state's
 * `dropped` counts what fell off the front. The same door plays a
 * recording back, and writes one for as long as the Recording its
 * record() hands out lives. */

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilio/advanced/Transport.h"
#include "sigilio/source/State.h"
#include "sigilio/source/Source.h"

namespace sigil::io {

class Feed;
class RecordingWriter;

namespace detail {
/** The writer one record() call opened, shared by the feed that
 *  appends to it and the Recording that stops it. */
struct RecordingSlot;
/** The door every handle onto one feed shares. */
class FeedDoor;
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
  friend class detail::FeedDoor;

  std::string m_sender;
  std::chrono::duration<double> m_arrivedAt{};
  std::chrono::steady_clock::time_point m_receivedAt{};
  uint64_t m_revision = 0;
};

/** WHERE A SEND GOES. */
struct SendOptions {
  /** ONE peer, named the way a message's `sender()` is; empty sends to
   *  the door's own peer, or to every peer a listening door holds. */
  std::string to;
};

/** HOW A FEED IS OPENED: `hub.listen(uri, {.capacity = 64})`. It stands
 *  beside the class it configures rather than inside it because a member
 *  initializer of a nested class is not in hand until the class around
 *  it closes, and the constructor below takes this one as its default. */
struct ListenOptions {
  /** Messages kept for receive() before the oldest is dropped. */
  size_t capacity = 256;
  /** Where send() goes from a listening end that holds no peer of its
   *  own, named the way a message's `sender()` is; empty sends nowhere
   *  but through the door's own peer. */
  std::string peer;
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
  friend class detail::FeedDoor;
  explicit Recording(std::shared_ptr<detail::RecordingSlot> slot);

  std::shared_ptr<detail::RecordingSlot> m_slot;
};

/** A FEED: a copyable handle onto one door, keyed by URI. Every copy
 *  reads the same door — `auto sky = hub.listen(uri); sky.latest();` —
 *  and the door closes for every holder when close() is called, or when
 *  the last handle onto it goes. A handle made empty reads nothing: no
 *  message, a closed state, and every send false. */
class Feed {
 public:
  /** A handle onto nothing. */
  Feed() = default;

  /** A door of its own named @p uri under @p options, opened by nobody:
   *  a test puts messages on it through `testing::inletOf()`. A hub
   *  hands out the feeds it opens through `Hub::listen()`. */
  explicit Feed(std::string uri, ListenOptions options = {});

  /** No more messages are taken, for every holder. What was received
   *  stays readable, and the opened end's close runs once. */
  void close() const;

  /** THE NEWEST MESSAGE WHOLE: its revision, when it arrived, its
   *  payload and who sent it, read out together so they are one
   *  message's; nothing before the first. It is latched rather than
   *  queued, so draining through receive() leaves it standing. */
  std::optional<Message> latest() const;

  /** The next message not yet taken through any handle onto this door,
   *  in order; nothing when none is waiting. Never waits for one. */
  std::optional<Message> receive() const;

  /** WHERE THE DOOR STANDS, in one value read out together: whether it
   *  is open, how many messages have come and how many fell off the
   *  front, the local end it bound and what went wrong. Two reads
   *  compare equal exactly when nothing a reader shows has moved. */
  FeedState state() const;

  /** The URI this feed was opened on, as it was written; empty on a
   *  handle onto nothing. */
  const std::string& uri() const;

  /** Sends @p payload back through the opened end — to the door's own
   *  peer, or to the `ListenOptions::peer` it was opened with, or, with
   *  `.to`, to ONE sender named the way a message's `sender()` is,
   *  `udp://127.0.0.1:52341`, which is how a door that holds no peer of
   *  its own answers the one that wrote to it. False when the way is
   *  one-way for that purpose, when the feed is closed, and when no
   *  transport opened it. */
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
  [[nodiscard]] Recording record(std::filesystem::path path) const;

  /** Whether this handle holds a door. */
  explicit operator bool() const { return m_door != nullptr; }

  /** Whether two handles hold the same door. */
  bool operator==(const Feed& other) const { return m_door == other.m_door; }

 private:
  friend class Hub;
  friend class Inlet;
  friend Inlet testing::inletOf(const Feed& feed);
  friend struct std::hash<Feed>;
  explicit Feed(std::shared_ptr<detail::FeedDoor> door)
      : m_door(std::move(door)) {}

  std::shared_ptr<detail::FeedDoor> m_door;
};

}  // namespace sigil::io

/** Handles onto one door hash alike, so a feed keys a map. */
template <>
struct std::hash<sigil::io::Feed> {
  size_t operator()(const sigil::io::Feed& feed) const noexcept {
    return std::hash<const void*>()(feed.m_door.get());
  }
};
