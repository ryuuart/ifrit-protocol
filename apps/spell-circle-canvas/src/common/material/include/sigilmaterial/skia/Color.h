#pragma once

/** @file
 * The colour bridge between Skia and this library: `SkColor4f` in, this
 * library's `Color` out, and back — and the colour verbs answered in
 * Skia's colour, so a renderer-side caller reaches this library's
 * arithmetic without spelling the crossing at every site. Both colours
 * are four straight (not premultiplied) sRGB floats in the same order,
 * so the conversion is a field-for-field copy and nothing else — no
 * transfer function, no premultiply, no clamp.
 *
 * `Color` CONVERTS FROM AN SkColor4f ON ITS OWN, so a Skia caller hands
 * one to anything taking a colour and writes one into any field that is
 * one. What is here is the named spellings — the way back, which the
 * colour cannot carry without naming Skia, and the palette form — and
 * `toColor` is that same conversion under a name, not a second copy of
 * it.
 */

#include <include/core/SkColor.h>
#include <sigilmaterial/color/Color.h>

#include <span>
#include <vector>

namespace sigil::material::skia {

/** A Skia colour as this library's — the conversion `Color` already
 *  performs, named for a call that wants to say so. */
constexpr Color toColor(const SkColor4f& c) noexcept { return Color(c); }

/** This library's colour as Skia's. */
constexpr SkColor4f toSkColor(const Color& c) noexcept {
  return {c.r, c.g, c.b, c.a};
}

/** THE COLOUR VERBS ANSWERED IN SKIA'S COLOUR: the leaf's arithmetic,
 *  crossed both ways in one place, for the consumer whose slots are
 *  `SkColor4f` and whose call sites would otherwise spell the crossing
 *  around every one of them.
 *
 *  The arithmetic is NOT restated here — each of these is the leaf verb
 *  with a conversion either side, so there is one definition of what
 *  scaling, lightening or mixing a colour means and a renderer cannot
 *  drift from it. */
constexpr SkColor4f withAlpha(const SkColor4f& c, float a) noexcept {
  return toSkColor(sigil::material::withAlpha(toColor(c), a));
}

/** @p c scaled by @p k in every channel, at alpha @p a — or at its own
 *  alpha, which is what a negative @p a asks for. */
constexpr SkColor4f scale(const SkColor4f& c, float k,
                          float a = -1.0f) noexcept {
  return toSkColor(sigil::material::scale(toColor(c), k, a));
}

/** @p k added to each colour channel, clamped at 1, alpha kept. */
constexpr SkColor4f lighten(const SkColor4f& c, float k) noexcept {
  return toSkColor(sigil::material::lighten(toColor(c), k));
}

/** @p a and @p b mixed a fraction @p t apart IN LINEAR LIGHT, alpha
 *  mixed as it is given. */
inline SkColor4f mixLinear(const SkColor4f& a, const SkColor4f& b, float t) {
  return toSkColor(sigil::material::mixLinear(toColor(a), toColor(b), t));
}

/** A palette converted in one call — the shape a generator taking a list
 *  of colours is handed. A convenience with no caller in this tree: it is
 *  kept because the two crossings are a pair, and a consumer holding
 *  Skia colours should not write the loop again. */
inline std::vector<Color> toColors(std::span<const SkColor4f> in) {
  std::vector<Color> out;
  out.reserve(in.size());
  for (const SkColor4f& c : in) out.push_back(toColor(c));
  return out;
}

}  // namespace sigil::material::skia
