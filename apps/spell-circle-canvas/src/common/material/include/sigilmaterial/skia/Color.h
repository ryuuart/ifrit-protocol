#pragma once

/** @file
 * @ingroup material-skia
 *
 * The colour crossing between Skia and this library: `SkColor4f` in,
 * this library's `Color` out, and back. Both are four straight sRGB
 * floats in the same order, so it is a field-for-field copy and nothing
 * else. `Color` converts from an `SkColor4f` on its own; what is here is
 * that conversion under a name, and the way back — which a colour cannot
 * carry without naming a renderer.
 *
 * The colour verbs are not answered here. A consumer holding Skia's
 * colour crosses once and reaches `withAlpha`, `scale`, `lighten` and
 * `mixLinear` themselves, in <sigilmaterial/color/Color.h>, so there is
 * one definition of what each of them means.
 */

#include <include/core/SkColor.h>
#include <sigilmaterial/color/Color.h>

namespace sigil::material::skia {

/** A Skia colour as this library's — the conversion `Color` already
 *  performs, named for a call that wants to say so. */
constexpr Color toColor(const SkColor4f& c) noexcept { return Color(c); }

/** This library's colour as Skia's. */
constexpr SkColor4f toSkColor(const Color& c) noexcept {
  return {c.r, c.g, c.b, c.a};
}

}  // namespace sigil::material::skia
