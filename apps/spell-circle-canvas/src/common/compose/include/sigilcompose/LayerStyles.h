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
#include "sigilcompose/Decorations.h" // PathFormat keylines in the presets
#include "sigilcompose/Material.h"
#include "sigilcompose/Util.h" // drop shadow lives there already

#include <include/core/SkCanvas.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRRect.h>
#include <include/effects/SkGradient.h>
#include <include/effects/SkImageFilters.h>

#include <cmath>

namespace sigil::compose::styles {

namespace detail {
/** 0xRRGGBB → SkColor4f (straight alpha). */
inline SkColor4f rgb(uint32_t hex, float a = 1.0f) {
  return {(float)((hex >> 16) & 0xff) / 255.0f,
          (float)((hex >> 8) & 0xff) / 255.0f, (float)(hex & 0xff) / 255.0f,
          a};
}
inline SkColor4f scale(SkColor4f c, float k, float a) {
  return {c.fR * k, c.fG * k, c.fB * k, a};
}
inline SkColor4f toward(SkColor4f c, SkColor4f target, float t, float a) {
  return {c.fR + (target.fR - c.fR) * t, c.fG + (target.fG - c.fG) * t,
          c.fB + (target.fB - c.fB) * t, a};
}
inline sk_sp<SkShader> vRamp(float y0, float y1,
                             std::vector<SkColor4f> colors,
                             std::vector<float> stops) {
  SkPoint pts[2] = {{0, y0}, {0, y1}};
  return SkShaders::LinearGradient(
      pts, SkGradient({{colors.data(), colors.size()},
                       {stops.data(), stops.size()},
                       SkTileMode::kClamp},
                      {}));
}
} // namespace detail

/** Photoshop Drop Shadow — util::shadow under a coherent name. Attach as
 *  the FIRST background so the fill paints over it. */
inline util::Shadow dropShadow(SkColor4f color = {0, 0, 0, 0.5f},
                               SkVector offset = {3, 3}, float size = 6) {
  return util::shadow(color, offset, size);
}

/** Inner Shadow: a blurred band hugging the inner edges — the recessed/
 *  punched-in look (and one half of every fake bevel). `offset` follows
 *  Photoshop: the direction the shadow is CAST, so (0, 3) casts downward
 *  and the band hugs the TOP inner edge.
 *
 *  Implementation note (the y2k-study bug): this is a FINITE stroked band
 *  clipped inside — never a mask-blurred inverse fill, whose bounds are
 *  device-dependent and flood the whole interior when cached at a
 *  non-origin offset. */
struct InnerShadow {
  SkColor4f color = {0, 0, 0, 0.5f};
  SkVector offset = {0, 3};
  float size = 5; // blur extent, px

  bool operator==(const InnerShadow &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    c.save();
    c.clipPath(ctx.outline, true);
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(color, nullptr);
    p.setStyle(SkPaint::kStroke_Style);
    const float reach =
        std::max(size, 1.0f) +
        std::max(std::abs(offset.fX), std::abs(offset.fY));
    p.setStrokeWidth(reach);
    if (size > 0)
      p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, size * 0.5f));
    // Cast semantics (empirically pinned by the bevel tests): shifting the
    // stroked ring WITH the cast thickens its in-clip half on the edge the
    // shadow falls from — offset (0,3) casts down, band hugs the top.
    c.translate(offset.fX, offset.fY);
    c.drawPath(ctx.outline, p);
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
  /** Paint reach beyond the node's bounds (recording cull grows by this). */
  float bleed() const { return size * 2 + spread; }

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

/** Text (or any layer) glow: the node's rendered layer re-emitted blurred
 *  beneath itself (DropShadow at zero offset keeps the content). Attach
 *  with .effect(); chain with .then() for a hotter double glow. The neon /
 *  FUI treatment: `text(...).effect(styles::textGlow(cyan, 6))`. */
inline Effect textGlow(SkColor4f color, float sigma) {
  return Effect::filter(SkImageFilters::DropShadow(
      0, 0, sigma, sigma, color.toSkColor(), nullptr));
}

// ---------------------------------------------------------------------------
// Preset bundles (REFERENCES.md §2–3) — LayerStyle values for Element::style()

/** The Aqua pill body (REFERENCES.md §3 "Aqua Gel", height-relative): flat
 *  tint, a dark inner band recessed from the top (d=.08H s=.25H), a pale
 *  lift along the bottom (s=.3H), and the drop (d=.06H s=.2H) — everything
 *  computed from the node's size at paint, so one value dresses any pill.
 *  Value-comparable: a static aqua button prunes without memo. */
struct AquaBody {
  SkColor4f tint = detail::rgb(0x1E8FFF);
  bool dropShadow = true;
  /** bleed() can't see the future layout size — reserve cull reach for
   *  pills up to this tall (raise it for hero-scale gel). */
  float expectedHeight = 64.0f;

  bool operator==(const AquaBody &) const = default;
  float bleed() const { return dropShadow ? expectedHeight * 0.30f : 0.0f; }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float H = ctx.size.height();
    if (dropShadow) {
      util::Shadow{detail::scale(tint, 0.28f, 0.55f),
                   {0, H * 0.06f},
                   H * 0.20f}
          .paint(c, ctx);
    }
    SkPaint body;
    body.setAntiAlias(true);
    body.setColor4f(tint, nullptr);
    c.drawPath(ctx.outline, body);
    InnerShadow{detail::scale(tint, 0.30f, 0.45f), {0, H * 0.08f}, H * 0.25f}
        .paint(c, ctx);
    InnerShadow{detail::toward(tint, {1, 1, 1, 1}, 0.75f, 0.35f),
                {0, -H * 0.10f},
                H * 0.30f}
        .paint(c, ctx); // the bottom refraction lift
  }
};

/** The Aqua highlight lens: a white ramp lens across the top half of the
 *  shape (clipped inside), the signature "wet" specular. */
struct AquaGloss {
  float insetXFrac = 0.06f;
  float topFrac = 0.05f, bottomFrac = 0.48f;
  float alphaTop = 0.85f, alphaBottom = 0.08f;

  bool operator==(const AquaGloss &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float W = ctx.size.width(), H = ctx.size.height();
    const SkRect lens = SkRect::MakeLTRB(W * insetXFrac, H * topFrac,
                                         W * (1 - insetXFrac), H * bottomFrac);
    SkPaint p;
    p.setAntiAlias(true);
    p.setShader(detail::vRamp(lens.top(), lens.bottom(),
                              {{1, 1, 1, alphaTop}, {1, 1, 1, alphaBottom}},
                              {0.0f, 1.0f}));
    c.save();
    c.clipPath(ctx.outline, true);
    c.drawRRect(SkRRect::MakeRectXY(lens, lens.height() / 2, lens.height() / 2),
                p);
    c.restore();
  }
};

/** The drop-in Aqua Gel bundle (§3 preset): body + gloss + hairline. Use on
 *  a pill — `box().corners({h/2}).style(styles::aquaGel(tint))` — with no
 *  fill() (the body paints it). */
inline LayerStyle aquaGel(SkColor4f tint = detail::rgb(0x1E8FFF)) {
  PathFormat hairline;
  hairline.width = 1.0f;
  hairline.strokeFill = Fill::color(detail::scale(tint, 0.45f, 0.6f));
  hairline.align = PathFormat::Align::Inner;
  return LayerStyle{{Decoration(AquaBody{tint})},
                    {Decoration(AquaGloss{}), Decoration(hairline)}};
}

/** The Y2K chrome body (§3 preset, exact ramp): the sunset-chrome vertical
 *  gradient with its hard horizon at 49/51%, clipped to the shape. */
struct ChromeBody {
  bool operator==(const ChromeBody &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    p.setShader(detail::vRamp(
        0, ctx.size.height(),
        {detail::rgb(0xF4F7FA), detail::rgb(0x97A1AC), detail::rgb(0x3A4654),
         detail::rgb(0x1E2833), detail::rgb(0x5C6B7C), detail::rgb(0xDCE4EA)},
        {0.0f, 0.35f, 0.49f, 0.51f, 0.62f, 1.0f}));
    c.drawPath(ctx.outline, p);
  }
};

/** The drop-in Y2K Chrome bundle (§3 preset): drop shadow, chrome ramp,
 *  inner shade, chisel bevel, dark keyline outside. */
inline LayerStyle y2kChrome() {
  PathFormat keyline;
  keyline.width = 2.0f;
  keyline.strokeFill = Fill::color(detail::rgb(0x10141A));
  keyline.align = PathFormat::Align::Outer;
  return LayerStyle{
      {Decoration(util::Shadow{{0, 0, 0, 0.45f}, {0, 6}, 10}),
       Decoration(ChromeBody{})},
      {Decoration(InnerShadow{detail::rgb(0x001020, 0.30f), {0, 3}, 4}),
       Decoration(BevelEmboss{3, 5, 120, {1, 1, 1, 0.5f}, {0, 0, 0, 0.65f}}),
       Decoration(keyline)}};
}

} // namespace sigil::compose::styles
