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
 *       .fx({.effect = fx::rise(20), .over = unit::Word})
 *       .fx({.where = sel::text(u8"TWO"),
 *            .effect = fx::waveLoop(),
 *            .progress = &phase});
 *
 * The combinators and the effects the runtime itself evaluates —
 * `fx::keys`, `fx::seq`, `fx::mix`, `fx::hold`, `fx::scramble`, `fx::pass`
 * and the `fx::effect` door — are declared with the kernel in `Text.h`;
 * this header holds the presets, which are values built from the same
 * `TextEffect` constructor any caller may use.
 *
 * One-shot effects consume progress 0→1; loop effects (waveLoop) read a
 * WRAPPING bound phase (an Output stepped mod 1), and a looping CASCADE
 * (`motion::Spread::loopMs`) reads the same wrapping phase and re-opens every
 * unit's beat once per wrap. Everything renders
 * through batched RSXform draws — moving text is never per-glyph draw
 * calls — and every preset declares the reach its motion needs so the
 * recording cull does not truncate it.
 *
 * Every effect here also carries whether it MOVES its glyphs off the pen
 * positions the layout gave them (`TextEffect::displaces`), which is what
 * decides the grid a live run's origins are rounded to. `rise`, `slide`,
 * `pop`, `spinIn`, `scatter` and `waveLoop` move them; `typeOn`,
 * `variableAxisSweep`, `tint`, `scramble` and `pass` do not; `keys` reads its
 * own table and the combinators derive from their operands. Only `fx::effect`
 * has to be told.
 */

#include <sigilcompose/core/Text.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose::fx {

namespace detail {
// The three curves the presets below shape their glyphs with are
// Choreograph's own — `easeOutCubic`, `easeOutExpo` and the
// shape-parameterised `easeOutBack` — called directly here rather than
// wrapped, because a preset shapes one float per glyph per frame and an
// EaseFn through a std::function would put an indirect call in that
// loop. `motion::ease::outBack` is the same curve in the form a
// Transition holds.
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
        m.dy = (1 - choreograph::easeOutExpo(t)) * distancePx;
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
        m.dx = (1 - choreograph::easeOutCubic(t)) * distancePx;
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
            fromScale + (1 - fromScale) * choreograph::easeOutBack(t, overshoot);
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
        const float e = choreograph::easeOutCubic(t);
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
        const float e = choreograph::easeOutCubic(t);
        GlyphMod m;
        m.dx = (1 - e) * dx;
        m.dy = (1 - e) * dy;
        m.rotateDeg = (1 - e) * lean;
        m.alpha = std::min(1.0f, t * 1.7f);
        return m;
      },
      std::abs(radiusPx) + detail::kNominalSizePx * 0.5f);
}

/** A variable-font axis SWEPT across local progress: `from` at t = 0,
 *  `to` at t = 1. Pair it with a stagger and a weight rolls along the
 *  line. The held coordinate is `TextEffect::variableAxis`, in the
 *  kernel, because the span verb that holds an axis is built on it. */
[[nodiscard]] inline TextEffect variableAxisSweep(const char (&tag)[5],
                                                  float from, float to) {
  const sigil::weave::FontVariation coordinate(tag, from);
  return TextEffect(
      "variableAxisSweep",
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

namespace sigil::compose {

// ---------------------------------------------------------------------------
// The marquee — text in motion that costs a repaint and never a reflow

/** The seamless ticker (news crawl, y2k status bar): `content` twice in a
 *  row inside a clipped box, slid by a caller-owned WRAPPING phase Output
 *  in px. Step the phase over [-(w + gap), 0] where w = the content's
 *  width — measure(content, fonts).width() gives it — and the loop is
 *  invisible. Binding translateX is paint-only volatility: the strip's
 *  recording replays every frame, nothing re-records. Keep `content`
 *  keyless (it mounts twice). */
inline Element marquee(const Element& content,
                       const choreograph::Output<float>* phase,
                       float gap = 0.0f) {
  return box().clip(true).child(box()
                                    .row()
                                    .gap(gap)
                                    .shrink(0)
                                    .alignSelf(Align::Start)
                                    .translateX(phase)
                                    .child(content)
                                    .child(content));
}

/** The width-pinned marquee: each copy rides in a fixed `contentWidth`
 *  box, so text content can NEVER wrap against the clip viewport. An
 *  unpinned strip resolves its width against the clip box instead and
 *  wraps to two lines, which is what the overload above risks with text.
 *  Measure once — `ctx.measure(strip).width()` — and pass it here; wrap
 *  the phase over [-(contentWidth + gap), 0]. */
inline Element marquee(Element content, float contentWidth,
                       const choreograph::Output<float>* phase,
                       float gap = 0.0f) {
  auto pinned = [&] {
    return box().width(Dim(contentWidth)).shrink(0).child(content);
  };
  return box().clip(true).child(box()
                                    .row()
                                    .gap(gap)
                                    .shrink(0)
                                    .alignSelf(Align::Start)
                                    .translateX(phase)
                                    .child(pinned())
                                    .child(pinned()));
}

}  // namespace sigil::compose
