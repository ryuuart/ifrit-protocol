#pragma once

/** @file
 * OpenColorIO view transforms as materials. In a build that found no
 * OpenColorIO the feature still links: `available()` is false and every
 * factory answers the empty LUT material a bad config would.
 * SIGILMATERIAL_ENABLE_OCIO says which build this is.
 *
 * OCIO's own GPU codegen emits GLSL, HLSL, MSL and OSL and never SkSL, so
 * it cannot shade here directly. Each factory builds a CPU processor for
 * the requested transform, bakes it once, holds the bake as a texture,
 * and applies it through a recipe whose one open slot, `content`, is the
 * layer being transformed: a renderer binds that slot to its output and
 * gets one sample per pixel and nothing of OCIO proper per frame.
 *
 * WHICH RECIPE depends on the transform. A transform whose channels are
 * INDEPENDENT — an exponent, a gamma, a contrast, a per-channel tone
 * curve — carries no more information than one response curve per
 * channel, and bakes to a single row through `responseRecipe()`; a
 * transform that mixes channels needs the volume and bakes to a 3D LUT
 * through `lutRecipe()`. Independence is established from the CPU
 * processor, not assumed from the transform's type. The row recipe is
 * declared channelwise, so a renderer on an eight-bit surface can drop
 * the program entirely and run a 256-entry per-channel table instead;
 * `lutSize` is therefore meaningless to a transform that bakes to a row.
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

/** The ABI both bake recipes share: the number of samples along one
 *  axis of the bake in the `lut` slot. */
struct LutParams {
  float lutSize;
};

/** The trilinear 3D-LUT recipe: children `content` (the layer, left to
 *  the renderer) and `lut` (the baked slices). Unpremultiplies, maps,
 *  repremultiplies, so straight colours go through the transform. */
const std::shared_ptr<const Recipe>& lutRecipe();

/** The per-channel response recipe, for a transform whose channels are
 *  independent: children `content` (the layer, left to the renderer) and
 *  `lut`, which here is ONE ROW of `lutSize` samples carrying the
 *  responses of red, green and blue in the row's own channels. Three
 *  taps rather than the volume's two, unpremultiplied and
 *  repremultiplied the same way, and DECLARED CHANNELWISE, so a renderer
 *  on an eight-bit surface may run it as a table instead. */
const std::shared_ptr<const Recipe>& responseRecipe();

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

/** A colour-space conversion @p src to @p dst from the same config
 *  sources. Same failure contract. */
Material convert(std::string_view config, std::string_view src,
                 std::string_view dst, int lutSize = 33);

/** A plain exponent (gamma) transform baked through OCIO's raw config —
 *  needs no config file. A quick grade, and the plumbing test. Its
 *  channels are independent, so it bakes to a response row. */
Material exponent(float gamma, int lutSize = 33);

}  // namespace sigil::material::ocio
