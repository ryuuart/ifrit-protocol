/** @file
 * The feed: the conduit a transport delivers into and readers drain,
 * the end a transport opened and the one close that runs on it, the
 * file every message can be copied into, and the recording a feed plays
 * back instead of listening.
 */

#include "sigilio/hub/Feed.h"

#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

#include "sigilio/hub/Recording.h"

namespace sigil::io {

namespace detail {
/** The writer one record() call opened. The feed appends under its own
 *  lock and then this one; a Recording stops it under this one alone,
 *  so stopping never waits on the feed and the two are never taken the
 *  other way round. */
struct RecordingSlot {
  std::mutex mutex;
  std::unique_ptr<RecordingWriter> writer;

  /** Closes the file; nothing more is written through this slot. */
  void stop() {
    const std::lock_guard lock(mutex);
    writer.reset();
  }
};
}  // namespace detail

Recording::Recording(std::shared_ptr<detail::RecordingSlot> slot)
    : m_slot(std::move(slot)) {}

Recording::~Recording() { stop(); }

Recording::Recording(Recording&& other) noexcept
    : m_slot(std::move(other.m_slot)) {}

Recording& Recording::operator=(Recording&& other) noexcept {
  if (this != &other) {
    stop();
    m_slot = std::move(other.m_slot);
  }
  return *this;
}

void Recording::stop() {
  if (m_slot) m_slot->stop();
  m_slot.reset();
}

bool Recording::stopped() const {
  if (!m_slot) return true;
  const std::lock_guard lock(m_slot->mutex);
  return m_slot->writer == nullptr;
}

Feed::Feed(std::string uri, FeedPolicy policy)
    : m_uri(std::move(uri)), m_policy(policy) {}

Feed::~Feed() { close(); }

void Feed::deliverLocked(std::shared_ptr<const Bytes> payload,
                         std::chrono::duration<double> arrivedAt,
                         std::chrono::steady_clock::time_point receivedAt,
                         std::string sender) {
  if (m_closed) return;
  // Every message carries a payload, empty ones included, so a reader of
  // one never meets a message with nothing where its bytes should be.
  if (!payload) payload = std::make_shared<const Bytes>();
  Message message(std::move(payload), std::move(sender), arrivedAt,
                  ++m_revision);
  message.m_receivedAt = receivedAt;
  // The whole message is latched under the same lock that stamped it,
  // so a reader asking what the newest message is and who sent it is
  // answered one message.
  m_latest = message;
  // The frame is written under the lock that stamped the message, so a
  // recording lists messages in the order the feed took them however
  // many threads are delivering. A file that stops taking frames ends
  // the recording and says so through its state's error: a recording that went
  // quiet without a word would replay as a run that stopped early.
  if (m_recorder) {
    const std::lock_guard recording(m_recorder->mutex);
    if (!m_recorder->writer) {
      // Its handle stopped it: nothing more goes to that file.
    } else if (!m_recorder->writer->append(message)) {
      m_recorder->writer.reset();
      m_error = "the recording stopped: its file could not take a frame";
    }
  }
  m_messages.push_back(std::move(message));
  // A reader that cannot keep up loses the OLDEST messages: the newest
  // is what a frame draws, and latest() has it whatever the queue does.
  while (m_messages.size() > m_policy.capacity) {
    m_messages.pop_front();
    ++m_dropped;
  }
}

void Feed::deliver(Bytes payload, std::string sender) {
  const auto now = std::chrono::steady_clock::now();
  // Shared before the lock: copying a message is the delivering
  // thread's own work, not something the next reader waits behind.
  auto shared = std::make_shared<const Bytes>(std::move(payload));
  const std::lock_guard lock(m_mutex);
  deliverLocked(std::move(shared), now - m_made, now, std::move(sender));
}

void Feed::deliver(Bytes payload, std::chrono::duration<double> arrivedAt) {
  auto shared = std::make_shared<const Bytes>(std::move(payload));
  const std::lock_guard lock(m_mutex);
  // A message carrying its own time carries no sender: it is a
  // recording's frame, and a recording is the messages and not who sent
  // them.
  deliverLocked(std::move(shared), arrivedAt, receivedAtLocked(arrivedAt),
                std::string());
}

void Feed::fail(std::string why) {
  const std::lock_guard lock(m_mutex);
  m_error = std::move(why);
}

std::function<void()> Feed::closeLocked() {
  m_closed = true;
  if (m_recorder) m_recorder->stop();
  m_recorder.reset();
  std::function<void()> ending = std::move(m_openedEnd.close);
  m_openedEnd.close = nullptr;
  return ending;
}

void Feed::close() {
  std::function<void()> ending;
  {
    const std::lock_guard lock(m_mutex);
    if (m_closed) return;
    ending = closeLocked();
  }
  if (ending) ending();
}

void Feed::open(OpenedFeed opened) {
  std::function<void()> unwanted;
  {
    const std::lock_guard lock(m_mutex);
    if (!m_wasOpened && !m_closed) {
      // AN END WITH NOTHING IN IT, HANDED TO A FEED CARRYING A REASON,
      // IS NO END: a transport that could not open the URI left the
      // reason here and has nothing to give back. The feed stays
      // unopened with that reason standing, which is what tells the
      // next ask for this URI to open it again, and there is nothing
      // to close.
      const bool nothing = !opened.close && !opened.send && !opened.sendTo &&
                           opened.localAddress.empty();
      if (nothing && !m_error.empty()) return;
      m_wasOpened = true;
      m_openedEnd = std::move(opened);
      // What a door says about itself as it opens is left standing: a
      // transport that answered from one thread and failed on another
      // before this ran is a door with a reason, and the reason an
      // earlier ask left was taken off before this open began.
      return;
    }
    // A feed takes one end. A second one, and one handed to a feed that
    // is already closed, is closed here instead of kept: otherwise a
    // door nobody can read through stays open.
    unwanted = std::move(opened.close);
  }
  if (unwanted) unwanted();
}

std::optional<Message> Feed::latest() const {
  const std::lock_guard lock(m_mutex);
  return m_latest;
}

std::optional<Message> Feed::receive() {
  const std::lock_guard lock(m_mutex);
  if (m_messages.empty()) return std::nullopt;
  Message message = std::move(m_messages.front());
  m_messages.pop_front();
  return message;
}

FeedState Feed::state() const {
  const std::lock_guard lock(m_mutex);
  FeedState state;
  state.readiness = m_closed       ? ReadyState::Closed
                    : m_wasOpened ? ReadyState::Open
                                  : ReadyState::Connecting;
  state.revision = m_revision;
  state.dropped = m_dropped;
  state.localAddress = m_openedEnd.localAddress;
  state.error = m_error;
  return state;
}

bool Feed::send(const Bytes& payload, const SendOptions& options) const {
  std::function<bool(const Bytes&)> outward;
  std::function<bool(std::string_view, const Bytes&)> addressed;
  {
    const std::lock_guard lock(m_mutex);
    if (m_closed) return false;
    if (options.to.empty())
      outward = m_openedEnd.send;
    else
      addressed = m_openedEnd.sendTo;
  }
  // Outside the lock: a transport that waits on a socket must not stop
  // a reader from draining what has already arrived.
  if (!options.to.empty()) return addressed ? addressed(options.to, payload) : false;
  return outward ? outward(payload) : false;
}

std::vector<std::string> Feed::peers() const {
  std::function<std::vector<std::string>()> attached;
  {
    const std::lock_guard lock(m_mutex);
    if (m_closed) return {};
    attached = m_openedEnd.peers;
  }
  // Outside the lock, as a send is: the transport answers from its own
  // thread's list, and a reader never waits for a socket.
  return attached ? attached() : std::vector<std::string>{};
}

Recording Feed::record(std::filesystem::path path) {
  // The file is opened, and emptied, before the lock is taken: a reader
  // never waits on a disk.
  auto slot = std::make_shared<detail::RecordingSlot>();
  slot->writer = std::make_unique<RecordingWriter>(path);
  std::shared_ptr<detail::RecordingSlot> previous;
  {
    const std::lock_guard lock(m_mutex);
    if (!slot->writer->good()) {
      m_error = "cannot write a feed recording at " + path.string();
      return Recording();
    }
    if (m_closed) return Recording();
    previous = std::exchange(m_recorder, slot);
  }
  // One recording at a time: the one this replaces ends here, and its
  // handle reads as stopped from now on.
  if (previous) previous->stop();
  return Recording(std::move(slot));
}

void Feed::replay(std::vector<Message> recording) {
  const std::lock_guard lock(m_mutex);
  m_recording = std::move(recording);
  m_replayed = 0;
  m_replaying = true;
  m_origin.reset();
  m_replayOrigin.reset();
  // The recording IS this feed's door: a later ask for the same URI is
  // handed the feed as it stands rather than reading the file again
  // over a playback that is already running.
  m_wasOpened = true;
}

void Feed::advance(std::chrono::duration<double> time) {
  std::function<void()> ending;
  {
    const std::lock_guard lock(m_mutex);
    if (!m_replaying || m_closed) return;
    // The first advance is the origin: a recording starts when its feed
    // is first moved forward, whatever the caller's clock reads then,
    // so a feed opened in the middle of a run still plays from its
    // first frame.
    if (!m_origin) {
      m_origin = time;
      m_replayOrigin = std::chrono::steady_clock::now();
    }
    const std::chrono::duration<double> elapsed = time - *m_origin;
    while (m_replayed != m_recording.size() &&
           m_recording[m_replayed].arrivedAt() <= elapsed) {
      const Message& recorded = m_recording[m_replayed];
      ++m_replayed;
      deliverLocked(recorded.payload, recorded.arrivedAt(),
                    receivedAtLocked(recorded.arrivedAt()), std::string());
    }
    if (m_replayed != m_recording.size()) return;
    // Nothing else is coming: a reader that watches its state learns
    // that the recording ran out rather than waiting on a door that
    // will never open again.
    ending = closeLocked();
  }
  if (ending) ending();
}

std::chrono::steady_clock::time_point Feed::receivedAtLocked(
    std::chrono::duration<double> arrivedAt) const {
  using Clock = std::chrono::steady_clock;
  using Duration = Clock::duration;
  const auto origin = m_replayOrigin.value_or(m_made);
  const double ticks =
      std::chrono::duration<double, Duration::period>(arrivedAt).count();
  if (!std::isfinite(ticks) || ticks < 0 ||
      ticks >= static_cast<double>(std::numeric_limits<Duration::rep>::max()))
    return origin;
  const Duration offset(static_cast<Duration::rep>(ticks));
  if (origin > Clock::time_point::max() - offset) return origin;
  return origin + offset;
}

}  // namespace sigil::io
