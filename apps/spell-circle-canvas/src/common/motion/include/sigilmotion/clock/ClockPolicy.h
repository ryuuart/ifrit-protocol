#pragma once

/** @file
 * @ingroup motion-clock
 *
 * Who moves a clock: the wall, a caller's stated steps, or nobody — and
 * the budget of clock time after which a caller is told it has run.
 */

#include <sigilmotion/clock/FrameClock.h>

#include <cstdint>
#include <optional>

namespace sigil::motion {

/** WHO MOVES THE CLOCK. The one choice a run is made repeatable by: a
 *  host that draws under anything but `Wall` draws frames whose time is
 *  a function of what it was told, never of how fast the machine ran. */
enum class ClockPolicy : uint8_t {
  /** The wall clock: a frame moves by the time that passed since the
   *  last, paused and time-scaled. What a person watching a window
   *  sees. */
  Wall,
  /** The caller's clock: a frame the host draws on its own moves
   *  nothing, and only a stated step does. */
  Advance,
  /** Nobody's: no frame moves, a step included. */
  Pause,
  /** The wall clock, except that a frame drawn while something the run
   *  asked for is still arriving moves nothing. */
  PauseWhileLoading,
};

/** A FRAME CLOCK UNDER A POLICY, and the budget set with it.
 *
 *  Two kinds of frame reach it. One the host draws on its own — a
 *  window's vsync, a server's loop — is `frame()`, and moves by the wall
 *  under `Wall`, by the wall unless something is arriving under
 *  `PauseWhileLoading`, and not at all otherwise. One a caller states is
 *  `step()`, and moves by exactly the step under `Advance` — no time
 *  scale and no stall clamp, since the caller chose it — and not at all
 *  otherwise. A frame that moves nothing still consumes its wall
 *  reading, so a return to `Wall` measures from there rather than
 *  catching up on the stretch it stood still for.
 *
 *  `setHeld()` is the pause a person presses and `setTimeScale()` the
 *  speed they pick: both leave the policy alone, the first holding every
 *  frame and step, the second scaling the wall's frames only.
 *
 *  A BUDGET is clock seconds from the moment it was set; the frame on
 *  which the clock reaches it answers `budgetExpired()` once, and the
 *  budget is gone. A clock that does not move never spends one, so a
 *  budget under `Pause` never runs out.
 *
 *  The clock reads a wall reading only where it is handed one or asked
 *  to take one, so a test states every reading and is repeatable. */
class PolicyClock {
 public:
  explicit PolicyClock(FrameClockOptions options = {}) : m_wall(options) {}

  /** Replaces the policy, from the next frame on, and the budget with
   *  it: @p budgetSeconds of clock time from now, or none. A negative
   *  or non-finite budget is none. */
  void setPolicy(ClockPolicy policy,
                 std::optional<double> budgetSeconds = std::nullopt);

  /** The policy frames move by. */
  [[nodiscard]] ClockPolicy policy() const { return m_policy; }

  /** Whether the wall moves this clock: false under every policy a
   *  repeatable run is drawn under. */
  [[nodiscard]] bool wall() const { return m_policy == ClockPolicy::Wall; }

  /** ONE FRAME THE HOST DRAWS ON ITS OWN, at the wall reading
   *  @p nowSeconds, with @p arriving saying whether something the run
   *  asked for is still on its way. Answers the delta the frame moves
   *  by. */
  double frame(double nowSeconds, bool arriving = false);

  /** …at the steady clock's reading now. */
  double frame(bool arriving = false);

  /** ONE FRAME A CALLER STATES, @p deltaSeconds long: moves by exactly
   *  that under `Advance` while not held, and by nothing otherwise.
   *  Answers the delta the frame moves by; a negative delta moves
   *  nothing. */
  double step(double deltaSeconds);

  /** Holds every frame and every step where they stand, or lets them
   *  go, keeping the policy: the pause a person presses. */
  void setHeld(bool held) { m_wall.setPaused(held); }
  [[nodiscard]] bool held() const { return m_wall.paused(); }

  /** Clock seconds per wall second, for the wall's frames: 1 is real
   *  time. A stated step ignores it. */
  void setTimeScale(double scale) { m_wall.setTimeScale(scale); }
  [[nodiscard]] double timeScale() const { return m_wall.timeScale(); }

  /** Whether a frame now would move nothing whatever it was handed:
   *  held, paused by the policy, or waiting on an arrival under
   *  `PauseWhileLoading` as the last frame found it. */
  [[nodiscard]] bool still() const;

  /** Clock seconds since the clock started, or since it last restarted. */
  [[nodiscard]] double elapsed() const { return m_elapsed; }

  /** Frames taken since the clock started or last restarted, both kinds
   *  and whether or not they moved. */
  [[nodiscard]] uint64_t frames() const { return m_frames; }

  /** Starts the count again at zero seconds and zero frames, keeping the
   *  policy, the hold and whatever budget is left: what a host does as a
   *  new session opens under it. */
  void restart();

  /** Clock seconds of the budget still to run; nothing where none was
   *  set or it has run out. */
  [[nodiscard]] std::optional<double> budgetRemaining() const;

  /** Whether the budget ran out on the frame just taken. True on that
   *  frame alone. */
  [[nodiscard]] bool budgetExpired() const { return m_expired; }

 private:
  /** Counts one frame that moved by @p delta, and spends the budget. */
  double account(double delta);

  FrameClock m_wall;
  ClockPolicy m_policy = ClockPolicy::Wall;
  std::optional<double> m_budgetEnds;  // the elapsed seconds it runs out at
  double m_elapsed = 0.0;
  uint64_t m_frames = 0;
  bool m_expired = false;
  bool m_waiting = false;  // the last frame found something arriving
};

}  // namespace sigil::motion
