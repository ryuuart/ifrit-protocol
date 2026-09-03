/** @file
 * Element's own paint — the transitionable fill, opacity, the blend, the
 * layer and backdrop effects, and the stacking index. A paint as a
 * fill, and the glyph fill, are Fills.cpp's.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

Element& Element::fill(motion::Animatable<Fill> f) {
  m_node->paint.fill = std::move(f);
  // Symmetric with fill(Material): the fill setters are last-wins — a plain
  // fill after a live-material fill must actually take effect (and release
  // the node from the live-volatile path). staticMaterial must drop too, or
  // a stale equal-comparing recipe would over-prune this new fill.
  // Dropping the WHOLE block (not just its members) keeps propsEqual's
  // block-presence check aligned with a node that never had a material.
  m_node->materialData = {};
  return *this;
}

Element& Element::effect(material::skia::Effect e) {
  m_node->fxData.ensure().layerEffect = std::move(e);
  return *this;
}

Element& Element::backdrop(material::skia::Effect e) {
  m_node->fxData.ensure().backdropEffect = std::move(e);
  return *this;
}

Element& Element::opacity(motion::Animatable<float> o) {
  m_node->paint.opacity = std::move(o);
  return *this;
}

Element& Element::blend(SkBlendMode mode) {
  m_node->paint.blendMode = mode;
  return *this;
}

Element& Element::zIndex(int z) {
  m_node->paint.zIndex = z;
  return *this;
}

}  // namespace sigil::compose
