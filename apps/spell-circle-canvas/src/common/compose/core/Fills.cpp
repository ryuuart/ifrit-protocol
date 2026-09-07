/** @file
 * A paint as a node's fill: the frame a paint context supplies, the two
 * collapses onto the reconciler's Fill slot, and the two verbs that put a
 * paint on a description.
 *
 * The paint model itself is SigilMaterial's. What is compose's is the
 * routing: a static paint collapses to a Fill and rides the existing
 * caching and prune path, while a live or geometry-dependent one is kept
 * whole on the node's material slot so the painter can resolve it against
 * the frame it is drawn at.
 */

#include <include/core/SkShader.h>
#include <sigilcompose/core/Paint.h>

#include <utility>

#include "ComposeInternal.h"

namespace sigil::compose {

material::skia::PaintFrame frameOf(const PaintContext& ctx) {
  material::skia::PaintFrame frame;
  frame.size = ctx.size;
  frame.rootSize = ctx.rootSize;
  frame.toRoot = ctx.toRoot;
  frame.seconds = ctx.elapsedSeconds;
  frame.contentScale = ctx.contentScale;
  return frame;
}

Fill toFill(const material::skia::Paint& paint) {
  if (paint.isSolid()) return Fill::color(paint.solidColor());
  if (sk_sp<SkShader> s = paint.staticShader())
    return Fill::shader(std::move(s));
  return Fill::none();
}

Fill resolveFill(const material::skia::Paint& paint, const PaintContext& ctx) {
  // A solid has no coordinates and nothing to resolve, so it answers the
  // same colour at every frame — asking first is what keeps a solid off
  // the shader path entirely.
  if (paint.isSolid()) return Fill::color(paint.solidColor());
  if (paint.isNone()) return Fill::none();
  if (sk_sp<SkShader> s = paint.shaderFor(frameOf(ctx)))
    return Fill::shader(std::move(s));
  return Fill::none();
}

Element& Element::textFill(material::skia::Paint m) {
  m_node->textData.ensure().metricFill = std::move(m);
  return *this;
}

Element& Element::fill(material::skia::Paint m) {
  detail::MaterialData& slots = m_node->materialData.ensure();
  if (m.isAnimated() || m.geometryDependent()) {
    // Live paints re-resolve per frame; geometry-dependent ones resolve
    // when the node records (and re-record on size change) — both route
    // through the material slot so the painter resolves with the frame.
    slots.live = std::move(m);
    m_node->paint.fill.reset();
    slots.recipe.reset();
  } else {
    m_node->paint.fill = motion::Animatable<Fill>{toFill(m)};
    slots.recipe = std::move(m);  // the prune signature
    slots.live.reset();
  }
  return *this;
}

}  // namespace sigil::compose
