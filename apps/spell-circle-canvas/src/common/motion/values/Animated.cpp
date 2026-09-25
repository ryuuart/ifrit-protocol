/** @file
 * The operations on a held motion: reading the value for this frame,
 * retargeting a running ramp from where it is when the target moves, and
 * starting an entrance from the value the description declared — and the
 * ramp itself, the keyed segments a ticker steps into a live value.
 */

#include "sigilmotion/values/Animated.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace sigil::motion {

namespace {

/** A length a held motion is timed in: seconds as a FLOAT, which is the
 *  precision every held motion's delay and length are summed at before
 *  the ramp reads them as a double. */
float segmentSeconds(std::chrono::duration<float> length) {
  return length.count();
}

/** ONE KEYED RAMP writing one live value: a list of segments, each
 *  holding or easing from the end of the last, stepped by the ticker.
 *
 *  A segment's value at a time is `from + (to − from) · ease(t / length)`
 *  in float, the time is the sum of the frame deltas in double, and the
 *  segment that owns a time is the first whose length the remaining time
 *  does not exceed — so a boundary belongs to the segment it ends. At or
 *  past the end of the last segment the value is that segment's end, the
 *  ramp stops writing, and the ticker drops it on the same frame. */
class Ramp final : public detail::Stepped {
 public:
  struct Segment {
    float from = 0.0f;
    float to = 0.0f;
    double seconds = 0.0;
    Easing ease;  ///< empty: held at `from` (a hold), or linear
  };

  Ramp(std::shared_ptr<detail::Cell<float>> cell, std::vector<Segment> segments)
      : m_cell(std::move(cell)), m_segments(std::move(segments)) {
    m_writer = ++m_cell->writer;
    m_cell->moving = true;
    for (const Segment& segment : m_segments) m_total += segment.seconds;
  }

  bool advance(double deltaSeconds) override {
    // A motion started on the same value since has taken it over.
    if (m_cell->writer != m_writer) return false;
    m_time += deltaSeconds;
    if (m_time >= m_total) {
      m_cell->value = m_segments.empty() ? m_cell->value : m_segments.back().to;
      m_cell->moving = false;
      return false;
    }
    double at = m_time;
    for (const Segment& segment : m_segments) {
      if (segment.seconds < at) {
        at -= segment.seconds;
        continue;
      }
      const float unit = (float)(at / segment.seconds);
      const float shaped = segment.ease ? segment.ease(unit) : unit;
      m_cell->value = segment.from + (segment.to - segment.from) * shaped;
      break;
    }
    return true;
  }

 private:
  std::shared_ptr<detail::Cell<float>> m_cell;
  std::vector<Segment> m_segments;
  uint32_t m_writer = 0;
  double m_time = 0.0;
  double m_total = 0.0;
};

/** A hold at @p value for @p seconds: a segment whose ends agree. */
Ramp::Segment hold(float value, double seconds) {
  return {value, value, seconds, [](float) { return 0.0f; }};
}

/** Starts @p segments on @p held's value. */
void start(Ticker& ticker, AnimatedFloat& held,
           std::vector<Ramp::Segment> segments) {
  ticker.run(std::make_shared<Ramp>(held.value.cell(), std::move(segments)));
}

}  // namespace

void AnimatedFloat::stop() {
  const std::shared_ptr<detail::Cell<float>>& cell = value.cell();
  ++cell->writer;
  cell->moving = false;
}

float resolveFloatAt(const AnimatedFloat* animated, const Animatable<float>& property) {
  // A live value — shaped through its stages when it has any — is read
  // here, the one place a bound float is read, so every consumer gets the
  // stages for free.
  if (property.identity()) return property.value();
  if (animated && animated->started) return animated->current();
  return property.value();
}

bool transitionFloatAt(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                       const Animatable<float>& previousValue,
                       const Animatable<float>& nextValue,
                       const std::optional<Transition>& fallback) {
  ResolvedProperty<float> prev = resolveProperty(previousValue, fallback);
  ResolvedProperty<float> next = resolveProperty(nextValue, fallback);
  // Snap semantics must actually LAND: a lingering ramp from an earlier
  // transition would shadow the constant description forever
  // (resolveFloatAt prefers a started ramp), so the snap paths stop it.
  auto snapAnim = [&] {
    if (auto& anim = held; anim && anim->started) {
      anim->stop();
      anim->started = false;
    }
  };
  if (next.live || !next.transition) {
    snapAnim();
    return false;  // live, or a constant snap
  }
  if (prev.live) {
    snapAnim();
    return false;  // live → constant: snap (no meaningful "from")
  }

  auto& anim = held;
  // A running motion already headed at this exact target keeps flying —
  // an unrelated prop patch mid-entrance must not restart it (and must
  // never re-hold its delay).
  if (anim && anim->started && anim->isMoving() && anim->target == next.target)
    return true;
  const float current = anim && anim->started ? anim->current() : prev.target;
  if (current == next.target) {
    // The value COINCIDES with the new target, but a moving ramp that
    // passed the keeps-flying guard is provably headed somewhere else —
    // left alone it would carry the value to a STALE target (permanent,
    // since identical re-describes prune). Stop it; the description's
    // own value (== next.target) shows through.
    if (anim && anim->started && anim->isMoving() && anim->target != next.target)
      snapAnim();
    return anim && anim->isMoving();
  }

  if (!anim) anim = std::make_unique<AnimatedFloat>();
  anim->value = current;  // seed the retarget start point
  anim->started = true;
  anim->target = next.target;
  std::vector<Ramp::Segment> segments;
  const float delay = segmentSeconds(next.transition->delay);
  if (delay > 0) segments.push_back(hold(current, delay));  // the stagger primitive
  segments.push_back({current, next.target,
                      segmentSeconds(next.transition->duration),
                      next.transition->easing()});
  start(ticker, *anim, std::move(segments));
  return true;
}

void mountEntrance(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                   const Animatable<float>& property, float extraDelaySeconds) {
  const Transitioned<float>* transitioned = property.described();
  if (!transitioned) return;
  // animate(through({…})): the multi-segment entrance — checked BEFORE
  // the from==value guard (a shake 0→−20→0 starts and ends equal).
  if (transitioned->waypoints.size() >= 2) {
    auto& anim = held;
    if (!anim) anim = std::make_unique<AnimatedFloat>();
    const float first = transitioned->waypoints.front().second;
    anim->value = first;
    anim->started = true;
    anim->target = transitioned->waypoints.back().second;
    std::vector<Ramp::Segment> segments;
    const float lead = segmentSeconds(transitioned->spec.delay) +
                       extraDelaySeconds +
                       segmentSeconds(transitioned->waypoints.front().first);
    if (lead > 0) segments.push_back(hold(first, lead));
    for (size_t i = 1; i < transitioned->waypoints.size(); ++i) {
      const float length = segmentSeconds(transitioned->waypoints[i].first -
                                          transitioned->waypoints[i - 1].first);
      segments.push_back({transitioned->waypoints[i - 1].second,
                          transitioned->waypoints[i].second,
                          std::max(length, 0.0f), transitioned->spec.easing()});
    }
    start(ticker, *anim, std::move(segments));
    return;
  }
  if (!transitioned->from || *transitioned->from == transitioned->value) return;
  auto& anim = held;
  if (!anim) anim = std::make_unique<AnimatedFloat>();
  anim->value = *transitioned->from;
  anim->started = true;
  anim->target = transitioned->value;
  std::vector<Ramp::Segment> segments;
  const float delay = segmentSeconds(transitioned->spec.delay) +
                      extraDelaySeconds;  // a staggered entrance's carry
  if (delay > 0)  // stagger: hold the `from` before entering
    segments.push_back(hold(*transitioned->from, delay));
  segments.push_back({*transitioned->from, transitioned->value,
                      segmentSeconds(transitioned->spec.duration),
                      transitioned->spec.easing()});
  start(ticker, *anim, std::move(segments));
}

bool isLive(const AnimatedFloat* animated, const Animatable<float>& property) {
  return property.isRunning() || (animated && animated->isMoving());
}

void progressRamp(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                  const Transition& spec, float extraDelaySeconds) {
  if (!held) held = std::make_unique<AnimatedFloat>();
  held->value = 0.0f;
  held->started = true;
  held->target = 1.0f;
  std::vector<Ramp::Segment> segments;
  const float delay = segmentSeconds(spec.delay) + extraDelaySeconds;
  if (delay > 0) segments.push_back(hold(0.0f, delay));
  segments.push_back({0.0f, 1.0f, segmentSeconds(spec.duration), spec.easing()});
  start(ticker, *held, std::move(segments));
}

}  // namespace sigil::motion
