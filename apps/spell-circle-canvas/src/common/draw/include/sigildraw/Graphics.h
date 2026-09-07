#pragma once

/** @file
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
 *  lives across frames: a sketch keeps one as a member and opens a
 *  frame on it whenever it has something to draw there.
 *
 *      draw::Graphics buffer{200, 200};       // a member of the sketch
 *
 *      void draw(Pen& pen) override {
 *        Pen& g = buffer.begin(pen);          // the buffer's own pen
 *        g.background(0);
 *        g.fill(255, 120, 80);
 *        g.circle(100, 100, 40);
 *        buffer.end();
 *        pen.image(buffer, 20, 20);           // and onto the frame
 *      }
 *
 *  ITS PIXELS ARE THE HOST'S. The buffer is formed at the host pen's
 *  own density, through the host's canvas, so it lives where the host
 *  draws — on the device when the host is on one — and falls back to
 *  raster where that canvas cannot make a surface. It is re-formed when
 *  the density changes and kept otherwise, so what was drawn on it
 *  stands until something draws over it, exactly as p5's does.
 *
 *  ITS CLOCK IS THE HOST'S. `begin` reads the host pen's frame count,
 *  elapsed time, step and fonts onto the buffer's pen, so a material
 *  resolved there and a shaped line of text there agree with the frame
 *  around them. Its style is its own and holds between frames, as a
 *  pen's does. */
class Graphics {
 public:
  /** @p width by @p height in canvas units — the units the buffer's pen
   *  draws in and the units `image` places it by, whatever density it
   *  ends up formed at. */
  Graphics(float width, float height);
  Graphics(const Graphics&) = delete;
  Graphics& operator=(const Graphics&) = delete;

  /** Opens a frame on the buffer and hands back its pen. The buffer is
   *  formed here on first use, transparent, and the host pen's clock
   *  and fonts are read onto it. */
  Pen& begin(Pen& host);
  /** Closes it. The pen's style survives; the pixels stand. */
  void end();

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
  bool m_open = false;
};

}  // namespace sigil::draw
