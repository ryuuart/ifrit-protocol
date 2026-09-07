/** @file
 * Element's transform lanes — the 2D translate, rotate, scale and skew
 * about the transform origin, the motion path, and the depth lanes with
 * the view a node declares for its children.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

Element& Element::translateX(motion::Animatable<float> v) {
  m_node->paint.translateX = std::move(v);
  return *this;
}

Element& Element::translateY(motion::Animatable<float> v) {
  m_node->paint.translateY = std::move(v);
  return *this;
}

Element& Element::travel(MotionPath along) {
  m_node->motionData.ensure() = std::move(along);
  return *this;
}

Element& Element::rotate(motion::Animatable<float> v) {
  m_node->paint.rotate = std::move(v);
  return *this;
}

Element& Element::scale(motion::Animatable<float> v) {
  m_node->paint.scale = std::move(v);
  return *this;
}

Element& Element::scaleX(motion::Animatable<float> v) {
  m_node->paint.scaleX = std::move(v);
  return *this;
}

Element& Element::scaleY(motion::Animatable<float> v) {
  m_node->paint.scaleY = std::move(v);
  return *this;
}

Element& Element::skewX(motion::Animatable<float> v) {
  m_node->paint.skewX = std::move(v);
  return *this;
}

Element& Element::skewY(motion::Animatable<float> v) {
  m_node->paint.skewY = std::move(v);
  return *this;
}

Element& Element::transformOrigin(float fx, float fy) {
  m_node->paint.originX = fx;
  m_node->paint.originY = fy;
  m_node->paint.originPx = false;
  return *this;
}

Element& Element::transformOriginPx(SkPoint p) {
  m_node->paint.originX = p.x();
  m_node->paint.originY = p.y();
  m_node->paint.originPx = true;
  return *this;
}

Element& Element::rotateX(motion::Animatable<float> v) {
  m_node->depthData.ensure().rotateX = std::move(v);
  return *this;
}

Element& Element::rotateY(motion::Animatable<float> v) {
  m_node->depthData.ensure().rotateY = std::move(v);
  return *this;
}

Element& Element::rotateZ(motion::Animatable<float> v) {
  // ONE lane: the 2D rotation IS the rotation about the viewing axis, and
  // a second field for the same turn would be two settings of one thing.
  return rotate(std::move(v));
}

Element& Element::translateZ(motion::Animatable<float> v) {
  m_node->depthData.ensure().translateZ = std::move(v);
  return *this;
}

Element& Element::scaleZ(motion::Animatable<float> v) {
  m_node->depthData.ensure().scaleZ = std::move(v);
  return *this;
}

Element& Element::perspective(motion::Animatable<float> v) {
  m_node->depthData.ensure().perspective = std::move(v);
  return *this;
}

Element& Element::perspectiveOrigin(float fx, float fy) {
  detail::DepthData& depth = m_node->depthData.ensure();
  depth.perspectiveOriginX = fx;
  depth.perspectiveOriginY = fy;
  return *this;
}

Element& Element::transformOrigin3d(float fx, float fy, float zPx) {
  // The x and y ARE transformOrigin()'s fields, so the 2D pivot and the
  // 3D one can never disagree about where the plane turns.
  transformOrigin(fx, fy);
  m_node->depthData.ensure().originZ = zPx;
  return *this;
}

Element& Element::preserve3d(bool on) {
  m_node->depthData.ensure().preserve3d = on;
  return *this;
}

Element& Element::backface(Backface facing) {
  m_node->depthData.ensure().backface = facing;
  return *this;
}

}  // namespace sigil::compose
