/** @file
 * The flipbook: one recipe over a sheet and an optional motion sheet, in
 * each language a renderer speaks, and the builder that states the grid
 * and binds what drives it.
 */

#include "sigilmaterial/texture/Flipbook.h"

#include <sigilmaterial/advanced/Recipe.h>
#include <sigilshaders/MaterialTexture.h>

#include <algorithm>
#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <utility>

namespace sigil::material {

namespace {

/** The flipbook's ABI. `frame` and `progress` below zero are unbound, and
 *  the clock drives; `straight` is one when the sheet is data. */
struct FlipbookParameters {
  float columns = 8;
  float rows = 8;
  float frames = 64;
  float framesPerSecond = 30;
  float loop = 1;
  float blendFrames = 0;
  float startFrame = 0;
  float frame = -1;
  float progress = -1;
  float motionStrength = 0;
  float straight = 0;
  glm::vec2 sheetSize{1, 1};
  glm::vec2 motionSize{1, 1};
};

const std::shared_ptr<const Recipe>& flipbookRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<FlipbookParameters>("material.flipbook")
          .slot("sheet")
          .slot("motionVectors")
          .frame(FrameInput::Time)
          .frame(FrameInput::Resolution)
          .body(Target::SkSL,
                std::string(texture::shaderSource("Flipbook.sksl")))
          .body(Target::Slang,
                std::string(texture::shaderSource("Flipbook.slang"))));
  return recipe;
}

glm::vec2 sizeOf(const Texture& texture) {
  const glm::ivec2 size = texture.size();
  return {(float)std::max(size.x, 1), (float)std::max(size.y, 1)};
}

}  // namespace

Material flipbook(Texture sheet, FlipbookOptions options) {
  const int columns = std::max(options.columns, 1);
  const int rows = std::max(options.rows, 1);
  const int cells = columns * rows;
  FlipbookParameters parameters;
  parameters.columns = (float)columns;
  parameters.rows = (float)rows;
  parameters.frames =
      (float)(options.frames > 0 ? std::min(options.frames, cells) : cells);
  parameters.framesPerSecond = options.framesPerSecond;
  parameters.loop = options.loop ? 1.0f : 0.0f;
  parameters.blendFrames = options.blendFrames ? 1.0f : 0.0f;
  parameters.startFrame = options.startFrame;
  parameters.straight =
      sheet.colorSpace() == media::ColorSpace::Data ? 1.0f : 0.0f;
  // A smaller copy of a sheet averages neighbouring cells into one texel,
  // which shows as the next frame bleeding in at the edge of this one.
  if (!sheet.mipmaps()) sheet.mipmaps(false);
  parameters.sheetSize = sizeOf(sheet);
  if (options.motionVectors && options.motionVectors->valid()) {
    parameters.motionStrength = options.motionStrength;
    parameters.motionSize = sizeOf(*options.motionVectors);
  }

  Material material(flipbookRecipe(), parameters);
  material.slot("sheet", std::move(sheet));
  if (options.motionVectors && options.motionVectors->valid()) {
    Texture motion = std::move(*options.motionVectors);
    motion.colorSpace(media::ColorSpace::Data);
    if (!motion.mipmaps()) motion.mipmaps(false);
    material.slot("motionVectors", std::move(motion));
  }
  if (options.progress) material.bind("progress", std::move(*options.progress));
  if (options.frame) material.bind("frame", std::move(*options.frame));
  return material;
}

}  // namespace sigil::material
