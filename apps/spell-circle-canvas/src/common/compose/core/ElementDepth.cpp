/** @file
 * The depth lanes — the turns and the move along the viewing axis, the
 * view a node declares for its children, the shared space, and which
 * face of a turned plane is drawn.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& DepthVerbs<Derived>::rotateX(motion::Animatable<float> v) {
  declare(Property::RotateX)->depthData.ensure().rotateX = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::rotateY(motion::Animatable<float> v) {
  declare(Property::RotateY)->depthData.ensure().rotateY = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::translateZ(motion::Animatable<float> v) {
  declare(Property::TranslateZ)->depthData.ensure().translateZ = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::scaleZ(motion::Animatable<float> v) {
  declare(Property::ScaleZ)->depthData.ensure().scaleZ = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::perspective(motion::Animatable<float> v) {
  declare(Property::Perspective)->depthData.ensure().perspective = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::perspectiveOrigin(Dimension x, Dimension y) {
  detail::DepthData& depth =
      declare(Property::PerspectiveOrigin)->depthData.ensure();
  depth.perspectiveOriginX = x;
  depth.perspectiveOriginY = y;
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::preserve3d(bool on) {
  declare(Property::Preserve3d)->depthData.ensure().preserve3d = on;
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::backface(material::Backface facing) {
  declare(Property::Backface)->depthData.ensure().backface = facing;
  return self();
}

template class DepthVerbs<Element>;
template class DepthVerbs<Text>;
template class DepthVerbs<Image>;
template class DepthVerbs<Band>;

}  // namespace sigil::compose
