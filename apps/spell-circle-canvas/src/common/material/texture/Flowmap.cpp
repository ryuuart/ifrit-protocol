/** @file
 * The flow map: one recipe over a source and a flow texture, in each
 * language a renderer speaks.
 */

#include "sigilmaterial/texture/Flowmap.h"

#include <sigilmaterial/advanced/Recipe.h>
#include <sigilshaders/MaterialTexture.h>

#include <algorithm>
#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <utility>

namespace sigil::material {

namespace {

struct FlowmapParameters {
  float strength = 0.1f;
  float speed = 0.5f;
  glm::vec2 flowSize{1, 1};
};

const std::shared_ptr<const Recipe>& flowmapRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<FlowmapParameters>("material.flowmap")
          .slot("source")
          .slot("flow")
          .frame(FrameInput::Time)
          .frame(FrameInput::Resolution)
          .body(Target::SkSL,
                std::string(texture::shaderSource("Flowmap.sksl")))
          .body(Target::Slang,
                std::string(texture::shaderSource("Flowmap.slang"))));
  return recipe;
}

}  // namespace

Material flowmap(Material source, FlowmapOptions options) {
  Texture flow = std::move(options.flow);
  flow.colorSpace(media::ColorSpace::Data);
  const glm::ivec2 size = flow.size();
  FlowmapParameters parameters{
      .strength = options.strength,
      .speed = options.speed,
      .flowSize = {(float)std::max(size.x, 1), (float)std::max(size.y, 1)}};
  Material material(flowmapRecipe(), parameters);
  material.slot("source", std::move(source));
  material.slot("flow", std::move(flow));
  return material;
}

}  // namespace sigil::material
