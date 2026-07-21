#pragma once

/** @file
 * SigilCompose kinetic typography presets — the contemporary motion-design
 * grammar (REFERENCES.md §8: the stagger-reveal / wave / pop / spin moves
 * as practiced on Behance/IG) as pure GlyphEffect VALUES over the kernel's
 * GlyphFx seam. Compose them onto any text():
 *
 *   GlyphFx fx;
 *   fx.effect = glyphfx::rise();
 *   fx.stagger = {.eachMs = 28, .durationMs = 480};
 *   fx.progress = with(1.0f, {900ms, &ch::easeOutQuad}); // or bind an Output
 *   text(u8"KINETIC", display).glyphFx(std::move(fx));
 *
 * One-shot effects consume progress 0→1; loop effects (waveLoop) read a
 * WRAPPING bound phase (Output stepped mod 1). Everything renders through
 * batched RSXform draws — flashy text is never per-glyph draw calls.
 */

#include "sigilcompose/Compose.h"

#include <cmath>

namespace sigil::compose::glyphfx {

namespace detail {
inline float easeOutCubic(float t) {
  const float u = 1 - t;
  return 1 - u * u * u;
}
inline float easeOutBack(float t, float s = 1.70158f) {
  const float u = t - 1;
  return 1 + (s + 1) * u * u * u + s * u * u;
}
} // namespace detail

/** The stagger-reveal workhorse: glyphs rise from `distancePx` below their
 *  rest while fading in (ease-out-cubic). */
inline GlyphEffectFn rise(float distancePx = 26) {
  return [distancePx](const GlyphInfo &, float t) {
    GlyphMod m;
    m.dy = (1 - detail::easeOutCubic(t)) * distancePx;
    m.alpha = std::min(1.0f, t * 1.7f);
    return m;
  };
}

/** Slide-in from the side (negative = from the left). */
inline GlyphEffectFn slide(float distancePx = -32) {
  return [distancePx](const GlyphInfo &, float t) {
    GlyphMod m;
    m.dx = (1 - detail::easeOutCubic(t)) * distancePx;
    m.alpha = std::min(1.0f, t * 1.7f);
    return m;
  };
}

/** Scale-overshoot entrance (back.out(1.7) — the elastic pop). */
inline GlyphEffectFn pop(float fromScale = 0.35f, float overshoot = 1.70158f) {
  return [fromScale, overshoot](const GlyphInfo &, float t) {
    GlyphMod m;
    m.scale = fromScale + (1 - fromScale) * detail::easeOutBack(t, overshoot);
    m.alpha = std::min(1.0f, t * 2.2f);
    return m;
  };
}

/** Tumble-in: glyphs spin from `degrees` while rising and fading. */
inline GlyphEffectFn spinIn(float degrees = 70, float risePx = 14) {
  return [degrees, risePx](const GlyphInfo &, float t) {
    const float e = detail::easeOutCubic(t);
    GlyphMod m;
    m.rotateDeg = (1 - e) * degrees;
    m.dy = (1 - e) * risePx;
    m.alpha = std::min(1.0f, t * 1.7f);
    return m;
  };
}

/** Hard typewriter: a glyph is absent, then simply THERE (pair with a
 *  short durationMs and Start stagger). */
inline GlyphEffectFn typeOn() {
  return [](const GlyphInfo &, float t) {
    GlyphMod m;
    m.alpha = t >= 0.5f ? 1.0f : 0.0f;
    return m;
  };
}

/** Endless float: glyph i bobs on a sine, phase-shifted per glyph. Bind
 *  progress to a WRAPPING phase Output (t = fract(seconds / period)) and
 *  set stagger.eachMs = 0 so every glyph reads the same master phase. */
inline GlyphEffectFn waveLoop(float amplitudePx = 5,
                              float phasePerGlyph = 0.18f) {
  return [amplitudePx, phasePerGlyph](const GlyphInfo &g, float t) {
    GlyphMod m;
    m.dy = std::sin(((float)g.index * phasePerGlyph + t) * 6.2831853f) *
           amplitudePx;
    return m;
  };
}

} // namespace sigil::compose::glyphfx
