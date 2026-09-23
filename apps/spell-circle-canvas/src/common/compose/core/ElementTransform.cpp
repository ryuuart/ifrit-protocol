/** @file
 * The 2D transform lanes — translate, rotate, scale and skew about the
 * transform origin, the motion path, and the stacking index.
 */

#include <type_traits>

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& TransformVerbs<Derived>::translateX(motion::Animatable<float> v) {
  declarations()->fields.translateX() = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::translateY(motion::Animatable<float> v) {
  declarations()->fields.translateY() = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::travel(MotionPath along) {
  declarations()->motionData.ensure() = std::move(along);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::rotate(motion::Animatable<float> v) {
  declarations()->fields.rotate() = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::scale(motion::Animatable<float> v) {
  declarations()->fields.scale() = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::scaleX(motion::Animatable<float> v) {
  declarations()->fields.scaleX() = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::scaleY(motion::Animatable<float> v) {
  declarations()->fields.scaleY() = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::skewX(motion::Animatable<float> v) {
  declarations()->fields.skewX() = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::skewY(motion::Animatable<float> v) {
  declarations()->fields.skewY() = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::transformOrigin(Dimension x, Dimension y,
                                                  Dimension z) {
  if constexpr (std::is_same_v<Derived, Rule>) {
    // The depth lives on the description, which a rule's layer does not
    // carry, so a rule states the flat pivot alone.
    const detail::DeclaredFields::TransformOrigin origin =
        declarations()->fields.transformOrigin();
    origin.x = x;
    origin.y = y;
    if (z != Dimension(0.0f))
      detail::warnRuleCannotState(Property::TransformOriginZ);
    return self();
  }
  detail::ElementNode* node = declarations();
  const detail::DeclaredFields::TransformOrigin origin =
      node->fields.transformOrigin();
  origin.x = x;
  origin.y = y;
  detail::Box<detail::DepthData>& depth = node->transformOriginZ();
  if (z.unit == Dimension::Unit::Pct) {
    detail::warnPercentOriginDepth();
    z = Dimension(0.0f);
  }
  // The depth lives in the block a flat node does not carry, so only a
  // pivot off the plane, or a node that already has the block, writes it.
  if (depth || z != Dimension(0.0f)) depth.ensure().originZ = z;
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::zIndex(int z) {
  declarations()->fields.zIndex() = z;
  return self();
}

template class TransformVerbs<Element>;
template class TransformVerbs<Text>;
template class TransformVerbs<Image>;
template class TransformVerbs<Band>;
template class TransformVerbs<Rule>;

}  // namespace sigil::compose
