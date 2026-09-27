/** @file
 * The raster bake a painted tile program runs.
 */

#include "sigilmaterial/skia/Painted.h"

#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilmedia/advanced/Skia.h>

#include <utility>

namespace sigil::material::skia {

std::function<media::PixelSource(glm::ivec2 size, uint32_t seed)> painted(
    Painter painter) {
  if (!painter) return {};
  return [painter = std::move(painter)](glm::ivec2 size,
                                        uint32_t seed) -> media::PixelSource {
    if (size.x <= 0 || size.y <= 0) return {};
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size.x, size.y));
    if (!surface) return {};
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    painter(*canvas, SkSize::Make((float)size.x, (float)size.y), seed);
    return media::PixelSource(surface->makeImageSnapshot());
  };
}

}  // namespace sigil::material::skia
