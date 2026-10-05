/** @file
 * A node's lane list: the fixed rows, in order, with the endpoints a
 * patch ramps from or to where a description does not carry one.
 */

#include <sigilworld/advanced/Lanes.h>

#include <optional>

namespace sigil::world {

float standingValue(Slot slot) {
  switch (slot) {
    case kScaleX:
    case kScaleY:
    case kScaleZ:
      return 1.0f;
    default:
      return 0.0f;
  }
}

void lanesOf(const NodeDescription& node, std::vector<Lane>& out) {
  out.clear();
  out.reserve(kLaneCount);
  const auto push = [&out](Slot slot, const motion::Animatable<float>* value) {
    out.push_back(Lane{
        value, motion::LaneSlot<LaneFamily>{LaneFamily::Slot, (size_t)slot},
        standingValue(slot)});
  };
  const Transform& t = node.transform;
  push(kTranslateX, &t.translateX);
  push(kTranslateY, &t.translateY);
  push(kTranslateZ, &t.translateZ);
  push(kRotateX, &t.rotateX);
  push(kRotateY, &t.rotateY);
  push(kRotateZ, &t.rotateZ);
  push(kScaleX, &t.scaleX);
  push(kScaleY, &t.scaleY);
  push(kScaleZ, &t.scaleZ);
  push(kOriginX, &t.originX);
  push(kOriginY, &t.originY);
  push(kOriginZ, &t.originZ);
  push(kAxisDegrees, &t.axisDegrees);
  push(kAlongDistance, node.along ? &node.along->distance : nullptr);
}

}  // namespace sigil::world
