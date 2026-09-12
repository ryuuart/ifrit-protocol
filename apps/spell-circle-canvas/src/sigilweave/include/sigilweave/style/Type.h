#pragma once

/** @file
 * @ingroup shaping
 *
 * `Type` — a text style's parameters as a PARTIAL: every field optional, so
 * a call site states the two it changes and says nothing about the rest.
 * With the total an unset field falls back to (`initialType`), the two
 * merges (`merge` folds one partial into another, `overlay` resolves one
 * against what it inherits from), and the `TextStyle` a total builds
 * (`toTextStyle`, `textStyle`). `Type` decides nothing — there is no type
 * scale and no opinion about which face stands in for which; a study's
 * decisions are its own.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/style/Length.h>
#include <sigilweave/style/TextStyle.h>

#include <optional>
#include <vector>

namespace sigil::weave {

/** THE PARAMETERS OF A TEXT STYLE, EVERY ONE OPTIONAL — A PARTIAL.
 *
 *  Two things at once, and they do not pull against each other:
 *
 *  A DESIGNATED-INIT AGGREGATE rather than a positional signature. Every
 *  caller needs a face, a size and a colour, and then some subset of
 *  tracking, condensation and variable-font axes — and which subset differs
 *  per call site. A positional helper cannot grow another parameter without
 *  breaking every existing call; an aggregate can.
 *
 *      textStyle({.face = faceMono, .size = 10.5f, .color = kInk,
 *                 .track = 1.2f})
 *
 *  AND A PARTIAL: a field nobody set is not a field set to a default. A
 *  `Type` that names a weight and nothing else says "heavier, and the rest
 *  as it was", which is what lets a style be stated once at the top of a
 *  tree and adjusted at a leaf. `overlay()` is the one step of that; the
 *  values an unset field means, when nothing is left above it to inherit
 *  from, are `initialType()`'s.
 *
 *  A SIZE MAY BE RELATIVE (Length.h): `em(0.75f)` of a size decided
 *  elsewhere, `rem(2)` of the root's, `lh(1)` of the line's. It is resolved
 *  as it is overlaid, where the number it is relative to is known, so a
 *  total `Type` always carries pixels.
 *
 *  Anything not here is added to the RETURNED style — a mask-filter blur, a
 *  kPlus blend, a mandatory underlay. Those are per-artefact decisions and
 *  stay at the call site. */
struct Type {
  /** null → unset: the face inherited, or the FontContext's default family
   *  (plus its fallback chain) when nothing above names one. */
  sk_sp<SkTypeface> face;
  /** The type size. Pixels are implicit, so `.size = 13` is thirteen of
   *  them; a relative length is resolved against what it is overlaid on. */
  std::optional<Length> size;
  std::optional<SkColor4f> color;
  /** px of tracking added after each cluster (ShapingStyle::letterSpacing).
   *  NOT per-mille: a reference that quotes tracking in per-mille is
   *  converted at the call site, where the unit's own em size is known. */
  std::optional<float> track;
  /** Horizontal condensation (ShapingStyle::scaleX) — how to condense a
   *  face that has no `wdth` axis to ask instead. */
  std::optional<float> condense;
  /** Present and > 0 → a `wght` variable-font axis. Weight changes
   *  advances, so this participates in shaping identity. Present and 0 is
   *  the face's own weight, stated: it overrides an inherited axis with
   *  none. */
  std::optional<float> weight;
  /** Present and != 0 → a `slnt` axis (negative leans right, per the
   *  OpenType sign). */
  std::optional<float> slant;
  /** Hard-edged glyph rasterisation (ShapingStyle::aliased). Skia takes
   *  edging from the SkFont, never from the paint, so this is the only way
   *  to ask — `paint.setAntiAlias(false)` is silently ignored on text. */
  std::optional<bool> aliased;
  /** The glyph paint's own antialias flag (edges of strokes/decorations on
   *  the paint, not the glyph edging above). */
  std::optional<bool> antiAlias;
  /** Send `color` to the paint through an 8-bit sRGB word — Skia's
   *  `setColor(SkColor)` — instead of as float.
   *
   *  NOT a no-op and not an equivalent spelling: the round trip quantises
   *  each channel to one of 256 values, and Skia climbs a byte back to
   *  float by multiplying by 1/255 where `hexColor()` divides by 255, which
   *  lands one ulp apart on 126 of the 256 byte values. A palette taken
   *  from a reference's own ARGB words wants this ladder; a colour
   *  computed in float does not. */
  std::optional<bool> color8;
  /** Anything else in design space — appended after weight/slant, so the
   *  order is stable and two styles built the same way share one
   *  varied-face memo entry. A merge replaces an axis already present where
   *  it stands rather than appending a second setting of it. */
  std::vector<FontVariation> variations;

  bool operator==(const Type&) const = default;

  /** Whether it states NOTHING — the partial that changes no field, which
   *  overlays onto anything as itself. */
  [[nodiscard]] bool empty() const {
    return face == nullptr && !size && !color && !track && !condense &&
           !weight && !slant && !aliased && !antiAlias && !color8 &&
           variations.empty();
  }
};

/** EVERY FIELD ENGAGED, WITH THE VALUE AN UNSET ONE MEANS when nothing is
 *  left above it to inherit from: a null face (the font context's default
 *  family), 16 px, opaque black, no tracking, no condensation, no weight
 *  and no slant axis, antialiased glyphs on an antialiased paint, and the
 *  float colour ladder rather than the 8-bit one.
 *
 *  The root of a cascade: a partial overlaid on this is a total. */
[[nodiscard]] Type initialType();

/** FOLDS `over` INTO `into` — a pure field copy, which is how two partials
 *  written about the same text become one.
 *
 *  Every field `over` sets replaces `into`'s; a field it leaves unset
 *  leaves `into`'s alone. Variations are appended, an axis already present
 *  replaced where it stands, so the order is stable.
 *
 *  A RELATIVE SIZE IS COPIED VERBATIM and not resolved: two partials
 *  together know no more about what it is relative to than either knew
 *  alone. Resolving is `overlay()`'s, where a base is in reach. */
Type& merge(Type& into, const Type& over);

/** `over` RESOLVED AGAINST `base` — one step of a cascade.
 *
 *  Field by field, `over` wins where it sets a field and `base` stands
 *  where it does not. Variations are appended, an axis already present
 *  replaced where it stands.
 *
 *  A RELATIVE `over.size` BECOMES PIXELS HERE, which is the one thing this
 *  does that a plain merge cannot: `em` multiplies `base.size`, falling
 *  back to 16 px when the base states no size or states its own
 *  relatively; `rem` multiplies `rootSizePx`; `lh` multiplies
 *  `lineHeightPx`, and when that is 0 — no line height known — it
 *  multiplies 1.2 times the base size instead, a single-spaced line being
 *  about that much of the type it is set in. The face's own number is
 *  `lineHeightOf()` in `fonts/Shaper.h`, where the font service that can
 *  answer it lives; pass it here when the face is in reach. */
[[nodiscard]] Type overlay(const Type& base, const Type& over,
                           float rootSizePx = 16.0f, float lineHeightPx = 0.0f);

/** The `TextStyle` a TOTAL `Type` names, each unset field taking its
 *  `initialType()` value. A size still stated relatively has nothing left
 *  in reach to be relative to and is resolved against the initial 16 px. */
[[nodiscard]] TextStyle toTextStyle(const Type& total);

/** THE SET FIELDS OF `over` APPLIED TO A STYLE THAT IS ALREADY TOTAL —
 *  a cascade step whose base is a built `TextStyle` rather than a `Type`.
 *
 *  A relative size resolves against `base`'s own `fontSize` for `em`, the
 *  initial 16 px for `rem`, and 1.2 times that font size for `lh`; the
 *  style carries no line height, so a caller who knows one resolves the
 *  size itself through `overlay(const Type&, const Type&, …)`.
 *
 *  `color8` chooses the ladder for a colour `over` itself states. A partial
 *  that sets the flag and no colour changes nothing: the base's colour is
 *  already a number in the paint, and re-sending it through the byte would
 *  quantise a colour nobody restated. */
[[nodiscard]] TextStyle overlay(TextStyle base, const Type& over);

/** Type{} → the TextStyle it names, every field it leaves unset taking its
 *  initial value. */
[[nodiscard]] inline TextStyle textStyle(const Type& t) {
  return toTextStyle(overlay(initialType(), t));
}

}  // namespace sigil::weave
