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

void PlaybackState::relist(const std::shared_ptr<PlaybackState>& self) {
  if (listed) return;
  if (const std::shared_ptr<Running> running = home.lock()) {
    (timer ? running->timers : running->motions).push_back(self);
    listed = true;
  }
}

Duration PlaybackState::total() const {
  if (passes <= 0) return Duration(std::numeric_limits<double>::infinity());
  return delay + pass * (double)passes;
}

bool PlaybackState::advance(double deltaSeconds) {
  if (cancelled || completed) return false;
  if (paused) return true;
  moveTo(time + Duration(reversed ? -deltaSeconds : deltaSeconds));
  return !completed && !cancelled;
}

void PlaybackState::moveTo(Duration to) {
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

void PlaybackState::show() {
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

  /** Every member of @p group at @p offset, the group @p length long. */
  void placeGroup(Duration offset,
                  std::vector<std::shared_ptr<PlaybackState>> group,
                  Duration length) {
    for (auto& member : group) m_items.push_back({offset, std::move(member), {}, false});
    m_previousStart = offset;
    m_previousEnd = offset + length;
    m_end = std::max(m_end, m_previousEnd);
    pass = m_end;
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

}  // namespace

Timeline& Timeline::place(
    std::vector<std::shared_ptr<detail::PlaybackState>> group, Position when) {
  if (!m_state || group.empty()) return *this;
  detail::TimelineState& timeline = timelineOf(m_state);
  // A collective starts together; the next item placed after it follows
  // the member that ends last.
  const Duration offset = timeline.resolve(when);
  Duration length{};
  for (const auto& member : group) length = std::max(length, member->total());
  timeline.placeGroup(offset, std::move(group), length);
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

void Engine::start(const std::shared_ptr<detail::PlaybackState>& state) {
  state->home = m_running;
  state->relist(state);
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
