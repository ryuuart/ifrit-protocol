#pragma once

/** @file
 * @ingroup motion-clock
 *
 * THE ENGINE: the one clock animations, timelines and timers run on —
 * `engine.animate(target, tween)`, `engine.timeline()`, `engine.timer()`
 * — the playback every one of them answers to, and, for the host that
 * owns the engine, the frame that moves it: the wall clock or a stated
 * step, under a policy and a budget.
 */

#include <sigilcore/callable/Callable.h>
#include <sigilmotion/advanced/ClockPolicy.h>
#include <sigilmotion/time/Duration.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Tween.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::motion {

class Engine;

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

class PlaybackState;
class TimelineState;
class TimerState;

/** The motion `engine.animate` starts on one live value: the tween
 *  resolved to a path from where the value stands, written into its cell
 *  on every step. Built by `Engine::animate` and `Timeline::add`. */
std::shared_ptr<PlaybackState> animationOf(
    std::shared_ptr<Cell<float>> cell, const Tween<float>& tween);
}  // namespace detail

/** WHERE A TIMELINE ITEM STARTS, relative to what is already on it. */
struct Position {
  enum class Kind : uint8_t { At, AfterEnd, AfterPrevious, WithPrevious, AtLabel };
  Kind kind = Kind::AfterEnd;
  Duration offset{};
  std::string label;
};
/** At @p time from the timeline's start. */
inline Position at(Duration time) { return {Position::Kind::At, time, {}}; }
/** After everything on the timeline has ended, plus @p offset — where an
 *  item goes when no position is named. */
inline Position afterEnd(Duration offset = {}) {
  return {Position::Kind::AfterEnd, offset, {}};
}
/** When the item added just before ends, plus @p offset. */
inline Position afterPrevious(Duration offset = {}) {
  return {Position::Kind::AfterPrevious, offset, {}};
}
/** When the item added just before starts, plus @p offset. */
inline Position withPrevious(Duration offset = {}) {
  return {Position::Kind::WithPrevious, offset, {}};
}
/** At the label @p name, plus @p offset. */
inline Position atLabel(std::string name, Duration offset = {}) {
  return {Position::Kind::AtLabel, offset, std::move(name)};
}

/** HOW A TIMER RUNS. */
struct TimerOptions {
  /** Above zero: EXACTLY this many updates per second, counted from total
   *  time, whatever the host draws at — a simulation's fixed step. */
  double stepRate = 0.0;
  /** Above zero: AT MOST this many updates per second, the frames between
   *  skipped — a throttle, anime.js's `frameRate`. */
  double frameRate = 0.0;
  /** The most fixed steps one frame may run before it drops time; a hitch
   *  longer than that runs slow for one frame rather than spiralling. */
  int catchUp = 8;
  /** How long the timer runs; zero runs it until it is cancelled. */
  Duration duration{};
  /** Held before the first update. */
  Duration delay{};
};

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

/** One tween running on one live value. */
class Animation : public Playback {
 public:
  using Playback::Playback;
};

/** A CALLBACK THE ENGINE RUNS every frame, at a fixed rate, or throttled —
 *  until it is cancelled or its duration runs out. */
class Timer : public Playback {
 public:
  using Playback::Playback;
  /** Under a `stepRate`: the fraction of a step left over after this
   *  frame's stepping — the render interpolant that stops a fixed-rate
   *  simulation juddering at a draw rate that is not a multiple of it. */
  [[nodiscard]] float betweenSteps() const;
  /** Under a `stepRate`: whether this frame dropped time because the
   *  backlog passed `catchUp`. Anything measured on such a frame — a
   *  residual, a convergence rate — is meaningless. */
  [[nodiscard]] bool droppedTime() const;
  /** Under a `stepRate`: how many steps this frame ran. */
  [[nodiscard]] int stepsThisFrame() const;
};

/** TWEENS AND CALLS PLACED IN TIME, played as one: anime.js's
 *  `createTimeline()`. An item with no position starts after everything
 *  already on it has ended. */
class Timeline : public Playback {
 public:
  using Playback::Playback;
  /** Plays @p tween on @p target, starting at @p when. A tween with no
   *  `from` starts from the value @p target holds when the item starts. */
  Timeline& add(Animatable<float>& target, Tween<float> tween,
                Position when = afterEnd());
  /** Calls @p callback when the timeline passes @p when, going forwards. */
  Timeline& call(std::function<void()> callback, Position when = afterEnd());
  /** Names a moment for `atLabel()`. */
  Timeline& label(std::string name, Position when = afterEnd());
};

/** WHAT THE ENGINE IS BUILT WITH, by whoever owns it. */
struct EngineOptions {
  /** Above zero: every frame the engine takes on its own moves EXACTLY
   *  this far — the headless sweep, a capture, a test. Zero: the wall
   *  clock moves it. */
  Duration fixedStep{};
  /** The most the wall clock moves one frame: a stalled or suspended
   *  frame does not jump every motion forward by the length of the stall. */
  Duration maxWallStep = std::chrono::milliseconds(250);
  /** Engine time per wall time: 1 is real time. */
  double speed = 1.0;
};

/**
 * THE ENGINE: the one clock a sketch's animations, timelines and timers
 * run on, spelled at the call site — `ctx.engine.timeline()` — and never a
 * hidden process default. A plain `main()` or a test constructs one with
 * no arguments.
 *
 *     motion::Animatable<float> glow = motion::animatable(0.0f);
 *     engine.animate(glow, {.to = 1.0f, .duration = 400ms});
 *     engine.timeline()
 *         .add(title, {.to = 1.0f, .duration = 750ms}, motion::at(500ms))
 *         .add(rule, {.to = 1.0f}, motion::withPrevious())
 *         .call([&] { armed = true; });
 *     engine.timer([&] { wave = std::sin(engine.elapsed().count()); });
 *
 * Not thread-safe: one engine per animation domain, touched only from its
 * thread. Everything under "The host" below is for whoever owns the engine
 * and moves it; a sketch never spells it.
 */
class Engine {
 public:
  explicit Engine(EngineOptions options = {});

  /** RUNS @p tween ON @p target, a live value — made live from the value
   *  it holds if it is not one. A tween with no `from` starts where the
   *  value stands. A second animation on the same value takes it over;
   *  under `Composition::Blend` it rides on top of the first instead. */
  Animation animate(Animatable<float>& target, Tween<float> tween);
  /** A TIMELINE, running from now. */
  Timeline timeline();
  /** A CALLBACK the engine runs every frame (or at @p options' rate) until
   *  it is cancelled. It names what it reads — `[] {…}`,
   *  `[](Duration delta) {…}`, `[](Duration delta, Duration elapsed) {…}`
   *  — and may answer false to stop. */
  template <class Callback>
    requires core::PrefixCallable<Callback, void(Duration, Duration)>
  Timer timer(Callback onUpdate, TimerOptions options = {}) {
    return startTimer(
        [held = std::move(onUpdate)](Duration delta, Duration elapsed) mutable {
          if constexpr (std::is_void_v<decltype(core::callPrefix(held, delta,
                                                                 elapsed))>) {
            core::callPrefix(held, delta, elapsed);
            return true;
          } else {
            return (bool)core::callPrefix(held, delta, elapsed);
          }
        },
        options);
  }

  /** Engine time so far: the sum of every frame's movement. */
  [[nodiscard]] Duration elapsed() const { return m_elapsed; }
  /** WHETHER ANYTHING IS DECLARED TO MOVE: a running animation, timeline
   *  or timer, or a motion a host started. The signal a host sleeps on —
   *  a declaration, never a proof that numbers change. */
  [[nodiscard]] bool isRunning() const;

  // ── the host ────────────────────────────────────────────────────────

  /** ONE FRAME THE ENGINE TAKES ON ITS OWN: the fixed step, or the wall
   *  clock's movement since the last frame — held, time-scaled and
   *  stall-clamped, and moving nothing under a policy that says so.
   *  Answers how far it moved. */
  Duration advance();
  /** …at the wall reading @p wallSeconds, which is what a test states. */
  Duration advanceWall(double wallSeconds);
  /** A FRAME A CALLER STATES: moves to engine time @p to — by exactly the
   *  difference, not scaled and not clamped — unless held or paused by the
   *  policy. Time only goes forward, and the next wall frame counts from
   *  its own reading. Answers how far it moved. */
  Duration advance(Duration to);

  /** Who moves the clock, from the next frame on, and a budget of engine
   *  time from now after which `isBudgetExpired()` says so once. */
  void setPolicy(ClockPolicy policy,
                 std::optional<Duration> budget = std::nullopt);
  [[nodiscard]] ClockPolicy policy() const { return m_policy; }
  /** Whether the wall moves this engine: false under every policy a
   *  repeatable run is drawn under. */
  [[nodiscard]] bool isWall() const { return m_policy == ClockPolicy::Wall; }
  /** Whether something the run asked for is still arriving — under
   *  `ClockPolicy::PauseWhileLoading`, a frame moves nothing while it is. */
  void setArriving(bool arriving) { m_arriving = arriving; }
  /** Holds every frame where it stands, keeping the policy: the pause a
   *  person presses. */
  void setHeld(bool held) { m_held = held; }
  [[nodiscard]] bool isHeld() const { return m_held; }
  void setSpeed(double speed) { m_options.speed = speed; }
  [[nodiscard]] double speed() const { return m_options.speed; }
  /** Whether a frame now would move nothing: held, paused by the policy,
   *  or waiting on an arrival. */
  [[nodiscard]] bool isPaused() const;
  /** Frames taken since the engine started or last restarted. */
  [[nodiscard]] uint64_t frames() const { return m_frames; }
  /** Engine time of the budget still to run; none where none was set or
   *  it has run out. */
  [[nodiscard]] std::optional<Duration> budgetRemaining() const;
  /** Whether the budget ran out on the frame just taken; true on that
   *  frame alone. */
  [[nodiscard]] bool isBudgetExpired() const { return m_expired; }
  /** Counts from zero again — time and frames — keeping the policy, the
   *  hold and whatever budget is left: a new session opening under it. */
  void restart();

  /** Starts stepping @p motion from the next frame on, before every timer.
   *  A finished motion leaves the engine. */
  void run(std::shared_ptr<detail::Stepped> motion);

 private:
  Timer startTimer(std::function<bool(Duration, Duration)> onUpdate,
                   TimerOptions options);
  /** Moves every motion and timer by @p delta and counts the frame. */
  Duration step(Duration delta);

  EngineOptions m_options;
  ClockPolicy m_policy = ClockPolicy::Wall;
  std::optional<Duration> m_budgetEnds;
  Duration m_elapsed{};
  uint64_t m_frames = 0;
  double m_lastWall = -1.0;
  bool m_expired = false;
  bool m_held = false;
  bool m_arriving = false;
  std::vector<std::shared_ptr<detail::Stepped>> m_motions;
  std::vector<std::shared_ptr<detail::Stepped>> m_timers;
};

}  // namespace sigil::motion
