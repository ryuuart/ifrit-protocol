/** @file
 * One channel as a grey value: one recipe over a source, in each language
 * a renderer speaks.
 */

#include "sigilmaterial/texture/Channel.h"

#include <sigilmaterial/advanced/Recipe.h>
#include <sigilshaders/MaterialTexture.h>

#include <memory>
#include <string>
#include <utility>

namespace sigil::material {

namespace {

/** `which` is the channel by its enumerator: 0 alpha, 1 luminance, 2 red,
 *  3 green, 4 blue. */
struct ChannelParameters {
  float which = 0;
};

const std::shared_ptr<const Recipe>& channelRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<ChannelParameters>("material.channel")
          .slot("source")
          .body(Target::SkSL,
                std::string(texture::shaderSource("Channel.sksl")))
          .body(Target::Slang,
                std::string(texture::shaderSource("Channel.slang"))));
  return recipe;
}

}  // namespace

Material channel(Material source, ColorChannel which) {
  Material material(channelRecipe(), ChannelParameters{(float)which});
  material.slot("source", std::move(source));
  return material;
}

}  // namespace sigil::material
