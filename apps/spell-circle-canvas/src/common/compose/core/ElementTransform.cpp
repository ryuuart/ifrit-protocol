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
Derived& TransformVerbs<Derived>::transformOrigin(float fx, float fy) {
  detail::ElementNode* node = declarations();
  node->paint.originX = fx;
  node->paint.originY = fy;
  node->paint.originPx = false;
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::transformOriginPx(SkPoint p) {
  detail::ElementNode* node = declarations();
  node->paint.originX = p.x();
  node->paint.originY = p.y();
  node->paint.originPx = true;
  return self();
}

template <class Derived>
Derived& TransformVerbs<Derived>::zIndex(int z) {
  declarations()->paint.zIndex = z;
  return self();
}

template class TransformVerbs<Element>;

}  // namespace sigil::compose
