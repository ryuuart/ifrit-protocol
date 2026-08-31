/** @file
 * A node's lane list: the fixed rows, in order, with the endpoints a
 * patch ramps from or to where a description does not carry one.
 */

#include <sigilworld/element/Lanes.h>

#include <optional>

namespace sigil::world {

float standingValue(Slot slot) {
  switch (slot) {
    case kScaleX:
    case kScaleY:
    case kScaleZ:
    // A window with no head and span covers the whole loop, so a
    // description that drops one ramps back to the whole loop rather
    // than to nothing.
    case kWindowHead:
    case kWindowSpan:
    // An emitter with no dial on it shines at full strength in its own
    // colour, which is the emitter's fields standing unscaled.
    case kIntensity:
    case kEmissionRed:
    case kEmissionGreen:
    case kEmissionBlue:
      return 1.0f;
    default:
      return 0.0f;
  }
}

void lanesOf(const ElementNode& node, std::vector<Lane>& out) {
  out.clear();
  out.reserve(kLaneCount);
  const auto push = [&out](Slot slot, const motion::Animatable<float>* value) {
    out.push_back(
        Lane{value, core::LaneSlot<LaneFamily>{LaneFamily::Slot, (size_t)slot},
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
  push(kWindowHead, node.window ? &node.window->head : nullptr);
  push(kWindowSpan, node.window ? &node.window->span : nullptr);

  // The emitter's rows stand where the emitter itself stands, so a
  // dropped dial ramps back to the light's own field rather than to one.
  const Emission* emission = node.emission ? &*node.emission : nullptr;
  const auto pushEmitter =
      [&out](Slot slot, const std::optional<motion::Animatable<float>>* value,
             float standing) {
        out.push_back(
            Lane{value && *value ? &**value : nullptr,
                 core::LaneSlot<LaneFamily>{LaneFamily::Slot, (size_t)slot},
                 standing});
      };
  pushEmitter(kIntensity, emission ? &emission->intensity : nullptr,
              node.light ? node.light->intensity : standingValue(kIntensity));
  pushEmitter(kEmissionRed, emission ? &emission->red : nullptr,
              node.light ? node.light->color.r : standingValue(kEmissionRed));
  pushEmitter(kEmissionGreen, emission ? &emission->green : nullptr,
              node.light ? node.light->color.g : standingValue(kEmissionGreen));
  pushEmitter(kEmissionBlue, emission ? &emission->blue : nullptr,
              node.light ? node.light->color.b : standingValue(kEmissionBlue));
}

}  // namespace sigil::world
