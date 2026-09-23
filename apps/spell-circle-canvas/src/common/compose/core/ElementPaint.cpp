/** @file
 * The node's own surface — the transitionable fill, and the paint a
 * fill collapses from.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the fill box's diagnostic

#include "ComposeInternal.h"

namespace sigil::compose {

namespace {

/** The once-per-process diagnostic behind a fill handed a text unit: a
 *  box has no glyphs, words or lines to restart a paint on. */
void warnFillTakesNoTextUnit() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[compose] fill() was given a text unit (Glyph, Cluster, Word, Line or "
      "Sentence), which a box has none of — the paint is stretched over "
      "PaintBox::Element. A unit restarts an ink. (warned once)\n");
}

}  // namespace

template <class Derived>
Derived& PaintVerbs<Derived>::fill(motion::Animatable<Fill> f) {
  detail::ElementNode* node = declarations();
  node->fields.fill() = std::move(f);
  // The box is part of the fill's statement, so a fill with no picture to
  // place states the element's own and ends whatever an earlier one said.
  node->fields.fillBox() = PaintBox::Element;
  // Symmetric with fill(Material): the fill setters are last-wins — a plain
  // fill after a live-material fill must actually take effect (and release
  // the node from the live-volatile path). staticMaterial must drop too, or
  // a stale equal-comparing recipe would over-prune this new fill.
  // Dropping the WHOLE block (not just its members) keeps propertiesEqual's
  // block-presence check aligned with a node that never had a material.
  node->materialData = {};
  return self();
}

template <class Derived>
Derived& PaintVerbs<Derived>::fill(material::skia::Paint m, PaintBox box) {
  switch (box) {
    case PaintBox::Element:
    case PaintBox::Padding:
    case PaintBox::Content:
      break;
    // A fill does not inherit, so the element that stated it is the one
    // painting it: the subtree's box is its own.
    case PaintBox::Subtree:
      box = PaintBox::Element;
      break;
    // The canvas IS root anchoring: the unit square maps onto the canvas
    // and the shader is sampled through the node's place in it, which is
    // the one mechanism the paint library already resolves through.
    case PaintBox::Canvas:
      m.worldSpace(true);
      break;
    case PaintBox::Glyph:
    case PaintBox::Cluster:
    case PaintBox::Word:
    case PaintBox::Line:
    case PaintBox::Sentence:
      warnFillTakesNoTextUnit();
      box = PaintBox::Element;
      break;
  }
  detail::ElementNode* node = declarations();
  std::optional<motion::Animatable<Fill>>& fill = node->fields.fill();
  node->fields.fillBox() = box;
  detail::MaterialData& slots = node->materialData.ensure();
  if (m.isAnimated() || m.geometryDependent()) {
    // Live paints re-resolve per frame; geometry-dependent ones resolve
    // when the node records (and re-record on size change) — both route
    // through the material slot so the painter resolves with the frame.
    slots.live = std::move(m);
    fill.reset();
    slots.recipe.reset();
  } else {
    fill = motion::Animatable<Fill>{toFill(m)};
    slots.recipe = std::move(m);  // the prune signature
    slots.live.reset();
  }
  return self();
}

template <class Derived>
Derived& PaintVerbs<Derived>::fillSurface(const SurfacePaint& paint,
                                          PaintBox box) {
  if (box != PaintBox::Element && !paint.none())
    if (std::optional<material::skia::Paint> placed = paint.collapsedPaint())
      return fill(std::move(*placed), box);
  if (detail::textUnitOf(box)) warnFillTakesNoTextUnit();
  paint.apply(self());
  return self();
}

template class PaintVerbs<Element>;
template class PaintVerbs<Text>;
template class PaintVerbs<Image>;
template class PaintVerbs<Band>;
template class PaintVerbs<Rule>;

}  // namespace sigil::compose
