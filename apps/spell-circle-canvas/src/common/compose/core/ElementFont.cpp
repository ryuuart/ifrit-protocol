/** @file
 * The font and the ink — the partial every passage under a node is set
 * in, its longhands, and the colour or paint text and unnamed marks are
 * painted in.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the ink box's diagnostics
#include <sigilmaterial/color/Color.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <type_traits>
#include <utility>

#include "ComposeInternal.h"

namespace sigil::compose {

namespace {

/** The once-per-process diagnostic behind an ink handed a text unit on a
 *  node that is no passage: a box has no glyphs, words or lines of its
 *  own to restart the paint on. */
void warnInkTextUnitNeedsAPassage() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[compose] ink() names a text unit (Glyph, Cluster, Word, Line or "
      "Sentence) on a node that is no passage — the unit was dropped and "
      "the paint is stretched over PaintBox::Element. State the unit on the "
      "text leaf, a span of it, or a rule that lands on it. (warned once)\n");
}

/** The once-per-process diagnostic behind an ink handed `Padding` or
 *  `Content`, which are rectangles of a fill's box: an ink is laid on
 *  the text it reaches, never on a node's padding. */
void warnInkTakesNoFillBox() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[compose] ink() was given PaintBox::Padding or PaintBox::Content, "
      "which place a fill inside its box — the paint is stretched over "
      "PaintBox::Element. (warned once)\n");
}

}  // namespace

template <class Derived>
Derived& FontVerbs<Derived>::font(sigil::weave::Type partial) {
  detail::CascadeData& cascade = declarations()->font();
  if (!cascade.font) cascade.font.emplace();
  // Later wins field by field: the partial written last replaces what it
  // names and leaves the rest as an earlier call or a class left it.
  sigil::weave::merge(*cascade.font, partial);
  // A face written here stands over a family named before it, and a lean
  // is a style of its own, which is no italic.
  if (partial.face || partial.keywords.find(sigil::weave::TypeField::Face))
    cascade.fontFamily.reset();
  if (partial.slant)
    cascade.italic = false;
  else if (partial.keywords.find(sigil::weave::TypeField::Slant))
    cascade.italic.reset();
  // A colour written here IS the ink — one lane, one bit — so it states
  // the ink as `ink()` does: a keyword stated about the ink before ends,
  // and a property the ink was read from and a paint no longer stand.
  if (partial.color) {
    declarations()->ink();
    cascade.inkVar.reset();
    cascade.inkPaint.reset();
    cascade.inkBox = PaintBox::Element;
    cascade.statesInk = true;
  }
  return self();
}

// The font's longhands: each is `font()` with one field, so the two
// spellings fold into one partial and the later statement wins.

template <class Derived>
Derived& FontVerbs<Derived>::fontFamily(std::string family) {
  // An empty name is the default family, which is a face the partial can
  // state; any other name waits for the cascade, where a context is.
  if (family.empty()) return font({.face = sigil::weave::defaultFace()});
  detail::CascadeData& cascade = declarations()->font();
  if (cascade.font) {
    cascade.font->face.reset();
    cascade.font->keywords.clear(sigil::weave::TypeField::Face);
    if (cascade.font->empty()) cascade.font.reset();
  }
  cascade.fontFamily = std::move(family);
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::fontSize(sigil::weave::Length size) {
  return font({.size = size});
}

template <class Derived>
Derived& FontVerbs<Derived>::fontWeight(float weight) {
  return font({.weight = weight});
}

template <class Derived>
Derived& FontVerbs<Derived>::fontStyle(FontStyle style) {
  // CSS leans right with a positive angle; OpenType's slnt axis leans
  // right with a negative one, so the angle is written negated. Normal
  // and Italic state no lean at all.
  font({.slant = style.kind == FontStyle::Kind::Oblique ? 0.0f - style.degrees
                                                        : 0.0f});
  if (style.kind == FontStyle::Kind::Italic)
    declarations()->font().italic = true;
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::letterSpacing(sigil::weave::Length tracking) {
  return font({.track = tracking});
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(material::Color colour) {
  detail::CascadeData& cascade = declarations()->ink();
  if (!cascade.font) cascade.font.emplace();
  cascade.font->color = colour;
  cascade.inkVar.reset();
  cascade.inkPaint.reset();
  cascade.inkBox = PaintBox::Element;
  cascade.statesInk = true;
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(VarRef reference) {
  detail::CascadeData& cascade = declarations()->ink();
  cascade.inkVar = reference;
  if (cascade.font) cascade.font->color.reset();
  cascade.inkPaint.reset();
  cascade.inkBox = PaintBox::Element;
  cascade.statesInk = true;
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(SurfacePaint paint, PaintBox box) {
  detail::CascadeData& cascade = declarations()->ink();
  // A PLAIN COLOUR is the ink lane as it has always been, and has no unit
  // square for a box to stretch. A paint that happens to be flat is not
  // one: it overrides the glyphs of a leaf set in a style of its own,
  // which an inherited colour does not reach.
  const std::optional<Fill> flat =
      paint.writtenAsPaint() ? std::nullopt : paint.collapsedFill();
  if (flat && flat->kind == Fill::Kind::Color && !flat->references())
    return ink(flat->colorValue);
  // A box the ink cannot stretch a paint over is read as the element's
  // own, and said so: the padding and the content rectangles place a
  // fill, and a text unit needs a passage to cut. A rule and a span may
  // land on one, so they keep theirs.
  if (box == PaintBox::Padding || box == PaintBox::Content) {
    warnInkTakesNoFillBox();
    box = PaintBox::Element;
  }
  constexpr bool landsOnText =
      std::is_same_v<Derived, Rule> || std::is_same_v<Derived, SpanStyle>;
  if (!landsOnText && detail::textUnitOf(box) &&
      declarations()->kind != detail::Kind::Text) {
    warnInkTextUnitNeedsAPassage();
    box = PaintBox::Element;
  }
  // An empty paint STATES the lane and holds nothing, which clears an
  // ancestor's paint and leaves the colour in force standing.
  if (paint.none()) {
    cascade.statesInk = true;
    cascade.inkPaint.reset();
    cascade.inkBox = box;
    return self();
  }
  // A fill the slot cannot hold — a live binding, the ink in force, a
  // custom property — leaves the ink exactly where it was, paint
  // included: a reference to the ink IS the ink, and a bound fill has no
  // paint to inherit. Nothing is written until there is something to
  // write, so a standing paint survives the asking.
  std::optional<material::skia::Paint> stored = paint.collapsedPaint();
  if (!stored) return self();
  cascade.statesInk = true;
  cascade.inkPaint = std::move(stored);
  cascade.inkBox = box;
  return self();
}

template class FontVerbs<Element>;
template class FontVerbs<Text>;
template class FontVerbs<Image>;
template class FontVerbs<Band>;
template class FontVerbs<Rule>;
template class FontVerbs<SpanStyle>;

}  // namespace sigil::compose
