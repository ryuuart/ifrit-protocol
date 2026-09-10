/** @file
 * The two outline adaptors: painting an inner decoration against the
 * edges a mask selects, and against a concentric copy of the outline.
 */

#include <sigilcompose/brush/Adaptors.h>

namespace sigil::compose {

void EdgeSlice::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  PaintContext local = ctx;
  // The selected runs are OPEN and bound no area, so the outline they were
  // cut from is carried for an aligned stroke to clip against; without it
  // an Inner- or Outer-aligned inner decoration clips to nothing and the
  // whole mark is discarded. A slice of a slice keeps the outer one's
  // shape, which is the only one that still bounds anything.
  if (local.silhouette.isEmpty()) local.silhouette = ctx.outline;
  local.outline = geometry::path::edges(ctx.outline, mask, step);
  inner.paint(canvas, local);
}

void Inset::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  PaintContext local = ctx;
  if (px != 0) local.outline = geometry::path::insetOutline(ctx.outline, px);
  inner.paint(canvas, local);
}

}  // namespace sigil::compose
