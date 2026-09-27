#pragma once

/** @file
 * @ingroup motion-clock
 *
 * THE PLAYBACK: the control every animation, timeline and timer answers
 * to — play, pause, seek, reverse, revert — and, in `detail`, the state
 * behind it that an engine steps: where it stands in time, how long a
 * pass is, how many passes, and which way it runs.
 */

#include <sigilmotion/time/Duration.h>

#include <functional>
#include <memory>
#include <vector>

namespace sigil::motion {

namespace detail {
/** ONE THING THE ENGINE STEPS: advanced by a frame's delta in seconds,
 *  answering whether it still runs. */
class Stepped {
 public:
  virtual ~Stepped() = default;
  virtual bool advance(double deltaSeconds) = 0;
  /** Declared to move: what the engine's own `isRunning()` asks. A paused
   *  playback stays on the engine and says no. */
  [[nodiscard]] virtual bool isRunning() const { return true; }
};

/** WHAT AN ENGINE IS STEPPING, in order: its motions, then its timers.
 *  Shared with the playbacks it runs, so one that finished and is played
 *  again goes back on the engine that started it. */
struct Running {
  std::vector<std::shared_ptr<Stepped>> motions;
  std::vector<std::shared_ptr<Stepped>> timers;
};

/** WHAT EVERY PLAYBACK SHARES: where it stands in time, how long a pass
 *  is, how many passes, which way it runs, and whether it is paused,
 *  completed or cancelled. A kind says what showing it at a time means. */
class PlaybackState : public Stepped {
 public:
  Duration delay{};
  Duration pass{};
  int passes = 1;  ///< 0: for ever
  bool alternate = false;
  bool reversed = false;
  bool paused = false;
  bool completed = false;
  bool cancelled = false;
  Duration time{};
  std::function<void()> onComplete;
  /** The engine running it, and whether it is on that engine's list now:
   *  a playback that finished leaves the list, and one played again goes
   *  back on it. */
  std::weak_ptr<Running> home;
  bool listed = false;
  bool timer = false;

  /** Back on the engine that started it, if it left. */
  void relist(const std::shared_ptr<PlaybackState>& self);
  /** The whole length, delay included; the largest duration for ever. */
  [[nodiscard]] Duration total() const;
  bool advance(double deltaSeconds) override;
  /** Moves to @p to, shows it there, and completes at either end. */
  void moveTo(Duration to);
  /** Shows the playback at `time`: before the delay the start; then the
   *  pass it is in, backwards on every other pass when it alternates. */
  void show();
  [[nodiscard]] bool isRunning() const override {
    return !paused && !completed && !cancelled;
  }

  /** Shows the playback @p into its pass; @p waiting while it holds for
   *  its delay. */
  virtual void showAt(Duration into, bool waiting) = 0;
  /** Puts every target back where it was before the playback started. */
  virtual void revertTargets() {}
  /** Called once at completion, before the callback. */
  virtual void finished() {}
};
}  // namespace detail

/** A PLAYBACK — what `animate`, `timeline` and `timer` hand back — and
 *  the control every one of them answers to. A handle: copies control the
 *  same playback, and the engine keeps running it whether or not a handle
 *  is kept. */
class Playback {
 public:
  Playback() = default;

  /** Runs from where it stands; a completed playback starts again. */
  Playback& play();
  /** Holds where it stands; the engine stops advancing it. */
  Playback& pause();
  /** Runs again from where it was paused. */
  Playback& resume();
  /** Back to the start, running. */
  Playback& restart();
  /** Runs the other way from where it stands. */
  Playback& reverse();
  /** Flips direction at the end of every pass from now on. */
  Playback& alternate();
  /** Moves to @p time from its start and shows it there, running every
   *  update up to it. */
  Playback& seek(Duration time);
  /** Jumps to the end: the targets take their final values and it
   *  completes. */
  Playback& complete();
  /** Stops where it stands and leaves the engine; the targets keep the
   *  values they hold. */
  Playback& cancel();
  /** Stops and puts every target back where it was before it started. */
  Playback& revert();
  /** Called once, when it completes. */
  Playback& onComplete(std::function<void()> callback);

  /** Declared to move: started, not paused, not completed. */
  [[nodiscard]] bool isRunning() const;
  [[nodiscard]] bool isPaused() const;
  [[nodiscard]] bool isCompleted() const;
  /** Time from its start, delay included. */
  [[nodiscard]] Duration currentTime() const;
  /** How far through, 0 to 1, over every pass. */
  [[nodiscard]] float progress() const;

  explicit Playback(std::shared_ptr<detail::PlaybackState> state)
      : m_state(std::move(state)) {}
  /** The state behind the handle, for the engine that steps it. */
  [[nodiscard]] const std::shared_ptr<detail::PlaybackState>& state() const {
    return m_state;
  }

 protected:
  std::shared_ptr<detail::PlaybackState> m_state;
};

}  // namespace sigil::motion
