/** @file
 * The node's own surface — the transitionable fill, and the paint a
 * fill collapses from.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& PaintVerbs<Derived>::fill(motion::Animatable<Fill> f) {
  detail::ElementNode* node = declarations();
  node->paint.fill = std::move(f);
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
Derived& PaintVerbs<Derived>::fill(material::skia::Paint m) {
  detail::ElementNode* node = declarations();
  detail::MaterialData& slots = node->materialData.ensure();
  if (m.isAnimated() || m.geometryDependent()) {
    // Live paints re-resolve per frame; geometry-dependent ones resolve
    // when the node records (and re-record on size change) — both route
    // through the material slot so the painter resolves with the frame.
    slots.live = std::move(m);
    node->paint.fill.reset();
    slots.recipe.reset();
  } else {
    node->paint.fill = motion::Animatable<Fill>{toFill(m)};
    slots.recipe = std::move(m);  // the prune signature
    slots.live.reset();
  }
  return self();
}

template class PaintVerbs<Element>;
template class PaintVerbs<Text>;
template class PaintVerbs<Image>;
template class PaintVerbs<Band>;

}  // namespace sigil::compose
