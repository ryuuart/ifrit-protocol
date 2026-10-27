/** @file
 * The compositing lanes — the opacity, the blend, and the two filters
 * over a node's layer.
 */

#include "description/ComposeInternal.h"
#include "runtime/Warning.h"

namespace sigil::compose {

namespace {

/** A coverage step reads the shape of the layer it dresses, which a
 *  subtree filter does not have: said once, and the step paints nothing. */
void warnCoverageIsAMaterialsEffect(const material::Filter& filter) {
  static thread_local bool warned = false;
  if (warned || filter.coverage().empty()) return;
  warned = true;
  sigil::compose::detail::reportWarning(
      "filter() was given Filter::shadow, stroke or bevel, which "
      "read a layer's coverage and paint nothing over a subtree; state them "
      "in a material's effects and fill() with it. (warned once)");
}

}  // namespace

template <class Derived>
Derived& EffectVerbs<Derived>::opacity(motion::Animatable<float> amount) {
  declarations()->fields.opacity() = std::move(amount);
  return self();
}

template <class Derived>
Derived& EffectVerbs<Derived>::blendMode(material::BlendMode mode) {
  declarations()->fields.blendMode() = material::skia::toSkBlendMode(mode);
  return self();
}

template <class Derived>
Derived& EffectVerbs<Derived>::filter(material::Filter effect) {
  warnCoverageIsAMaterialsEffect(effect);
  declarations()->fxData.ensure().layerEffect = std::move(effect);
  return self();
}

template <class Derived>
Derived& EffectVerbs<Derived>::backdropFilter(material::Filter effect) {
  declarations()->fxData.ensure().backdropEffect = std::move(effect);
  return self();
}

template class EffectVerbs<Element>;
template class EffectVerbs<Text>;
template class EffectVerbs<Image>;
template class EffectVerbs<Band>;
template class EffectVerbs<Rule>;

}  // namespace sigil::compose
