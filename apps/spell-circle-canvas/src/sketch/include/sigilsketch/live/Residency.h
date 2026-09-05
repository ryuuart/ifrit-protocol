#pragma once

/** @file
 * The last few sketches opened, held alive and paused, so that coming
 * back to one is not opening it again.
 */

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sigil::sketch {

class Host;

/** HOW MANY SESSIONS STAY RESIDENT.
 *
 *  Switching sketches keeps the ones already opened alive rather than
 *  throwing them away: setup runs once per sketch instead of once per
 *  visit, and the rolling frame windows survive a look at something
 *  else, so the readout on return is the sketch's own numbers and not a
 *  ring filling from zero.
 *
 *  Three is what a comparison needs — the one being changed, the one it
 *  is being read against, and the one to come back to. Each resident
 *  session holds its whole retained scene and every image and pipeline
 *  it made, so the count is a memory budget as much as a convenience,
 *  which is why it is one small number and not a policy. */
inline constexpr std::size_t kResidentSessions = 3;

/** THE RESIDENT SET: sessions opened, one of them presented.
 *
 *  A key names a session, and the file it was opened from is the natural
 *  one. Presenting a key the set does not hold opens it; presenting one
 *  it does hold hands back what is already running, and what leaves when
 *  the set is full is the one presented longest ago.
 *
 *  NOTHING HERE RELOADS. A host rebuilds itself when the file under it
 *  changes and that rebuild restarts its own session from zero, which is
 *  exactly the behaviour an edit wants; this set only decides which
 *  session the window is looking at. A session nothing is looking at is
 *  not driven at all — it is not polled and its clock does not advance —
 *  so it stands where it was left. */
class Residency {
 public:
  /** Builds the session for a key the set does not hold. */
  using Open = std::function<std::unique_ptr<Host>()>;

  /** Non-throwing so that a window can hold one for the life of the
   *  process: it starts empty, and everything it owns arrives later. */
  explicit Residency(std::size_t capacity = kResidentSessions) noexcept;
  ~Residency();
  Residency(const Residency&) = delete;
  Residency& operator=(const Residency&) = delete;

  struct Presented {
    Host* host = nullptr;
    /** True when this call built it, false when it was already resident.
     *  A caller holding per-session state of its own starts that state
     *  over on the first and keeps it on the second. */
    bool opened = false;
  };

  /** Presents the session for @p key, opening one through @p open when
   *  none is resident and evicting the least recently presented once
   *  the set is over capacity. */
  Presented present(const std::string& key, const Open& open);

  /** The session being presented; null before the first present and
   *  after clear(). */
  [[nodiscard]] Host* presented() const;

  /** DROPS THE PRESENTED SESSION and keeps the rest of the set warm. What
   *  a window does when the frame the presented session drew failed: that
   *  one host's failure is not the others', so it goes and they stay,
   *  ready to be presented again without a rebuild. Does nothing when the
   *  set is empty. */
  void dropPresented();

  [[nodiscard]] std::size_t size() const { return m_sessions.size(); }
  [[nodiscard]] std::size_t capacity() const { return m_capacity; }
  /** The keys held, most recently presented first. */
  [[nodiscard]] std::vector<std::string> keys() const;

  /** Releases every session. A host owning device-backed images has to
   *  go while the device that made them is still up, so the owner says
   *  when rather than leaving it to its own destruction. */
  void clear();

 private:
  struct Resident {
    std::string key;
    std::unique_ptr<Host> host;
  };

  std::size_t m_capacity;
  /** Most recently presented first, which makes the least recently
   *  presented the back and eviction a pop. */
  std::vector<Resident> m_sessions;
};

}  // namespace sigil::sketch
