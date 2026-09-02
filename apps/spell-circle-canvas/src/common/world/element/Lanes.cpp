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
    // An environment believed in full on both sides, which is what a
    // panorama placed with no dials on it means.
    case kEnvironmentDiffuse:
    case kEnvironmentSpecular:
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
        Lane{value, motion::LaneSlot<LaneFamily>{LaneFamily::Slot, (size_t)slot},
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
                 motion::LaneSlot<LaneFamily>{LaneFamily::Slot, (size_t)slot},
                 standing});
      };
  // A node carries an emitter OR an environment map, and both read the
  // strength and colour rows — a panorama placed in a set is an emitter
  // of a kind, and its tint is the colour it shines in.
  const Environment* sky = node.environment ? &*node.environment : nullptr;
  const auto standing = [&](Slot slot, float fromLight, float fromSky) {
    if (node.light) return fromLight;
    if (sky) return fromSky;
    return standingValue(slot);
  };
  pushEmitter(kIntensity, emission ? &emission->intensity : nullptr,
              standing(kIntensity, node.light ? node.light->intensity : 0,
                       sky ? sky->intensity : 0));
  pushEmitter(kEmissionRed, emission ? &emission->red : nullptr,
              standing(kEmissionRed, node.light ? node.light->color.r : 0,
                       sky ? sky->tint.x : 0));
  pushEmitter(kEmissionGreen, emission ? &emission->green : nullptr,
              standing(kEmissionGreen, node.light ? node.light->color.g : 0,
                       sky ? sky->tint.y : 0));
  pushEmitter(kEmissionBlue, emission ? &emission->blue : nullptr,
              standing(kEmissionBlue, node.light ? node.light->color.b : 0,
                       sky ? sky->tint.z : 0));

  // The environment's own six, standing where the environment stands.
  const SkyDials* dials = node.sky ? &*node.sky : nullptr;
  const auto pushSky =
      [&](Slot slot, std::optional<motion::Animatable<float>> SkyDials::*member,
          float own) {
        const std::optional<motion::Animatable<float>>* value =
            dials ? &(dials->*member) : nullptr;
        out.push_back(
            Lane{value && *value ? &**value : nullptr,
                 motion::LaneSlot<LaneFamily>{LaneFamily::Slot, (size_t)slot},
                 sky ? own : standingValue(slot)});
      };
  pushSky(kEnvironmentDiffuse, &SkyDials::diffuse, sky ? sky->diffuse : 1);
  pushSky(kEnvironmentSpecular, &SkyDials::specular, sky ? sky->specular : 1);
  pushSky(kEnvironmentRoughness, &SkyDials::roughnessBias,
          sky ? sky->roughnessBias : 0);
  pushSky(kEnvironmentCrossfade, &SkyDials::crossfade, sky ? sky->crossfade : 0);
  pushSky(kBackdrop, &SkyDials::backdrop, sky ? sky->backdrop.intensity : 0);
  pushSky(kBackdropBlur, &SkyDials::backdropBlur,
          sky ? sky->backdrop.blur : 0);
}

}  // namespace sigil::world
