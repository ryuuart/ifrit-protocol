#pragma once

/** @file
 * OpenColorIO view transforms as materials. In a build that found no
 * OpenColorIO the feature still links: `available()` is false and every
 * factory answers the empty LUT material a bad config would.
 * SIGILMATERIAL_ENABLE_OCIO says which build this is.
 *
 * OCIO's own GPU codegen emits GLSL, HLSL, MSL and OSL and never SkSL, so
 * it cannot shade here directly. Each factory builds a CPU processor for
 * the requested transform, bakes it into a 3D LUT once, holds the LUT as
 * a texture, and applies it through a trilinear recipe whose one open
 * slot, `content`, is the layer being transformed: a renderer binds that
 * slot to its output and gets one sample per pixel and nothing of OCIO
 * proper per frame.
 *
 * Colour contract: what the content carries is treated as the
 * transform's INPUT space. For a display/view transform, author in the
 * config's scene-linear role and the view maps linear to display.
 *
 * LUTs bake to F16 because F32 textures are not linearly filterable on
 * Apple GPUs — a trilinear sampler over an F32 LUT would fall back to
 * point sampling and band.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <memory>
#include <string_view>

namespace sigil::material::ocio {

/** True when OCIO support was compiled in AND the runtime can create its
 *  built-in raw config. */
bool available();

/** The LUT recipe's ABI. */
struct LutParams {
  float lutSize;
};

/** The trilinear 3D-LUT recipe: children `content` (the layer, left to
 *  the renderer) and `lut` (the baked slices). Unpremultiplies, maps,
 *  repremultiplies, so straight colours go through the transform. */
const std::shared_ptr<const Recipe>& lutRecipe();

/** An OCIO display/view, baked to a LUT material. @p config is a
 *  filesystem path to a .ocio/.ocioz config, or an "ocio://" URI for the
 *  ASWF built-in configs ("ocio://default", "ocio://studio-config-latest",
 *  "ocio://cg-config-latest"). @p display / @p view name a pair from that
 *  config. Input is the config's scene_linear role. An empty material,
 *  with the error reported, on failure — a bad config name must not take
 *  the canvas down. */
Material viewTransform(std::string_view config, std::string_view display,
                       std::string_view view, int lutSize = 33);

/** A colour-space conversion @p src to @p dst from the same config
 *  sources. Same failure contract. */
Material convert(std::string_view config, std::string_view src,
                 std::string_view dst, int lutSize = 33);

/** A plain exponent (gamma) transform baked through OCIO's raw config —
 *  needs no config file. A quick grade, and the plumbing test. */
Material exponent(float gamma, int lutSize = 33);

}  // namespace sigil::material::ocio
