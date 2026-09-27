#pragma once

/** @file
 * @ingroup compose-typography
 *
 * SigilCompose typography — THE STOCK TEXT EFFECTS as plain values: every
 * entrance is a `motion::Tween<Displaced>` a caller may read, copy and
 * change field by field before handing it to `textFx::entrance` (a track)
 * or `textFx::enter` (an effect for the combinators).
 *
 *   text(u8"ONE LINE, TWO MOVES", display)
 *       .textFx(textFx::entrance(textFx::rise(20), {.unit = weave::Unit::Word}))
 *       .textFx({.where = weave::selectors::text(u8"TWO"),
 *                .effect = textFx::waveLoop(),
 *                .progress = &phase});
 *
 * Each carries the timing a stock entrance runs at — `kEntranceDuration` per
 * unit, one unit `kEntranceStep` after the one before — which is the track's
 * own default, so a stock value on a track reads as it did as a bare effect.
 * The one loop here, `waveLoop`, is an effect rather than a tween; its brief
 * says why.
 */

#include <sigilcompose/typography/Entrance.h>
#include <sigilcompose/typography/TextEffect.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcore/compute/Noise.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/values/Tween.h>
#include <sigilweave/style/ShapingStyle.h>

#include <chrono>
#include <cmath>
#include <numbers>

namespace sigil::compose::textFx {

/** The track's own timing a stock entrance carries: 450ms per glyph, one
 *  glyph 30ms after the one before. */
inline constexpr std::chrono::milliseconds kEntranceDuration{450};
inline constexpr std::chrono::milliseconds kEntranceStep{30};

/** The stagger-reveal workhorse: glyphs rise from `distancePx` below their
 *  rest while fading in. Ease-out-expo motion; alpha completes over the
 *  first 35% of local progress, so a glyph is fully opaque while it is
 *  still moving rather than fading and settling together. */
[[nodiscard]] inline motion::Tween<Displaced> rise(float distancePx = 26) {
  return {.from = Displaced{.dy = distancePx},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outExpo};
}

/** Slide-in from the side (negative = from the left). */
[[nodiscard]] inline motion::Tween<Displaced> slide(float distancePx = -32) {
  return {.from = Displaced{.dx = distancePx, .fadeOver = 1.0f / 1.7f},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outCubic};
}

/** Scale-overshoot entrance (back.out(1.7) — the elastic pop). */
[[nodiscard]] inline motion::Tween<Displaced> pop(float fromScale = 0.35f,
                                                  float overshoot = 1.70158f) {
  return {.from = Displaced{.scale = fromScale, .fadeOver = 1.0f / 2.2f},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outBack(overshoot)};
}

/** Tumble-in: glyphs spin from `degrees` while rising and fading. */
[[nodiscard]] inline motion::Tween<Displaced> spinIn(float degrees = 70,
                                                     float risePx = 14) {
  return {.from = Displaced{.dy = risePx, .rotateDeg = degrees,
                            .fadeOver = 1.0f / 1.7f},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outCubic};
}

/** Seeded scatter: every glyph flies in from its own random offset inside
 *  a `radiusPx` disc, with its own random lean. */
[[nodiscard]] inline motion::Tween<Displaced> scatter(float radiusPx = 40,
                                                      float leanDeg = 24) {
  return {.from = Displaced{.scatterPx = radiusPx, .scatterLeanDeg = leanDeg,
                            .fadeOver = 1.0f / 1.7f},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outCubic};
}

/** Hard typewriter: a glyph is absent, then simply THERE at the middle of
 *  its beat (pair with a short duration and a first-to-last stagger).
 *
 *  Two equal keyframes on the opacity lane: the first steps from 0 to 1 at
 *  its own end (`motion::ease::steps(1)`), the second holds. Coverage only
 *  — the glyph appears where it already was, so it does not displace. */
[[nodiscard]] inline motion::Tween<Displaced> typeOn() {
  const Displaced there{.fadeOver = 0, .opacity = 1};
  return {.from = Displaced{.fadeOver = 0, .opacity = 0},
          .keyframes = {{.to = there, .ease = motion::ease::steps(1)},
                        {.to = there}},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::linear};
}

/** A variable-font axis SWEPT across local progress: `from` at t = 0,
 *  `to` at t = 1, straight. Pair it with a stagger and a weight rolls along
 *  the line. The held coordinate is `textFx::variableAxis`, because the
 *  span verb that holds an axis is built on it. An axis coordinate is not
 *  placement, so the sweep does not displace. */
[[nodiscard]] inline motion::Tween<Displaced> variableAxisSweep(
    const char (&tag)[5], float from, float to) {
  return {.from = Displaced{.fadeOver = 0,
                            .axis = sigil::weave::FontVariation(tag, from)},
          .to = Displaced{.fadeOver = 0,
                          .axis = sigil::weave::FontVariation(tag, to)},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::linear};
}

/** Endless float: glyph i bobs on a sine, phase-shifted per glyph. Bind
 *  progress to a WRAPPING phase value (t = fract(seconds / period)) and
 *  give the track a tween with no delay (`.tween = {}`) so every glyph
 *  reads the same master phase.
 *  Amplitude is in EM — keep it at or under 0.15em, past which descenders
 *  of adjacent glyphs collide — and the phase shift is RADIANS per glyph,
 *  where roughly 0.4–0.6 gives one readable travelling wave.
 *
 *  AN EFFECT, NOT A TWEEN: its deviation reads each glyph's index and font
 *  size, which a tween — one path every unit walks — never sees, and it
 *  reads a wrapping phase with no home to arrive at. */
[[nodiscard]] inline TextEffect waveLoop(float amplitudeEm = 0.10f,
                                         float phaseRadPerGlyph = 0.5f) {
  return TextEffect(
      "waveLoop", {amplitudeEm, phaseRadPerGlyph},
      [amplitudeEm, phaseRadPerGlyph](const GlyphInfo& g, float t,
                                      core::noise::Mix64Stream&) {
        GlyphModifier m;
        m.dy = std::sin(t * 2.0f * std::numbers::pi_v<float> -
                        (float)g.index * phaseRadPerGlyph) *
               amplitudeEm * (g.fontSize > 0 ? g.fontSize : 16.0f);
        return m;
      },
      std::abs(amplitudeEm) * kNominalSizePx);
}

}  // namespace sigil::compose::textFx
