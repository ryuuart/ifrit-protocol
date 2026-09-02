/** @file
 * The two outline adaptors: painting an inner decoration against the
 * edges a mask selects, and against a concentric copy of the outline.
 */

#include <sigilcompose/kit/Silhouettes.h>

namespace sigil::compose {

void EdgeSlice::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  PaintContext local = ctx;
  local.outline = geometry::path::edges(ctx.outline, mask, step);
  inner.paint(canvas, local);
}

void Inset::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  PaintContext local = ctx;
  if (px != 0) local.outline = geometry::path::insetOutline(ctx.outline, px);
  inner.paint(canvas, local);
}

}  // namespace sigil::compose
