#pragma once

/** @file
 * The one owner of the process's declaration to the system that it is
 * doing user-facing work, and the reasons that keep it declared.
 */

#include <functional>
#include <set>
#include <utility>

namespace ifrit::qt {

/** Why the process says it is doing user-facing work. */
enum class ActivityReason {
  /** A window is offering its frames to other applications, which
   *  watch them whether or not the window is on screen. */
  Publishing,
  /** Some part of a window is on screen, in front of other applications
   *  or behind them. */
  Visible,
};

/** THE HOLD IS TAKEN WHEN THE FIRST REASON STANDS AND RELEASED ONLY WHEN
 *  THE LAST ONE FALLS. Every party that wants the process treated as
 *  user-facing records its reason here, keyed by what it holds for, and
 *  none of them begins or ends the system's activity itself — so a
 *  window going out of sight cannot release a hold that publishing
 *  still needs, and publishing stopping cannot release one a visible
 *  window still needs.
 *
 *  On macOS the declaration is what keeps the system from demoting every
 *  thread of a process it believes is idle in the background, which it
 *  does to an application that is not frontmost however much it draws.
 *  The class itself is platform-free: @p onChange is told `true` when
 *  the hold is taken and `false` when it is released, exactly once per
 *  transition. Not thread-safe; the caller keeps it on one thread. */
class ActivityHold {
 public:
  using Change = std::function<void(bool held)>;

  explicit ActivityHold(Change onChange) : m_onChange(std::move(onChange)) {}

  /** Records whether @p reason stands for @p holder. Recording what is
   *  already recorded changes nothing. */
  void set(const void* holder, ActivityReason reason, bool stands);

  /** Withdraws every reason @p holder recorded, as when it is destroyed. */
  void withdraw(const void* holder);

  /** Whether @p reason stands for @p holder. */
  [[nodiscard]] bool stands(const void* holder, ActivityReason reason) const {
    return m_standing.contains({holder, reason});
  }

  /** Whether any reason stands, which is whether the hold is taken. */
  [[nodiscard]] bool held() const { return !m_standing.empty(); }

 private:
  void announce(bool wasHeld);

  std::set<std::pair<const void*, ActivityReason>> m_standing;
  Change m_onChange;
};

}  // namespace ifrit::qt
