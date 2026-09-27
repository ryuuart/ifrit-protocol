/** @file
 * The image base: one recipe with a single slot a texture fills, in each
 * language a renderer speaks, so an image stacks and fills a surface
 * channel as any program does.
 */

#include "sigilmaterial/texture/Image.h"

#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/texture/Texture.h>

#include <memory>
#include <utility>

namespace sigil::material {

namespace {

struct ImageParameters {};

const std::shared_ptr<const Recipe>& imageRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<ImageParameters>("material.image")
          .slot("image")
          .body(Target::SkSL, "half4 main(float2 xy) { return image.eval(xy); }\n")
          .body(Target::Slang,
                "float4 surface(float2 uv) { return image.Sample(uv); }\n"));
  return recipe;
}

}  // namespace

Material image(media::PixelSource pixels, ImageOptions options) {
  Texture texture(std::move(pixels));
  texture.tile(options.repeat, options.repeatY.value_or(options.repeat));
  Material material(imageRecipe(), ImageParameters{});
  material.slot("image", std::move(texture));
  return material;
}

Environment environment(media::PixelSource pixels, EnvironmentOptions options) {
  if (options.size.x <= 0 || options.size.y <= 0) {
    options.size = glm::vec2(pixels.size());
  }
  return environment(image(std::move(pixels), {.repeat = Repeat::Repeat,
                                               .repeatY = Repeat::Pad}),
                     std::move(options));
}

}  // namespace sigil::material
