#pragma once
/** @file
 * The pen a mark is drawn with where the painter holds a canvas: begun on
 * that canvas over the node the paint context describes, and ended when
 * the scope closes, so the canvas comes back as the pen found it.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Paint.h>
#include <sigildraw/Pen.h>

namespace sigil::compose::detail {

/** The pen's frame for the node @p context describes: its box, the clock,
 *  the fonts, and the pointer and keys the host fed the composer. */
inline draw::Frame penFrame(const PaintContext& context) {
  draw::Frame frame;
  frame.width = context.size.x;
  frame.height = context.size.y;
  frame.seconds = context.elapsedSeconds;
  frame.fonts = context.fonts;
  frame.mouseX = context.pointer.at.x;
  frame.mouseY = context.pointer.at.y;
  frame.mouseIsPressed = context.pointer.pressed;
  if (context.keys) {
    frame.keyIsPressed = context.keys->pressed;
    frame.key = context.keys->key;
    frame.keyCode = context.keys->keyCode;
    frame.keysDown = context.keys->down;
  }
  return frame;
}

/** A pen begun on @p canvas for one mark, ended with the scope. */
class PenOnCanvas {
 public:
  PenOnCanvas(SkCanvas& canvas, const PaintContext& context) {
    m_pen.begin(canvas, penFrame(context));
    m_pen.inherit(context.ink, context.font);
  }
  ~PenOnCanvas() { m_pen.end(); }
  PenOnCanvas(const PenOnCanvas&) = delete;
  PenOnCanvas& operator=(const PenOnCanvas&) = delete;
  draw::Pen& pen() { return m_pen; }

 private:
  draw::Pen m_pen;
};

}  // namespace sigil::compose::detail
