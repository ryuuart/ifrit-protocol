#pragma once

#include <include/core/SkRect.h>
#include <sigilmaterial/core/Material.h>

namespace sigil::material::kit {

/** A softly curved colour CRT with optical bloom, RGB spread, raster
 * lines, vignette and fine grain — `field::crt`, the composition of the
 * beam, the light and the glass, at one restrained set of numbers.
 * Bounds use the content's coordinates. The returned material leaves two
 * slots open: `content`, the picture or layer, and `bloom`, which a
 * layer effect fills from that same layer blurred and a fill must be
 * given itself whatever the bloom's strength. Explicit seconds animate
 * grain reproducibly; zero holds a still. A surface that wants one of
 * the three on its own reaches for it in `<sigilmaterial/field/Crt.h>`. */
Material crt(const SkRect& bounds, float seconds = 0);

}  // namespace sigil::material::kit
