/** @file
 * A paint as a node's fill: the frame a paint context supplies, the
 * collapse onto the reconciler's Fill slot, and the resolves a painter
 * reads a fill through.
 *
 * The paint model itself is SigilMaterial's. What is compose's is the
 * routing: a static paint collapses to a Fill and rides the existing
 * caching and prune path, while a live or geometry-dependent one is kept
 * whole on the node's material slot so the painter can resolve it against
 * the frame it is drawn at.
 */

#include <include/core/SkShader.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilcompose/core/Paint.h>

#include <optional>
#include <utility>

#include "ComposeInternal.h"

namespace sigil::compose {

material::FrameData frameOf(const PaintContext& ctx) {
  material::FrameData frame;
  frame.resolution = {ctx.size.width(), ctx.size.height()};
  frame.rootResolution = {ctx.rootSize.width(), ctx.rootSize.height()};
  frame.world = ctx.toRoot.matrix;
  frame.seconds = ctx.elapsedSeconds;
  frame.contentScale = ctx.contentScale;
  return frame;
}

Fill Fill::fromMaterial(const material::Material& material) {
  // A flat colour is the colour lane, as a colour written directly is.
  if (const material::Color* color = material.color();
      color && material.layers().empty())
    return Fill::color(*color);
  return Fill{material::skia::paint(material)};
}

Fill toFill(const material::Paint& paint) {
  if (paint.isSolid()) return Fill::color(paint.solidColor());
  if (paint.isNone() || !material::skia::staticShader(paint))
    return Fill::none();
  return Fill{paint};
}

Fill resolveInk(const material::Paint& paint, const PaintContext& ctx) {
  if (ctx.inkAnchorSize.isEmpty()) return resolveFill(paint, ctx);
  if (paint.isSolid()) return Fill::color(paint.solidColor());
  if (paint.isNone()) return Fill::none();
  // The anchor box stands in for the canvas: a root-anchored build maps
  // the unit square onto `rootSize` and samples the field through the
  // inverse of `toRoot`, which is exactly "the slice of that box this
  // node stands on".
  material::FrameData frame = frameOf(ctx);
  frame.rootResolution = {ctx.inkAnchorSize.width(), ctx.inkAnchorSize.height()};
  frame.world = ctx.inkAnchorToRoot.matrix;
  material::Paint anchored = paint;
  anchored.worldSpace(true);
  if (sk_sp<SkShader> shader = material::skia::shader(anchored, frame))
    return Fill{material::skia::paint(std::move(shader))};
  return Fill::none();
}

Fill resolveFill(const material::Paint& paint, const PaintContext& ctx) {
  // A solid has no coordinates and nothing to resolve, so it answers the
  // same colour at every frame — asking first is what keeps a solid off
  // the shader path entirely.
  if (paint.isSolid()) return Fill::color(paint.solidColor());
  if (paint.isNone()) return Fill::none();
  // A static paint already holds its shader, and handing the same paint
  // back keeps the fill equal to the one the node stored.
  if (!paint.isRunning() && !paint.geometryDependent()) return toFill(paint);
  if (sk_sp<SkShader> shader = material::skia::shader(paint, frameOf(ctx)))
    return Fill{material::skia::paint(std::move(shader))};
  return Fill::none();
}

Fill resolveFill(const Fill& fill, const PaintContext& ctx) {
  if (fill.kind == Fill::Kind::Paint) return resolveFill(fill.paint(), ctx);
  return resolveRef(fill, ctx);
}

}  // namespace sigil::compose
