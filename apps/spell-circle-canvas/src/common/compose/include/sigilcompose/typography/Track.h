#pragma once

/** @file
 * @ingroup compose-typography
 *
 * SigilCompose typography — the TRACK: one entry of a text leaf's `textFx()`
 * list, which is which glyphs (`weave::Selector`), what deviation from rest
 * (`TextEffect`), when each unit's beat opens and how long it runs (a
 * `motion::Tween`, the one description of a motion, whose staggered
 * fields resolve over the units as siblings), what a unit is,
 * and the master progress that drives it — with `Beats`, which list a
 * schedule numbers its beats against, and `Beat`, one beat of a resolved
 * schedule read back where the text put it.
 */

#include <sigilgeometry/path/Outline.h>
#include <sigilcompose/typography/Selector.h>
#include <sigilcompose/typography/TextEffect.h>
#include <sigilcompose/typography/TextUnit.h>
#include <sigilcore/comparable/Fields.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Interpolate.h>
#include <sigilmotion/values/Tween.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>

#include <cstdint>

namespace sigil::compose {

/** WHICH LIST A SCHEDULE NUMBERS ITS BEATS AGAINST.
 *
 *  `Selection` numbers the units the track's OWN selector resolved: a
 *  track addressing one word beats once, whatever the paragraph's word
 *  count is. That is the right answer for a track that owns its text, and
 *  the wrong one for two tracks sharing a paragraph — their beats line up
 *  only while their selections happen to resolve lists of the same length,
 *  and the frame they stop doing so the two halves of every unit start
 *  arriving at different times with no diagnostic.
 *
 *  `Text` numbers every unit of the schedule's granularity in the whole
 *  paragraph, addressed or not, so word ten is beat ten in every track
 *  that beats over words. Two tracks that partition one paragraph then
 *  share one clock BY CONSTRUCTION rather than by coincidence. */
enum class Beats : uint8_t { Selection, Text };

/** The beat-numbering names, spelled the way a schedule reads:
 *  `beats::Text`. */
namespace beats {
inline constexpr Beats Selection = Beats::Selection;
inline constexpr Beats Text = Beats::Text;
}  // namespace beats

/** ONE TRACK: which glyphs, what deviation, how the beats spread, and the
 *  master progress that drives it.
 *
 *  `progress` takes the full Animatable treatment — a plain constant,
 *  a `with()`/`animate()` transition (retarget-safe: each track owns its
 *  own transition slot, so retargeting the second track leaves the first
 *  alone), or a live value. One-shot effects consume 0→1; loop
 *  effects read a WRAPPING bound phase. While any track's progress moves
 *  the element paints live; once every track settles it caches like a
 *  static leaf. */
struct Track {
  sigil::weave::Selector where;  ///< default: every glyph
  TextEffect effect;             /**< what it does */
  /** THE MOTION EVERY UNIT RUNS, as a collective: the units are
   *  siblings, so `.delay = motion::stagger(30ms)` steps from one unit to
   *  the next, `motion::stagger({0ms, 620ms})` spreads a fixed span across
   *  however many there are, `motion::cues({…})` states every start, and a
   *  plain duration opens every unit together. `.duration` is how long one
   *  unit's own motion lasts — under `within`, one inner unit's. A `loop`
   *  other than zero re-opens every beat once per beat plus `.loopDelay`,
   *  offset by its start, for as long as the master progress wraps, and
   *  `.alternate` runs every other cycle backwards. What a unit's motion
   *  runs is its local progress: from `.from` (0) to `.to` (1) on `.ease`
   *  — straight when none is named, since the effect carries its own
   *  curve — through any `.keyframes`, and a `.from`, `.to` or keyframe
   *  written as a `stagger()` resolves per unit too. The master progress
   *  [0,1] spans the last unit's start plus its duration: the per-unit
   *  time remap, which is SigilMotion's and says nothing about text. The
   *  fields after this say what a unit IS, which is the whole of what makes
   *  this a schedule over TEXT.
   *
   *  Written whole, a tween takes `motion::Tween`'s own defaults: no
   *  delay (every unit together) and 250ms. */
  motion::Tween<float> tween{.duration = std::chrono::milliseconds(450),
                             .delay = motion::stagger(std::chrono::milliseconds(30))};
  /** A second stagger inside every beat, over `innerUnit`: words, then the
   *  letters inside each word. */
  std::optional<motion::Staggered<motion::Duration>> within;
  /** Which units get a beat. It is what makes the remap above more than
   *  per-glyph spacing: `unit = weave::Unit::Word` beats once per word, and
   *  every glyph of that word shares its beat. The default,
   *  `weave::Unit::Cluster`, is per-glyph for ordinary Latin text and keeps a
   *  base letter attached to its combining marks everywhere else. It is
   *  the word an Annotation spells the same way: one `Unit`, one name for
   *  the thing a schedule and a reading are both addressed by. */
  sigil::weave::Unit unit = sigil::weave::Unit::Cluster;
  /** Which units the NESTED stagger — `within` — beats over inside each of
   *  `unit`'s beats. Read only when `within` is set. */
  sigil::weave::Unit innerUnit = sigil::weave::Unit::Glyph;
  /** WHICH LIST those beats are numbered against — see `Beats`. The
   *  default numbers the track's own selection, which is what a track
   *  that owns its text means; `beats::Text` numbers the paragraph, which
   *  is what two tracks partitioning one paragraph need if they are to
   *  share a clock. ONE setting governs both levels of a nested schedule,
   *  as one `loop` governs both periods. */
  Beats beatsOver = Beats::Selection;
  motion::Animatable<float> progress = 1.0f;
  /** Pixels beyond the element's box this track may paint, which the
   *  recording cull grows by. Negative means "ask the effect", which is
   *  what every preset answers for itself; set it when a keyed lambda
   *  throws glyphs further than the default allows. Over-reporting is
   *  safe, under-reporting truncates cached output with no diagnostic. */
  float reach = -1.0f;
  /** SKIP THE SNAPPING for the glyphs this track addresses. A driven
   *  rotation, alpha, colour multiplier and axis coordinate are quantized
   *  before they reach the draw, because each distinct value is both a
   *  distinct batch bucket and a distinct glyph-atlas strike. Continuous
   *  values buy smoothness with exactly that: one strike minted per value
   *  and every addressed glyph rasterized again every frame. Set it where
   *  the steps show — a slow lift at display size, a tint sweeping along a
   *  wordmark — and nowhere else. A glyph any addressing track declares
   *  continuous is continuous. */
  bool continuous = false;

  /** How far this track really reaches: its own number when it declares
   *  one, otherwise its effect's. */
  [[nodiscard]] float reachPx() const {
    return reach >= 0 ? reach : effect.reach();
  }
  /** The track's timing as the schedule reads it. */
  [[nodiscard]] motion::Timing timing() const {
    return motion::timingOf(tween, within);
  }
  /** WHAT A UNIT'S MOTION READS at local progress @p local — its place
   *  in its own beat, 0 to 1 — for the unit at @p place among the
   *  siblings: `from` to `to` on the tween's curve, through its keyframes.
   *  A tween naming none of them reads @p local itself. */
  [[nodiscard]] float unitProgress(float local, motion::Place place = {}) const {
    if (!tween.from && !tween.to && tween.keyframes.empty() && !tween.ease)
      return local;
    const float start = tween.from ? tween.from->at(place) : 0.0f;
    const motion::Easing& curve = tween.ease;
    const auto shaped = [&curve](const motion::Easing& own, float unit) {
      if (own) return own(unit);
      return curve ? curve(unit) : unit;
    };
    if (tween.keyframes.empty())
      return motion::interpolate(start, tween.to ? tween.to->at(place) : 1.0f,
                                 shaped({}, local));
    // Keyframes split the unit's own duration: one with none takes an
    // equal share, and the local progress walks them in order.
    const motion::Duration share =
        tween.duration.at(place) / (double)tween.keyframes.size();
    motion::Duration total{};
    for (const motion::Keyframe<float>& step : tween.keyframes)
      total += step.duration.value_or(share);
    motion::Duration into = total * (double)local;
    float at = start;
    for (const motion::Keyframe<float>& step : tween.keyframes) {
      const motion::Duration length = step.duration.value_or(share);
      if (into <= length) {
        const float unit = length > motion::Duration{} ? (float)(into / length) : 1.0f;
        return motion::interpolate(at, step.to, shaped(step.ease, unit));
      }
      into -= length;
      at = step.to;
    }
    return at;
  }
  /** THE SPAN what `progress` maps onto when this track numbers
   *  @p unitCount units, each holding @p innerUnitCount inner units — the
   *  moment the last beat closes, and above all the duration a progress
   *  transition should carry so the schedule runs at its authored times.
   *  `Composer::scheduleSpan` reads the same number off a MOUNTED track,
   *  with the unit counts the laid-out text supplies; the two agree because
   *  one resolved body computes both. */
  [[nodiscard]] motion::Duration span(uint32_t unitCount,
                                      uint32_t innerUnitCount = 1) const {
    return timing().span(unitCount, innerUnitCount);
  }
  /** Structural equality, EXCLUDING `progress` — an Animatable is compared
   *  where every other animated slot is, by the reconciler. */
  bool sameShape(const Track& other) const {
    return where == other.where && effect == other.effect &&
           motion::tweenEqual(tween, other.tween) && within == other.within &&
           unit == other.unit &&
           innerUnit == other.innerUnit && beatsOver == other.beatsOver &&
           reach == other.reach && continuous == other.continuous;
  }
  /** Full equality: the shape above plus the progress. */
  bool operator==(const Track& other) const {
    return sameShape(other) && motion::propertyEqual(progress, other.progress);
  }
};

/** FIELD PIN: a field added to Track is a build failure until it is ruled
 *  on in sameShape() above — participate, or a stated reason not to — and
 *  named in this binding. A timing field left out makes two different
 *  schedules compare equal, the text node prunes, and it keeps beating to
 *  the old ladder forever. `progress` is deliberately NOT compared in
 *  sameShape(): it is an Animatable, and the reconciler compares it through
 *  propertyEqual with every other animated slot. A staggered field converts
 *  from anything its value does, which a counted pin cannot see past, so
 *  this one is spelled out. Defined here, never called. */
inline void trackFieldPin(Track& track) {
  auto& [where, effect, tween, within, unit, innerUnit, beatsOver, progress,
         reach, continuous] = track;
  (void)where, (void)effect, (void)tween, (void)within, (void)unit,
      (void)innerUnit, (void)beatsOver, (void)progress, (void)reach,
      (void)continuous;
}

/** ONE BEAT OF A RESOLVED SCHEDULE, WHERE THE TEXT PUT IT — what
 *  `Composer::beatsOf` reports.
 *
 *  A stagger is otherwise an invisible remap: it numbers units, spreads
 *  them, and tells nobody. Anything that must travel WITH a schedule and is
 *  not a glyph — a bouncing ball, a playhead, a travelling underline, a
 *  caret, a per-unit meter — then has to restate the delay ladder in its own
 *  arithmetic, which stops agreeing with the engine the moment the schedule
 *  nests or takes a cue table. This is the schedule read back instead.
 *
 *  The schedule half — `unitIndex`, `start`, `localProgress`, `running` —
 *  is `motion::Beat`, answered by the same schedule the glyphs are drawn
 *  through. What this library adds is where the beat LANDED. */
struct Beat : motion::Beat {
  /** The unit's laid-out rect, in the composer's coordinate space: the
   *  axis-aligned bound of the advance boxes the layout placed for the
   *  glyphs this track addresses in this beat. It follows a wrapped line,
   *  a mixed-style run's own size, a path run's curve and a vertical
   *  column's axis, because it is read off the placement rather than
   *  measured again. */
  geometry::path::Rect rect;

  bool operator==(const Beat&) const = default;
};

}  // namespace sigil::compose
