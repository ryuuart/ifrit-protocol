#pragma once
// The type the kit's component cases are set in: the aliased monospace
// style a pixel bake is read back cell by cell from, and the height of one
// line at a size, which is what a caption, a panel head and a row are
// measured against.

#include <sigilweave/style/Type.h>

#include "Host.h"

namespace {

/** An aliased monospace style at @p size. Aliased because a pixel bake
 *  thresholds coverage into a one-bit mask, and monospace because a case
 *  that reads cells back off one advance needs every cell to have it. */
sigil::weave::TextStyle pixelStyle(float size) {
  return sigil::weave::textStyle(
      {.face = sigil::weave::ports::pickTypeface(
           {"Menlo", "DejaVu Sans Mono", "Courier New"}),
       .size = size,
       .color = SkColor4f{1, 1, 1, 1},
       .aliased = true});
}

/** One line of type at @p size, as the layout will size it. */
float lineHeight(float size) {
  return intrinsicSize(
             box().children({text(u8"Hg", weave::textStyle({.size = size}))}),
             fonts())
      .height();
}

}  // namespace
