#pragma once

/** @file
 * @ingroup material-texture
 *
 * ONE CHANNEL AS A GREY VALUE: what a texture that packs several values
 * into one image — a mask in red, a noise in green, a gradient in blue —
 * hands each place that reads one of them.
 */

#include <sigilmaterial/core/Material.h>

namespace sigil::material {

/** The channel @p which of @p source as an opaque grey: red, green, blue,
 *  alpha, or the luminance of the three. It stands wherever a material is
 *  taken — a surface's roughness, a layer's mask, a base — and reads a
 *  data texture's channel exactly as stored. */
Material channel(Material source, ColorChannel which);

}  // namespace sigil::material
