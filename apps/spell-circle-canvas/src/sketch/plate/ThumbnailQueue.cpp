/** @file
 * The still queue: one worker, one render in flight, and the asks
 * waiting behind it.
 */

#include <sigilsketch/plate/ThumbnailQueue.h>

#include <algorithm>
#include <utility>

namespace sigil::sketch {

ThumbnailQueue::ThumbnailQueue(Render render, Report report)
    : m_render(std::move(render)),
      m_report(std::move(report)),
      m_worker(&ThumbnailQueue::loop, this) {}

ThumbnailQueue::~ThumbnailQueue() { stop(); }

void ThumbnailQueue::fill(std::vector<int> indices) {
  if (m_abandon.load(std::memory_order_relaxed)) return;
  {
    const std::lock_guard lock(m_mutex);
    for (int index : indices) {
      if (m_failed.count(index) || m_queued.count(index)) continue;
      m_queued.insert(index);
      m_pending.push_back(index);
    }
  }
  m_wake.notify_one();
}

bool ThumbnailQueue::request(int index) {
  if (m_abandon.load(std::memory_order_relaxed)) return false;
  {
    const std::lock_guard lock(m_mutex);
    if (m_failed.count(index) || m_inFlight == index) return false;
    // A ROW ON SCREEN IS WHAT THE ONE RENDER SHOULD BE SPENT ON, so an
    // ask moves the index to the front of the queue rather than adding a
    // second entry for it.
    const auto at = std::find(m_pending.begin(), m_pending.end(), index);
    if (at != m_pending.end()) m_pending.erase(at);
    m_queued.insert(index);
    m_pending.push_front(index);
  }
  m_wake.notify_one();
  return true;
}

void ThumbnailQueue::cancel(int index) {
  const std::lock_guard lock(m_mutex);
  if (m_inFlight == index) return;  // one already rendering finishes
  const auto at = std::find(m_pending.begin(), m_pending.end(), index);
  if (at != m_pending.end()) m_pending.erase(at);
  m_queued.erase(index);
}

void ThumbnailQueue::endFill() {
  m_abandon.store(true, std::memory_order_relaxed);
  const std::lock_guard lock(m_mutex);
  m_pending.clear();
  m_queued.clear();
}

void ThumbnailQueue::stop() {
  // LEAVING IS NOT WAITING FOR A PICTURE. A still is a walk from zero to
  // the sketch's declared moment, so a render in flight can have minutes
  // left in it; a join alone would spend every one of them. The render is
  // let go first and the join then costs one frame of whatever it was
  // walking.
  m_abandon.store(true, std::memory_order_relaxed);
  {
    const std::lock_guard lock(m_mutex);
    m_stop = true;
    m_pending.clear();
    m_queued.clear();
  }
  m_wake.notify_all();
  if (m_worker.joinable()) m_worker.join();
}

int ThumbnailQueue::inFlight() const {
  const std::lock_guard lock(m_mutex);
  return m_inFlight;
}

size_t ThumbnailQueue::pending() const {
  const std::lock_guard lock(m_mutex);
  return m_pending.size();
}

bool ThumbnailQueue::ended() const {
  return m_abandon.load(std::memory_order_relaxed);
}

void ThumbnailQueue::loop() {
  for (;;) {
    int index = -1;
    {
      std::unique_lock lock(m_mutex);
      m_wake.wait(lock, [this] { return m_stop || !m_pending.empty(); });
      if (m_stop) return;
      index = m_pending.front();
      m_pending.pop_front();
      m_inFlight = index;
    }

    const ThumbnailOutcome outcome = m_render(index, m_abandon);

    int remaining = 0;
    {
      const std::lock_guard lock(m_mutex);
      m_inFlight = -1;
      m_queued.erase(index);
      // A render that was let go says nothing about the sketch, so it is
      // not remembered as one that cannot be drawn.
      if (outcome == ThumbnailOutcome::Failed) m_failed.insert(index);
      remaining = (int)m_pending.size();
    }
    if (outcome == ThumbnailOutcome::Stopped) continue;
    if (m_report) m_report(index, outcome, remaining);
  }
}

}  // namespace sigil::sketch
