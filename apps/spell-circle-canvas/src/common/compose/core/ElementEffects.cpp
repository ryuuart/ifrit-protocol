/** @file
 * The compositing lanes — the opacity, the blend, and the two filters
 * over a node's layer.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& EffectVerbs<Derived>::opacity(motion::Animatable<float> o) {
  declare(Property::Opacity)->paint.opacity = std::move(o);
  return self();
}

template <class Derived>
Derived& EffectVerbs<Derived>::blendMode(SkBlendMode mode) {
  declare(Property::BlendMode)->paint.blendMode = mode;
  return self();
}

template <class Derived>
Derived& EffectVerbs<Derived>::filter(material::skia::Effect e) {
  declarations()->fxData.ensure().layerEffect = std::move(e);
  return self();
}

template <class Derived>
Derived& EffectVerbs<Derived>::backdropFilter(material::skia::Effect e) {
  declarations()->fxData.ensure().backdropEffect = std::move(e);
  return self();
}

template class EffectVerbs<Element>;
template class EffectVerbs<Text>;
template class EffectVerbs<Image>;
template class EffectVerbs<Band>;
template class EffectVerbs<Rule>;

}  // namespace sigil::compose
