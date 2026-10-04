#pragma once

/** @file
 * How the window's pane looks at a sketch's canvas: the magnification
 * and the offset a reader has chosen, the one function that draws a
 * frame of the pane through them, the scale a frame is drawn at while a
 * zoom is still moving, and which pointer events the sketch hears.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/core/Placement.h>

#include <chrono>
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

/** The point of the canvas under @p panePoint when the canvas stands at
 *  @p placement: the inverse of the placement a frame is drawn through. */
[[nodiscard]] SkPoint canvasPointAt(const sigil::sketch::Placement& placement,
                                    SkPoint panePoint);

/** The dark the pane shows behind the canvas: around it, and through
 *  any part of it a sketch left without coverage. */
inline constexpr SkColor kPaneMatte = SkColorSetRGB(0x0b, 0x0a, 0x14);

/** WHERE A FRAME DRAWN AT A HELD SCALE IS DRAWN FIRST, before it is
 *  magnified onto the pane. Kept from frame to frame so a gesture does
 *  not ask the device for a new surface at every step of it; it grows
 *  when a frame needs more of it and is otherwise reused. */
struct PaneLayer {
  sk_sp<SkSurface> surface;
};

/** DRAWS ONE FRAME OF THE PANE into @p target, whose drawable extent is
 *  @p pane. Outside the canvas the pane is left without coverage, so
 *  whatever the host draws behind the pane shows there. Inside it,
 *  @p paint is called once with a canvas moved into canvas units and
 *  clipped to the part of the canvas the pane shows — the pane's clip is
 *  what lets a magnified frame cost what the pane costs, because
 *  everything outside it is rejected before it is drawn — and whatever
 *  @p paint leaves without coverage is filled with the matte after it, so
 *  a sketch grounded in nothing reads against the window's own dark.
 *
 *  @p drawnScale is the scale, in pane units per canvas unit, the sketch
 *  is drawn at. Zero, or the view's own scale, draws it straight onto
 *  @p target under @p view. Any other scale draws the visible part of the
 *  canvas at that scale into @p layer and magnifies the result onto the
 *  pane, so a sketch whose work is pinned to the scale it is drawn at —
 *  a buffer formed at it, a recording traced at it — keeps that work
 *  while the view's scale runs on. A layer larger than twice the pane
 *  either way, or one the device will not make, draws straight instead. */
void drawPane(SkCanvas& target, SkSize pane, SkSize canvas,
              const CanvasView& view,
              const std::function<void(SkCanvas&)>& paint,
              float drawnScale = 0.0f, PaneLayer* layer = nullptr);

/** THE SCALE A FRAME IS DRAWN AT WHILE THE VIEW'S IS MOVING. A zoom
 *  gesture changes the view's scale at every step, and a sketch drawn at
 *  each of those scales would redo, at each step, whatever it keeps at
 *  the scale it is drawn at. So the scale a frame is drawn at is held
 *  while the view's moves, and the held frame is magnified to the view:
 *  it follows the view again once the view has stood still for
 *  `kSettle`, and at once whenever the view has run more than twice or
 *  less than half the held scale, so a held frame is never magnified or
 *  reduced further than that. Every scale is in pane units per canvas
 *  unit. */
class ZoomHold {
 public:
  using Clock = std::chrono::steady_clock;
  /** How long the view's scale must stand still before a frame is drawn
   *  at it: longer than the gap between two steps of one wheel spin or
   *  pinch, short enough that a reader who has stopped sees the sharp
   *  frame at once. */
  static constexpr std::chrono::milliseconds kSettle{150};

  /** The scale to draw the frame shown at @p now at, when the view's
   *  scale is @p viewScale. */
  [[nodiscard]] float drawnScale(float viewScale, Clock::time_point now);
  /** Lets go of the held scale, so the next frame is drawn at the
   *  view's: what a new sketch starts from. */
  void release() { *this = {}; }

 private:
  float m_drawn = 0.0f;
  float m_seen = 0.0f;
  Clock::time_point m_movedAt{};
};

/** WHICH POINTER EVENTS THE SKETCH HEARS when the item reporting them is
 *  larger than its canvas. A pointer over the canvas with no button down
 *  is heard; one off it is not. A press begun on the canvas is heard
 *  wherever it is dragged, up to and including its release; a press
 *  begun off the canvas is not the sketch's, however far over the canvas
 *  it is dragged. */
class PointerGate {
 public:
  /** Whether a pointer event reaches the sketch: @p pressed is whether
   *  the button is down in it, @p onCanvas whether it stands over the
   *  canvas. Every event is passed, heard or not, because a press is
   *  told from a drag by the one before it. */
  [[nodiscard]] bool admit(bool pressed, bool onCanvas);

 private:
  /** Whether the button was down in the last event. */
  bool m_buttonDown = false;
  /** Whether the press now down began on the canvas. */
  bool m_pressOwned = false;
};
