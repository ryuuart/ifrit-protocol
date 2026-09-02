#pragma once

/** @file
 * @ingroup shaping
 *
 * The parameters of a text style as a designated-init aggregate, and the
 * `TextStyle` they build. `Type` decides nothing — there is no type scale
 * and no opinion about which face stands in for which; a study's
 * decisions are its own.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/style/TextStyle.h>

#include <vector>

namespace sigil::weave {

/** The parameters of a text style, as a designated-init aggregate rather
 *  than a positional signature.
 *
 *  Every caller needs a face, a size and a colour, and then some subset of
 *  tracking, condensation and variable-font axes — and which subset differs
 *  per call site. A positional helper cannot grow another parameter
 *  without breaking every existing call; an aggregate can.
 *
 *      textStyle({.face = faceMono, .size = 10.5f, .color = kInk,
 *                 .track = 1.2f})
 *
 *  Anything not here is added to the RETURNED style — a mask-filter blur, a
 *  kPlus blend, a mandatory underlay. Those are per-artefact decisions and
 *  stay at the call site. */
struct Type {
  /** null → the FontContext's default family (plus its fallback chain). */
  sk_sp<SkTypeface> face;
  float size = 16.0f;
  SkColor4f color = {0, 0, 0, 1};
  /** px of tracking added after each cluster (ShapingStyle::letterSpacing).
   *  NOT per-mille: a reference that quotes tracking in per-mille is
   *  converted at the call site, where the unit's own em size is known. */
  float track = 0.0f;
  /** Horizontal condensation (ShapingStyle::scaleX) — how to condense a
   *  face that has no `wdth` axis to ask instead. */
  float condense = 1.0f;
  /** > 0 → a `wght` variable-font axis. Weight changes advances, so this
   *  participates in shaping identity. */
  float weight = 0.0f;
  /** != 0 → a `slnt` axis (negative leans right, per the OpenType sign). */
  float slant = 0.0f;
  /** Hard-edged glyph rasterisation (ShapingStyle::aliased). Skia takes
   *  edging from the SkFont, never from the paint, so this is the only way
   *  to ask — `paint.setAntiAlias(false)` is silently ignored on text. */
  bool aliased = false;
  /** The glyph paint's own antialias flag (edges of strokes/decorations on
   *  the paint, not the glyph edging above). */
  bool antiAlias = true;
  /** Send `color` to the paint through an 8-bit sRGB word — Skia's
   *  `setColor(SkColor)` — instead of as float.
   *
   *  NOT a no-op and not an equivalent spelling: the round trip quantises
   *  each channel to one of 256 values, and Skia climbs a byte back to
   *  float by multiplying by 1/255 where `hex()` divides by 255, which
   *  lands one ulp apart on 126 of the 256 byte values. A palette taken
   *  from a reference's own ARGB words wants this ladder; a colour
   *  computed in float does not. */
  bool color8 = false;
  /** Anything else in design space — appended after weight/slant, so the
   *  order is stable and two styles built the same way share one
   *  varied-face memo entry. */
  std::vector<FontVariation> variations;
};

/** Type{} → the TextStyle it names. */
inline TextStyle textStyle(const Type& t) {
  TextStyle s;
  s.shaping.typeface = t.face;
  s.shaping.fontSize = t.size;
  s.shaping.letterSpacing = t.track;
  s.shaping.scaleX = t.condense;
  s.shaping.aliased = t.aliased;
  if (t.color8)
    s.paint.foreground.setColor(t.color.toSkColor());
  else
    s.paint.foreground.setColor4f(t.color, nullptr);
  s.paint.foreground.setAntiAlias(t.antiAlias);
  if (t.weight > 0) s.variation("wght", t.weight);
  if (t.slant != 0) s.variation("slnt", t.slant);
  for (const FontVariation& v : t.variations) s.shaping.variations.push_back(v);
  return s;
}

/** The same call under a name that says nothing of what it builds;
 *  `textStyle` is the spelling. */
[[deprecated("the same call is textStyle")]] inline TextStyle type(
    const Type& t) {
  return textStyle(t);
}

}  // namespace sigil::weave
