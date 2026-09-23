#pragma once

/** @file
 * How the window's pane looks at a sketch's canvas: the magnification
 * and the offset a reader has chosen, and the one function that draws a
 * frame of the pane through them.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <include/core/SkSize.h>
#include <sigilsketch/core/Placement.h>

#include <functional>

class SkCanvas;

/** HOW THE PANE LOOKS AT THE CANVAS: how far the canvas is magnified and
 *  where it has been moved to. The pane is what is on screen and the
 *  canvas is what the sketch declared, so a reader zooming in changes
 *  this and nothing else — the surface a frame is drawn into stays the
 *  size of the pane, and the part of the canvas outside it is clipped
 *  away before it is drawn.
 *
 *  Spelled in whatever units the pane is measured in: device pixels for
 *  the frame, the item's own units for a pointer read back through it. */
struct CanvasView {
  /** Pane units per canvas unit. Zero, the default, fits the whole
   *  canvas into the pane, centred, at the largest scale that shows all
   *  of it. */
  float scale = 0.0f;
  /** Where the canvas's centre stands from the pane's centre. */
  SkPoint offset = {0.0f, 0.0f};

  /** The same view in units @p factor times as fine: the view a pointer
   *  reads in item units, carried into the device pixels its frame is
   *  drawn in. */
  [[nodiscard]] CanvasView scaled(float factor) const {
    return {scale * factor, {offset.x() * factor, offset.y() * factor}};
  }
};

/** WHERE A CANVAS OF @p canvas UNITS LANDS on a pane of @p pane under
 *  @p view: the scale it is drawn at and the point its top-left corner
 *  reaches. The frame is drawn through it and a pointer is read back
 *  through its inverse, so both call this and nothing else. */
[[nodiscard]] sigil::sketch::Placement placeCanvas(SkSize canvas, SkSize pane,
                                                   const CanvasView& view);

/** The dark the pane shows behind the canvas: around it, and through
 *  any part of it a sketch left without coverage. */
inline constexpr SkColor kPaneMatte = SkColorSetRGB(0x0b, 0x0a, 0x14);

/** DRAWS ONE FRAME OF THE PANE into @p target, whose drawable extent is
 *  @p pane. Outside the canvas the pane is left without coverage, so
 *  whatever the host draws behind the pane shows there. Inside it,
 *  @p paint is called once with @p target moved into canvas units under
 *  @p view and clipped to the part of the canvas the pane shows — the
 *  pane's clip is what lets a magnified frame cost what the pane costs,
 *  because everything outside it is rejected before it is drawn — and
 *  whatever @p paint leaves without coverage is filled with the matte
 *  after it, so a sketch grounded in nothing reads against the window's
 *  own dark. */
void drawPane(SkCanvas& target, SkSize pane, SkSize canvas,
              const CanvasView& view,
              const std::function<void(SkCanvas&)>& paint);
