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
#include <include/core/SkColorFilter.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRRect.h>
#include <include/core/SkString.h>
#include <include/effects/SkGradient.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <array>
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

/** The water/heat warp: the node's rendered layer resampled through a
 *  sine displacement field — P3R's sea distortion (y shifted by a sine
 *  of x; §1: amp ≈ 2% of height, ~2 waves across, slow phase) and every
 *  glass-shimmer cousin. Attach with .effect() (warps the node's own
 *  layer) or .backdrop() (warps what's beneath). An Effect is a static
 *  value: animate by re-describing with a moving `phase` — the node
 *  re-records per change, so reserve animation for feature moments or
 *  pair with Cache::None. `vertical` displaces x by a sine of y
 *  instead. */
inline Effect ripple(float amplitudePx, float wavelengthPx,
                     float phase = 0.0f, bool vertical = false) {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform shader content;
      uniform float uAmp;
      uniform float uFreq;   // radians per px
      uniform float uPhase;
      uniform float uVertical;
      half4 main(float2 p) {
        float2 q = p;
        if (uVertical > 0.5)
          q.x += sin(p.y * uFreq + uPhase) * uAmp;
        else
          q.y += sin(p.x * uFreq + uPhase) * uAmp;
        return content.eval(q);
      }
    )"));
    if (!effect)
      SkDebugf("sigilcompose ripple shader: %s\n", err.c_str());
    return effect;
  }();
  if (!fx)
    return {};
  return Effect::shader(
      fx, {{"uAmp", amplitudePx},
           {"uFreq", 6.2831853f / std::max(wavelengthPx, 1.0f)},
           {"uPhase", phase},
           {"uVertical", vertical ? 1.0f : 0.0f}});
}

// ---------------------------------------------------------------------------
// Preset bundles (REFERENCES.md §2–3) — LayerStyle values for Element::style()

/** Knobs the Aqua bundle exposes (defaults = the §2 measured pill). */
struct AquaGelOptions {
  float lensAlphaTop = 0.72f;   ///< §2: white .72→0
  float lensBottomFrac = 0.52f; ///< §2: lens y ∈ [4%, 52%]
  float lensInsetXFrac = 0.05f; ///< §2: x ∈ [5%, 95%]; ~0.16 on spheres
  float bottomGlow = 0.85f;     ///< the light-from-below strength (screen)
  bool halo = true;             ///< §2 luminous halo drop (vs no shadow)
  /** bleed() can't see the future layout size — reserve cull reach for
   *  gel up to this tall (raise for hero scale; the halo reaches .65H). */
  float expectedHeight = 64.0f;
  bool operator==(const AquaGelOptions &) const = default;
};

/** The Aqua pill body (REFERENCES.md §2/§3, height-relative): deep→light
 *  vertical body ramp, a dark band recessed from the top, the SCREEN
 *  bottom glow (the Aqua signature — light emerging from below), and the
 *  luminous tint halo underneath. Everything computed from the node's
 *  size at paint, so one value dresses any pill; value-comparable, so a
 *  static aqua button prunes without memo. */
struct AquaBody {
  SkColor4f tint = detail::rgb(0x1E8FFF);
  AquaGelOptions opts;

  bool operator==(const AquaBody &) const = default;
  float bleed() const {
    return opts.halo ? opts.expectedHeight * 0.65f : 0.0f;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float H = ctx.size.height();
    if (opts.halo) { // §2: rgba(66,140,240,.5) (0,10) blur16 on a 40px pill
      util::Shadow{detail::toward(tint, {1, 1, 1, 1}, 0.30f, 0.5f),
                   {0, H * 0.25f},
                   H * 0.40f}
          .paint(c, ctx);
    }
    SkPaint body; // §2 body: deep at top → saturated → light at bottom
    body.setAntiAlias(true);
    body.setShader(detail::vRamp(
        0, H,
        {detail::scale(tint, 0.55f, 0.9f), tint,
         detail::toward(tint, {1, 1, 1, 1}, 0.35f, 0.95f)},
        {0.0f, 0.55f, 1.0f}));
    c.drawPath(ctx.outline, body);
    InnerShadow{detail::scale(tint, 0.30f, 0.45f), {0, H * 0.08f}, H * 0.25f}
        .paint(c, ctx);
    if (opts.bottomGlow > 0) { // §2: bottom glow, screen, fading by 45%H
      SkPaint glow;
      glow.setAntiAlias(true);
      glow.setBlendMode(SkBlendMode::kScreen);
      glow.setShader(detail::vRamp(
          H * 0.55f, H,
          {{1, 1, 1, 0},
           detail::toward(tint, {1, 1, 1, 1}, 0.80f, opts.bottomGlow)},
          {0.0f, 1.0f}));
      c.save();
      c.clipPath(ctx.outline, true);
      c.drawRect(SkRect::MakeLTRB(0, H * 0.5f, ctx.size.width(), H), glow);
      c.restore();
    }
  }
};

/** The Aqua highlight lens: a white ramp lens across the top half of the
 *  shape (clipped inside), the signature "wet" specular. */
struct AquaGloss {
  float insetXFrac = 0.05f;
  float topFrac = 0.04f, bottomFrac = 0.52f;
  float alphaTop = 0.72f, alphaBottom = 0.0f;

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

/** The drop-in Aqua Gel bundle (§2/§3): body + gloss + hairline. Use on a
 *  pill — `box().corners({h/2}).style(styles::aquaGel(tint))` — with no
 *  fill() (the body paints it). Pass options to retune the lens/glow. */
inline LayerStyle aquaGel(SkColor4f tint = detail::rgb(0x1E8FFF),
                          AquaGelOptions opts = {}) {
  PathFormat hairline;
  hairline.width = 1.0f;
  hairline.strokeFill = Fill::color(detail::scale(tint, 0.45f, 0.6f));
  hairline.align = PathFormat::Align::Inner;
  return LayerStyle{{Decoration(AquaBody{tint, opts})},
                    {Decoration(AquaGloss{opts.lensInsetXFrac, 0.04f,
                                          opts.lensBottomFrac,
                                          opts.lensAlphaTop, 0.0f}),
                     Decoration(hairline)}};
}

/** The sphere-tuned Aqua bundle: dome lens (16% inset, upper half) and a
 *  hotter bottom glow — the Aqua orb / traffic-light geometry. */
inline LayerStyle aquaOrb(SkColor4f tint = detail::rgb(0x1E8FFF),
                          float expectedDiameter = 128.0f) {
  AquaGelOptions opts;
  opts.lensInsetXFrac = 0.16f;
  opts.lensBottomFrac = 0.50f;
  opts.bottomGlow = 0.95f;
  opts.expectedHeight = expectedDiameter;
  return aquaGel(tint, opts);
}

/** Which chrome the Y2K bundle wears (both from §2/§3, exact stops). */
struct ChromeOptions {
  enum class Palette : uint8_t {
    Steel, ///< the §3 dark preset ramp (logo plates)
    Silver ///< the §2 silver window-chrome ramp
  };
  Palette palette = Palette::Steel;
  bool horizonSliver = true; ///< white specular sliver straddling 50%
  float keylineWidth = 2.0f;
  SkColor4f keyline = detail::rgb(0x10141A);
  float bevelDepth = 3.0f, bevelSize = 5.0f;
  bool operator==(const ChromeOptions &) const = default;
};

/** The chrome horizon: the hard ramp stop sits at half height. Position
 *  hand-added glints against `y = kChromeHorizonFrac * H`. */
inline constexpr float kChromeHorizonFrac = 0.50f;

/** The Y2K chrome body: the palette's vertical ramp with its hard horizon,
 *  clipped to the shape. */
struct ChromeBody {
  ChromeOptions::Palette palette = ChromeOptions::Palette::Steel;
  bool operator==(const ChromeBody &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    if (palette == ChromeOptions::Palette::Silver) {
      p.setShader(detail::vRamp(
          0, ctx.size.height(),
          {detail::rgb(0xFDFDFD), detail::rgb(0xD2D8DD), detail::rgb(0xA5ADB5),
           detail::rgb(0x6F7880), detail::rgb(0xE9ECEF), detail::rgb(0xC6CDD3),
           detail::rgb(0x9BA3AC)},
          {0.0f, 0.2f, 0.48f, 0.5f, 0.52f, 0.8f, 1.0f}));
    } else {
      p.setShader(detail::vRamp(
          0, ctx.size.height(),
          {detail::rgb(0xF4F7FA), detail::rgb(0x97A1AC), detail::rgb(0x3A4654),
           detail::rgb(0x1E2833), detail::rgb(0x5C6B7C), detail::rgb(0xDCE4EA)},
          {0.0f, 0.35f, 0.49f, 0.51f, 0.62f, 1.0f}));
    }
    c.drawPath(ctx.outline, p);
  }
};

/** The §2 finishing pass: 1px white top edge + the white specular sliver
 *  straddling the horizon (both clipped inside).
 *
 *  The sliver is a SPECULAR band, so it fades out at both ends. Drawn as a
 *  hard 4%..96% rectangle it ended in two blunt vertical stubs, and on a
 *  chrome wordmark — where the glyphs already chop the band into segments —
 *  those stubs read as a strikethrough rule someone forgot to finish. */
struct ChromeSliver {
  float horizonFrac = kChromeHorizonFrac;
  /** Fraction of the width the highlight takes to reach full strength. */
  float falloff = 0.22f;
  bool operator==(const ChromeSliver &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float W = ctx.size.width(), H = ctx.size.height();
    c.save();
    c.clipPath(ctx.outline, true);
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f({1, 1, 1, 0.9f}, nullptr);
    c.drawRect(SkRect::MakeXYWH(0, 0, W, 1), p); // the top edge

    // One horizontal alpha ramp reused for the hot line and the bloom
    // under it; the bloom also falls off vertically, so the pair reads as
    // light gathering along the horizon rather than as two drawn rules.
    const float horizon = H * horizonFrac;
    const float fade = std::clamp(falloff, 0.02f, 0.49f);
    const float mid[4] = {0.0f, fade, 1.0f - fade, 1.0f};
    auto band = [&](float y, float height, float alpha) {
      const SkColor4f colors[4] = {{1, 1, 1, 0},
                                   {1, 1, 1, alpha},
                                   {1, 1, 1, alpha},
                                   {1, 1, 1, 0}};
      SkPoint pts[2] = {{0, y}, {W, y}};
      SkPaint bp;
      bp.setAntiAlias(true);
      bp.setShader(SkShaders::LinearGradient(
          pts, SkGradient({{colors, 4}, {mid, 4}, SkTileMode::kClamp}, {})));
      c.drawRect(SkRect::MakeXYWH(0, y, W, height), bp);
    };
    band(horizon - 1, 1, 0.85f);
    band(horizon, 1, 0.28f);
    band(horizon + 1, 1, 0.16f);
    band(horizon + 2, 2, 0.07f);
    c.restore();
  }
};

/** The drop-in Y2K Chrome bundle (§2/§3): drop shadow, palette ramp,
 *  horizon sliver, chisel bevel, dark keyline outside. Silver skips the
 *  dark top band (§2 silver wants the white top edge instead). */
inline LayerStyle y2kChrome(ChromeOptions opts = {}) {
  PathFormat keyline;
  keyline.width = opts.keylineWidth;
  keyline.strokeFill = Fill::color(opts.keyline);
  keyline.align = PathFormat::Align::Outer;
  LayerStyle bundle;
  bundle.under = {Decoration(util::Shadow{{0, 0, 0, 0.45f}, {0, 6}, 10}),
                  Decoration(ChromeBody{opts.palette})};
  // The sliver rides the PLATE, under any content: chrome wordmarks keep
  // the sheen line behind the glyphs — as a foreground it struck through
  // the type (the mispositioned-sheen finding).
  if (opts.horizonSliver)
    bundle.under.push_back(Decoration(ChromeSliver{}));
  if (opts.palette == ChromeOptions::Palette::Steel)
    bundle.over.push_back(
        Decoration(InnerShadow{detail::rgb(0x001020, 0.30f), {0, 3}, 4}));
  bundle.over.push_back(Decoration(BevelEmboss{
      opts.bevelDepth, opts.bevelSize, 120, {1, 1, 1, 0.5f},
      {0, 0, 0, 0.65f}}));
  if (opts.keylineWidth > 0)
    bundle.over.push_back(Decoration(keyline));
  return bundle;
}

// ---------------------------------------------------------------------------
// Chrome type (§2) — unit-space Materials for Element::textFill()

/** The §2 sunset-chrome ramp in UNIT space: feed straight to textFill()
 *  and the hard horizon crosses the capitals at half cap height —
 *  `text(u8"CHROME", display).textFill(styles::sunsetChromeText())`. */
inline Material sunsetChromeText() {
  return Material::linear({0, 0}, {0, 1},
                          {{0.0f, detail::rgb(0xEAF6FF)},
                           {0.12f, detail::rgb(0x9CCFF3)},
                           {0.35f, detail::rgb(0x3C7FC0)},
                           {0.495f, detail::rgb(0x0B2A52)},
                           {0.505f, detail::rgb(0x7A4A1A)},
                           {0.62f, detail::rgb(0xB98A46)},
                           {0.82f, detail::rgb(0xE8CE9A)},
                           {1.0f, detail::rgb(0xFDF6E3)}});
}

/** The §2 silver-chrome ramp in unit space, for textFill(). */
inline Material silverChromeText() {
  return Material::linear({0, 0}, {0, 1},
                          {{0.0f, detail::rgb(0xFDFDFD)},
                           {0.2f, detail::rgb(0xD2D8DD)},
                           {0.48f, detail::rgb(0xA5ADB5)},
                           {0.5f, detail::rgb(0x6F7880)},
                           {0.52f, detail::rgb(0xE9ECEF)},
                           {0.8f, detail::rgb(0xC6CDD3)},
                           {1.0f, detail::rgb(0x9BA3AC)}});
}

// ---------------------------------------------------------------------------
// Gloss contour — the PS Satin / "Gloss Contour" curve (§3 anatomy)

/** The shape's blurred coverage remapped through a 256-entry CONTOUR
 *  table, tinted, clipped inside the shape — the moving light band in gel
 *  and chrome that a plain gradient can't fake (it follows the SHAPE's
 *  distance field, not a screen axis). Same math as SkTableMaskFilter
 *  (the seams-audit entry), built as blur→alpha-table on one image-filter
 *  chain so it composes with the node's other chrome in a single paint. */
struct GlossContour {
  SkColor4f color = {1, 1, 1, 0.85f};
  float sigma = 6.0f;
  SkVector offset = {0, -3};
  std::array<uint8_t, 256> table{};

  bool operator==(const GlossContour &o) const {
    return color == o.color && sigma == o.sigma && offset == o.offset &&
           table == o.table;
  }
  float bleed() const { return sigma * 3.0f; }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(color, nullptr);
    p.setImageFilter(SkImageFilters::ColorFilter(
        SkColorFilters::TableARGB(table.data(), nullptr, nullptr, nullptr),
        SkImageFilters::Blur(sigma, sigma, nullptr)));
    c.save();
    c.clipPath(ctx.outline, true); // satin lives INSIDE the shape
    c.translate(offset.fX, offset.fY);
    c.drawPath(ctx.outline, p);
    c.restore();
  }
};

/** A ring contour: the table peaks where blurred coverage crosses
 *  `center` (0 = rim, 1 = deep interior), `width` wide — the classic PS
 *  "Ring" gloss. */
inline std::array<uint8_t, 256> glossRing(float center = 0.55f,
                                          float width = 0.35f) {
  std::array<uint8_t, 256> t{};
  for (int i = 0; i < 256; ++i) {
    const float a = (float)i / 255.0f;
    const float d = std::abs(a - center) / std::max(0.05f, width * 0.5f);
    const float peak = std::max(0.0f, 1.0f - d);
    t[(size_t)i] = (uint8_t)std::lround(255.0f * peak * peak *
                                        (3.0f - 2.0f * peak)); // smoothstep
  }
  return t;
}

/** The drop-in gloss band. Attach as a foreground: it reads the node's
 *  outline and paints over the fill. */
inline GlossContour gloss(SkColor4f color = {1, 1, 1, 0.85f},
                          float sigma = 6.0f, SkVector offset = {0, -3},
                          float ringCenter = 0.55f, float ringWidth = 0.35f) {
  GlossContour g;
  g.color = color;
  g.sigma = sigma;
  g.offset = offset;
  g.table = glossRing(ringCenter, ringWidth);
  return g;
}

} // namespace sigil::compose::styles
