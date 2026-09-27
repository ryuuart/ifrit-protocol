#pragma once

/** @file
 * The two recipes a baked transform is applied through, private to the
 * ocio feature: every factory in it answers a material over one of them,
 * so a consumer never names either.
 */

#include <sigilmaterial/advanced/Recipe.h>

#include <memory>

namespace sigil::material::ocio {

/** The ABI both bake recipes share: the number of samples along one
 *  axis of the bake in the `lut` slot. */
struct LutParameters {
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

}  // namespace sigil::material::ocio
