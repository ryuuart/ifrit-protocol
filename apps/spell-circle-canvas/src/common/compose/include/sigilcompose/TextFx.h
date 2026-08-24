#pragma once

/** @file
 * SigilCompose text-fx presets — the stock entrances, loops and
 * combinators for the kernel's multi-track `fx()` seam, all as plain
 * comparable `TextEffect` VALUES.
 *
 * One track:
 *
 *   text(u8"KINETIC", display)
 *       .fx({.effect = fx::rise(),
 *            .stagger = {.eachMs = 28, .durationMs = 480},
 *            .progress = with(1.0f, {900ms, &ch::easeOutQuad})});
 *
 * Several tracks compose per glyph — offsets and rotations add, scale and
 * alpha multiply — and each carries its own selector, cascade and
 * progress:
 *
 *   text(u8"ONE LINE, TWO MOVES", display)
 *       .fx({.effect = fx::rise(20), .stagger = stagger(unit::Word)})
 *       .fx({.where = sel::text(u8"TWO"),
 *            .effect = fx::waveLoop(),
 *            .progress = &phase});
 *
 * A published keyframe list is a value here too: `fx::keys` takes the
 * table, `fx::seq` and `fx::mix` put whole effects in sequence and in
 * parallel, and `fx::hold` keeps one from performing before its unit's
 * beat opens.
 *
 * One-shot effects consume progress 0→1; loop effects (waveLoop) read a
 * WRAPPING bound phase (an Output stepped mod 1), and a looping CASCADE
 * (`Stagger::loopMs`) reads the same wrapping phase and re-opens every
 * unit's beat once per wrap. Everything renders
 * through batched RSXform draws — moving text is never per-glyph draw
 * calls — and every preset declares the reach its motion needs so the
 * recording cull does not truncate it.
 *
 * Every effect here also carries whether it MOVES its glyphs off the pen
 * positions the layout gave them (`TextEffect::displaces`), which is what
 * decides the grid a live run's origins are rounded to. `rise`, `slide`,
 * `pop`, `spinIn`, `scatter` and `waveLoop` move them; `typeOn`, `axis`,
 * `tint`, `scramble` and `pass` do not; `keys` reads its own table and the
 * combinators derive from their operands. Only `fx::effect` has to be told.
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose::fx {

namespace detail {
inline float easeOutCubic(float t) {
  const float u = 1 - t;
  return 1 - u * u * u;
}
/** Ease-out-expo: 1 − 2^(−10t), the curve CSS spells
 *  cubic-bezier(0.16, 1, 0.3, 1). */
inline float easeOutExpo(float t) {
  return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}
inline float easeOutBack(float t, float s = 1.70158f) {
  const float u = t - 1;
  return 1 + (s + 1) * u * u * u + s * u * u;
}
/** What a scale-only effect may push past the box: a glyph grown by
 *  `factor` about its own centre escapes by half the excess in each
 *  direction, and no effect here knows the font size at construction, so
 *  the nominal display size below stands in. Over-reporting is safe. */
inline constexpr float kNominalSizePx = 96.0f;
}  // namespace detail

/** The stagger-reveal workhorse: glyphs rise from `distancePx` below their
 *  rest while fading in. Ease-out-expo motion; alpha completes over the
 *  first 35% of local progress, so a glyph is fully opaque while it is
 *  still moving rather than fading and settling together. */
[[nodiscard]] inline TextEffect rise(float distancePx = 26) {
  return TextEffect(
      "rise", {distancePx},
      [distancePx](const GlyphInfo&, float t, Rng&) {
        GlyphMod m;
        m.dy = (1 - detail::easeOutExpo(t)) * distancePx;
        m.alpha = std::min(1.0f, t / 0.35f);
        return m;
      },
      std::abs(distancePx));
}

/** Slide-in from the side (negative = from the left). */
[[nodiscard]] inline TextEffect slide(float distancePx = -32) {
  return TextEffect(
      "slide", {distancePx},
      [distancePx](const GlyphInfo&, float t, Rng&) {
        GlyphMod m;
        m.dx = (1 - detail::easeOutCubic(t)) * distancePx;
        m.alpha = std::min(1.0f, t * 1.7f);
        return m;
      },
      std::abs(distancePx));
}

/** Scale-overshoot entrance (back.out(1.7) — the elastic pop). */
[[nodiscard]] inline TextEffect pop(float fromScale = 0.35f,
                                    float overshoot = 1.70158f) {
  // The curve peaks above 1 by roughly overshoot/10 at these parameters.
  const float peak = 1.0f + std::max(overshoot, 0.0f) * 0.1f;
  return TextEffect(
      "pop", {fromScale, overshoot},
      [fromScale, overshoot](const GlyphInfo&, float t, Rng&) {
        GlyphMod m;
        m.scale =
            fromScale + (1 - fromScale) * detail::easeOutBack(t, overshoot);
        m.alpha = std::min(1.0f, t * 2.2f);
        return m;
      },
      (peak - 1.0f) * detail::kNominalSizePx);
}

/** Tumble-in: glyphs spin from `degrees` while rising and fading. */
[[nodiscard]] inline TextEffect spinIn(float degrees = 70, float risePx = 14) {
  return TextEffect(
      "spinIn", {degrees, risePx},
      [degrees, risePx](const GlyphInfo&, float t, Rng&) {
        const float e = detail::easeOutCubic(t);
        GlyphMod m;
        m.rotateDeg = (1 - e) * degrees;
        m.dy = (1 - e) * risePx;
        m.alpha = std::min(1.0f, t * 1.7f);
        return m;
      },
      // A rotated glyph's corners swing out of its advance box; half the
      // nominal size covers any angle.
      std::abs(risePx) + detail::kNominalSizePx * 0.5f);
}

/** Hard typewriter: a glyph is absent, then simply THERE (pair with a
 *  short durationMs and Start stagger). */
[[nodiscard]] inline TextEffect typeOn() {
  return TextEffect(
      "typeOn", {},
      [](const GlyphInfo&, float t, Rng&) {
        GlyphMod m;
        m.alpha = t >= 0.5f ? 1.0f : 0.0f;
        return m;
      },
      // Coverage only: the glyph appears where it already was.
      0.0f, {}, /*displaces=*/false);
}

/** Endless float: glyph i bobs on a sine, phase-shifted per glyph. Bind
 *  progress to a WRAPPING phase Output (t = fract(seconds / period)) and
 *  set stagger.eachMs = 0 so every glyph reads the same master phase.
 *  Amplitude is in EM — keep it at or under 0.15em, past which descenders
 *  of adjacent glyphs collide — and the phase shift is RADIANS per glyph,
 *  where roughly 0.4–0.6 gives one readable travelling wave. */
[[nodiscard]] inline TextEffect waveLoop(float amplitudeEm = 0.10f,
                                         float phaseRadPerGlyph = 0.5f) {
  return TextEffect(
      "waveLoop", {amplitudeEm, phaseRadPerGlyph},
      [amplitudeEm, phaseRadPerGlyph](const GlyphInfo& g, float t, Rng&) {
        GlyphMod m;
        m.dy = std::sin(t * 6.2831853f - (float)g.index * phaseRadPerGlyph) *
               amplitudeEm * (g.fontSize > 0 ? g.fontSize : 16.0f);
        return m;
      },
      std::abs(amplitudeEm) * detail::kNominalSizePx);
}

/** Seeded scatter: every glyph flies in from its own random offset inside
 *  a `radiusPx` disc, with its own random lean. The draw is stable across
 *  frames and relayouts (Rng is seeded from the glyph's identity), which
 *  is what lets a settled scatter cache instead of jittering forever. */
[[nodiscard]] inline TextEffect scatter(float radiusPx = 40,
                                        float leanDeg = 24) {
  return TextEffect(
      "scatter", {radiusPx, leanDeg},
      [radiusPx, leanDeg](const GlyphInfo&, float t, Rng& rng) {
        const float dx = rng.signedUnit() * radiusPx;
        const float dy = rng.signedUnit() * radiusPx;
        const float lean = rng.signedUnit() * leanDeg;
        const float e = detail::easeOutCubic(t);
        GlyphMod m;
        m.dx = (1 - e) * dx;
        m.dy = (1 - e) * dy;
        m.rotateDeg = (1 - e) * lean;
        m.alpha = std::min(1.0f, t * 1.7f);
        return m;
      },
      std::abs(radiusPx) + detail::kNominalSizePx * 0.5f);
}

/** A VARIABLE-FONT AXIS held at one coordinate for every glyph the track
 *  addresses — a grade, an optical size, a slant applied at draw time with
 *  no reshape.
 *
 *  Only an ADVANCE-INVARIANT axis is honoured: the glyphs keep the pen
 *  positions shaping gave them, so an axis that moves advances would leave
 *  them sitting wrong. The runtime probes the face once per axis and
 *  refuses one that does, drawing at the shaped face and warning once —
 *  GRAD is the advance-invariant weight most faces carry, while wght
 *  belongs in the shaping style, which re-shapes. */
[[nodiscard]] inline TextEffect axis(const char (&tag)[5], float value) {
  const sigil::weave::FontVariation coordinate(tag, value);
  return TextEffect(
      "axis",
      {(float)(unsigned char)tag[0], (float)(unsigned char)tag[1],
       (float)(unsigned char)tag[2], (float)(unsigned char)tag[3], value},
      [coordinate](const GlyphInfo&, float, Rng&) {
        GlyphMod m;
        m.axis = coordinate;
        return m;
      },
      // Only an ADVANCE-INVARIANT axis is honoured, which is exactly the
      // condition that the pen positions do not move.
      0.0f, {}, /*displaces=*/false);
}

/** The same axis SWEPT across local progress: `from` at t = 0, `to` at
 *  t = 1. Pair it with a stagger and a weight rolls along the line. */
[[nodiscard]] inline TextEffect axis(const char (&tag)[5], float from,
                                     float to) {
  const sigil::weave::FontVariation coordinate(tag, from);
  return TextEffect(
      "axisSweep",
      {(float)(unsigned char)tag[0], (float)(unsigned char)tag[1],
       (float)(unsigned char)tag[2], (float)(unsigned char)tag[3], from, to},
      [coordinate, from, to](const GlyphInfo&, float t, Rng&) {
        GlyphMod m;
        sigil::weave::FontVariation driven = coordinate;
        driven.value = from + (to - from) * std::clamp(t, 0.0f, 1.0f);
        m.axis = driven;
        return m;
      },
      0.0f, {}, /*displaces=*/false);
}

/** A COLOUR REVEAL AS A CASCADE: the glyphs read @p from at local 0 and
 *  @p to at local 1 — a karaoke wipe, a highlight sweeping a word, an
 *  initial catching its colour as it lands.
 *
 *  THE ELEMENT IS SET IN `to`, AND THE EFFECT MULTIPLIES DOWN TOWARD
 *  `from`. That inversion is the one thing to get right here. A `GlyphMod`
 *  carries `colorMul`, a per-channel MULTIPLIER over every pass the glyph's
 *  style draws, and a multiplier can only take a colour toward black — so
 *  the DESTINATION is what the style paints, and the origin is reached by
 *  dividing. The arguments still read in time order and the division is
 *  done here: `fx::tint(pale, sung)` on a line set in `sung` wipes it from
 *  pale to sung. Set the line in `from` and it draws pale throughout,
 *  which is the obvious first mistake and has no diagnostic.
 *
 *  Multiplying is also what lets this tint a gradient-filled or
 *  image-filled line without knowing what fills it. Its cost is that a
 *  DESTINATION CHANNEL OF ZERO cannot be departed from — nothing multiplies
 *  0 into anything else — so that channel holds at 0 for the whole ramp
 *  whatever @p from says there. The way UP is the other two colour terms:
 *  `GlyphMod::colorAdd` is the hard flash over whatever the style paints,
 *  `GlyphMod::colorScreen` the glow that brightens toward white without
 *  clipping — both usually spoken through a `fx::keys` table.
 *
 *  Alpha is untouched: a reveal that also fades wants an alpha track, which
 *  composes with this one. The ramp is a smoothstep because a hard cut at
 *  display size flickers at any frame rate; the width of the edge is bought
 *  with the cascade's `durationMs`, not with the curve. */
[[nodiscard]] inline TextEffect tint(SkColor4f from, SkColor4f to) {
  const SkColor4f origin{to.fR > 0 ? from.fR / to.fR : 1.0f,
                         to.fG > 0 ? from.fG / to.fG : 1.0f,
                         to.fB > 0 ? from.fB / to.fB : 1.0f, 1.0f};
  return TextEffect(
      "tint", {from.fR, from.fG, from.fB, from.fA, to.fR, to.fG, to.fB, to.fA},
      [origin](const GlyphInfo&, float t, Rng&) {
        const float e = t * t * (3.0f - 2.0f * t);
        GlyphMod m;
        m.colorMul = {origin.fR + (1.0f - origin.fR) * e,
                      origin.fG + (1.0f - origin.fG) * e,
                      origin.fB + (1.0f - origin.fB) * e, 1.0f};
        return m;
      },
      // Colour only: a wipe repaints letters, it does not move them.
      0.0f, {}, /*displaces=*/false);
}

/** THE DECODING TEXT: every glyph churns through `charset` before landing
 *  on the letter the text actually says.
 *
 *  A substitution keeps the ORIGINAL glyph's pen position, so it is only
 *  honoured where the replacement has the original's advance — the runtime
 *  measures both and refuses the rest, drawing the true letter. That makes
 *  a monospaced face the natural home for this, and a charset of
 *  same-width characters the way to get it out of a proportional one.
 *
 *  Each glyph resolves at its own seeded moment and all of them have
 *  resolved by t = 1. `steps` is how many times a glyph re-rolls across its
 *  whole local time; the churn is seeded from the glyph's identity, so it
 *  is the same churn on every frame. */
[[nodiscard]] TextEffect scramble(
    std::u32string charset = U"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
    int steps = 14);

/** A SHADER PASS AS A TRACK'S EFFECT — "a shader per letter" without a
 *  shader per letter. The track's units are rendered ONCE into a layer and
 *  @p material runs once over that layer, handed each unit's box and each
 *  unit's own cascade clock as uniform data, so per-letter treatment is
 *  data rather than scene structure and the cost is one draw plus one pass
 *  whatever the unit count is.
 *
 *      auto burn = Material::sksl(emberDissolve).uniform("uEdgeWidth", .15f);
 *      text(u8"EMBER DECODE", display)
 *          .fx({.effect = fx::pass(burn),
 *               .stagger = stagger(unit::Cluster, {.eachMs = 260})});
 *
 *  THE MATERIAL MUST CARRY SKSL SOURCE (`Material::sksl(std::string, …)`),
 *  because the unit count is baked into the compiled shader — a runtime
 *  effect's array size is fixed at compile and SkSL has no uniform-bounded
 *  loop. The RUNTIME owns that specialization: it prepends
 *
 *      uniform shader uContent;        // the units' rendered layer
 *      uniform float4 uUnitRect[N];    // per unit: x, y, w, h
 *      uniform float2 uUnitPhase[N];   // per unit: local 0→1, seed
 *      const int kUnitCount = N;      // the loop bound
 *
 *  and compiles once per distinct unit count, cached for the process —
 *  write the source against those names and do not declare them. Any other
 *  material warns once and returns an EMPTY effect, so the track draws its
 *  glyphs at rest rather than nothing.
 *
 *  THE COORDINATES ARE THE NODE'S OWN PX: `main(xy)`, `uUnitRect` and the
 *  layer all share the frame the letters were laid out in, and the layer
 *  is sampled at the device's resolution, so a 2x host stays sharp with no
 *  supersampled bake. `uUnitRect` entries are the SAME rects
 *  `Composer::beatsOf` reports (that query lifts them to composer space);
 *  `uUnitPhase[i].x` is the same `Beat::localT`, driven by the track's
 *  progress through its cascade, so the pass, a mark and the glyphs can
 *  never disagree about the schedule. `uUnitPhase[i].y` is a per-unit seed
 *  in [1, 256), stable across frames and relayouts, which is what lets a
 *  seeded dissolve settle and cache instead of churning forever. Under a
 *  nested cascade there is one entry per (outer, inner) beat, matching the
 *  beats the query reports.
 *
 *  THE PASS IS BOUNDED, unlike a raw `Element::effect` shader: it paints
 *  the node's box grown by the track's `reach` and nothing outside it. An
 *  effect built here declares the material's `bleed()` as its reach, so a
 *  pass that marks beyond the letters says how far on the value that
 *  paints, or on the track.
 *
 *  ORDER AGAINST DEVIATION TRACKS on the same node: deviations apply
 *  FIRST. The layer holds the addressed glyphs as every deviation track
 *  left them — risen, scattered, tinted — and the pass reads those pixels,
 *  because a pass is post-processing and pixels are what it processes. A
 *  glyph addressed by a pass draws only inside that pass's layer, never
 *  directly as well; several pass tracks run in declaration order, each
 *  over its own selection's layer, and a glyph two passes address renders
 *  in both. A path baseline and a vertical column place glyphs before any
 *  of this, so a pass rides both, with unit rects turned the way the
 *  layout turned the letters.
 *
 *  A pass is a WHOLE-TRACK statement: inside `fx::seq`, `fx::mix` or
 *  `fx::hold` its material is not consulted and the operand contributes
 *  the identity. Sequence a pass by driving its progress; gate its onset
 *  in its own SkSL, which holds the whole schedule.
 *
 *  THE DECLARED REST — `fx::pass(m).restsAt(0)`, `.restsAt(1)`,
 *  `.restsAt(0, 1)`: the author's promise that the SkSL is an EXACT
 *  pass-through at those unit phases — at a declared phase it returns its
 *  input pixels untouched. When every addressed unit's resolved local time
 *  sits on a declared phase, the runtime skips the layer and the shader
 *  and draws the batches directly, so a pass on a node that repaints for
 *  unrelated reasons (an orbiting `onPath` ring under a settled pass)
 *  stops paying for a shader that is changing nothing. The promise is
 *  UNVERIFIABLE, in the same family as `Track::reach` and a material's
 *  `bleed()`: declare a phase where the shader is not a pass-through and
 *  the picture POPS at the seam, snapping between shaded and raw glyphs as
 *  the schedule crosses the declared phase, with no diagnostic. The
 *  comparison is exact — which the schedule supplies, a one-shot cascade
 *  clamping a unit to exactly 0 before its beat and exactly 1 after. Under
 *  a LOOPING cascade (`Stagger::loopMs`) a unit touches 0 only at the
 *  instant its beat re-opens, so `restsAt(0)` effectively never engages
 *  there — correctly, the cycle is always mid-flight somewhere — while a
 *  unit RESTS at exactly 1 between beats, so `restsAt(1)` engages whenever
 *  no beat is mid-cycle. Undeclared, a pass always runs. The declaration
 *  rides the effect's comparable params, so two passes differing only in
 *  their rests compare unequal and re-patch. */
[[nodiscard]] TextEffect pass(Material material);

/** THE ESCAPE HATCH: an ad-hoc effect body under an author-given key.
 *
 *  The key IS the identity — two effects with the same key compare equal
 *  and the reconciler will prune one onto the other, so give a different
 *  body a different key or the old one silently keeps drawing. Bake the
 *  parameters that vary into the key (or into `params`) rather than
 *  capturing them silently.
 *
 *  `reach` is how far past the element's box the body may push a glyph;
 *  the default covers the shipped presets' range.
 *
 *  THE ONE PLACEMENT FACT THE LIBRARY CANNOT INFER lives here too. Every
 *  other effect answers `TextEffect::displaces` for itself — a preset knows
 *  its own deviation, `fx::keys` reads its table, `fx::seq`, `fx::mix` and
 *  `fx::hold` derive from their operands — but a lambda is opaque until it
 *  runs, so this door assumes the moving answer and takes
 *  `.displacing(false)` as the promise that the body leaves every pen
 *  position alone. */
[[nodiscard]] inline TextEffect effect(std::string key, GlyphModFn program,
                                       float reach = 48.0f,
                                       std::vector<float> params = {}) {
  return TextEffect(std::move(key), std::move(params), std::move(program),
                    reach);
}

/** ONE ENTRY OF A `fx::keys` TABLE: where it sits in local time, the
 *  deviation there, and — optionally — the curve for the segment that
 *  STARTS at it. */
struct Key {
  float at = 0;  ///< local time, 0→1
  GlyphMod mod;  ///< the deviation at that moment
  /** The curve this entry's own segment is interpolated with, overriding
   *  the table's. Unset takes the table's; the LAST entry's is never read,
   *  because no segment starts there. */
  choreograph::EaseFn ease;
};

/** THE KEYFRAME TABLE: a list of (local time, deviation) entries, and the
 *  curve between them.
 *
 *      const TextEffect rubberBand = fx::keys({
 *          {0.00f, {}},
 *          {0.30f, {.scaleX = 1.25f, .scaleY = 0.75f}},
 *          {0.50f, {.scaleX = 1.15f, .scaleY = 0.85f}},
 *          {1.00f, {}},
 *      }, &choreograph::easeInOutCubic);
 *
 *  Entries are read IN ORDER and each pair is one segment; local time
 *  before the first entry holds the first entry's deviation and time after
 *  the last holds the last's. Two entries at the same moment are a STEP,
 *  and the later one is what the step lands on.
 *
 *  THE CURVE APPLIES PER SEGMENT, not across the table: `at` says where a
 *  segment ends, the curve says how it is crossed, and every segment runs
 *  the whole curve. That is what a published keyframe list means — a table
 *  crossed by one curve end to end would ease into the first entry and out
 *  of the last and run the middle at whatever slope the curve happened to
 *  have there. Unset, a segment is linear.
 *
 *  Interpolation is COMPONENTWISE and follows the `fx::seq` crossfade
 *  exactly, because it is the same arithmetic: `codepoint` cuts at the
 *  middle of the segment rather than lerping, since there is no half-way
 *  glyph between two outlines; `axis` lerps only when the two entries name
 *  the SAME tag, and otherwise cuts the same way.
 *
 *  The table IS the identity — two `keys` over the same numbers and the
 *  same named curves compare equal and prune — and it declares its own
 *  reach from the offsets, growths and leans it publishes. */
[[nodiscard]] TextEffect keys(std::vector<Key> table,
                              choreograph::EaseFn ease = nullptr);

/** NOTHING UNTIL THE BEAT OPENS: `effect` as it is, except that a unit
 *  whose beat has not begun paints nothing at all.
 *
 *  A cascade hands every unit a local time clamped to [0,1], so a unit
 *  waiting its turn is handed 0 — and an effect that deviates at 0 is
 *  already performing before its beat. A substitution is the case that
 *  shows: `fx::scramble` churns from local 0, so a glyph still waiting
 *  shows a WRONG letter rather than no letter. This says "not yet".
 *
 *  The hold is ALPHA 0, not the identity: the point is a glyph that has not
 *  arrived, and the identity is a glyph sitting at rest, which for a
 *  substitution is the answer the effect exists to withhold. Alpha
 *  multiplies across tracks, so this is a VETO — a glyph whose held track
 *  has not opened paints nothing however many other tracks have opened on
 *  it. Put the hold on the track that owns the glyph's arrival.
 *
 *  A ONE-SHOT effect is what this is for. A loop effect reads a wrapping
 *  phase that passes through 0 on every cycle, and a held loop would blink
 *  its glyphs out each time it did. A LOOPING CASCADE (`Stagger::loopMs`)
 *  has nothing for this to withhold either: its fold keeps every unit
 *  somewhere in its cycle — there is no "not yet" — and local time touches
 *  0 only at the instant a beat re-opens, so the veto blanks that single
 *  instant and nothing else. An effect on a looping cascade gates its own
 *  arrival, the way a streak table's head is its own entrance. */
[[nodiscard]] TextEffect hold(TextEffect effect);

/** PHASES IN LOCAL TIME: each phase sees a renormalized 0→1 over its own
 *  window, so `fx::seq(a.until(0.35f), b.until(0.75f).xfade(0.10f), c)`
 *  plays `a` over the first 35% of every unit's beat, `b` over the next
 *  40% and `c` over the rest — each running its full curve.
 *
 *  The default joint is a hard cut. `.xfade(f)` on the ENDING phase lerps
 *  its deviation into the next one's, componentwise, over the last `f` of
 *  local time before the joint.
 *
 *  A sequence is NOT a keyframe table over effects, and neither combinator
 *  is the other's special case: a phase is an EFFECT re-clocked over its
 *  window and free to move throughout it, where a key is one deviation
 *  standing still and lerped toward. What they do share is the
 *  componentwise interpolation — the crossfade here and a segment there run
 *  the same arithmetic, so the substitutions cut the same way in both. */
[[nodiscard]] TextEffect seq(std::vector<Phase> phases);
template <typename... Rest>
[[nodiscard]] TextEffect seq(Phase first, Rest&&... rest) {
  std::vector<Phase> phases;
  phases.reserve(1 + sizeof...(Rest));
  phases.push_back(std::move(first));
  (phases.push_back(Phase(std::forward<Rest>(rest))), ...);
  return seq(std::move(phases));
}

/** BOTH AT ONCE: evaluates every operand at the same local t and composes
 *  the results by the same algebra stacked tracks use — dx/dy and
 *  rotation add, scale and alpha multiply. Comparable when its operands
 *  are. */
[[nodiscard]] TextEffect mix(std::vector<TextEffect> effects);
template <typename... Rest>
[[nodiscard]] TextEffect mix(TextEffect first, Rest&&... rest) {
  std::vector<TextEffect> effects;
  effects.reserve(1 + sizeof...(Rest));
  effects.push_back(std::move(first));
  (effects.push_back(std::forward<Rest>(rest)), ...);
  return mix(std::move(effects));
}

}  // namespace sigil::compose::fx
