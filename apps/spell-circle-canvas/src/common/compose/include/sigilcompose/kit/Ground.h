#pragma once

/** @file
 * SigilCompose kit — the two fills a flat ground is DRESSED with: a
 * vignette, which is a radial ramp from nothing at the middle to a colour
 * at the corners, and a grain, which is value noise folded into a colour
 * as LIGHT rather than as hue.
 *
 * Both are ordinary `Fill`s. Lay one over a ground as a full-bleed child
 * and the ground keeps its own colour; give one to a box and the box is
 * the dressing. Neither decides a look: the colour, the reach and the
 * strength are the caller's, and the construction — which is the part
 * every page that wanted a vignette wrote differently — is here.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <include/core/SkShader.h>
#include <include/core/SkSize.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <sigilcompose/core/Paint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace sigil::compose::kit {

/** THE DARKENING TOWARD THE CORNERS of a surface @p over px, as a fill to
 *  lay over the ground it darkens.
 *
 *  Transparent out to @p clear of the way to the far corner, then ramped
 *  to @p edge at the corner itself — so `clear = 0` shades from the
 *  middle and `clear = 1` shades nothing. The radius is measured to the
 *  CORNER rather than to the nearer edge, which is why the shading meets
 *  all four corners at the same value on a surface that is not square.
 *
 *      box().absolute().inset(0).fill(kit::vignette(ctx.size, {0, 0, 0, 0.5f}))
 */
[[nodiscard]] inline Fill vignette(SkSize over, SkColor4f edge,
                                   float clear = 0.45f) {
  const SkPoint centre{over.width() * 0.5f, over.height() * 0.5f};
  const float radius = std::hypot(centre.fX, centre.fY);
  const float hold = std::clamp(clear, 0.0f, 1.0f);
  SkColor4f inner = edge;
  inner.fA = 0;
  return radialGradient(centre, std::max(radius, 1.0f), {inner, edge},
                        {hold, 1.0f});
}

/** @p over WITH A GRAIN IN IT: value noise collapsed to one channel and
 *  soft-lit over the colour, so the ground is dressed in light rather
 *  than speckled in hue — which is what a coloured ground under raw
 *  fractal noise turns into.
 *
 *  @p amount is how far the grain reaches from the neutral that leaves
 *  the colour exactly as it was, so 0 IS @p over and 1 is the noise at
 *  full strength. @p frequency is features per px: around 0.8 is film
 *  grain, around 0.05 is paper.
 *
 *      box().absolute().inset(0).fill(kit::grained(kGround, 0.05f))
 */
[[nodiscard]] inline Fill grained(SkColor4f over, float amount = 0.06f,
                                  float frequency = 0.8f) {
  const float strength = std::clamp(amount, 0.0f, 1.0f);
  if (strength <= 0) return Fill::color(over);
  sk_sp<SkShader> noise =
      SkShaders::MakeFractalNoise(frequency, frequency, 2, 0.0f);
  if (!noise) return Fill::color(over);
  // Skia's fractal noise is COLOURED. Dropped to luminance it is one
  // channel, and one channel soft-lit over a colour moves its value
  // without moving its hue.
  sk_sp<SkShader> value = SkShaders::Blend(
      SkBlendMode::kLuminosity,
      SkShaders::Color(SkColorSetARGB(255, 128, 128, 128)), std::move(noise));
  // Mid grey is soft-light's identity, so compositing the grain over it at
  // `strength` alpha makes the strength linear and 0 exact.
  sk_sp<SkShader> reached = SkShaders::Blend(
      SkBlendMode::kSrcOver,
      SkShaders::Color(SkColorSetARGB(255, 128, 128, 128)),
      SkShaders::Blend(
          SkBlendMode::kDstIn, std::move(value),
          SkShaders::Color(SkColorSetARGB(
              (uint8_t)std::lround(strength * 255.0f), 255, 255, 255))));
  return Fill::shader(SkShaders::Blend(SkBlendMode::kSoftLight,
                                       SkShaders::Color(over.toSkColor()),
                                       std::move(reached)));
}

}  // namespace sigil::compose::kit
