/** @file
 * The depth lanes — the turns and the move along the viewing axis, the
 * view a node declares for its children, the shared space, and which
 * face of a turned plane is drawn.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& DepthVerbs<Derived>::rotateX(motion::Animatable<float> v) {
  declarations()->depthData.ensure().rotateX = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::rotateY(motion::Animatable<float> v) {
  declarations()->depthData.ensure().rotateY = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::translateZ(motion::Animatable<float> v) {
  declarations()->depthData.ensure().translateZ = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::scaleZ(motion::Animatable<float> v) {
  declarations()->depthData.ensure().scaleZ = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::perspective(motion::Animatable<float> v) {
  declarations()->depthData.ensure().perspective = std::move(v);
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::perspectiveOrigin(float fx, float fy) {
  detail::DepthData& depth = declarations()->depthData.ensure();
  depth.perspectiveOriginX = fx;
  depth.perspectiveOriginY = fy;
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::transformOrigin3d(float fx, float fy, float zPx) {
  // The x and y ARE transformOrigin()'s fields, so the 2D pivot and the
  // 3D one can never disagree about where the plane turns.
  self().transformOrigin(fx, fy);
  declarations()->depthData.ensure().originZ = zPx;
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::preserve3d(bool on) {
  declarations()->depthData.ensure().preserve3d = on;
  return self();
}

template <class Derived>
Derived& DepthVerbs<Derived>::backface(material::Backface facing) {
  declarations()->depthData.ensure().backface = facing;
  return self();
}

template class DepthVerbs<Element>;

}  // namespace sigil::compose
