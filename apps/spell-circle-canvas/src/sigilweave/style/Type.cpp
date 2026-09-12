/** @file
 * The partial's merges: the total an unset field falls back to, the pure
 * field copy that folds two partials into one, the resolving merge that
 * turns a relative size into pixels, and the TextStyle a total builds.
 */

#include "sigilweave/style/Type.h"

namespace sigil::weave {

namespace {

/** The type size a `Type` that states none is set at, and the size every
 *  `rem` is a multiple of when no root is named. */
constexpr float kInitialSizePx = 16.0f;
/** What a single-spaced line is taken to be when the face that would answer
 *  is not in reach: about this much of the type size it is set in. */
constexpr float kAssumedLineHeightFactor = 1.2f;

bool sameAxis(const FontVariation& a, const FontVariation& b) {
  return a.tag[0] == b.tag[0] && a.tag[1] == b.tag[1] && a.tag[2] == b.tag[2] &&
         a.tag[3] == b.tag[3];
}

/** Sets one axis in `axes`, replacing it where it already stands so a
 *  merged list never carries two settings of one axis and its order is the
 *  order the axes were first mentioned in. */
void setAxis(std::vector<FontVariation>& axes, const FontVariation& axis) {
  for (FontVariation& present : axes)
    if (sameAxis(present, axis)) {
      present.value = axis.value;
      return;
    }
  axes.push_back(axis);
}

/** The px a length comes to. `againstPx` is what `em` multiplies and what
 *  an unknown line height is derived from; `rootSizePx` what `rem`
 *  multiplies; `lineHeightPx` what `lh` multiplies, 0 meaning unknown. */
float resolvePx(Length length, float againstPx, float rootSizePx,
                float lineHeightPx) {
  switch (length.unit) {
    case Length::Unit::Px:
      return length.value;
    case Length::Unit::Em:
      return length.value * againstPx;
    case Length::Unit::Rem:
      return length.value * rootSizePx;
    case Length::Unit::Lh:
      return length.value * (lineHeightPx > 0
                                 ? lineHeightPx
                                 : againstPx * kAssumedLineHeightFactor);
  }
  return length.value;
}

/** The px a relative size in a partial over `base` is stated against. A
 *  base that states no size — or states its own relatively, which a
 *  resolved total never does — leaves the initial size as the only number
 *  there is to multiply. */
float sizeAgainst(const Type& base) {
  if (base.size && !base.size->relative()) return base.size->value;
  return kInitialSizePx;
}

}  // namespace

Type initialType() {
  Type initial;
  initial.size = Length(kInitialSizePx);
  initial.color = SkColor4f{0, 0, 0, 1};
  initial.track = 0.0f;
  initial.condense = 1.0f;
  initial.weight = 0.0f;
  initial.slant = 0.0f;
  initial.aliased = false;
  initial.antiAlias = true;
  initial.color8 = false;
  return initial;
}

Type& merge(Type& into, const Type& over) {
  if (over.face) into.face = over.face;
  if (over.size) into.size = over.size;
  if (over.color) into.color = over.color;
  if (over.track) into.track = over.track;
  if (over.condense) into.condense = over.condense;
  if (over.weight) into.weight = over.weight;
  if (over.slant) into.slant = over.slant;
  if (over.aliased) into.aliased = over.aliased;
  if (over.antiAlias) into.antiAlias = over.antiAlias;
  if (over.color8) into.color8 = over.color8;
  for (const FontVariation& axis : over.variations)
    setAxis(into.variations, axis);
  return into;
}

Type overlay(const Type& base, const Type& over, float rootSizePx,
             float lineHeightPx) {
  Type total = base;
  merge(total, over);
  // The one field a copy cannot settle: a relative size is a statement
  // ABOUT the base's, so here is where it becomes a number.
  if (over.size && over.size->relative())
    total.size = Length(
        resolvePx(*over.size, sizeAgainst(base), rootSizePx, lineHeightPx));
  return total;
}

TextStyle toTextStyle(const Type& total) {
  TextStyle style;
  style.shaping.typeface = total.face;
  style.shaping.fontSize =
      total.size ? resolvePx(*total.size, kInitialSizePx, kInitialSizePx, 0.0f)
                 : kInitialSizePx;
  style.shaping.letterSpacing = total.track.value_or(0.0f);
  style.shaping.scaleX = total.condense.value_or(1.0f);
  style.shaping.aliased = total.aliased.value_or(false);
  const SkColor4f color = total.color.value_or(SkColor4f{0, 0, 0, 1});
  if (total.color8.value_or(false))
    style.paint.foreground.setColor(color.toSkColor());
  else
    style.paint.foreground.setColor4f(color, nullptr);
  style.paint.foreground.setAntiAlias(total.antiAlias.value_or(true));
  if (total.weight && *total.weight > 0) style.variation("wght", *total.weight);
  if (total.slant && *total.slant != 0) style.variation("slnt", *total.slant);
  for (const FontVariation& axis : total.variations)
    setAxis(style.shaping.variations, axis);
  return style;
}

TextStyle overlay(TextStyle base, const Type& over) {
  if (over.face) base.shaping.typeface = over.face;
  if (over.size) {
    const float against =
        base.shaping.fontSize > 0 ? base.shaping.fontSize : kInitialSizePx;
    base.shaping.fontSize =
        resolvePx(*over.size, against, kInitialSizePx, 0.0f);
  }
  if (over.track) base.shaping.letterSpacing = *over.track;
  if (over.condense) base.shaping.scaleX = *over.condense;
  if (over.aliased) base.shaping.aliased = *over.aliased;
  if (over.color) {
    if (over.color8.value_or(false))
      base.paint.foreground.setColor(over.color->toSkColor());
    else
      base.paint.foreground.setColor4f(*over.color, nullptr);
  }
  if (over.antiAlias) base.paint.foreground.setAntiAlias(*over.antiAlias);
  if (over.weight && *over.weight > 0) base.variation("wght", *over.weight);
  if (over.slant && *over.slant != 0) base.variation("slnt", *over.slant);
  for (const FontVariation& axis : over.variations)
    setAxis(base.shaping.variations, axis);
  return base;
}

}  // namespace sigil::weave
