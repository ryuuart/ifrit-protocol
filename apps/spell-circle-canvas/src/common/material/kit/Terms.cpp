/** @file
 * The one term text, in each language a renderer speaks.
 */

#include "sigilmaterial/kit/Terms.h"

#include "ShaderSources.h"

namespace sigil::material::kit {

namespace {

/** THE MODULE, as a device compiler takes it. A Slang renderer loads
 *  this text into its compiler session under the name `Shading` and its
 *  own shaders import it, which is how one text serves both the
 *  renderer's scaffold and every material body compiled beside it. */
std::string slangSource() { return shaderSource("Shading.slang"); }

/** THE SAME TEXT AS A 2D PAINT TAKES IT. SkSL has no modules and no
 *  export qualifier; everything else in the term source is spelled the
 *  same in both languages, which is the constraint the source is written
 *  under and the whole reason two renderers can share one arithmetic. */
std::string skSLSource() {
  std::string out = slangSource();
  const auto strip = [&out](std::string_view what) {
    for (size_t at = out.find(what); at != std::string::npos;
         at = out.find(what, at))
      out.erase(at, what.size());
  };
  strip("module Shading;");
  strip("public ");
  return out;
}

}  // namespace

const std::string& termsSource(Target target) {
  static const std::string kSlang = slangSource();
  static const std::string kSkSL = skSLSource();
  switch (target) {
    case Target::Slang:
      return kSlang;
    case Target::SkSL:
      return kSkSL;
  }
  return kSkSL;
}

}  // namespace sigil::material::kit
