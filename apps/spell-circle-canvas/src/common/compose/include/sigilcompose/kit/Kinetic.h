#pragma once

/** @file
 * SigilCompose KIT — kinetic type: the stock entrances and loops for the
 * kernel's multi-track `fx()` seam, all as plain comparable `TextEffect`
 * VALUES built from the same constructor any caller may use.
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
 *       .fx({.effect = fx::rise(20), .over = weave::unit::Word})
 *       .fx({.where = weave::sel::text(u8"TWO"),
 *            .effect = fx::waveLoop(),
 *            .progress = &phase});
 *
 * The effects the runtime itself evaluates — `fx::keys`, `fx::seq`,
 * `fx::mix`, `fx::hold`, `fx::scramble`, `fx::pass` and the `fx::effect`
 * door — are the seam's, declared with it in
 * <sigilcompose/typography/TextEffect.h>; this header holds the presets,
 * which are values over that seam and need nothing it does not expose.
 *
 * One-shot effects consume progress 0→1; loop effects (waveLoop) read a
 * WRAPPING bound phase (an Output stepped mod 1), and a looping CASCADE
 * (`motion::Spread::loopMs`) reads the same wrapping phase and re-opens
 * every unit's beat once per wrap. Everything renders through batched
 * RSXform draws — moving text is never per-glyph draw calls — and every
 * preset declares the reach its motion needs so the recording cull does
 * not truncate it.
 *
 * Every effect here also carries whether it MOVES its glyphs off the pen
 * positions the layout gave them (`TextEffect::displaces`), which is what
 * decides the grid a live run's origins are rounded to. `rise`, `slide`,
 * `pop`, `spinIn`, `scatter` and `waveLoop` move them; `typeOn`,
 * `variableAxisSweep` and `tint` do not.
 */

#include <choreograph/Easing.h>
#include <include/core/SkColor.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/typography/TextEffect.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Transition.h>
#include <sigilweave/style/ShapingStyle.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace sigil::compose::fx {

// A scale-only effect's reach is read against `kNominalSizePx`, the
// display size the seam declares reaches at when no effect knows its font
// size at construction. `motion::ease::outBack` is the same curve as
// `Curve::OutBack` in the form a Transition holds.

/** THE CURVE an entrance settles on — named rather than a callable,
 *  because a preset shapes one float per glyph per frame and an `EaseFn`
 *  through a `std::function` puts an indirect call in that loop. These
 *  are Choreograph's own, called directly by the body. `OutBack` is the
 *  shape-parameterised overshoot the elastic pop lands with. */
enum class Curve : uint8_t { OutCubic, OutExpo, OutBack };

/** THE ENTRANCE: one shape for every staggered reveal, said in the lanes
 *  a glyph can travel on.
 *
 *  Every entrance is the same sentence — the glyph starts displaced and
 *  the displacement is multiplied by `1 - curve(t)` until it is home,
 *  while the alpha completes over the first `fadeOver` of local progress
 *  so a glyph is opaque while it is still moving. What differs between a
 *  rise, a slide and a tumble is only WHICH LANES carry the displacement,
 *  so they are props here and not six bodies.
 *
 *      text(u8"KINETIC", display).fx({.effect = fx::enter({.dy = 26})})
 *
 *  The `scatter` lanes are the one lane that is not a constant: each
 *  glyph draws its own offset inside a `scatterPx` disc, and its own lean
 *  up to `scatterLeanDeg`, from the stream seeded on the glyph's own
 *  identity — so the draw is stable across frames and relayouts, which is
 *  what lets a settled scatter cache instead of jittering forever. */
struct Entrance {
  float dx = 0;                ///< px to the side the glyph comes in from
  float dy = 0;                ///< px below (positive) or above its rest
  float rotateDeg = 0;         ///< the lean it straightens out of
  float fromScale = 1;         ///< the size it grows from (1 = no growth)
  float overshoot = 1.70158f;  ///< OutBack's shape, ignored by the others
  float scatterPx = 0;         ///< seeded per-glyph disc, added to dx/dy
  float scatterLeanDeg = 0;    ///< seeded per-glyph lean, added to rotateDeg
  /** The share of local progress the fade takes; 0 enters at full alpha. */
  float fadeOver = 0.35f;
  Curve curve = Curve::OutCubic;

  bool operator==(const Entrance&) const = default;
};

/** The entrance as an effect value. */
[[nodiscard]] inline TextEffect enter(Entrance e) {
  // A rotated or scaled glyph swings its corners out of its advance box;
  // half the nominal size covers any angle, and OutBack's overshoot peaks
  // about a tenth of its shape above 1.
  float reach =
      std::max(std::abs(e.dx), std::abs(e.dy)) + std::abs(e.scatterPx);
  if (e.rotateDeg != 0 || e.scatterLeanDeg != 0) reach += kNominalSizePx * 0.5f;
  if (e.curve == Curve::OutBack)
    reach += std::max(e.overshoot, 0.0f) * 0.1f * kNominalSizePx;
  const bool moves = e.dx != 0 || e.dy != 0 || e.rotateDeg != 0 ||
                     e.fromScale != 1 || e.scatterPx != 0 ||
                     e.scatterLeanDeg != 0;
  return TextEffect(
      "enter",
      {e.dx, e.dy, e.rotateDeg, e.fromScale, e.overshoot, e.scatterPx,
       e.scatterLeanDeg, e.fadeOver, (float)e.curve},
      [e](const GlyphInfo&, float t, core::noise::Mix64Stream& rng) {
        const float eased = e.curve == Curve::OutExpo
                                ? choreograph::easeOutExpo(t)
                            : e.curve == Curve::OutBack
                                ? choreograph::easeOutBack(t, e.overshoot)
                                : choreograph::easeOutCubic(t);
        const float left = 1.0f - eased;
        float dx = e.dx, dy = e.dy, lean = e.rotateDeg;
        if (e.scatterPx != 0) {
          dx += rng.signedUnit() * e.scatterPx;
          dy += rng.signedUnit() * e.scatterPx;
        }
        if (e.scatterLeanDeg != 0) lean += rng.signedUnit() * e.scatterLeanDeg;
        GlyphMod m;
        m.dx = left * dx;
        m.dy = left * dy;
        m.rotateDeg = left * lean;
        if (e.fromScale != 1) m.scale = e.fromScale + (1 - e.fromScale) * eased;
        m.alpha = e.fadeOver > 0 ? std::min(1.0f, t / e.fadeOver) : 1.0f;
        return m;
      },
      reach, {}, moves);
}

/** The stagger-reveal workhorse: glyphs rise from `distancePx` below their
 *  rest while fading in. Ease-out-expo motion; alpha completes over the
 *  first 35% of local progress, so a glyph is fully opaque while it is
 *  still moving rather than fading and settling together. */
[[nodiscard]] inline TextEffect rise(float distancePx = 26) {
  return enter({.dy = distancePx, .curve = Curve::OutExpo});
}

/** Slide-in from the side (negative = from the left). */
[[nodiscard]] inline TextEffect slide(float distancePx = -32) {
  return enter({.dx = distancePx, .fadeOver = 1.0f / 1.7f});
}

/** Scale-overshoot entrance (back.out(1.7) — the elastic pop). */
[[nodiscard]] inline TextEffect pop(float fromScale = 0.35f,
                                    float overshoot = 1.70158f) {
  return enter({.fromScale = fromScale,
                .overshoot = overshoot,
                .fadeOver = 1.0f / 2.2f,
                .curve = Curve::OutBack});
}

/** Tumble-in: glyphs spin from `degrees` while rising and fading. */
[[nodiscard]] inline TextEffect spinIn(float degrees = 70, float risePx = 14) {
  return enter({.dy = risePx, .rotateDeg = degrees, .fadeOver = 1.0f / 1.7f});
}

/** Seeded scatter: every glyph flies in from its own random offset inside
 *  a `radiusPx` disc, with its own random lean. */
[[nodiscard]] inline TextEffect scatter(float radiusPx = 40,
                                        float leanDeg = 24) {
  return enter({.scatterPx = radiusPx,
                .scatterLeanDeg = leanDeg,
                .fadeOver = 1.0f / 1.7f});
}

/** Hard typewriter: a glyph is absent, then simply THERE (pair with a
 *  short durationMs and Start stagger). */
[[nodiscard]] inline TextEffect typeOn() {
  return TextEffect(
      "typeOn", {},
      [](const GlyphInfo&, float t, core::noise::Mix64Stream&) {
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
      [amplitudeEm, phaseRadPerGlyph](const GlyphInfo& g, float t,
                                      core::noise::Mix64Stream&) {
        GlyphMod m;
        m.dy = std::sin(t * geometry::path::kTau -
                        (float)g.index * phaseRadPerGlyph) *
               amplitudeEm * (g.fontSize > 0 ? g.fontSize : 16.0f);
        return m;
      },
      std::abs(amplitudeEm) * kNominalSizePx);
}

/** A variable-font axis SWEPT across local progress: `from` at t = 0,
 *  `to` at t = 1. Pair it with a stagger and a weight rolls along the
 *  line. The held coordinate is `TextEffect::variableAxis`, on the seam,
 *  because the span verb that holds an axis is built on it. */
[[nodiscard]] inline TextEffect variableAxisSweep(const char (&tag)[5],
                                                  float from, float to) {
  const sigil::weave::FontVariation coordinate(tag, from);
  return TextEffect(
      "variableAxisSweep",
      {(float)(unsigned char)tag[0], (float)(unsigned char)tag[1],
       (float)(unsigned char)tag[2], (float)(unsigned char)tag[3], from, to},
      [coordinate, from, to](const GlyphInfo&, float t,
                             core::noise::Mix64Stream&) {
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
      [origin](const GlyphInfo&, float t, core::noise::Mix64Stream&) {
        const float e = motion::ease::smoothstep(t);
        GlyphMod m;
        m.colorMul = {origin.fR + (1.0f - origin.fR) * e,
                      origin.fG + (1.0f - origin.fG) * e,
                      origin.fB + (1.0f - origin.fB) * e, 1.0f};
        return m;
      },
      // Colour only: a wipe repaints letters, it does not move them.
      0.0f, {}, /*displaces=*/false);
}

}  // namespace sigil::compose::fx
