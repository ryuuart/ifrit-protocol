/** @file
 * The node's own surface — the transitionable fill, and the paint a
 * fill collapses from.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the fill box's diagnostic

#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>

#include "ComposeInternal.h"
#include "MaterialEffects.h"

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

/** A paint placed as a node's fill over @p box: a static one collapses
 *  onto the fill slot, a live or geometry-dependent one is kept whole on
 *  the material slot for the painter to resolve with the frame. */
void placePaint(detail::ElementNode* node, material::Paint m, PaintBox box) {
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
  std::optional<motion::Animatable<Fill>>& fill = node->fields.fill();
  node->fields.fillBox() = box;
  detail::MaterialData& slots = node->materialData.ensure();
  if (m.isRunning() || m.geometryDependent()) {
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
}

}  // namespace

template <class Derived>
Derived& PaintVerbs<Derived>::fill(motion::Animatable<Fill> f) {
  // A paint written as a fill is the paint: it takes the paint's own
  // route, so a live or geometry-dependent one is resolved with the frame.
  if (const Fill* plain = f.constant();
      plain && plain->kind == Fill::Kind::Paint) {
    placePaint(declarations(), detail::paintOf(*plain), PaintBox::Element);
    return self();
  }
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
Derived& PaintVerbs<Derived>::fill(material::Material material, PaintBox box) {
  if (const material::Filter* effects = material.effects())
    detail::applyEffects(*declarations(), *effects);
  fill(Fill::fromMaterial(material), box);
  // A surface that takes light keeps its material: the painter shades it
  // under the lighting in force, and paints the fill above where none is.
  if (material::skia::isLit(material))
    declarations()->materialData.ensure().surfaced = std::move(material);
  return self();
}

template <class Derived>
Derived& PaintVerbs<Derived>::fill(Fill fill, PaintBox box) {
  if (fill.kind == Fill::Kind::Paint) {
    placePaint(declarations(), detail::paintOf(fill), box);
    return self();
  }
  if (detail::textUnitOf(box)) warnFillTakesNoTextUnit();
  return this->fill(motion::Animatable<Fill>{std::move(fill)});
}

template class PaintVerbs<Element>;
template class PaintVerbs<Text>;
template class PaintVerbs<Image>;
template class PaintVerbs<Band>;
template class PaintVerbs<Rule>;

}  // namespace sigil::compose
