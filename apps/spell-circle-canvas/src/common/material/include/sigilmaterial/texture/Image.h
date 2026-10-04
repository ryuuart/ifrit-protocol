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

class Texture;

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

/** @p texture as a material, preserving its placement, region, tiling
 *  and sampling. The same value can fill a shape or a surface channel. */
Material image(Texture texture);

/** @p pixels, a latitude-longitude (equirectangular) picture, as the
 *  environment a lit surface reflects — read across the picture's own
 *  size unless `options.size` says otherwise, and repeating round the
 *  horizon. */
Environment environment(media::PixelSource pixels,
                        EnvironmentOptions options = {});

}  // namespace sigil::material
