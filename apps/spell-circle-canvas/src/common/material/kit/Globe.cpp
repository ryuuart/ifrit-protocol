/** @file
 * The globe recipe: one arithmetic in both languages a renderer speaks,
 * reading the node's resolution because the disc is inscribed in the
 * node.
 */

#include "sigilmaterial/kit/Globe.h"

#include <sigilmaterial/core/Terms.h>
#include <sigilshaders/MaterialKit.h>

#include <string>

namespace sigil::material::kit {

namespace {

/** The globe's whole reading, as source in @p target. One text, written
 *  in Slang and crossed by the core, so the ball a device shades and the
 *  ball a raster surface paints are the same ball; each target's entry
 *  is appended to it and is the one line that spells the return in that
 *  target's own types. */
const std::string& globePrelude(Target target) {
  static const std::string kSlang =
      std::string(shaderSource("GlobePrelude.slang"));
  static const std::string kSkSL = skSLFromSlang(kSlang);
  return target == Target::Slang ? kSlang : kSkSL;
}

}  // namespace

const std::shared_ptr<const Recipe>& globeRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(
          Recipe::of<GlobeParams>("kit.globe")
              .body(Target::SkSL, std::string(globePrelude(Target::SkSL))
                                      .append(shaderSource("Globe.sksl")))
              .body(Target::Slang, std::string(globePrelude(Target::Slang))
                                       .append(shaderSource("Globe.slang")))
              .frame(FrameInput::Resolution));
  return recipe;
}

Material globe(const GlobeParams& params) {
  return Material(globeRecipe(), params);
}

}  // namespace sigil::material::kit
