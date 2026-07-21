#pragma once

/** @file
 * SigilCompose layer styles — the PHOTOSHOP ROUTE to rich surfaces: fake
 * bevels, metallic sheens, inner shadows, glows and overlays built from
 * gradients + blurs + blend modes, never shaders. This is the compositional
 * peer to the SkSL route (Sdf.h / Material::sksl): illusory, art-directable,
 * and the native idiom of the y2k / layer-styles era — a chrome button is a
 * gradient ramp, a bevel is two opposed inner shadows, glass is a highlight
 * lens over a body ramp.
 *
 * Every style is a VALUE DecorationScheme (defaulted equality), so styled
 * chrome prunes and caches like any static decoration. Attach with
 * `.background()` (shadows/glows under the fill), `.foreground()` or the
 * `.stroke()`/overlay slots (above content), matching Photoshop's own
 * stacking: drop shadow < outer glow < fill < pattern/gradient/color
 * overlay < inner glow < inner shadow < bevel planes < stroke.
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Material.h"
#include "sigilcompose/Util.h" // drop shadow lives there already

#include <include/core/SkCanvas.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>

#include <cmath>

namespace sigil::compose::styles {

/** Photoshop Drop Shadow — util::shadow under a coherent name. Attach as
 *  the FIRST background so the fill paints over it. */
inline util::Shadow dropShadow(SkColor4f color = {0, 0, 0, 0.5f},
                               SkVector offset = {3, 3}, float size = 6) {
  return util::shadow(color, offset, size);
}

/** Inner Shadow: the shape's INVERSE, blurred and offset, clipped inside —
 *  the recessed/punched-in look (and one half of every fake bevel).
 *  `offset` follows Photoshop: the direction the shadow is CAST, so
 *  (0, 3) casts downward and the band hugs the TOP inner edge. */
struct InnerShadow {
  SkColor4f color = {0, 0, 0, 0.5f};
  SkVector offset = {0, 3};
  float size = 5; // blur extent, px

  bool operator==(const InnerShadow &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    c.save();
    c.clipPath(ctx.outline, true);
    SkPath inverse = ctx.outline;
    inverse.toggleInverseFillType();
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(color, nullptr);
    if (size > 0)
      p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, size * 0.5f));
    // Cast semantics: the occluder (the inverse) shifts AGAINST the cast
    // direction, so the band lands on the edge the shadow falls from.
    c.translate(-offset.x(), -offset.y());
    c.drawPath(inverse, p);
    c.restore();
  }
};

/** Inner Glow: an inner shadow with no offset — edges light up inward. */
inline InnerShadow innerGlow(SkColor4f color, float size) {
  return InnerShadow{color, {0, 0}, size};
}

/** Outer Glow: the shape re-drawn blurred (optionally spread wider) —
 *  attach as a background; the fill covers the center. */
struct OuterGlow {
  SkColor4f color = {1, 1, 1, 0.8f};
  float size = 8;   // blur extent, px
  float spread = 0; // hard expansion before the blur, px

  bool operator==(const OuterGlow &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(color, nullptr);
    if (spread > 0) {
      p.setStyle(SkPaint::kStrokeAndFill_Style);
      p.setStrokeWidth(spread * 2);
    }
    if (size > 0)
      p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, size * 0.5f));
    c.drawPath(ctx.outline, p);
  }
};

/** Bevel & Emboss, the fake-3D workhorse: two OPPOSED inner shadows — a
 *  highlight plane hugging the lit edges and a shadow plane hugging the
 *  far edges. Pure illusion (no lighting model): the y2k plastic/metal
 *  look. `angleDeg` is Photoshop's light angle (CCW from +x, light COMING
 *  FROM that direction; 120° ≈ upper-left). */
struct BevelEmboss {
  float depth = 3;     // plane offset, px
  float size = 4;      // soften blur, px
  float angleDeg = 120;
  SkColor4f highlight = {1, 1, 1, 0.65f};
  SkColor4f shadow = {0, 0, 0, 0.45f};

  bool operator==(const BevelEmboss &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float rad = angleDeg * 3.1415927f / 180.0f;
    // Canvas y grows downward: light FROM angle → the vector pointing away
    // from the light. An inner shadow's visible edge is OPPOSITE its offset.
    const SkVector away = {-std::cos(rad) * depth, std::sin(rad) * depth};
    InnerShadow{highlight, away, size}.paint(c, ctx);          // lit edges
    InnerShadow{shadow, {-away.fX, -away.fY}, size}.paint(c, ctx); // far edges
  }
};

/** Color / Gradient / Pattern Overlay: any Material over the shape with a
 *  blend mode and opacity — Photoshop's three overlay styles are one value
 *  here because Material is already polymorphic. (Overlay materials should
 *  be static or live — a geometry-dependent SDF material would resolve
 *  without context here.) */
struct Overlay {
  Material material;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  float opacity = 1.0f;

  bool operator==(const Overlay &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    if (material.isSolid())
      p.setColor4f(material.solidColor(), nullptr);
    else if (sk_sp<SkShader> s = material.asShader())
      p.setShader(std::move(s));
    else
      return;
    p.setBlendMode(blend);
    if (opacity < 1.0f)
      p.setAlphaf(p.getAlphaf() * opacity);
    c.drawPath(ctx.outline, p);
  }
};

inline Overlay colorOverlay(SkColor4f color,
                            SkBlendMode blend = SkBlendMode::kSrcOver,
                            float opacity = 1.0f) {
  return Overlay{Material::solid(color), blend, opacity};
}
inline Overlay gradientOverlay(Material gradient,
                               SkBlendMode blend = SkBlendMode::kSrcOver,
                               float opacity = 1.0f) {
  return Overlay{std::move(gradient), blend, opacity};
}

} // namespace sigil::compose::styles
