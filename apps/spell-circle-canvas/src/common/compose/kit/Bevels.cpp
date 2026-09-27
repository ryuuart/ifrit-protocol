/** @file
 * The bevel's paint — one ring, or two — and the era token sets: what
 * each toolkit's edge was, in tones, depth, corner and softness.
 */

#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/skia/Color.h>

#include <algorithm>
#include <cmath>

#include "MaterialEffects.h"

namespace sigil::compose::kit {
namespace {

/** ONE RING, drawn or moulded. The tokens mean the same thing on both
 *  sides of that choice, so the two mechanisms differ only in which value
 *  is built here. */
void ring(draw::Pen& pen, const PaintContext& ctx, const Bevel& b,
          const material::Color& light, const material::Color& shadow,
          float depth, float shadowDepth, bool sunken,
          geometry::path::Edge edges, styles::BevelEnds ends) {
  const float lit = depth;
  const float drop = shadowDepth > 0 ? shadowDepth : depth;
  if (lit <= 0 && drop <= 0) return;
  if (b.softness > 0) {
    // A moulded edge has one depth and one blur, and turns over by
    // swapping the two planes rather than by swapping the light. It has
    // no bands, so the edge mask has nothing to select. It is the
    // material's bevel effect, painted where this ring stands.
    material::CoverageEffect bevel;
    bevel.kind = material::CoverageEffect::Kind::Bevel;
    bevel.bevel = {.depth = std::max(lit, drop),
                   .size = b.softness,
                   .angleDegrees = b.angleDeg,
                   .highlight = sunken ? shadow : light,
                   .shadow = sunken ? light : shadow};
    detail::CoverageMark{bevel}.paint(pen, ctx);
    return;
  }
  styles::BevelPair{light, shadow,   lit,         drop,  sunken,
                    3.0f,  b.corner, b.antiAlias, edges, ends}
      .paint(pen, ctx);
}

}  // namespace

float Bevel::reach() const {
  const float outer = std::max({depth, shadowDepth, softness});
  if (!inner) return outer;
  return std::max(outer, inner->gap + std::max({inner->depth,
                                                inner->shadowDepth, softness}));
}

void Bevel::paint(draw::Pen& pen, const PaintContext& ctx) const {
  ring(pen, ctx, *this, light, shadow, depth, shadowDepth, sunken, edges, ends);
  if (!inner) return;
  PaintContext local = ctx;
  if (inner->gap != 0) {
    local.outline = geometry::path::fromSk(geometry::path::insetOutline(geometry::path::toSk(ctx.outline), inner->gap));
    // The shape a narrowed outline was cut from is concentric with it, so
    // the inner ring's clip moves in with its marks; leaving the outer
    // shape here would clip the inner ring against a boundary it no longer
    // stands on.
    if (!ctx.silhouette.empty())
      local.silhouette = geometry::path::fromSk(geometry::path::insetOutline(geometry::path::toSk(ctx.silhouette), inner->gap));
  }
  ring(pen, local, *this, inner->light, inner->shadow, inner->depth,
       inner->shadowDepth, sunken != inner->inverted, inner->edges,
       inner->ends);
}

Bevel ambientBevel(Bevel fallback) {
  return core::environment::inheritedOr(fallback);
}

Element& bevelled(Element& e, const Bevel& b) {
  Bevel outer = b;
  outer.inner.reset();
  e.overlay(outer);
  if (!b.inner) return e;
  Bevel in = outer;
  in.light = b.inner->light;
  in.shadow = b.inner->shadow;
  in.depth = b.inner->depth;
  in.shadowDepth = b.inner->shadowDepth;
  in.sunken = b.sunken != b.inner->inverted;
  in.edges = b.inner->edges;
  in.ends = b.inner->ends;
  e.foreground(inset(b.inner->gap, Decoration(in)));
  return e;
}

namespace bevels {

Bevel motif(material::Color light, material::Color shadow, float depth,
            bool sunken) {
  Bevel b;
  b.light = light;
  b.shadow = shadow;
  b.depth = depth;
  b.shadowDepth = depth;
  b.sunken = sunken;
  b.corner = styles::BevelCorner::Mitre;
  b.antiAlias = false;
  return b;
}

Bevel motifEtched(material::Color light, material::Color shadow, float depth,
                  bool sunken) {
  // Half of one pixel is no pixel: below two the groove has no room for
  // two rings and is the plain shadow.
  const float half = std::floor(depth * 0.5f);
  if (half < 1) return motif(light, shadow, depth, sunken);
  Bevel b = motif(light, shadow, half, sunken);
  b.corner = styles::BevelCorner::MitreFar;
  b.inner = BevelInner{half, light, shadow, half, half, true};
  return b;
}

Bevel flash(material::Color face, float gap) {
  Bevel b;
  b.light = sigil::material::lighten(face, 0.15f);
  b.shadow = sigil::material::scale(face, 0.40f);
  b.depth = 3;
  b.shadowDepth = 2;
  if (gap > 0)
    b.inner = BevelInner{
        gap,
        sigil::material::withAlpha(sigil::material::lighten(face, 0.16f), 0.8f),
        sigil::material::withAlpha(sigil::material::scale(face, 0.45f), 0.85f),
        2,
        1,
        false};
  return b;
}

Bevel skin(material::Color light, material::Color shadow, float depth,
           bool sunken) {
  Bevel b;
  b.light = light;
  b.shadow = shadow;
  b.depth = depth;
  b.shadowDepth = depth;
  b.sunken = sunken;
  return b;
}

Bevel plate(material::Color light, material::Color shadow, float depth,
            float softness, bool sunken) {
  Bevel b;
  b.light = light;
  b.shadow = shadow;
  b.depth = depth;
  b.softness = softness;
  b.angleDeg = 118;
  b.sunken = sunken;
  return b;
}

}  // namespace bevels

}  // namespace sigil::compose::kit
