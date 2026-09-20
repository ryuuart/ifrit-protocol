#pragma once

#include <include/core/SkRect.h>
#include <sigilmaterial/core/Material.h>

namespace sigil::material::kit {

/** A softly curved colour CRT with optical bloom, RGB spread, raster
 * lines, vignette and fine grain. Bounds use the content's coordinates.
 * The returned material leaves two slots open: `content`, the picture or
 * layer, and `bloom`, which a layer effect fills from that same layer
 * blurred and a fill must be given itself. Explicit seconds animate
 * grain reproducibly; zero holds a still. */
Material crt(const SkRect& bounds, float seconds = 0);

}  // namespace sigil::material::kit
