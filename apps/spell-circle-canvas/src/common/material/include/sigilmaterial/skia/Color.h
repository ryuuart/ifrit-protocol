#pragma once

/** @file
 * @ingroup material-skia
 *
 * The colour bridge between Skia and this library: `SkColor4f` in, this
 * library's `Color` out, and back, with the colour verbs answered in
 * Skia's colour. Both are four straight sRGB floats in the same order,
 * so the conversion is a field-for-field copy and nothing else. `Color`
 * converts from an `SkColor4f` on its own; what is here is the named
 * spellings, the way back, and the palette form.
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
