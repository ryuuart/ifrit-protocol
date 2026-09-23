/** @file
 * The font and the ink — the partial every passage under a node is set
 * in, its longhands, and the colour or paint text and unnamed marks are
 * painted in.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilweave/style/Type.h>

#include <utility>

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& FontVerbs<Derived>::font(sigil::weave::Type partial) {
  detail::CascadeData& cascade = declare(Property::Font)->cascadeData.ensure();
  if (!cascade.font) cascade.font.emplace();
  // Later wins field by field: the partial written last replaces what it
  // names and leaves the rest as an earlier call or a class left it.
  sigil::weave::merge(*cascade.font, partial);
  // A colour written here is the ink, so a property the ink was read from
  // before no longer stands, and neither does a paint.
  if (partial.color) {
    cascade.inkVar.reset();
    cascade.inkPaint.reset();
    cascade.statesInk = true;
  }
  return self();
}

// The font's longhands: each is `font()` with one field, so the two
// spellings fold into one partial and the later statement wins.

template <class Derived>
Derived& FontVerbs<Derived>::fontFamily(sk_sp<SkTypeface> face) {
  return font({.face = std::move(face)});
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
Derived& FontVerbs<Derived>::fontStyle(float slant) {
  return font({.slant = slant});
}

template <class Derived>
Derived& FontVerbs<Derived>::letterSpacing(sigil::weave::Length tracking) {
  return font({.track = tracking});
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(material::Color colour) {
  detail::CascadeData& cascade = declare(Property::Ink)->cascadeData.ensure();
  if (!cascade.font) cascade.font.emplace();
  cascade.font->color = material::skia::toSkColor(colour);
  cascade.inkVar.reset();
  cascade.inkPaint.reset();
  cascade.statesInk = true;
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(VarRef reference) {
  detail::CascadeData& cascade = declare(Property::Ink)->cascadeData.ensure();
  cascade.inkVar = reference;
  if (cascade.font) cascade.font->color.reset();
  cascade.inkPaint.reset();
  cascade.statesInk = true;
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(SurfacePaint paint, PaintAnchor anchor) {
  detail::CascadeData& cascade = declare(Property::Ink)->cascadeData.ensure();
  // A PLAIN COLOUR is the ink lane as it has always been. A paint that
  // happens to be flat is not one: it overrides the glyphs of a leaf set
  // in a style of its own, which an inherited colour does not reach.
  const std::optional<Fill> flat =
      paint.writtenAsPaint() ? std::nullopt : paint.collapsedFill();
  if (flat && flat->kind == Fill::Kind::Color && !flat->references())
    return ink(flat->colorValue);
  // An empty paint STATES the lane and holds nothing, which clears an
  // ancestor's paint and leaves the colour in force standing.
  if (paint.none()) {
    cascade.statesInk = true;
    cascade.inkPaint.reset();
    cascade.inkAnchor = anchor;
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
  cascade.inkAnchor = anchor;
  return self();
}

template class FontVerbs<Element>;
template class FontVerbs<Text>;
template class FontVerbs<Image>;
template class FontVerbs<Band>;
template class FontVerbs<Rule>;
template class FontVerbs<Declarations>;

}  // namespace sigil::compose
