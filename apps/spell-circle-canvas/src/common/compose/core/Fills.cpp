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

namespace detail {

struct FillMaterial {
  material::Paint paint;
  /** The material a fill was written as; a fill written as a paint
   *  derives it, as that paint's base, the first time it is asked. */
  mutable std::optional<material::Material> material;
};

const material::Paint& paintOf(const Fill& fill) {
  static const material::Paint nothing;
  return fill.m_paint ? fill.m_paint->paint : nothing;
}

}  // namespace detail

Fill::Fill(material::Paint paint) {
  if (paint.isNone()) return;
  kind = Kind::Paint;
  m_paint = std::make_shared<const detail::FillMaterial>(
      detail::FillMaterial{std::move(paint), std::nullopt});
}

const material::Material* Fill::material() const {
  if (!m_paint) return nullptr;
  if (!m_paint->material)
    m_paint->material = material::skia::base(m_paint->paint);
  return &*m_paint->material;
}

bool Fill::needsFrame() const {
  return m_paint &&
         (m_paint->paint.isRunning() || m_paint->paint.geometryDependent());
}

bool Fill::operator==(const Fill& o) const {
  return kind == o.kind && colorValue == o.colorValue && ref == o.ref &&
         varId == o.varId &&
         (m_paint == o.m_paint ||
          (m_paint && o.m_paint && m_paint->paint == o.m_paint->paint));
}


material::FrameData frameOf(const PaintContext& ctx) {
  material::FrameData frame;
  frame.resolution = {ctx.size.x, ctx.size.y};
  frame.rootResolution = {ctx.rootSize.x, ctx.rootSize.y};
  frame.world = ctx.toRoot.matrix;
  frame.seconds = ctx.elapsedSeconds;
  frame.contentScale = ctx.contentScale;
  frame.recorder = ctx.recorder;
  return frame;
}

Fill Fill::fromMaterial(const material::Material& material) {
  // A flat colour is the colour lane, as a colour written directly is.
  if (const material::Color* color = material.color();
      color && material.layers().empty())
    return Fill::color(*color);
  material::Paint lowered = material::skia::paint(material);
  if (lowered.isNone()) return {};
  Fill fill;
  fill.kind = Kind::Paint;
  fill.m_paint = std::make_shared<const detail::FillMaterial>(
      detail::FillMaterial{std::move(lowered), material});
  return fill;
}

Fill toFill(const material::Paint& paint) {
  if (paint.isSolid()) return Fill::color(paint.solidColor());
  if (paint.isNone() || !material::skia::staticShader(paint))
    return Fill::none();
  return Fill{paint};
}

Fill resolveInk(const material::Paint& paint, const PaintContext& ctx) {
  if ((ctx.inkAnchorSize.x <= 0 || ctx.inkAnchorSize.y <= 0)) return resolveFill(paint, ctx);
  if (paint.isSolid()) return Fill::color(paint.solidColor());
  if (paint.isNone()) return Fill::none();
  // The anchor box stands in for the canvas: a root-anchored build maps
  // the unit square onto `rootSize` and samples the field through the
  // inverse of `toRoot`, which is exactly "the slice of that box this
  // node stands on".
  material::FrameData frame = frameOf(ctx);
  frame.rootResolution = {ctx.inkAnchorSize.x, ctx.inkAnchorSize.y};
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
  if (fill.kind == Fill::Kind::Paint) return resolveFill(detail::paintOf(fill), ctx);
  return resolveRef(fill, ctx);
}

}  // namespace sigil::compose
