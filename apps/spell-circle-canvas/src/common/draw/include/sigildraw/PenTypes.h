#pragma once

/** @file
 * WHAT A PEN TAKES AND WHAT IT DRAWS FOR SOMEONE ELSE, apart from the pen
 * itself: the frame a host supplies, what `clip` does with a shape, and
 * the two concepts a caller's own value satisfies to be drawn as a shape
 * or painted as a retained guest.
 *
 * They are here so that a host that hands a pen a frame, or a library
 * that declares itself paintable, includes what it needs and not the
 * whole surface.
 */

#include <include/core/SkPath.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigildraw/Retained.h>

#include <concepts>
#include <span>
#include <string>
#include <string_view>

namespace sigil::weave {
class FontContext;
}

namespace sigil::draw {

class Pen;

/** A GUEST: something another library keeps between frames and paints
 *  inside a box the pen names — a retained element tree, a shaped page.
 *  The guest's own library says how, by declaring
 *  `paintRetained(Pen&, const Guest&, const SkRect&, Slot)` in the
 *  guest's namespace, where argument lookup finds it; this library
 *  names no guest. */
template <class G>
concept Retainable =
    requires(Pen& pen, const G& guest, const SkRect& box, Slot slot) {
      paintRetained(pen, guest, box, slot);
    };

/** A SILHOUETTE: any value that answers a path over a size — the
 *  geometry kit's generators, or one of your own. */
template <class S>
concept Silhouette = requires(const S& s, SkSize size) {
  { s.path(size) } -> std::convertible_to<SkPath>;
};

/** WHAT `clip` DOES WITH THE SHAPE IT IS GIVEN. */
struct ClipOptions {
  /** Cuts the shape OUT of what may be drawn, instead of keeping only
   *  what falls inside it — p5's `{ invert: true }`. */
  bool invert = false;
};

/** WHAT A FRAME SUPPLIES THE PEN, none of it the sketch's to set: the
 *  canvas in its own pixels, the clock of whoever is stepping, the frame
 *  count, the fonts text is shaped with, and the pointer and keys a host
 *  fed. The clock is the caller's: a pen never reads the wall. */
struct Frame {
  float width = 0;
  float height = 0;
  /** Seconds since the sketch began, on the caller's clock. */
  double seconds = 0;
  /** The step this frame took, in seconds. */
  double deltaSeconds = 0;
  /** p5's `frameCount`: 1 on the first draw. */
  int frameCount = 0;
  weave::FontContext* fonts = nullptr;
  float mouseX = 0;
  float mouseY = 0;
  bool mouseIsPressed = false;
  bool keyIsPressed = false;
  /** The key most recently pressed, spelled as a keyboard spells it:
   *  "a", "ArrowLeft", "Enter". */
  std::string_view key;
  int keyCode = 0;
  /** Every key code held down this frame. */
  std::span<const int> keysDown;
};

}  // namespace sigil::draw
