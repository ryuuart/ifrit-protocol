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

/** @p paint set in @p ink — through an 8-bit sRGB word where @p eightBit,
 *  else as the four floats themselves, copied field for field. */
void setInk(SkPaint& paint, const material::Color& ink, bool eightBit) {
  const SkColor4f floats{ink.r, ink.g, ink.b, ink.a};
  if (eightBit)
    paint.setColor(floats.toSkColor());
  else
    paint.setColor4f(floats, nullptr);
}

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
 *  an unknown line height and an unmeasured figure width are derived from;
 *  `rootSizePx` what `rem` multiplies; `lineHeightPx` what `lh`
 *  multiplies, 0 meaning unknown.
 *
 *  A `ch` is the advance of "0" in the face, which only a face in hand can
 *  say, and this resolver holds numbers rather than faces: it takes the
 *  stand-in a half of the type size, which is what a face carrying no
 *  figure zero is measured as. A caller holding the face resolves that
 *  unit itself. */
float resolvePx(Length length, float againstPx, float rootSizePx,
                float lineHeightPx) {
  switch (length.unit) {
    case Length::Unit::Px:
      return length.value;
    case Length::Unit::Pt:
      return length.value * Length::kPointPx;
    case Length::Unit::Em:
      return length.value * againstPx;
    case Length::Unit::Rem:
      return length.value * rootSizePx;
    case Length::Unit::Ch:
      return length.value * againstPx * Length::kAssumedZeroAdvanceEm;
    case Length::Unit::Lh:
      return length.value * (lineHeightPx > 0
                                 ? lineHeightPx
                                 : againstPx * kAssumedLineHeightFactor);
  }
  return length.value;
}


/** The field @p field of @p total taken from @p base or from the initial
 *  style, which is what a keyword said about it means. Every field of a
 *  text style inherits, so `unset` reads as `inherit` and `inherit` is
 *  the base's own value. */
void applyKeyword(Type& total, const Type& base, TypeField field,
                  Keyword keyword) {
  const Type& from = keyword == Keyword::Initial ? initialType() : base;
  switch (field) {
    case TypeField::Face:
      total.face = from.face;
      return;
    case TypeField::Size:
      total.size = from.size;
      return;
    case TypeField::Color:
      total.color = from.color;
      return;
    case TypeField::Track:
      total.track = from.track;
      return;
    case TypeField::Condense:
      total.condense = from.condense;
      return;
    case TypeField::Weight:
      total.weight = from.weight;
      return;
    case TypeField::Slant:
      total.slant = from.slant;
      return;
    case TypeField::Aliased:
      total.aliased = from.aliased;
      return;
    case TypeField::AntiAlias:
      total.antiAlias = from.antiAlias;
      return;
    case TypeField::Color8:
      total.color8 = from.color8;
      return;
    case TypeField::Variations:
      total.variations = from.variations;
      return;
    case TypeField::Language:
      total.language = from.language;
      return;
    case TypeField::Features:
      total.features = from.features;
      return;
    case TypeField::OpticalKerning:
      total.opticalKerning = from.opticalKerning;
      return;
    case TypeField::WordSpacing:
      total.wordSpacing = from.wordSpacing;
      return;
    case TypeField::TextTransform:
      total.textTransform = from.textTransform;
      return;
    case TypeField::VerticalForm:
      total.verticalForm = from.verticalForm;
      return;
    case TypeField::Decorations:
      total.decorations = from.decorations;
      return;
    case TypeField::Underlays:
      total.underlays = from.underlays;
      return;
    case TypeField::Overlays:
      total.overlays = from.overlays;
      return;
    case TypeField::BaselineShift:
      total.baselineShift = from.baselineShift;
      return;
    case TypeField::kCount:
      return;
  }
}

/** The px a relative size in a partial over `base` is stated against. A
 *  base that states no size — or states its own relatively, which a
 *  resolved total never does — leaves the initial size as the only number
 *  there is to multiply. */
float sizeAgainst(const Type& base) {
  if (base.size && !base.size->relative()) return base.size->absolutePx();
  return kInitialSizePx;
}

}  // namespace

Type initialType() {
  Type initial;
  initial.face = defaultFace();
  initial.size = Length(kInitialSizePx);
  initial.color = material::Color{0, 0, 0, 1};
  initial.track = Length(0.0f);
  initial.condense = 1.0f;
  initial.weight = 0.0f;
  initial.slant = 0.0f;
  initial.aliased = false;
  initial.antiAlias = true;
  initial.color8 = false;
  initial.language = std::string{};
  initial.features = std::vector<FontFeature>{};
  initial.opticalKerning = false;
  initial.wordSpacing = Length(0.0f);
  initial.textTransform = TextTransform::kNone;
  initial.verticalForm = VerticalForm::kAuto;
  initial.decorations = std::vector<Decoration>{};
  initial.underlays = std::vector<PaintLayer>{};
  initial.overlays = std::vector<PaintLayer>{};
  initial.baselineShift = 0.0f;
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
  if (over.language) into.language = over.language;
  if (over.features) into.features = over.features;
  if (over.opticalKerning) into.opticalKerning = over.opticalKerning;
  if (over.wordSpacing) into.wordSpacing = over.wordSpacing;
  if (over.textTransform) into.textTransform = over.textTransform;
  if (over.verticalForm) into.verticalForm = over.verticalForm;
  if (over.decorations) into.decorations = over.decorations;
  if (over.underlays) into.underlays = over.underlays;
  if (over.overlays) into.overlays = over.overlays;
  if (over.baselineShift) into.baselineShift = over.baselineShift;
  // The keywords ACCUMULATE and are not applied: a merge folds two
  // partials into one partial, and there is no style in force here to
  // resolve `inherit` against. `overlay` is where they land.
  into.keywords.overlay(over.keywords);
  return into;
}

Type overlay(const Type& base, const Type& over, float rootSizePx,
             float lineHeightPx) {
  Type total = base;
  merge(total, over);
  // The fields a copy cannot settle: a relative size is a statement ABOUT
  // the base's, and a relative tracking is a statement about the size the
  // type comes to, so here is where each becomes a number.
  if (over.size && over.size->relative())
    total.size = Length(
        resolvePx(*over.size, sizeAgainst(base), rootSizePx, lineHeightPx));
  if (total.track && total.track->relative())
    total.track = Length(
        resolvePx(*total.track, sizeAgainst(total), rootSizePx, lineHeightPx));
  if (total.wordSpacing && total.wordSpacing->relative())
    total.wordSpacing = Length(resolvePx(*total.wordSpacing, sizeAgainst(total),
                                         rootSizePx, lineHeightPx));
  // A field written as a keyword takes the base's value or the initial
  // one, over whatever the merge copied and over what the resolver just
  // settled: the two are ONE layer, and the keyword is the statement that
  // wins. A resolved total states none of them, so nothing below applies
  // one twice.
  for (const KeywordTable<TypeField>::Entry& entry : over.keywords.entries())
    applyKeyword(total, base, entry.field, entry.keyword);
  total.keywords = {};
  return total;
}

TextStyle toTextStyle(const Type& total) {
  TextStyle style;
  style.shaping.typeface = total.face.value_or(nullptr);
  style.shaping.fontSize =
      total.size ? resolvePx(*total.size, kInitialSizePx, kInitialSizePx, 0.0f)
                 : kInitialSizePx;
  style.shaping.letterSpacing =
      total.track ? resolvePx(*total.track, style.shaping.fontSize,
                              kInitialSizePx, 0.0f)
                  : 0.0f;
  style.shaping.scaleX = total.condense.value_or(1.0f);
  style.shaping.aliased = total.aliased.value_or(false);
  setInk(style.paint.foreground,
         total.color.value_or(material::Color{0, 0, 0, 1}),
         total.color8.value_or(false));
  style.paint.foreground.setAntiAlias(total.antiAlias.value_or(true));
  if (total.weight && *total.weight > 0) style.variation("wght", *total.weight);
  if (total.slant && *total.slant != 0) style.variation("slnt", *total.slant);
  for (const FontVariation& axis : total.variations)
    setAxis(style.shaping.variations, axis);
  style.shaping.wordSpacing =
      total.wordSpacing ? resolvePx(*total.wordSpacing, style.shaping.fontSize,
                                    kInitialSizePx, 0.0f)
                        : 0.0f;
  if (total.language) style.shaping.languageTag = *total.language;
  if (total.features) style.shaping.fontFeatures = *total.features;
  style.shaping.opticalKerning = total.opticalKerning.value_or(false);
  style.shaping.textTransform =
      total.textTransform.value_or(TextTransform::kNone);
  style.shaping.verticalForm = total.verticalForm.value_or(VerticalForm::kAuto);
  if (total.decorations) style.paint.decorations = *total.decorations;
  if (total.underlays) style.paint.underlays = *total.underlays;
  if (total.overlays) style.paint.overlays = *total.overlays;
  style.paint.baselineShift = total.baselineShift.value_or(0.0f);
  return style;
}

TextStyle overlay(TextStyle base, const Type& over) {
  if (over.face) base.shaping.typeface = *over.face;
  if (over.size) {
    const float against =
        base.shaping.fontSize > 0 ? base.shaping.fontSize : kInitialSizePx;
    base.shaping.fontSize =
        resolvePx(*over.size, against, kInitialSizePx, 0.0f);
  }
  if (over.track)
    base.shaping.letterSpacing =
        resolvePx(*over.track, base.shaping.fontSize, kInitialSizePx, 0.0f);
  if (over.condense) base.shaping.scaleX = *over.condense;
  if (over.aliased) base.shaping.aliased = *over.aliased;
  if (over.color)
    setInk(base.paint.foreground, *over.color, over.color8.value_or(false));
  if (over.antiAlias) base.paint.foreground.setAntiAlias(*over.antiAlias);
  if (over.weight && *over.weight > 0) base.variation("wght", *over.weight);
  if (over.slant && *over.slant != 0) base.variation("slnt", *over.slant);
  for (const FontVariation& axis : over.variations)
    setAxis(base.shaping.variations, axis);
  if (over.wordSpacing)
    base.shaping.wordSpacing = resolvePx(
        *over.wordSpacing, base.shaping.fontSize, kInitialSizePx, 0.0f);
  if (over.language) base.shaping.languageTag = *over.language;
  if (over.features) base.shaping.fontFeatures = *over.features;
  if (over.opticalKerning) base.shaping.opticalKerning = *over.opticalKerning;
  if (over.textTransform) base.shaping.textTransform = *over.textTransform;
  if (over.verticalForm) base.shaping.verticalForm = *over.verticalForm;
  if (over.decorations) base.paint.decorations = *over.decorations;
  if (over.underlays) base.paint.underlays = *over.underlays;
  if (over.overlays) base.paint.overlays = *over.overlays;
  if (over.baselineShift) base.paint.baselineShift = *over.baselineShift;
  return base;
}

bool reshapes(const Type& partial) {
  // A field written as a KEYWORD is a field stated: whichever value it
  // resolves to, the glyphs are laid out again, so a keyword on any
  // shaping field answers here exactly as a number would.
  for (const KeywordTable<TypeField>::Entry& entry : partial.keywords.entries())
    switch (entry.field) {
      case TypeField::Color:
      case TypeField::AntiAlias:
      case TypeField::Color8:
      case TypeField::Decorations:
      case TypeField::Underlays:
      case TypeField::Overlays:
      case TypeField::BaselineShift:
      case TypeField::kCount:
        break;
      default:
        return true;
    }
  return partial.face.has_value() || partial.size || partial.track ||
         partial.condense || partial.weight || partial.slant ||
         partial.aliased || !partial.variations.empty() || partial.language ||
         partial.features || partial.opticalKerning || partial.wordSpacing ||
         partial.textTransform || partial.verticalForm;
}

}  // namespace sigil::weave
