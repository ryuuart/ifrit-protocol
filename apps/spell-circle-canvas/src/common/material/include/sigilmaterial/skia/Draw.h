#pragma once

/** @file
 * The one-call draw: a path filled with a material on a Skia canvas.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/core/Material.h>

namespace sigil::material::skia {

/** Clips to @p path and paints @p material across it, anti-aliased, at
 *  @p frame. Draws nothing when the material has no Skia program. */
void fill(SkCanvas& canvas, const SkPath& path, const Material& material,
          const FrameData& frame = {});

}  // namespace sigil::material::skia
