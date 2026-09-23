/** @file
 * The depth lanes — the turns and the move along the viewing axis, the
 * view a node declares for its children, the shared space, and which
 * face of a turned plane is drawn.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& DepthVerbs<Derived>::rotateX(motion::Animatable<float> v) {
  declarations()->rotateX() = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::rotateY(motion::Animatable<float> v) {
  declarations()->rotateY() = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::translateZ(motion::Animatable<float> v) {
  declarations()->translateZ() = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::scaleZ(motion::Animatable<float> v) {
  declarations()->scaleZ() = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::perspective(motion::Animatable<float> v) {
  declarations()->perspective() = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::perspectiveOrigin(Dimension x, Dimension y) {
  const detail::ElementNode::PerspectiveOrigin origin =
      declarations()->perspectiveOrigin();
  origin.x = x;
  origin.y = y;
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::preserve3d(bool on) {
  declarations()->preserve3d() = on;
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::backface(material::Backface facing) {
  declarations()->backface() = facing;
  return self();
}

template class DepthVerbs<Element>;
template class DepthVerbs<Text>;
template class DepthVerbs<Image>;
template class DepthVerbs<Band>;

}  // namespace sigil::compose
