/** @file
 * The compositing lanes — the opacity, the mount entrance written over
 * it, the blend, and the two filters over a node's layer.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& EffectVerbs<Derived>::opacity(motion::Animatable<float> o) {
  declarations()->paint.opacity = std::move(o);
  return self();
}

template <class Derived>
Derived& EffectVerbs<Derived>::appear(motion::Transition how) {
  return opacity(motion::animate(motion::from(0.0f).to(1.0f), std::move(how)));
}

template <class Derived>
Derived& EffectVerbs<Derived>::blend(SkBlendMode mode) {
  declarations()->paint.blendMode = mode;
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

}  // namespace sigil::compose
