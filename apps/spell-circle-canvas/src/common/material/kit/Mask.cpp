/** @file
 * The two mask bodies in each language a renderer speaks — a shaped
 * constant and a shaped reading of a source slot — and the factories
 * that fix which reading is used.
 *
 * A mask answers a SCALAR that its caller reads out of the red channel,
 * so both bodies return it in all three colour channels at full alpha:
 * a mask drawn on its own is a grey picture of where it applies.
 */

#include "sigilmaterial/kit/Mask.h"

#include <sigilmaterial/core/Program.h>
#include <sigilshaders/MaterialKit.h>

#include <string>
#include <string_view>
#include <utility>

namespace sigil::material::kit {

namespace {

Material sampled(Texture source, MaskParams params) {
  Material m(sampledMaskRecipe(), params);
  m.child(kMaskSourceSlot, std::move(source));
  return m;
}

}  // namespace

const std::shared_ptr<const Recipe>& constantMaskRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(
          Recipe::of<MaskParams>("mask.constant")
              .body(Target::SkSL,
                    std::string(shaderSource("MaskFit.sksl"))
                        .append(shaderSource("MaskConstant.sksl")))
              .body(Target::Slang,
                    std::string(shaderSource("MaskFit.slang"))
                        .append(shaderSource("MaskConstant.slang"))));
  return recipe;
}

const std::shared_ptr<const Recipe>& sampledMaskRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(
          Recipe::of<MaskParams>("mask.sampled")
              .child(std::string(kMaskSourceSlot))
              .body(Target::SkSL, std::string(shaderSource("MaskFit.sksl"))
                                      .append(shaderSource("MaskSampled.sksl")))
              .body(Target::Slang,
                    std::string(shaderSource("MaskFit.slang"))
                        .append(shaderSource("MaskSampled.slang"))));
  return recipe;
}

Material maskConstant(float value) {
  MaskParams params;
  params.value = value;
  return Material(constantMaskRecipe(), params);
}

Material maskMap(Texture map, int channel) {
  MaskParams params;
  params.channel = (float)channel;
  params.reading = (float)MaskReading::Channel;
  return sampled(std::move(map), params);
}

Material maskVertexColor(Texture colors, int channel) {
  return maskMap(std::move(colors), channel);
}

Material maskSlope(Texture normals, glm::vec3 up, float low, float high) {
  MaskParams params;
  params.reading = (float)MaskReading::Slope;
  params.axis = {up.x, up.y, up.z, 0};
  params.low = low;
  params.high = high;
  return sampled(std::move(normals), params);
}

Material maskHeight(Texture positions, float low, float high, glm::vec3 axis) {
  MaskParams params;
  params.reading = (float)MaskReading::Height;
  params.axis = {axis.x, axis.y, axis.z, 0};
  params.low = low;
  params.high = high;
  return sampled(std::move(positions), params);
}

namespace {

/** Whether @p material is one of the two shapes a mask has, and a report
 *  naming the rule when it is not.
 *
 *  Reshaping a material that is not a mask writes nothing, and a caller
 *  who is not told keeps a stack whose coverage silently reads as the
 *  material's own colour. The report says what a mask is so the fix is
 *  the next thing read, rather than a field name that means nothing on
 *  its own. */
bool isMask(const Material& material, const char* verb) {
  const Schema& params = material.recipe().params();
  if (params.find("low") && params.find("high") && params.find("inverted"))
    return true;
  reportOnce("kit::mask:" + std::string(verb) + ":" + material.recipe().name(),
             std::string(verb) + " reshapes a MASK, and recipe \"" +
                 material.recipe().name() +
                 "\" is not one; nothing was changed. A mask comes from "
                 "maskConstant, maskMap, maskVertexColor, maskSlope or "
                 "maskHeight. A mask says WHERE something applies; a "
                 "material that paints says WHAT.");
  return false;
}

}  // namespace

Material fit(Material mask, float low, float high) {
  if (!isMask(mask, "fit")) return mask;
  mask.set("low", low);
  mask.set("high", high);
  return mask;
}

Material invert(Material mask) {
  if (!isMask(mask, "invert")) return mask;
  mask.set("inverted", mask.get<float>("inverted") > 0.5f ? 0.0f : 1.0f);
  return mask;
}

}  // namespace sigil::material::kit
