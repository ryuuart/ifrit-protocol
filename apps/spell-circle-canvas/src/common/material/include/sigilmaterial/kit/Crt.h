#pragma once

#include <include/core/SkRect.h>
#include <sigilmaterial/core/Material.h>

namespace sigil::material::kit {

/** A softly curved colour CRT with optical bloom, RGB spread, raster
 * lines, vignette and fine grain. Bounds use the content's coordinates.
 * The returned material leaves content open for a picture or layer.
 * Explicit seconds animate grain reproducibly; zero holds a still. */
Material crt(const SkRect& bounds, float seconds = 0);

}  // namespace sigil::material::kit
