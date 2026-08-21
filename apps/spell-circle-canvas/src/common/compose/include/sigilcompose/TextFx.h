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
 * One-shot effects consume progress 0→1; loop effects (waveLoop) read a
 * WRAPPING bound phase (an Output stepped mod 1). Everything renders
 * through batched RSXform draws — moving text is never per-glyph draw
 * calls — and every preset declares the reach its motion needs so the
 * recording cull does not truncate it.
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
      0.0f);
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
      0.0f);
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
      0.0f);
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

/** THE ESCAPE HATCH: an ad-hoc effect body under an author-given key.
 *
 *  The key IS the identity — two effects with the same key compare equal
 *  and the reconciler will prune one onto the other, so give a different
 *  body a different key or the old one silently keeps drawing. Bake the
 *  parameters that vary into the key (or into `params`) rather than
 *  capturing them silently.
 *
 *  `reach` is how far past the element's box the body may push a glyph;
 *  the default covers the shipped presets' range. */
[[nodiscard]] inline TextEffect effect(std::string key, GlyphModFn program,
                                       float reach = 48.0f,
                                       std::vector<float> params = {}) {
  return TextEffect(std::move(key), std::move(params), std::move(program),
                    reach);
}

/** PHASES IN LOCAL TIME: each phase sees a renormalized 0→1 over its own
 *  window, so `fx::seq(a.until(0.35f), b.until(0.75f).xfade(0.10f), c)`
 *  plays `a` over the first 35% of every unit's beat, `b` over the next
 *  40% and `c` over the rest — each running its full curve.
 *
 *  The default joint is a hard cut. `.xfade(f)` on the ENDING phase lerps
 *  its deviation into the next one's, componentwise, over the last `f` of
 *  local time before the joint. */
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
