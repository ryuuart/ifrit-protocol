/** @file
 * The feed: the conduit a transport delivers into and readers drain,
 * the end a transport opened and the one close that runs on it, the
 * file every arrival can be copied into, and the recording a feed plays
 * back instead of listening.
 */

#include "sigilio/hub/Feed.h"

#include <utility>

#include "sigilio/hub/Recording.h"

namespace sigil::io {

Feed::Feed(std::string uri, Policy policy)
    : m_uri(std::move(uri)), m_policy(policy) {}

Feed::~Feed() { close(); }

void Feed::deliverLocked(std::shared_ptr<const Bytes> bytes, double at,
                         std::string from) {
  if (m_closed) return;
  // Every arrival carries bytes, empty ones included: latest() says
  // that nothing has arrived by being null, and an arrival that left it
  // null would say the opposite of what happened.
  if (!bytes) bytes = std::make_shared<const Bytes>();
  Arrival arrival;
  arrival.generation = ++m_generation;
  arrival.at = at;
  arrival.bytes = std::move(bytes);
  arrival.from = std::move(from);
  m_latest = arrival.bytes;
  // The whole arrival is latched beside its bytes, under the same lock
  // that stamped it, so a reader asking what the newest message is and
  // a reader asking who sent it are answered the same message.
  m_newest = arrival;
  // The frame is written under the lock that stamped the arrival, so a
  // recording lists messages in the order the feed took them however
  // many threads are delivering. A file that stops taking frames ends
  // the recording and says so through error(): a recording that went
  // quiet without a word would replay as a run that stopped early.
  if (m_writer && !m_writer->append(arrival)) {
    m_writer.reset();
    m_error = "the recording stopped: its file could not take a frame";
  }
  m_arrivals.push_back(std::move(arrival));
  // A reader that cannot keep up loses the OLDEST messages: the newest
  // is what a frame draws, and latest() has it whatever the queue does.
  while (m_arrivals.size() > m_policy.capacity) {
    m_arrivals.pop_front();
    ++m_dropped;
  }
}

void Feed::deliver(Bytes bytes) { deliver(std::move(bytes), std::string()); }

void Feed::deliver(Bytes bytes, std::string from) {
  const double at =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - m_made)
          .count();
  // Shared before the lock: copying a message is the delivering
  // thread's own work, not something the next reader waits behind.
  auto shared = std::make_shared<const Bytes>(std::move(bytes));
  const std::lock_guard lock(m_mutex);
  deliverLocked(std::move(shared), at, std::move(from));
}

void Feed::deliver(Bytes bytes, double at) {
  auto shared = std::make_shared<const Bytes>(std::move(bytes));
  const std::lock_guard lock(m_mutex);
  // A message carrying its own time carries no sender: it is a
  // recording's frame, and a recording is the messages and not who sent
  // them.
  deliverLocked(std::move(shared), at, std::string());
}

void Feed::fail(std::string why) {
  const std::lock_guard lock(m_mutex);
  m_error = std::move(why);
}

std::function<void()> Feed::closeLocked() {
  m_closed = true;
  m_writer.reset();
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

void Feed::opened(OpenedFeed opened) {
  std::function<void()> unwanted;
  {
    const std::lock_guard lock(m_mutex);
    if (!m_wasOpened && !m_closed) {
      m_wasOpened = true;
      m_openedEnd = std::move(opened);
      return;
    }
    // A feed takes one end. A second one, and one handed to a feed that
    // is already closed, is closed here instead of kept: otherwise a
    // door nobody can read through stays open.
    unwanted = std::move(opened.close);
  }
  if (unwanted) unwanted();
}

std::shared_ptr<const Bytes> Feed::latest() const {
  const std::lock_guard lock(m_mutex);
  return m_latest;
}

std::optional<Arrival> Feed::newest() const {
  const std::lock_guard lock(m_mutex);
  return m_newest;
}

uint64_t Feed::generation() const {
  const std::lock_guard lock(m_mutex);
  return m_generation;
}

std::optional<Arrival> Feed::receive() {
  const std::lock_guard lock(m_mutex);
  if (m_arrivals.empty()) return std::nullopt;
  Arrival arrival = std::move(m_arrivals.front());
  m_arrivals.pop_front();
  return arrival;
}

uint64_t Feed::dropped() const {
  const std::lock_guard lock(m_mutex);
  return m_dropped;
}

bool Feed::closed() const {
  const std::lock_guard lock(m_mutex);
  return m_closed;
}

std::string Feed::error() const {
  const std::lock_guard lock(m_mutex);
  return m_error;
}

std::string Feed::address() const {
  const std::lock_guard lock(m_mutex);
  return m_openedEnd.address;
}

bool Feed::send(const Bytes& bytes) const {
  std::function<bool(const Bytes&)> outward;
  {
    const std::lock_guard lock(m_mutex);
    if (m_closed) return false;
    outward = m_openedEnd.send;
  }
  // Outside the lock: a transport that waits on a socket must not stop
  // a reader from draining what has already arrived.
  return outward ? outward(bytes) : false;
}

void Feed::record(std::filesystem::path path) {
  // The file is opened, and emptied, before the lock is taken: a reader
  // never waits on a disk.
  std::unique_ptr<RecordingWriter> writer;
  if (!path.empty()) writer = std::make_unique<RecordingWriter>(path);
  const std::lock_guard lock(m_mutex);
  if (writer && !writer->good()) {
    m_error = "cannot write a feed recording at " + path.string();
    writer.reset();
  }
  m_writer = std::move(writer);
}

void Feed::replay(std::vector<Arrival> recording) {
  const std::lock_guard lock(m_mutex);
  m_recording = std::move(recording);
  m_replayed = 0;
  m_replaying = true;
  m_origin.reset();
}

void Feed::advance(double seconds) {
  std::function<void()> ending;
  {
    const std::lock_guard lock(m_mutex);
    if (!m_replaying || m_closed) return;
    // The first advance is the origin: a recording starts when its feed
    // is first moved forward, whatever the caller's clock reads then,
    // so a feed opened in the middle of a run still plays from its
    // first frame.
    if (!m_origin) m_origin = seconds;
    const double elapsed = seconds - *m_origin;
    while (m_replayed != m_recording.size() &&
           m_recording[m_replayed].at <= elapsed) {
      const Arrival& recorded = m_recording[m_replayed];
      ++m_replayed;
      deliverLocked(recorded.bytes, recorded.at, std::string());
    }
    if (m_replayed != m_recording.size()) return;
    // Nothing else is coming: a reader that watches closed() learns
    // that the recording ran out rather than waiting on a door that
    // will never open again.
    ending = closeLocked();
  }
  if (ending) ending();
}

}  // namespace sigil::io
