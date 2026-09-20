#pragma once

/** @file
 * @ingroup draw-pen
 *
 * A second canvas, off screen, with a pen of its own — what p5 calls a
 * Graphics: somewhere to draw once and blit many times, or to build a
 * picture the frame then reads back.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkSurface.h>
#include <sigildraw/Pen.h>

namespace sigil::draw {

/** AN OFFSCREEN CANVAS AND THE PEN THAT DRAWS ON IT — p5's
 *  `createGraphics(w, h)`, spelled as the value it returns because it
 *  lives across frames. ITS PIXELS ARE THE HOST'S, formed at the host
 *  pen's own density through the host's canvas, and a re-form keeps the
 *  picture by drawing the old surface into the new one. ITS CLOCK IS THE
 *  HOST'S, read onto the buffer's pen by `begin`; its style is its own
 *  and holds between frames, as a pen's does. */
class Graphics {
 public:
  /** @p width by @p height in canvas units — the units the buffer's pen
   *  draws in and the units `image` places it by, whatever density it
   *  ends up formed at. */
  Graphics(float width, float height);
  Graphics(const Graphics&) = delete;
  Graphics& operator=(const Graphics&) = delete;

  /** Opens a frame on the buffer and hands back its pen. The buffer is
   *  formed here on first use, transparent, and the host pen's clock,
   *  fonts, pointer and keys are read onto it. */
  Pen& begin(Pen& host);
  /** Closes it. The pen's style survives; the pixels stand. */
  void end();

  /** ANOTHER CANVAS SIZE for the buffer, in the same units the
   *  constructor took — what a host whose box has changed calls. The
   *  pixels are kept: the replacement surface is formed at the next
   *  `begin` with what this one holds drawn into it, scaled to the new
   *  extent. The same size again does nothing. */
  void resize(float width, float height);
  /** A FLOOR ON THE DENSITY the buffer is formed at, in device pixels per
   *  canvas unit. The buffer is formed at the host's own density or this,
   *  whichever is greater, so a picture a host means to photograph finer
   *  than it steps it is drawn finer from its first frame rather than
   *  magnified at the still. Zero, the default, is the host's density
   *  alone. A change takes effect at the next `begin`, pixels kept. */
  void setDensityFloor(float devicePixelsPerUnit);

  /** What has been drawn on it, as an image — what `pen.image` takes,
   *  and what a material takes as a shader's source. Null before the
   *  first `begin`. */
  [[nodiscard]] sk_sp<SkImage> image() const;

  [[nodiscard]] float width() const { return m_width; }
  [[nodiscard]] float height() const { return m_height; }
  /** The pixels it was formed at, which is the canvas size times the
   *  host's density; empty before the first `begin`. */
  [[nodiscard]] SkISize extent() const { return m_extent; }

  /** The buffer's own pen, between frames as well as during one — so a
   *  style set once, outside any frame, holds for every frame after. */
  Pen pen;

 private:
  void form(Pen& host);

  sk_sp<SkSurface> m_surface;
  SkISize m_extent = SkISize::MakeEmpty();
  float m_width;
  float m_height;
  float m_scale = 1.0f;
  float m_densityFloor = 0.0f;
  bool m_open = false;
};

}  // namespace sigil::draw
