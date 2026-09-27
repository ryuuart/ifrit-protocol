/** @file
 * The two outline adaptors: painting an inner decoration against the
 * edges a mask selects, and against a concentric copy of the outline.
 */

#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilcompose/brush/Adaptors.h>

namespace sigil::compose {

void EdgeSlice::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
  PaintContext local = ctx;
  // The selected runs are OPEN and bound no area, so the outline they were
  // cut from is carried for an aligned stroke to clip against; without it
  // an Inner- or Outer-aligned inner decoration clips to nothing and the
  // whole mark is discarded. A slice of a slice keeps the outer one's
  // shape, which is the only one that still bounds anything.
  if (local.silhouette.empty()) local.silhouette = ctx.outline;
  local.outline = geometry::path::fromSk(geometry::path::edges(geometry::path::toSk(ctx.outline), mask, step));
  inner.paint(pen, local);
}

void Inset::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
  PaintContext local = ctx;
  if (px != 0) local.outline = geometry::path::fromSk(geometry::path::insetOutline(geometry::path::toSk(ctx.outline), px));
  inner.paint(pen, local);
}

}  // namespace sigil::compose
