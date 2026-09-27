#pragma once

/** @file
 * @ingroup material-skia
 *
 * A picture painted into a Skia canvas, as the bake a pattern tile runs:
 * the painter draws into a raster canvas of the tile's whole-pixel size,
 * and what it drew is the picture the tile repeats.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSize.h>
#include <sigilmedia/core/PixelSource.h>

#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>

namespace sigil::material::skia {

/** Draws ONE tile into [0,0 .. size); `seed` is the tile's current seed —
 *  same seed, same tile. */
using Painter = std::function<void(SkCanvas&, SkSize size, uint32_t seed)>;

/** A tile program over @p painter — what `pattern::Tile::of` and
 *  `Tile::program` take: a transparent raster canvas of the size asked
 *  for, painted, and kept as a picture. An empty painter bakes nothing. */
std::function<media::PixelSource(glm::ivec2 size, uint32_t seed)> painted(
    Painter painter);

}  // namespace sigil::material::skia
