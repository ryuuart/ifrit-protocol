/** @file
 * The one term text, in each language a renderer speaks.
 */

#include "sigilmaterial/core/Terms.h"

#include <sigilshaders/MaterialCore.h>

#include <string>
#include <string_view>

namespace sigil::material {

namespace {

/** THE MODULE, as a device compiler takes it. A Slang renderer loads
 *  this text into its compiler session under the name `Shading` and its
 *  own shaders import it, which is how one text serves both the
 *  renderer's scaffold and every material body compiled beside it. */
std::string slangSource() { return std::string(shaderSource("Shading.slang")); }

}  // namespace

std::string skSLFromSlang(std::string_view slang) {
  std::string out(slang);
  const auto strip = [&out](std::string_view what) {
    for (size_t at = out.find(what); at != std::string::npos;
         at = out.find(what, at))
      out.erase(at, what.size());
  };
  strip("module Shading;");
  strip("public ");

  const auto isWord = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
  };
  const auto rename = [&](std::string_view from, std::string_view to) {
    for (size_t at = out.find(from); at != std::string::npos;
         at = out.find(from, at)) {
      const bool before = at > 0 && isWord(out[at - 1]);
      const size_t after = at + from.size();
      const bool behind = after < out.size() && isWord(out[after]);
      if (before || behind) {
        at = after;
        continue;
      }
      out.replace(at, from.size(), to);
      at += to.size();
    }
  };
  rename("frac", "fract");
  rename("lerp", "mix");
  return out;
}

const std::string& termsSource(Target target) {
  static const std::string kSlang = slangSource();
  static const std::string kSkSL = skSLFromSlang(kSlang);
  switch (target) {
    case Target::Slang:
      return kSlang;
    case Target::SkSL:
      return kSkSL;
  }
  return kSkSL;
}

}  // namespace sigil::material
