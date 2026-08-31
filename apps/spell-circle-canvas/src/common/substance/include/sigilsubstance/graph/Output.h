#pragma once

/** @file
 * A graph output, described: its identifier and label, the material
 * channel it feeds, and whether it is an image and encoded colour.
 */

#include <string>

namespace sigil::substance {

/** One graph output, described. `usage` is the canonical channel name
 *  the graph declared for it ("baseColor", "normal", ...; "" when the
 *  output has no channel), which is what a material builder keys on. */
struct Output {
  std::string identifier;
  std::string label;
  std::string usage;
  bool image = true;  ///< false for numerical outputs
  bool srgb = false;  ///< the graph declares this output as encoded colour
};

}  // namespace sigil::substance
