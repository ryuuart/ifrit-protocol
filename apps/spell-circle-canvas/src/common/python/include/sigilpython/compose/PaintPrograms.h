#pragma once

/** @file
 * Binding the programs a node paints with: the paint context a program
 * is lent for one call, and the readings that take a native paint
 * program and a native pen program from a Python callable.
 */

#include <pybind11/pybind11.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcore/callable/Callable.h>

#include <memory>
#include <thread>

class SkCanvas;

namespace sigil::draw {
class Pen;
}

namespace sigil::python {

/** One paint call's context, lent to Python for that call. The context
 *  carries pointers into the composer that painted it, valid only while
 *  the call runs, so every reading asks this wrapper first: it refuses
 *  once the call has returned, and on a thread other than the one that
 *  paints. Registered as `compose.PaintContext` under a shared holder. */
class BorrowedPaintContext {
 public:
  /** Lends @p context to the thread this is constructed on. */
  explicit BorrowedPaintContext(const compose::PaintContext& context);
  /** The context itself. Throws once the paint call it was lent to has
   *  returned, and when another thread asks. */
  const compose::PaintContext& get() const;
  /** Ends the loan, leaving this wrapper inert. */
  void invalidate();

 private:
  const std::thread::id m_thread;
  const compose::PaintContext* m_context;
};

/** A paint context lent to Python for as long as this stands. The bound
 *  `compose.PaintContext` refuses every reading once this goes out of
 *  scope, however the call it was made for returned. The interpreter
 *  lock must be held for the whole of its life. */
class PaintContextLoan {
 public:
  /** Lends @p context to the calling thread. */
  explicit PaintContextLoan(const compose::PaintContext& context);
  ~PaintContextLoan();
  PaintContextLoan(const PaintContextLoan&) = delete;
  PaintContextLoan& operator=(const PaintContextLoan&) = delete;

  /** The bound `compose.PaintContext` to hand the callback. */
  const pybind11::object& context() const { return m_object; }

 private:
  std::shared_ptr<BorrowedPaintContext> m_view;
  pybind11::object m_object;
};

/** A native canvas lent to Python for as long as this stands, for a
 *  native call whose callback is handed the canvas itself. The bound
 *  `draw.Canvas` refuses every verb once this goes out of scope, and on a
 *  thread other than the one that paints. The interpreter lock must be
 *  held for the whole of its life. */
class CanvasLoan {
 public:
  /** Lends @p canvas to the calling thread. */
  explicit CanvasLoan(SkCanvas& canvas);
  ~CanvasLoan();
  CanvasLoan(const CanvasLoan&) = delete;
  CanvasLoan& operator=(const CanvasLoan&) = delete;

  /** The bound `draw.Canvas` to hand the callback. */
  const pybind11::object& canvas() const { return m_object; }

 private:
  class Source;
  std::shared_ptr<Source> m_source;
  pybind11::object m_object;
};

/** A paint program read from @p value, a callable naming none, one or
 *  both of the canvas and the paint context, in that order. The callable
 *  is retained against the callback lifetime in force, the canvas and
 *  the context it is handed are lent for the one call, and what it
 *  raises leaves the native paint as a runtime error. Throws when @p
 *  value is not callable, or names more than the program offers. */
compose::PaintProgram paintProgram(pybind11::handle value);

/** A pen program read from @p value, a callable naming none, one or both
 *  of the pen and the paint context, in that order, on the same terms as
 *  `paintProgram`. What it answers is a `compose::PenProgram`, written
 *  out so this header names the pen without including it. */
core::Callable<void(draw::Pen&, const compose::PaintContext&)> penProgram(
    pybind11::handle value);

}  // namespace sigil::python
