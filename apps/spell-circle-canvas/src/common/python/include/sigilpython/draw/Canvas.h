#pragma once

/** @file
 * The canvas seam: the Skia canvas a binding draws on where the pen's
 * verbs are not what it needs, lent to Python for exactly as long as
 * whatever opened that canvas keeps it open.
 */

#include <pybind11/pybind11.h>

#include <memory>

class SkCanvas;

namespace sigil::python {

class BorrowedPen;

/** What a borrowed canvas asks for the canvas itself. A pen's frame, an
 *  owned surface and a pixel buffer each hold one open for a different
 *  span, so the source is asked again at every verb instead of a pointer
 *  being kept: an implementation throws from `canvas` the moment its own
 *  frame, thread or lifetime says the loan is over. */
class CanvasSource {
 public:
  virtual ~CanvasSource();
  CanvasSource(const CanvasSource&) = delete;
  CanvasSource& operator=(const CanvasSource&) = delete;

  /** The canvas to draw on. Throws once whatever opened it has closed,
   *  and when a thread it was not opened on asks. */
  virtual SkCanvas& canvas() const = 0;

 protected:
  CanvasSource() = default;
};

/** The canvas Python draws on, registered as `draw.Canvas` under
 *  pybind11's own holder, so another file adds verbs to it with
 *  `extend<BorrowedCanvas>(module, "draw.Canvas")` and names no holder.
 *  Retaining one extends nothing: every verb asks the source again. */
class BorrowedCanvas {
 public:
  /** Lends whatever @p source holds open. */
  explicit BorrowedCanvas(std::shared_ptr<CanvasSource> source);
  /** The canvas itself. Throws once the loan has ended. */
  SkCanvas& get() const;
  /** Ends the loan, leaving this wrapper inert. */
  void invalidate();

 private:
  std::shared_ptr<CanvasSource> m_source;
};

/** A source that asks @p pen for its canvas at every verb, so the pen's
 *  own drawing thread and callback checks decide when the loan is over
 *  and a pen between frames has no canvas to lend. */
std::shared_ptr<CanvasSource> penCanvasSource(std::shared_ptr<BorrowedPen> pen);

/** A bound `draw.Canvas` over @p source, for a binding registered ahead
 *  of the seam, whose own signature therefore cannot name the class. The
 *  interpreter lock must be held. */
pybind11::object borrowedCanvas(std::shared_ptr<CanvasSource> source);

/** The canvas inside @p value, which must be a bound `draw.Canvas`.
 *  Throws when it is something else, and when its loan has ended. */
SkCanvas& canvas(pybind11::handle value);

/** Ends the loan @p value holds, which must be a bound `draw.Canvas`, so
 *  a surface or buffer that is closing leaves behind no canvas Python
 *  can still draw through. */
void invalidateCanvas(pybind11::handle value);

}  // namespace sigil::python
