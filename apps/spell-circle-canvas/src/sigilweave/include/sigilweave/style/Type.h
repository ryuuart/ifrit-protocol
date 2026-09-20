#pragma once

/** @file
 * @ingroup weave-shaping
 *
 * `Type` — a text style's parameters as a PARTIAL: every field optional,
 * so a call site states the two it changes and says nothing about the
 * rest. With `initialType`, the two merges, and the `TextStyle` a total
 * builds. `Type` decides nothing: there is no type scale here.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/style/Decoration.h>
#include <sigilweave/style/Length.h>
#include <sigilweave/style/PaintLayer.h>
#include <sigilweave/style/ShapingStyle.h>
#include <sigilweave/style/TextStyle.h>

#include <optional>
#include <string>
#include <vector>

namespace sigil::weave {

/** THE PARAMETERS OF A TEXT STYLE, EVERY ONE OPTIONAL — A PARTIAL: a
 *  designated-init aggregate whose unset field is not a field set to a
 *  default but a field inherited, resolved one step at a time by
 *  `overlay` and bottoming out in `initialType`. A size, a tracking and a
 *  word spacing may be relative and are resolved as they are overlaid, so
 *  a total `Type` always carries pixels.
 *  @trap The foreground paint's own state beyond its colour — a shader, a
 *  mask-filter blur, a blend mode — is not here; add it to the RETURNED
 *  style, or set the leaf in a whole `TextStyle`. */
struct Type {
  /** Unset is the face inherited, or the `FontContext`'s default family
   *  and fallback chain when nothing above names one.
   *  @trap A STATED NULL — `.face = nullptr`, or `defaultFace()` — is the
   *  default family outright, whatever an ancestor named. */
  std::optional<sk_sp<SkTypeface>> face;
  /** The type size. Pixels are implicit, so `.size = 13` is thirteen of
   *  them; a relative length is resolved against what it is overlaid on. */
  std::optional<Length> size;
  std::optional<SkColor4f> color;
  /** Tracking added after each cluster. Pixels are implicit, so
   *  `.track = 1.2f` is that many; `em(-0.08f)` is a fraction of the SIZE
   *  THE TYPE RESOLVES TO, resolved as it is overlaid.
   *  @trap Not per-mille: a reference quoting tracking in per-mille is
   *  quoting thousandths of an em. */
  std::optional<Length> track;
  /** Horizontal condensation (ShapingStyle::scaleX) — how to condense a
   *  face that has no `wdth` axis to ask instead. */
  std::optional<float> condense;
  /** Present and greater than 0 is a `wght` axis, which changes advances
   *  and so participates in shaping identity; present and 0 is the face's
   *  own weight, stated, overriding an inherited axis with none. */
  std::optional<float> weight;
  /** Present and != 0 → a `slnt` axis (negative leans right, per the
   *  OpenType sign). */
  std::optional<float> slant;
  /** Hard-edged glyph rasterisation.
   *  @trap It is the only way to ask: Skia takes edging from the `SkFont`
   *  and `paint.setAntiAlias(false)` is silently ignored on text. */
  std::optional<bool> aliased;
  /** The glyph paint's own antialias flag (edges of strokes/decorations on
   *  the paint, not the glyph edging above). */
  std::optional<bool> antiAlias;
  /** Send `color` to the paint through an 8-bit sRGB word instead of as
   *  float. A palette taken from a reference's own ARGB words wants this
   *  ladder; a colour computed in float does not.
   *  @trap Not an equivalent spelling: the round trip quantises each
   *  channel to one of 256 values, and the climb back lands one ulp from
   *  a float division on most of them. */
  std::optional<bool> color8;
  /** Anything else in design space, appended after weight and slant so
   *  the order is stable and two styles built the same way share one
   *  varied-face memo entry; a merge replaces an axis already present
   *  where it stands. */
  std::vector<FontVariation> variations;
  /** BCP-47 tag of the language the passage is in, which chooses the
   *  fallback faces and the OpenType localised forms. Empty states no
   *  language; unset is the language inherited. */
  std::optional<std::string> language;
  /** OpenType features.
   *  @trap A set list REPLACES the inherited one whole, as CSS's
   *  `font-feature-settings` does. */
  std::optional<std::vector<FontFeature>> features;
  /** Pair spacing measured from the letters in place of the face's kerning
   *  table (ShapingStyle::opticalKerning). */
  std::optional<bool> opticalKerning;
  /** Extra space at each word gap (ShapingStyle::wordSpacing). Pixels are
   *  implicit; `em(0.25f)` is a fraction of the SIZE THE TYPE RESOLVES TO,
   *  resolved as tracking is. */
  std::optional<Length> wordSpacing;
  /** The case the passage is set in (ShapingStyle::textTransform). */
  std::optional<TextTransform> textTransform;
  /** How a run stands in a vertical column (ShapingStyle::verticalForm). */
  std::optional<VerticalForm> verticalForm;
  /** The line decorations: an underline, a strikethrough, a highlight. A
   *  set list replaces the inherited one, and a decoration naming no
   *  colour is drawn in the colour the text is set in. */
  std::optional<std::vector<Decoration>> decorations;
  /** The passes drawn beneath the glyphs — a halo, a shadow, a ring —
   *  and above them. A set list replaces the inherited one, and a pass
   *  whose paint colour is transparent is drawn in the colour the text is
   *  set in, on its own stroke, blur and offset. */
  std::optional<std::vector<PaintLayer>> underlays;
  std::optional<std::vector<PaintLayer>> overlays;

  bool operator==(const Type&) const = default;

  /** Whether it states NOTHING — the partial that changes no field, which
   *  overlays onto anything as itself. */
  [[nodiscard]] bool empty() const {
    return !face && !size && !color && !track && !condense && !weight &&
           !slant && !aliased && !antiAlias && !color8 && variations.empty() &&
           !language && !features && !opticalKerning && !wordSpacing &&
           !textTransform && !verticalForm && !decorations && !underlays &&
           !overlays;
  }
};

/** Whether the fields @p partial sets change how a passage is SHAPED, as
 *  against how it is painted. A consumer that can repaint a range without
 *  re-shaping it asks this first. */
[[nodiscard]] bool reshapes(const Type& partial);

/** THE FACE STATED AS THE FONT CONTEXT'S DEFAULT FAMILY: what a `Type`
 *  says with `.face = defaultFace()`, and what `initialType()` carries. */
[[nodiscard]] inline sk_sp<SkTypeface> defaultFace() { return nullptr; }

/** EVERY FIELD ENGAGED, WITH THE VALUE AN UNSET ONE MEANS when nothing
 *  is left above it to inherit from: a null face, 16 px, opaque black,
 *  and the neutral setting of everything else. It is the root of a
 *  cascade, so a partial overlaid on it is a total. */
[[nodiscard]] Type initialType();

/** FOLDS @p over INTO @p into — a pure field copy: every field @p over
 *  sets replaces @p into's, and variations are appended with an axis
 *  already present replaced where it stands.
 *  @trap A relative size is copied VERBATIM and not resolved; resolving
 *  is `overlay`'s, where a base is in reach. */
Type& merge(Type& into, const Type& over);

/** @p over RESOLVED AGAINST @p base — one step of a cascade, @p over
 *  winning where it sets a field. A RELATIVE SIZE, TRACKING AND WORD
 *  SPACING BECOME PIXELS HERE, which is the one thing this does that a
 *  plain merge cannot: `em` multiplies the base size, `rem` multiplies
 *  @p rootSizePx, and `lh` multiplies @p lineHeightPx — or, when that is
 *  0, 1.2 times the base size. */
[[nodiscard]] Type overlay(const Type& base, const Type& over,
                           float rootSizePx = 16.0f, float lineHeightPx = 0.0f);

/** The `TextStyle` a TOTAL `Type` names, each unset field taking its
 *  `initialType` value; a size still stated relatively is resolved
 *  against the initial 16 px. */
[[nodiscard]] TextStyle toTextStyle(const Type& total);

/** THE SET FIELDS OF @p over APPLIED TO A STYLE THAT IS ALREADY TOTAL.
 *  A relative size resolves against the base's own font size for `em`,
 *  the initial 16 px for `rem`, and 1.2 times that size for `lh`, the
 *  style carrying no line height of its own.
 *  @silent @p over sets `color8` and no colour: the base's colour is
 *  already a number in the paint. */
[[nodiscard]] TextStyle overlay(TextStyle base, const Type& over);

/** Type{} → the TextStyle it names, every field it leaves unset taking its
 *  initial value. */
[[nodiscard]] inline TextStyle textStyle(const Type& t) {
  return toTextStyle(overlay(initialType(), t));
}

}  // namespace sigil::weave
