#pragma once

/** @file
 * The colour bridge between Skia and this library: `SkColor4f` in, this
 * library's `Color` out, and back. Both are four straight (not
 * premultiplied) sRGB floats in the same order, so the conversion is a
 * field-for-field copy and nothing else — no transfer function, no
 * premultiply, no clamp.
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

/** A palette converted in one call — the shape every generator taking a
 *  list of colours is handed. */
inline std::vector<Color> toColors(std::span<const SkColor4f> in) {
  std::vector<Color> out;
  out.reserve(in.size());
  for (const SkColor4f& c : in) out.push_back(toColor(c));
  return out;
}

}  // namespace sigil::material::skia
