#pragma once

/** @file
 * SigilCompose KIT — the small rasters a sink stamps.
 *
 * A point sink draws one image per point. The image is not the sink's
 * business and not the scene's either: it is a stamp, made once, uploaded
 * once, and stamped thousands of times. The generators here bake those.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSurface.h>

namespace sigil::compose::kit {

/** A WHITE ROUND DOT on transparency, @p px square — the stamp a point
 *  cloud is drawn with when each point is a soft blob rather than a
 *  square.
 *
 *  White because a sink tints per point: a coloured stamp multiplies into
 *  every tint and there is no way to get back to the colour that was
 *  asked for.
 *
 *  @p margin is the transparent ring left around the circle, and it is
 *  load-bearing rather than tidiness: the disc is antialiased, and a
 *  circle drawn out to the image edge has its softened edge cut off by
 *  the sampler, which reads as a faint square around every point once the
 *  stamp is scaled up.
 *
 *  Bake it once and hold the result — this allocates a surface and
 *  rasterises. */
inline sk_sp<SkImage> dotSprite(int px = 32, float margin = 2.0f) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(px, px));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorWHITE);
  const float centre = (float)px * 0.5f;
  canvas->drawCircle(centre, centre, centre - margin, paint);
  return surface->makeImageSnapshot();
}

}  // namespace sigil::compose::kit
