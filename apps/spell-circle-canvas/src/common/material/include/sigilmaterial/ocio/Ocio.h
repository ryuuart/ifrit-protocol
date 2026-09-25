#pragma once

/** @file
 * @ingroup material-ocio
 *
 * OpenColorIO view transforms as materials: a transform baked once on
 * the CPU, held as a texture, and applied through a recipe whose one
 * open slot, `content`, is the layer being transformed. A transform
 * whose channels are INDEPENDENT bakes to a row, one that mixes them to
 * a 3D LUT, and independence is established from the processor rather
 * than assumed. What the content carries is the transform's INPUT
 * space. Without OpenColorIO the feature still links, empty.
 */

#include <sigilmaterial/core/Material.h>

#include <string_view>

/** OpenColorIO's view transforms as materials: a transform baked to a
 *  3D LUT or a 1D curve and sampled over a layer.
 *  Reach for this when a colour has to arrive in the space a pipeline
 *  agreed on rather than in the one it was authored in. The feature
 *  links whether or not the build found OpenColorIO: without it
 *  `available()` is false and every factory answers the empty LUT
 *  material, so a caller needs no build-time branch. */
namespace sigil::material::ocio {

/** True when OCIO support was compiled in AND the runtime can create its
 *  built-in raw config. */
bool available();

/** An OCIO display/view, baked to a material. @p config is a
 *  filesystem path to a .ocio/.ocioz config, or an "ocio://" URI for the
 *  ASWF built-in configs ("ocio://default", "ocio://studio-config-latest",
 *  "ocio://cg-config-latest"). @p display / @p view name a pair from that
 *  config. Input is the config's scene_linear role. @p lutSize is the
 *  samples per axis of the 3D LUT, and is unused where the transform
 *  bakes to a response row instead. An empty material, with the error
 *  reported, on failure — a bad config name must not take the canvas
 *  down. */
Material viewTransform(std::string_view config, std::string_view display,
                       std::string_view view, int lutSize = 33);

/** A colour-space conversion @p source to @p destination from the same config
 *  sources. Same failure contract. */
Material convert(std::string_view config, std::string_view source,
                 std::string_view destination, int lutSize = 33);

/** A plain exponent (gamma) transform baked through OCIO's raw config —
 *  needs no config file. A quick grade, and the plumbing test. Its
 *  channels are independent, so it bakes to a response row. */
Material exponent(float gamma, int lutSize = 33);

}  // namespace sigil::material::ocio
