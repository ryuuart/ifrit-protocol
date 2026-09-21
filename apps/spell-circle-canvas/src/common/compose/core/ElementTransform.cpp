/** @file
 * The 2D transform lanes — translate, rotate, scale and skew about the
 * transform origin, the motion path, and the stacking index.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& TransformVerbs<Derived>::translateX(motion::Animatable<float> v) {
  declarations()->paint.translateX = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::translateY(motion::Animatable<float> v) {
  declarations()->paint.translateY = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::travel(MotionPath along) {
  declarations()->motionData.ensure() = std::move(along);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::rotate(motion::Animatable<float> v) {
  declarations()->paint.rotate = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::scale(motion::Animatable<float> v) {
  declarations()->paint.scale = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::scaleX(motion::Animatable<float> v) {
  declarations()->paint.scaleX = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::scaleY(motion::Animatable<float> v) {
  declarations()->paint.scaleY = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::skewX(motion::Animatable<float> v) {
  declarations()->paint.skewX = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::skewY(motion::Animatable<float> v) {
  declarations()->paint.skewY = std::move(v);
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::transformOrigin(Dimension x, Dimension y,
                                                  Dimension z) {
  detail::ElementNode* node = declarations();
  node->paint.originX = x;
  node->paint.originY = y;
  if (z.unit == Dimension::Unit::Pct) {
    detail::warnPercentOriginDepth();
    z = Dimension(0.0f);
  }
  // The depth lives in the block a flat node does not carry, so only a
  // pivot off the plane, or a node that already has the block, writes it.
  if (node->depthData || z != Dimension(0.0f))
    node->depthData.ensure().originZ = z;
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::zIndex(int z) {
  declarations()->paint.zIndex = z;
  return self();
}

template class TransformVerbs<Element>;
template class TransformVerbs<Text>;
template class TransformVerbs<Image>;
template class TransformVerbs<Band>;

}  // namespace sigil::compose
