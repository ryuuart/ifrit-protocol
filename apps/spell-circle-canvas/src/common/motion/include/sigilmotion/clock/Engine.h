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
#include <sigilmotion/clock/Animation.h>
#include <sigilmotion/clock/Playback.h>
#include <sigilmotion/time/Duration.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Tween.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::motion {

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
   *  `from` starts from the value @p target holds when the item starts.
   *  Any value an `Animatable` holds that has a line between two of its
   *  values — a number, a vector, a colour. */
  template <Interpolable T>
  Timeline& add(Animatable<T>& target, std::type_identity_t<Tween<T>> tween,
                Position when = afterEnd()) {
    if (!m_state) return *this;
    return place({animationOn(target, tween)}, when);
  }
  /** Plays @p tween on EVERY ONE of @p targets as a collective starting at
   *  @p when: each target is a sibling, so a field written as `stagger()`
   *  or `cues()` resolves to that target's own value from its place in
   *  the run — `.delay = stagger(40ms)` starts one after the other. The
   *  next item placed after the group follows its last target's end. */
  template <Interpolable T>
  Timeline& add(std::span<Animatable<T>> targets,
                std::type_identity_t<Tween<T>> tween,
                Position when = afterEnd()) {
    if (!m_state) return *this;
    std::vector<std::shared_ptr<detail::PlaybackState>> group;
    group.reserve(targets.size());
    for (size_t index = 0; index < targets.size(); ++index)
      group.push_back(
          animationOn(targets[index], tween.resolved({index, targets.size()})));
    return place(std::move(group), when);
  }
  /** The same over a vector of targets. */
  template <Interpolable T>
  Timeline& add(std::vector<Animatable<T>>& targets,
                std::type_identity_t<Tween<T>> tween,
                Position when = afterEnd()) {
    return add(std::span<Animatable<T>>(targets), std::move(tween), when);
  }
  /** Calls @p callback when the timeline passes @p when, going forwards. */
  Timeline& call(std::function<void()> callback, Position when = afterEnd());
  /** Names a moment for `atLabel()`. */
  Timeline& label(std::string name, Position when = afterEnd());

 private:
  /** An animation of @p tween on @p target, made live, placed and not
   *  started: it takes the value over when the timeline reaches it, so it
   *  writes nothing until then. */
  template <typename T>
  static std::shared_ptr<detail::PlaybackState> animationOn(
      Animatable<T>& target, const Tween<T>& tween) {
    detail::makeLive(target);
    return std::make_shared<detail::AnimationState<T>>(target.cell(), tween);
  }
  /** Places @p group — one item, or a collective starting together — at
   *  @p when. */
  Timeline& place(std::vector<std::shared_ptr<detail::PlaybackState>> group,
                  Position when);
};

/** WHAT THE ENGINE IS BUILT WITH, by whoever owns it. */
struct EngineOptions {
  /** Above zero: every frame the engine takes on its own moves EXACTLY
   *  this far instead of by the wall — the headless sweep, a capture, a
   *  test — under the policies the wall moves under. Zero: the wall clock
   *  moves it. */
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
   *  it holds if it is not. A tween with no `from` starts where the value
   *  stands. A second animation on the same value takes it over; under
   *  `Composition::Blend` a value that adds rides the change on top of
   *  the first instead, and one that does not (a colour) starts again
   *  from where it stands. Any value an `Animatable` holds that has a line
   *  between two of its values — a number, a `glm::vec2`, a colour. */
  template <Interpolable T>
  Animation animate(Animatable<T>& target, std::type_identity_t<Tween<T>> tween) {
    detail::makeLive(target);
    const std::shared_ptr<detail::Cell<T>>& cell = target.cell();
    if constexpr (Additive<T>) {
      // BLEND: the change rides on top of the animation already writing
      // the value, so its velocity carries through.
      if (tween.composition == Composition::Blend && cell->moving) {
        if (auto running = std::static_pointer_cast<detail::AnimationState<T>>(
                cell->motion.lock());
            running && running->isRunning()) {
          const Tween<T> resolved = tween.resolved({});
          running->blend(resolved.rest() - running->target(),
                         resolved.delay.value(), resolved.duration.value(),
                         resolved.easing());
          return Animation(running);
        }
      }
    }
    auto state = std::make_shared<detail::AnimationState<T>>(cell, tween);
    state->claim();
    cell->motion = state;
    start(state);
    return Animation(state);
  }
  /** RUNS @p tween ON EVERY ONE of @p targets as a collective, from now:
   *  each target is a sibling, so `.delay = stagger(40ms)` starts them one
   *  after the other and `.to = stagger({0.0f, 360.0f})` spreads where
   *  they land. The timeline it hands back controls the run as one. */
  template <Interpolable T>
  Timeline animate(std::span<Animatable<T>> targets,
                   std::type_identity_t<Tween<T>> tween) {
    Timeline run = timeline();
    run.add(targets, std::move(tween), at(Duration{}));
    return run;
  }
  /** The same over a vector of targets. */
  template <Interpolable T>
  Timeline animate(std::vector<Animatable<T>>& targets,
                   std::type_identity_t<Tween<T>> tween) {
    return animate(std::span<Animatable<T>>(targets), std::move(tween));
  }
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
  /** Puts @p state on this engine's motions, from the next frame on. */
  void start(const std::shared_ptr<detail::PlaybackState>& state);
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
  std::shared_ptr<detail::Running> m_running = std::make_shared<detail::Running>();
};

}  // namespace sigil::motion
