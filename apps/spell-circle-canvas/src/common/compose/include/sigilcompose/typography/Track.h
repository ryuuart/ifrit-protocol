#pragma once

/** @file
 * SigilCompose typography — the TRACK: one entry of a text leaf's `fx()`
 * list, which is which glyphs (`weave::Selector`), what deviation from rest
 * (`TextEffect`), how the beats spread (`motion::Spread`), what a unit is,
 * and the master progress that drives it — with `Beats`, which list a
 * cascade numbers its beats against, and `Beat`, one beat of a resolved
 * cascade read back where the text put it.
 */

#include <include/core/SkRect.h>
#include <sigilcompose/typography/Selector.h>
#include <sigilcompose/typography/TextEffect.h>
#include <sigilcompose/typography/TextUnit.h>
#include <sigilcore/comparable/Fields.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>

#include <cstdint>

namespace sigil::compose {

/** WHICH LIST A CASCADE NUMBERS ITS BEATS AGAINST.
 *
 *  `Selection` numbers the units the track's OWN selector resolved: a
 *  track addressing one word beats once, whatever the paragraph's word
 *  count is. That is the right answer for a track that owns its text, and
 *  the wrong one for two tracks sharing a paragraph — their beats line up
 *  only while their selections happen to resolve lists of the same length,
 *  and the frame they stop doing so the two halves of every unit start
 *  arriving at different times with no diagnostic.
 *
 *  `Text` numbers every unit of the cascade's granularity in the whole
 *  paragraph, addressed or not, so word ten is beat ten in every track
 *  that beats over words. Two tracks that partition one paragraph then
 *  share one clock BY CONSTRUCTION rather than by coincidence. */
enum class Beats : uint8_t { Selection, Text };

/** The beat-numbering names, spelled the way a cascade reads:
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
 *  alone), or a `ch::Output` binding. One-shot effects consume 0→1; loop
 *  effects read a WRAPPING bound phase. While any track's progress moves
 *  the element paints live; once every track settles it caches like a
 *  static leaf. */
struct Track {
  sigil::weave::Selector where;  ///< default: every glyph
  TextEffect effect; /**< what it does */
  /** THE PER-UNIT TIME REMAP (the GSAP stagger model), which is
   *  SigilMotion's and says nothing about text: the master progress
   *  [0,1] spans `durationMs + eachMs·(N−1)` of virtual time, where N is
   *  the number of UNITS the cascade numbers, and unit i starts after its
   *  delay and runs for `durationMs`. The three fields below say what a
   *  unit IS, which is the whole of what makes this a cascade over TEXT
   *  rather than over a set's children or a feed's rows. */
  motion::Spread stagger;
  /** Which units get a beat. It is what makes the remap above more than
   *  per-glyph spacing: `over = weave::unit::Word` beats once per word, and
   *  every glyph of that word shares its beat. The default,
   *  `weave::unit::Cluster`, is per-glyph for ordinary Latin text and keeps a
   *  base letter attached to its combining marks everywhere else. */
  sigil::weave::Unit over = sigil::weave::Unit::Cluster;
  /** Which units the NESTED cascade — `stagger.then({…})` — beats over
   *  inside each of `over`'s beats. Read only when the spread nests; a
   *  spread with no inner level never looks at it. */
  sigil::weave::Unit innerOver = sigil::weave::Unit::Glyph;
  /** WHICH LIST those beats are numbered against — see `Beats`. The
   *  default numbers the track's own selection, which is what a track
   *  that owns its text means; `beats::Text` numbers the paragraph, which
   *  is what two tracks partitioning one paragraph need if they are to
   *  share a clock. ONE setting governs both levels of a nested cascade,
   *  as one `loopMs` governs both periods. */
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
  /** THE VIRTUAL SPAN, in ms: what `progress` maps onto when this track's
   *  cascade numbers @p unitCount units, each holding @p innerUnitCount
   *  units of a nested spread — the moment the last beat closes, and above
   *  all the duration a progress transition should carry so the schedule
   *  runs at its authored ms. `Composer::cascadeSpanMs` reads the same
   *  number off a MOUNTED track, with the unit counts the laid-out text
   *  supplies; the two agree because one resolved-cascade body computes
   *  both. */
  [[nodiscard]] float spanMs(uint32_t unitCount,
                             uint32_t innerUnitCount = 1) const {
    return stagger.spanMs(unitCount, innerUnitCount);
  }
  /** Structural equality, EXCLUDING `progress` — an Animatable is compared
   *  where every other animated slot is, by the reconciler. */
  bool sameShape(const Track& other) const {
    return where == other.where && effect == other.effect &&
           stagger == other.stagger && over == other.over &&
           innerOver == other.innerOver && beatsOver == other.beatsOver &&
           reach == other.reach && continuous == other.continuous;
  }
  /** Full equality: the shape above plus the progress. */
  bool operator==(const Track& other) const {
    return sameShape(other) && motion::propEqual(progress, other.progress);
  }
};

// FIELD PIN: a field added to Track is a build failure until it is ruled
// on in sameShape() above — participate, or a stated reason not to — and
// the count bumped. A cascade field left out makes two different cascades
// compare equal, the text node prunes, and it keeps beating to the old
// ladder forever. `progress` is deliberately NOT compared in sameShape():
// it is an Animatable, and the reconciler compares it through propEqual
// with every other animated slot.
static_assert(core::kFieldCount<Track> == 9,
              "Track gained or lost a field — rule on it in "
              "Track::sameShape(), then bump this count.");

/** ONE BEAT OF A RESOLVED CASCADE, WHERE THE TEXT PUT IT — what
 *  `Composer::beatsOf` reports.
 *
 *  A stagger is otherwise an invisible remap: it numbers units, spreads
 *  them, and tells nobody. Anything that must travel WITH a cascade and is
 *  not a glyph — a bouncing ball, a playhead, a travelling underline, a
 *  caret, a per-unit meter — then has to restate `i · eachMs` in its own
 *  arithmetic, which stops agreeing with the engine the moment the cascade
 *  nests or takes a cue table. This is the schedule read back instead.
 *
 *  The schedule half — `unitIndex`, `startMs`, `localT`, `active` — is
 *  `motion::Beat`, answered by the same cascade the glyphs are drawn
 *  through. What this library adds is where the beat LANDED. */
struct Beat : motion::Beat {
  /** The unit's laid-out rect, in the composer's coordinate space: the
   *  axis-aligned bound of the advance boxes the layout placed for the
   *  glyphs this track addresses in this beat. It follows a wrapped line,
   *  a mixed-style run's own size, a path run's curve and a vertical
   *  column's axis, because it is read off the placement rather than
   *  measured again. */
  SkRect rect = SkRect::MakeEmpty();

  bool operator==(const Beat&) const = default;
};

}  // namespace sigil::compose
