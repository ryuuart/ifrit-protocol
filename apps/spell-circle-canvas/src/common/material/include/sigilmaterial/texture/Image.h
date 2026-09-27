#pragma once

/** @file
 * @ingroup material-texture
 *
 * THE IMAGE BASE of a material: pixels from anywhere SigilMedia's
 * `PixelSource` reaches — a decoded image, an animated asset, a buffer, a
 * tile, frames arriving from a feed — as a material, so it stacks under
 * and over other materials and fills a surface channel.
 */

#include <sigilmaterial/core/Gradient.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmedia/core/PixelSource.h>

#include <optional>

namespace sigil::material {

/** How an image meets the region it paints. */
struct ImageOptions {
  /** What is painted past the image's edges, horizontally — and
   *  vertically too unless `repeatY` says otherwise. */
  Repeat repeat = Repeat::None;
  std::optional<Repeat> repeatY;
  bool operator==(const ImageOptions&) const = default;
};

/** @p pixels as a material, at their own size from the region's origin. */
Material image(media::PixelSource pixels, ImageOptions options = {});

}  // namespace sigil::material
