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

#include <optional>
#include <utility>

#include "ComposeInternal.h"

namespace sigil::compose {

bool SurfacePaint::none() const {
  if (const auto* fill = std::get_if<motion::Animatable<Fill>>(&m_value)) {
    const Fill* plain = fill->plain();
    return plain && plain->kind == Fill::Kind::None;
  }
  return std::get<material::skia::Paint>(m_value).isNone();
}

template <class Node>
Node& SurfacePaint::apply(Node& node) const {
  if (!none())
    std::visit([&](const auto& paint) { node.fill(paint); }, m_value);
  return node;
}

template Element& SurfacePaint::apply(Element&) const;
template Text& SurfacePaint::apply(Text&) const;
template Image& SurfacePaint::apply(Image&) const;
template Band& SurfacePaint::apply(Band&) const;
template Rule& SurfacePaint::apply(Rule&) const;

Fill SurfacePaint::resolve(const PaintContext& context) const {
  if (const auto* fill = std::get_if<motion::Animatable<Fill>>(&m_value)) {
    const auto value = motion::resolveProperty(*fill, std::nullopt);
    // A fill written as the ink in force, or as a custom property, takes
    // its colour from the node it is painted under.
    return resolveRef(value.binding ? value.binding->value() : value.target,
                      context);
  }
  return resolveFill(std::get<material::skia::Paint>(m_value), context);
}

std::optional<Fill> SurfacePaint::collapsedFill() const {
  if (const auto* fill = std::get_if<motion::Animatable<Fill>>(&m_value)) {
    const Fill* plain = fill->plain();
    if (!plain) return std::nullopt;
    return *plain;
  }
  const auto& paint = std::get<material::skia::Paint>(m_value);
  if (paint.isAnimated() || paint.geometryDependent()) return std::nullopt;
  return toFill(paint);
}

std::optional<material::skia::Paint> SurfacePaint::collapsedPaint() const {
  if (const auto* fill = std::get_if<motion::Animatable<Fill>>(&m_value)) {
    const Fill* plain = fill->plain();
    if (!plain || plain->references()) return std::nullopt;
    if (plain->kind == Fill::Kind::Color)
      return material::skia::Paint::solid(
          material::skia::toSkColor(plain->colorValue));
    if (plain->kind == Fill::Kind::Shader)
      return material::skia::Paint::shader(plain->shaderValue);
    return std::nullopt;
  }
  return std::get<material::skia::Paint>(m_value);
}

bool SurfacePaint::writtenAsPaint() const {
  return std::holds_alternative<material::skia::Paint>(m_value);
}

bool SurfacePaint::isAnimated() const {
  if (const auto* fill = std::get_if<motion::Animatable<Fill>>(&m_value))
    return fill->binding() != nullptr;
  return std::get<material::skia::Paint>(m_value).isAnimated();
}

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

Fill resolveInk(const material::skia::Paint& paint, const PaintContext& ctx) {
  if (ctx.inkAnchorSize.isEmpty()) return resolveFill(paint, ctx);
  if (paint.isSolid()) return Fill::color(paint.solidColor());
  if (paint.isNone()) return Fill::none();
  // The anchor box stands in for the canvas: a root-anchored build maps
  // the unit square onto `rootSize` and samples the field through the
  // inverse of `toRoot`, which is exactly "the slice of that box this
  // node stands on".
  material::skia::PaintFrame frame = frameOf(ctx);
  frame.rootSize = ctx.inkAnchorSize;
  frame.toRoot = ctx.inkAnchorToRoot;
  material::skia::Paint anchored = paint;
  anchored.worldSpace(true);
  if (sk_sp<SkShader> shader = anchored.shaderFor(frame))
    return Fill::shader(std::move(shader));
  return Fill::none();
}

Fill resolveFill(const material::skia::Paint& paint, const PaintContext& ctx) {
  // A solid has no coordinates and nothing to resolve, so it answers the
  // same colour at every frame — asking first is what keeps a solid off
  // the shader path entirely.
  if (paint.isSolid()) return Fill::color(paint.solidColor());
  if (paint.isNone()) return Fill::none();
  if (sk_sp<SkShader> shader = paint.shaderFor(frameOf(ctx)))
    return Fill::shader(std::move(shader));
  return Fill::none();
}

}  // namespace sigil::compose
