/** @file
 * The operations on a held motion: reading the value for this frame,
 * retargeting a running ramp from where it is when the target moves (or
 * blending the change onto it), and starting an entrance from the value
 * the description declared — and the ramp itself, the keyed segments a
 * engine steps into a live value.
 */

#include "sigilmotion/advanced/Held.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace sigil::motion {

namespace {

/** A length a held motion is timed in: seconds as a FLOAT, which is the
 *  precision every held motion's delay and length are summed at before
 *  the ramp reads them as a double. */
float segmentSeconds(std::chrono::duration<float> length) {
  return length.count();
}

/** ONE KEYED RAMP writing one live value: a hold, then a list of segments
 *  played once, a number of times or for ever — every other pass
 *  backwards when it alternates — with any blended changes added on top.
 *
 *  A segment's value at a time is `from + (to − from) · ease(t / length)`
 *  in float, the time is the sum of the frame deltas in double, and the
 *  segment that owns a time is the first whose length the remaining time
 *  does not exceed — so a boundary belongs to the segment it ends. At or
 *  past the end of the last pass and the last blended change the value is
 *  where they all come to rest, the ramp stops writing, and the engine
 *  drops it on the same frame. */
class Ramp final : public detail::Stepped {
 public:
  struct Segment {
    float from = 0.0f;
    float to = 0.0f;
    double seconds = 0.0;
    Easing ease;  ///< empty: held at `from`
  };

  /** @p passes: how many times the segments play; 0 plays them for ever. */
  Ramp(std::shared_ptr<detail::Cell<float>> cell, float start, double lead,
       std::vector<Segment> segments, int passes, bool alternate)
      : m_cell(cell),
        m_start(start),
        m_lead(lead),
        m_segments(std::move(segments)),
        m_passes(passes),
        m_alternate(alternate) {
    m_writer = ++cell->writer;
    cell->moving = true;
    // One pass summed on its own for the passes; the single pass's end
    // summed from the lead segment by segment, which is the rounding its
    // last frame is decided at.
    double single = m_lead;
    for (const Segment& segment : m_segments) {
      m_pass += segment.seconds;
      single += segment.seconds;
    }
    m_end = m_passes == 1 ? single
                          : (m_passes > 1 ? m_lead + m_pass * m_passes : -1.0);
  }

  /** ADDS A CHANGE ON TOP of the running motion: @p delta eased in over
   *  @p seconds after @p delay, counted from now. The running motion keeps
   *  its course, so the value's velocity carries through. */
  void blend(float delta, double delay, double seconds, Easing ease) {
    m_layers.push_back({m_time + delay, seconds, delta, std::move(ease)});
    if (m_end >= 0.0) m_end = std::max(m_end, m_time + delay + seconds);
  }

  /** Where the motion comes to rest, blended changes included. */
  float rest() const {
    float value = passEnd(m_passes > 0 ? m_passes - 1 : 0);
    for (const Layer& layer : m_layers) value += layer.delta;
    return value;
  }

  bool advance(double deltaSeconds) override {
    // Nothing reads a value whose every holder is gone, so its motion ends
    // with it; and a motion started on the same value since has taken it
    // over.
    const std::shared_ptr<detail::Cell<float>> cell = m_cell.lock();
    if (!cell || cell->writer != m_writer) return false;
    m_time += deltaSeconds;
    if (m_end >= 0.0 && m_time >= m_end) {
      cell->value = rest();
      cell->moving = false;
      return false;
    }
    float value = base(m_time);
    for (const Layer& layer : m_layers) {
      if (m_time <= layer.begin) continue;
      const double into = m_time - layer.begin;
      const float unit = layer.seconds > 0.0
                             ? (float)std::min(into / layer.seconds, 1.0)
                             : 1.0f;
      value += layer.delta * (layer.ease ? layer.ease(unit) : unit);
    }
    cell->value = value;
    return true;
  }

 private:
  struct Layer {
    double begin = 0.0;
    double seconds = 0.0;
    float delta = 0.0f;
    Easing ease;
  };

  /** Where pass @p index ends: backwards passes end where they began. */
  float passEnd(int index) const {
    if (m_segments.empty()) return m_start;
    const bool backwards = m_alternate && (index % 2 == 1);
    return backwards ? opening() : m_segments.back().to;
  }

  /** Where a pass sets out: its first segment's start, past every
   *  segment that lasts nothing, since such a step is taken at once. */
  float opening() const {
    float value = m_segments.front().from;
    for (const Segment& segment : m_segments) {
      if (segment.seconds > 0.0) break;
      value = segment.to;
    }
    return value;
  }

  /** One pass's value @p at seconds into it, played forwards. */
  float within(double at) const {
    for (const Segment& segment : m_segments) {
      if (segment.seconds < at) {
        at -= segment.seconds;
        continue;
      }
      // A step that lasts nothing is already where it goes.
      const float unit =
          segment.seconds > 0.0 ? (float)(at / segment.seconds) : 1.0f;
      const float shaped = segment.ease ? segment.ease(unit) : 0.0f;
      return segment.from + (segment.to - segment.from) * shaped;
    }
    return m_segments.empty() ? m_start : m_segments.back().to;
  }

  /** The motion without its blended changes, @p time seconds in. */
  float base(double time) const {
    if (m_segments.empty()) return m_start;
    if (time <= m_lead) return opening();
    double into = time - m_lead;
    if (m_passes == 1) return within(into);
    if (m_passes > 1 && into >= m_pass * m_passes) return passEnd(m_passes - 1);
    const double passes = m_pass > 0.0 ? std::floor(into / m_pass) : 0.0;
    const int index = (int)passes;
    into -= passes * m_pass;
    const bool backwards = m_alternate && (index % 2 == 1);
    return within(backwards ? m_pass - into : into);
  }

  /** Held weakly: the value's holders own it, not the motion writing it. */
  std::weak_ptr<detail::Cell<float>> m_cell;
  float m_start = 0.0f;
  double m_lead = 0.0;
  std::vector<Segment> m_segments;
  int m_passes = 1;
  bool m_alternate = false;
  std::vector<Layer> m_layers;
  uint32_t m_writer = 0;
  double m_time = 0.0;
  double m_pass = 0.0;
  double m_end = 0.0;  ///< negative: runs until replaced
};

/** How many times a tween's segments play: once plus its loops, or for
 *  ever (0) when it loops for ever. */
int passesOf(int loop) { return loop < 0 ? 0 : 1 + loop; }

/** Starts a ramp on @p held's value. */
void start(Engine& engine, HeldMotion& held, std::shared_ptr<Ramp> ramp) {
  held.running = ramp;
  engine.run(std::move(ramp));
}

/** THE STEPS OF @p tween's PATH, set out from @p from: one segment per
 *  keyframe, an undurationed step taking the tween's duration divided by
 *  the number of steps and an uncurved one the tween's curve. */
std::vector<Ramp::Segment> pathSegments(const Tween<float>& tween, float from) {
  std::vector<Ramp::Segment> segments;
  const Duration share =
      tween.duration.value() / (double)tween.keyframes.size();
  float at = from;
  for (const Keyframe<float>& step : tween.keyframes) {
    segments.push_back({at, step.to,
                        segmentSeconds(step.duration.value_or(share)),
                        step.ease ? step.ease : tween.easing()});
    at = step.to;
  }
  return segments;
}

/** PLAYS @p segments ON @p held from @p from, after @p tween's delay and
 *  as many times as it repeats, toward where it rests. A looping motion
 *  holds where each pass ends before the next starts, and a step that
 *  lasts nothing is taken at once, so the value holds through the delay
 *  where the first step that moves sets out from. */
void play(Engine& engine, std::unique_ptr<HeldMotion>& held,
          const Tween<float>& tween, float from,
          std::vector<Ramp::Segment> segments) {
  if (tween.loop != 0 && tween.loopDelay > Duration{})
    segments.push_back({segments.back().to,
                        segments.back().to,
                        segmentSeconds(tween.loopDelay),
                        {}});
  float opening = from;
  for (const Ramp::Segment& segment : segments) {
    if (segment.seconds > 0.0) break;
    opening = segment.to;
  }
  if (!held) held = std::make_unique<HeldMotion>();
  held->live = opening;
  held->started = true;
  held->target = tween.rest();
  const float lead = segmentSeconds(tween.delay.value());
  start(engine, *held,
        std::make_shared<Ramp>(held->live.cell(), from, lead > 0 ? lead : 0.0,
                               std::move(segments), passesOf(tween.loop),
                               tween.alternate));
}

}  // namespace

void HeldMotion::stop() {
  const std::shared_ptr<detail::Cell<float>>& cell = live.cell();
  ++cell->writer;
  cell->moving = false;
  running.reset();
}

float valueOf(const HeldMotion* animated, const Animatable<float>& property) {
  // A live value — shaped through its stages when it has any — is read
  // here, the one place a bound float is read, so every consumer gets the
  // stages for free.
  if (property.identity()) return property.value();
  if (animated && animated->started) return animated->value();
  return property.value();
}

bool retarget(Engine& engine, std::unique_ptr<HeldMotion>& held,
              const Animatable<float>& previousValue,
              const Animatable<float>& nextValue,
              const std::optional<Tween<float>>& fallback, Place place) {
  ResolvedProperty<float> prev =
      resolveProperty(previousValue, fallback, place);
  ResolvedProperty<float> next = resolveProperty(nextValue, fallback, place);
  // Snap semantics must actually LAND: a lingering ramp from an earlier
  // transition would shadow the constant description forever
  // (valueOf prefers a started ramp), so the snap paths stop it.
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
  if (anim && anim->started && anim->isRunning() && anim->target == next.target)
    return true;
  const float current = anim && anim->started ? anim->value() : prev.target;
  const Tween<float>& how = *next.transition;
  // BLEND: the change rides on top of the motion already running, so its
  // velocity carries through instead of stopping at the retarget.
  if (how.composition == Composition::Blend && anim && anim->started &&
      anim->isRunning() && anim->running) {
    auto& ramp = static_cast<Ramp&>(*anim->running);
    ramp.blend(next.target - anim->target, segmentSeconds(how.delay.value()),
               segmentSeconds(how.duration.value()), how.easing());
    anim->target = next.target;
    return true;
  }
  // A PATH WHOSE RESTING VALUE CHANGED plays its steps again, set out
  // from the value on screen; one described again with the same rest has
  // returned above or is pruned before it gets here.
  if (const Tween<float>* path = nextValue.described();
      path && !path->keyframes.empty() && prev.target != next.target) {
    const Tween<float> tween = path->resolved(place);
    play(engine, anim, tween, current, pathSegments(tween, current));
    return true;
  }
  if (current == next.target) {
    // The value COINCIDES with the new target, but a moving ramp that
    // passed the keeps-flying guard is provably headed somewhere else —
    // left alone it would carry the value to a STALE target (permanent,
    // since identical re-describes prune). Stop it; the description's
    // own value (== next.target) shows through.
    if (anim && anim->started && anim->isRunning() &&
        anim->target != next.target)
      snapAnim();
    return anim && anim->isRunning();
  }

  if (!anim) anim = std::make_unique<HeldMotion>();
  anim->live = current;  // seed the retarget start point
  anim->started = true;
  anim->target = next.target;
  const float delay =
      segmentSeconds(how.delay.value());  // the stagger primitive
  start(engine, *anim,
        std::make_shared<Ramp>(
            anim->live.cell(), current, delay > 0 ? delay : 0.0,
            std::vector<Ramp::Segment>{{current, next.target,
                                        segmentSeconds(how.duration.value()),
                                        how.easing()}},
            1, false));
  return true;
}

void enter(Engine& engine, std::unique_ptr<HeldMotion>& held,
           const Animatable<float>& property, Place place) {
  const Tween<float>* described = property.described();
  if (!described || !described->isEntrance()) return;
  const Tween<float> tween = described->resolved(place);
  // A path that names no `from` starts where it rests, as `Tween::at`
  // reads it, so its first step is the first place it goes.
  const float from = tween.start();
  if (!tween.keyframes.empty()) {
    play(engine, held, tween, from, pathSegments(tween, from));
    return;
  }
  // A from→to that goes nowhere and does not repeat is not an entrance.
  if (!tween.to || (from == tween.to->value() && tween.loop == 0)) return;
  play(engine, held, tween, from,
       {{from, tween.to->value(), segmentSeconds(tween.duration.value()),
         tween.easing()}});
}

bool isRunning(const HeldMotion* animated, const Animatable<float>& property) {
  return property.isRunning() || (animated && animated->isRunning());
}

void progress(Engine& engine, std::unique_ptr<HeldMotion>& held,
              const Tween<float>& spec) {
  if (!held) held = std::make_unique<HeldMotion>();
  held->live = 0.0f;
  held->started = true;
  held->target = 1.0f;
  const float delay = segmentSeconds(spec.delay.value());
  start(engine, *held,
        std::make_shared<Ramp>(
            held->live.cell(), 0.0f, delay > 0 ? delay : 0.0,
            std::vector<Ramp::Segment>{{0.0f, 1.0f,
                                        segmentSeconds(spec.duration.value()),
                                        spec.easing()}},
            1, false));
}

}  // namespace sigil::motion
