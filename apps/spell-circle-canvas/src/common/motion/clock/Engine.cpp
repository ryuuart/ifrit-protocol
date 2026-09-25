/** @file
 * The engine: the playback every animation, timeline and timer answers
 * to, the tween written into a live value, the timeline's placed items,
 * the timer's fixed and throttled rates, and the frame that moves them —
 * the wall clock or a stated step under a policy and a budget.
 */

#include "sigilmotion/clock/Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>

namespace sigil::motion {

namespace detail {

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
  void relist(const std::shared_ptr<PlaybackState>& self) {
    if (listed) return;
    if (const std::shared_ptr<Running> running = home.lock()) {
      (timer ? running->timers : running->motions).push_back(self);
      listed = true;
    }
  }

  /** The whole length, delay included; the largest duration for ever. */
  [[nodiscard]] Duration total() const {
    if (passes <= 0) return Duration(std::numeric_limits<double>::infinity());
    return delay + pass * (double)passes;
  }

  bool advance(double deltaSeconds) override {
    if (cancelled || completed) return false;
    if (paused) return true;
    moveTo(time + Duration(reversed ? -deltaSeconds : deltaSeconds));
    return !completed && !cancelled;
  }

  /** Moves to @p to, shows it there, and completes at either end. */
  void moveTo(Duration to) {
    const Duration end = total();
    time = std::clamp(to, Duration{}, end);
    show();
    const bool atEnd = reversed ? time <= Duration{} : time >= end;
    if (atEnd && !completed) {
      completed = true;
      finished();
      if (onComplete) onComplete();
    }
  }

  /** Shows the playback at `time`: before the delay the start; then the
   *  pass it is in, backwards on every other pass when it alternates. */
  void show() {
    if (time < delay) {
      showAt(Duration{}, true);
      return;
    }
    Duration into = time - delay;
    int index = 0;
    if (pass > Duration{}) {
      index = (int)std::floor(into / pass);
      if (passes > 0 && index >= passes) {
        index = passes - 1;
        into = pass;
      } else {
        into -= pass * (double)index;
      }
    }
    if (alternate && index % 2 == 1) into = pass - into;
    showAt(into, false);
  }

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

/** ONE TWEEN ON ONE LIVE VALUE: the path from where the value stood (or
 *  the tween's `from`) through its keyframes, plus any changes blended
 *  on top of it while it runs. */
class AnimationState final : public PlaybackState {
 public:
  struct Step {
    float to = 0.0f;
    Duration duration{};
    Easing ease;
  };
  struct Layer {
    Duration begin{};
    Duration duration{};
    float delta = 0.0f;
    Easing ease;
  };

  AnimationState(std::shared_ptr<Cell<float>> cell, const Tween<float>& tween)
      : m_cell(std::move(cell)) {
    const Tween<float> resolved = tween.resolved({});
    m_origin = m_cell->value;
    if (resolved.from) m_from = resolved.from->value();
    const Duration length = resolved.duration.value();
    if (resolved.keyframes.empty()) {
      m_steps.push_back({resolved.rest(), length, resolved.easing()});
    } else {
      const Duration share = length / (double)resolved.keyframes.size();
      for (const Keyframe<float>& step : resolved.keyframes)
        m_steps.push_back({step.to, step.duration.value_or(share),
                           step.ease ? step.ease : resolved.easing()});
    }
    delay = resolved.delay.value();
    for (const Step& step : m_steps) pass += step.duration;
    passes = resolved.loop < 0 ? 0 : 1 + resolved.loop;
    alternate = resolved.alternate;
  }

  /** TAKES THE VALUE OVER: from here on this animation writes it, and a
   *  motion started on it before stops. The value holds where it stands,
   *  or at the tween's `from`. */
  void claim() {
    if (m_claimed) return;
    m_claimed = true;
    m_origin = m_cell->value;
    m_writer = ++m_cell->writer;
    m_cell->moving = true;
    if (m_from) m_cell->value = *m_from;
  }

  /** Rides @p delta on top of the running path, over @p duration after
   *  @p wait, from now: the value's velocity carries through. */
  void blend(float delta, Duration wait, Duration duration, Easing ease) {
    m_layers.push_back({time + wait, duration, delta, std::move(ease)});
    const Duration end = time + wait + duration;
    // A single pass runs on, holding its end, until the layer lands; the
    // path itself is unchanged, only when it completes.
    if (passes == 1 && end > total()) pass += end - total();
  }

  void showAt(Duration into, bool waiting) override {
    claim();
    if (m_cell->writer != m_writer) {
      // A motion started on the same value since has taken it over.
      cancelled = true;
      return;
    }
    const float start = m_from ? *m_from : m_origin;
    float value = start;
    if (!waiting) {
      value = start;
      Duration remaining = into;
      for (const Step& step : m_steps) {
        if (remaining <= step.duration) {
          const float unit = step.duration > Duration{}
                                 ? (float)(remaining / step.duration)
                                 : 1.0f;
          value = value + (step.to - value) * (step.ease ? step.ease(unit) : unit);
          break;
        }
        remaining -= step.duration;
        value = step.to;
      }
    }
    for (const Layer& layer : m_layers) {
      if (time <= layer.begin) continue;
      const float unit =
          layer.duration > Duration{}
              ? (float)std::min((time - layer.begin) / layer.duration, 1.0)
              : 1.0f;
      value += layer.delta * (layer.ease ? layer.ease(unit) : unit);
    }
    m_cell->value = value;
  }

  void finished() override {
    if (m_cell->writer == m_writer) m_cell->moving = false;
  }

  void revertTargets() override {
    if (m_cell->writer == m_writer) {
      m_cell->value = m_origin;
      m_cell->moving = false;
    }
  }

  [[nodiscard]] const std::shared_ptr<Cell<float>>& cell() const { return m_cell; }
  [[nodiscard]] float target() const {
    float value = m_steps.empty() ? m_origin : m_steps.back().to;
    for (const Layer& layer : m_layers) value += layer.delta;
    return value;
  }

 private:
  std::shared_ptr<Cell<float>> m_cell;
  std::optional<float> m_from;
  float m_origin = 0.0f;
  std::vector<Step> m_steps;
  std::vector<Layer> m_layers;
  uint32_t m_writer = 0;
  bool m_claimed = false;
};

/** TWEENS AND CALLS PLACED IN TIME: each item plays at its own offset
 *  from the timeline's start, and the timeline's pass is as long as its
 *  last item. */
class TimelineState final : public PlaybackState {
 public:
  struct Item {
    Duration offset{};
    std::shared_ptr<PlaybackState> playback;  ///< null for a call
    std::function<void()> call;
    bool shown = false;
  };

  void place(Item item, Duration length) {
    m_previousStart = item.offset;
    m_previousEnd = item.offset + length;
    m_end = std::max(m_end, m_previousEnd);
    pass = m_end;
    m_items.push_back(std::move(item));
  }

  [[nodiscard]] Duration resolve(const Position& when) const {
    switch (when.kind) {
      case Position::Kind::At:
        return when.offset;
      case Position::Kind::AfterEnd:
        return m_end + when.offset;
      case Position::Kind::AfterPrevious:
        return m_previousEnd + when.offset;
      case Position::Kind::WithPrevious:
        return m_previousStart + when.offset;
      case Position::Kind::AtLabel: {
        const auto found = m_labels.find(when.label);
        return (found != m_labels.end() ? found->second : m_end) + when.offset;
      }
    }
    return m_end;
  }

  void label(std::string name, Duration at) { m_labels[std::move(name)] = at; }

  void showAt(Duration into, bool waiting) override {
    const Duration now = waiting ? Duration{} : into;
    for (Item& item : m_items) {
      if (item.call) {
        // A call fires going forwards, once, as the timeline passes it.
        if (!item.shown && now >= item.offset && now > m_shownTo) {
          item.shown = true;
          item.call();
        }
        if (now < item.offset) item.shown = false;
        continue;
      }
      if (now < item.offset && !item.shown) continue;
      item.shown = true;
      item.playback->moveTo(now - item.offset);
    }
    m_shownTo = now;
  }

  void revertTargets() override {
    for (auto it = m_items.rbegin(); it != m_items.rend(); ++it)
      if (it->playback) it->playback->revertTargets();
  }

 private:
  std::vector<Item> m_items;
  std::map<std::string, Duration> m_labels;
  Duration m_end{};
  Duration m_previousStart{};
  Duration m_previousEnd{};
  Duration m_shownTo{-1.0};
};

/** A CALLBACK ON THE ENGINE'S FRAMES: every frame, at a fixed rate counted
 *  from total time, or throttled to a rate. */
class TimerState final : public PlaybackState {
 public:
  TimerState(std::function<bool(Duration, Duration)> onUpdate,
             TimerOptions options)
      : m_onUpdate(std::move(onUpdate)), m_options(options) {
    delay = options.delay;
    pass = options.duration;
    passes = options.duration > Duration{} ? 1 : 0;
  }

  bool advance(double deltaSeconds) override {
    if (cancelled || completed) return false;
    if (paused) return true;
    const Duration delta(deltaSeconds);
    const Duration before = time;
    time += delta;
    if (time >= delay) {
      const Duration running = time - delay;
      const Duration step = before >= delay ? delta : running;
      if (!update(step, running)) {
        cancelled = true;
        return false;
      }
    }
    if (passes > 0 && time >= total()) {
      completed = true;
      if (onComplete) onComplete();
      return false;
    }
    return true;
  }

  void showAt(Duration, bool) override {}

  float betweenSteps = 0.0f;
  bool droppedTime = false;
  int stepsThisFrame = 0;

 private:
  /** One frame's worth of updates; false when the callback asks to stop. */
  bool update(Duration delta, Duration running) {
    m_total += delta;
    if (m_options.stepRate > 0.0) {
      // From TOTAL time, not a running accumulator: an accumulator
      // compared against a step slips over a long run, so the same moment
      // would land on either side of a step boundary depending on the draw
      // rate. The epsilon absorbs the sum of many small deltas landing a
      // hair under a whole step.
      const double total = m_total.count();
      const double want = std::floor(total * m_options.stepRate + 1e-9);
      double due = want - m_ran;
      droppedTime = false;
      if (due > (double)m_options.catchUp) {
        // Beyond the budget the backlog is dropped rather than carried:
        // carrying it makes the next frame longer, which grows the backlog.
        due = (double)m_options.catchUp;
        droppedTime = true;
      }
      const Duration step(1.0 / m_options.stepRate);
      bool alive = true;
      stepsThisFrame = 0;
      for (; stepsThisFrame < (int)due; ++stepsThisFrame) {
        alive = m_onUpdate(step, running);
        if (!alive) break;
      }
      m_ran = want;
      betweenSteps = (float)(total * m_options.stepRate - want);
      return alive;
    }
    if (m_options.frameRate > 0.0) {
      const Duration interval(1.0 / m_options.frameRate);
      m_waited += delta;
      if (m_waited < interval) return true;
      const Duration moved = m_waited;
      m_waited = Duration(std::fmod(m_waited.count(), interval.count()));
      return m_onUpdate(moved, running);
    }
    return m_onUpdate(delta, running);
  }

  std::function<bool(Duration, Duration)> m_onUpdate;
  TimerOptions m_options;
  Duration m_total{};
  Duration m_waited{};
  double m_ran = 0.0;
};

std::shared_ptr<PlaybackState> animationOf(std::shared_ptr<Cell<float>> cell,
                                           const Tween<float>& tween) {
  return std::make_shared<AnimationState>(std::move(cell), tween);
}

}  // namespace detail

// ---- the playback ----------------------------------------------------------

Playback& Playback::play() {
  if (!m_state) return *this;
  if (m_state->completed) {
    m_state->completed = false;
    m_state->time = m_state->reversed ? m_state->total() : Duration{};
  }
  m_state->paused = false;
  m_state->relist(m_state);
  return *this;
}

Playback& Playback::pause() {
  if (m_state) m_state->paused = true;
  return *this;
}

Playback& Playback::resume() {
  if (!m_state) return *this;
  m_state->paused = false;
  m_state->relist(m_state);
  return *this;
}

Playback& Playback::restart() {
  if (!m_state) return *this;
  m_state->completed = false;
  m_state->cancelled = false;
  m_state->paused = false;
  m_state->reversed = false;
  m_state->moveTo(Duration{});
  m_state->relist(m_state);
  return *this;
}

Playback& Playback::reverse() {
  if (!m_state) return *this;
  m_state->reversed = !m_state->reversed;
  m_state->completed = false;
  m_state->relist(m_state);
  return *this;
}

Playback& Playback::alternate() {
  if (m_state) m_state->alternate = !m_state->alternate;
  return *this;
}

Playback& Playback::seek(Duration time) {
  if (!m_state) return *this;
  m_state->completed = false;
  m_state->moveTo(time);
  if (!m_state->completed) m_state->relist(m_state);
  return *this;
}

Playback& Playback::complete() {
  if (m_state && !m_state->completed)
    m_state->moveTo(m_state->reversed ? Duration{} : m_state->total());
  return *this;
}

Playback& Playback::cancel() {
  if (m_state) m_state->cancelled = true;
  return *this;
}

Playback& Playback::revert() {
  if (!m_state) return *this;
  m_state->revertTargets();
  m_state->cancelled = true;
  return *this;
}

Playback& Playback::onComplete(std::function<void()> callback) {
  if (m_state) m_state->onComplete = std::move(callback);
  return *this;
}

bool Playback::isRunning() const { return m_state && m_state->isRunning(); }
bool Playback::isPaused() const { return m_state && m_state->paused; }
bool Playback::isCompleted() const { return m_state && m_state->completed; }

Duration Playback::currentTime() const {
  return m_state ? m_state->time : Duration{};
}

float Playback::progress() const {
  if (!m_state) return 0.0f;
  const Duration total = m_state->total();
  if (!(total > Duration{}) || !std::isfinite(total.count())) return 0.0f;
  return (float)std::clamp(m_state->time / total, 0.0, 1.0);
}

float Timer::betweenSteps() const {
  const auto* timer = dynamic_cast<const detail::TimerState*>(m_state.get());
  return timer ? timer->betweenSteps : 0.0f;
}

bool Timer::droppedTime() const {
  const auto* timer = dynamic_cast<const detail::TimerState*>(m_state.get());
  return timer && timer->droppedTime;
}

int Timer::stepsThisFrame() const {
  const auto* timer = dynamic_cast<const detail::TimerState*>(m_state.get());
  return timer ? timer->stepsThisFrame : 0;
}

// ---- the timeline ----------------------------------------------------------

namespace {

detail::TimelineState& timelineOf(const std::shared_ptr<detail::PlaybackState>& state) {
  return static_cast<detail::TimelineState&>(*state);
}

/** Makes @p target live, from the value it holds, if it is not. */
void makeLive(Animatable<float>& target) {
  if (!target.cell()) target = animatable(target.value());
}

}  // namespace

Timeline& Timeline::add(Animatable<float>& target, Tween<float> tween,
                        Position when) {
  if (!m_state) return *this;
  makeLive(target);
  detail::TimelineState& timeline = timelineOf(m_state);
  auto animation = std::static_pointer_cast<detail::AnimationState>(
      detail::animationOf(target.cell(), tween));
  // Placed, not started: the item takes the value over when the timeline
  // reaches it, so it writes nothing until then.
  const Duration offset = timeline.resolve(when);
  timeline.place({offset, animation, {}, false}, animation->total());
  return *this;
}

Timeline& Timeline::call(std::function<void()> callback, Position when) {
  if (!m_state) return *this;
  detail::TimelineState& timeline = timelineOf(m_state);
  timeline.place({timeline.resolve(when), nullptr, std::move(callback), false},
                 Duration{});
  return *this;
}

Timeline& Timeline::label(std::string name, Position when) {
  if (!m_state) return *this;
  detail::TimelineState& timeline = timelineOf(m_state);
  timeline.label(std::move(name), timeline.resolve(when));
  return *this;
}

// ---- the engine ------------------------------------------------------------

Engine::Engine(EngineOptions options) : m_options(options) {}

Animation Engine::animate(Animatable<float>& target, Tween<float> tween) {
  makeLive(target);
  const std::shared_ptr<detail::Cell<float>>& cell = target.cell();
  // BLEND: the change rides on top of the animation already writing the
  // value, so its velocity carries through.
  if (tween.composition == Composition::Blend && cell->moving) {
    if (auto running = std::static_pointer_cast<detail::AnimationState>(
            cell->motion.lock());
        running && running->isRunning()) {
      const Tween<float> resolved = tween.resolved({});
      running->blend(resolved.rest() - running->target(), resolved.delay.value(),
                     resolved.duration.value(), resolved.easing());
      return Animation(running);
    }
  }
  auto state = std::static_pointer_cast<detail::AnimationState>(
      detail::animationOf(cell, tween));
  state->claim();
  cell->motion = state;
  state->home = m_running;
  state->relist(state);
  return Animation(state);
}

Timeline Engine::timeline() {
  auto state = std::make_shared<detail::TimelineState>();
  state->home = m_running;
  state->relist(state);
  return Timeline(state);
}

Timer Engine::startTimer(std::function<bool(Duration, Duration)> onUpdate,
                         TimerOptions options) {
  auto state = std::make_shared<detail::TimerState>(std::move(onUpdate), options);
  state->timer = true;
  state->home = m_running;
  state->relist(state);
  return Timer(state);
}

void Engine::run(std::shared_ptr<detail::Stepped> motion) {
  if (motion) m_running->motions.push_back(std::move(motion));
}

bool Engine::isRunning() const {
  const auto declared = [](const std::shared_ptr<detail::Stepped>& stepped) {
    return stepped->isRunning();
  };
  return std::any_of(m_running->motions.begin(), m_running->motions.end(),
                     declared) ||
         std::any_of(m_running->timers.begin(), m_running->timers.end(),
                     declared);
}

bool Engine::isPaused() const {
  return m_held || m_policy == ClockPolicy::Pause ||
         (m_policy == ClockPolicy::PauseWhileLoading && m_arriving);
}

Duration Engine::advance() {
  if (m_options.fixedStep > Duration{}) {
    // The fixed step stands in for the wall's movement, so it moves under
    // the policies the wall does and under no other.
    const bool moves =
        !m_held && (m_policy == ClockPolicy::Wall ||
                    (m_policy == ClockPolicy::PauseWhileLoading && !m_arriving));
    return step(moves ? m_options.fixedStep : Duration{});
  }
  return advanceWall(std::chrono::duration<double>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count());
}

Duration Engine::advanceWall(double wallSeconds) {
  // The first reading only starts the count; every reading is consumed
  // whether or not the frame moves, so a return to the wall measures from
  // here rather than catching up on the stretch it stood still for.
  double delta = m_lastWall < 0.0 ? 0.0 : wallSeconds - m_lastWall;
  m_lastWall = wallSeconds;
  const bool moves =
      !m_held && (m_policy == ClockPolicy::Wall ||
                  (m_policy == ClockPolicy::PauseWhileLoading && !m_arriving));
  if (!moves) return step(Duration{});
  delta = std::clamp(delta, 0.0, m_options.maxWallStep.count()) * m_options.speed;
  return step(Duration(delta));
}

Duration Engine::advance(Duration to) {
  // A stated step takes the frame from the wall, so a return to the wall
  // starts its count again at the next reading rather than catching up on
  // the stretch the caller was stating steps for.
  m_lastWall = -1.0;
  const Duration delta = to - m_elapsed;
  const bool moves = !m_held && m_policy != ClockPolicy::Pause &&
                     std::isfinite(delta.count()) && delta > Duration{};
  return step(moves ? delta : Duration{});
}

Duration Engine::step(Duration delta) {
  m_elapsed += delta;
  ++m_frames;
  m_expired = false;
  // A budget met by a sum of steps is met to within what the sum rounds
  // by: sixty steps of a sixtieth fall short of one by a few ulps.
  if (m_budgetEnds && m_elapsed.count() >= m_budgetEnds->count() - 1e-9) {
    m_expired = true;
    m_budgetEnds.reset();
  }
  // The motions first, in the order they were started, then every timer
  // in the order it was registered: a timer reading a moving value reads
  // this frame's number. What finished leaves.
  const double seconds = delta.count();
  const auto stepAll = [seconds](std::vector<std::shared_ptr<detail::Stepped>>& list) {
    // Indexed, not iterated: a callback may start a playback, which
    // appends to this list while it is being walked.
    for (size_t i = 0; i < list.size();) {
      if (list[i]->advance(seconds)) {
        ++i;
        continue;
      }
      if (auto* playback = dynamic_cast<detail::PlaybackState*>(list[i].get()))
        playback->listed = false;
      list.erase(list.begin() + (long)i);
    }
  };
  stepAll(m_running->motions);
  stepAll(m_running->timers);
  return delta;
}

void Engine::setPolicy(ClockPolicy policy, std::optional<Duration> budget) {
  m_policy = policy;
  m_expired = false;
  m_arriving = false;
  m_budgetEnds.reset();
  if (budget && std::isfinite(budget->count()) && *budget >= Duration{})
    m_budgetEnds = m_elapsed + *budget;
}

std::optional<Duration> Engine::budgetRemaining() const {
  if (!m_budgetEnds) return std::nullopt;
  return *m_budgetEnds - m_elapsed;
}

void Engine::restart() {
  if (m_budgetEnds) *m_budgetEnds -= m_elapsed;
  m_elapsed = Duration{};
  m_frames = 0;
  m_expired = false;
}

}  // namespace sigil::motion
