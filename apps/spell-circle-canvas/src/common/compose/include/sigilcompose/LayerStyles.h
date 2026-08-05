#pragma once

/** @file
 * SigilCompose layer styles — rich surfaces the way an image editor makes
 * them: fake bevels, metallic sheens, inner shadows, glows and overlays,
 * built from gradients, blurs and blend modes and never from shaders. This
 * is the compositional peer to the SkSL route (Sdf.h, Material::sksl). It
 * models no lighting: a chrome button is a gradient ramp, a bevel is two
 * opposed inner shadows, glass is a highlight lens over a body ramp.
 *
 * Every style here is a VALUE decoration with defaulted equality, so styled
 * chrome prunes and caches like any other static decoration — which is the
 * reason to reach for these before writing a shader.
 *
 * ATTACHMENT IS THE CONTRACT. `.background()` paints BENEATH the node's
 * fill, so anything attached there is hidden by an opaque fill: it is
 * where shadows and outer glows belong, and where a body ramp belongs on a
 * node with no fill of its own. `.foreground()` paints above the fill, the
 * content and the children. The order that reads correctly is the familiar
 * one — drop shadow, outer glow, fill, colour/gradient overlay, inner glow,
 * inner shadow, bevel planes, stroke — and it is produced by which slot
 * each style is attached to plus the order within that slot.
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

/** Drop shadow — `util::shadow` under the name it has in this family.
 *  Attach as the FIRST background, so everything else paints over it. */
inline util::Shadow dropShadow(SkColor4f color = {0, 0, 0, 0.5f},
                               SkVector offset = {3, 3}, float size = 6) {
  return util::shadow(color, offset, size);
}

/** Inner shadow: a blurred band hugging the inner edges — the recessed,
 *  punched-in look, and one half of every fake bevel. `offset` is the
 *  direction the shadow is CAST, so (0, 3) casts downward and the band
 *  therefore hugs the TOP inner edge.
 *
 *  It is built as a FINITE stroked band clipped inside the outline, and
 *  must stay that way. The obvious alternative — blurring an inverse fill
 *  through a mask filter — has device-dependent bounds, so it floods the
 *  whole interior when the node is cached at a non-origin offset. */
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
    // Cast semantics: shifting the stroked ring WITH the cast direction
    // thickens the half that stays inside the clip on the edge the shadow
    // falls from — offset (0,3) casts down, so the band hugs the top.
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

/** Bevel and emboss, the fake-3D workhorse: two OPPOSED inner shadows — a
 *  highlight plane hugging the lit edges and a shadow plane hugging the far
 *  edges. There is no lighting model here; the depth is entirely in those
 *  two bands. `angleDeg` is the light angle, counter-clockwise from +x and
 *  naming the direction the light COMES FROM, so 120° is upper-left. */
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

/** Colour, gradient and pattern overlay in one value: any Material drawn
 *  over the shape with a blend mode and an opacity. They are not three
 *  styles here because a Material is already polymorphic.
 *
 *  **Overlay materials must be STATIC.** This scheme declares no volatility
 *  and resolves its material WITHOUT a paint context, and both halves of
 *  that bite silently: a LIVE material is sampled once, frozen into the
 *  cached picture, and never repaints; a geometry-dependent one resolves
 *  against a zero size, so anything scaled by the node's box degenerates.
 *  Use `decorations::wash` for either — it declares its animation and
 *  resolves against the real context. */
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
 *  beneath itself — a drop shadow at zero offset, which keeps the content
 *  on top. Attach with `.effect()`, and chain with `.then()` for a tighter
 *  core over a wider halo: `text(...).effect(styles::textGlow(cyan, 6))`. */
inline Effect textGlow(SkColor4f color, float sigma) {
  return Effect::filter(SkImageFilters::DropShadow(
      0, 0, sigma, sigma, color.toSkColor(), nullptr));
}

/** The water/heat warp: the node's rendered layer resampled through a sine
 *  displacement field — y shifted by a sine of x, or with `vertical`, x by
 *  a sine of y. Water reads convincingly at an amplitude of a few percent
 *  of the node's height with only a couple of waves across it. Attach with
 *  `.effect()` to warp the node's own layer, or `.backdrop()` to warp what
 *  is beneath it.
 *
 *  An Effect is a STATIC value, so animating this means re-describing with
 *  a moving `phase`, and the node re-records on every change. Keep it for
 *  moments that earn it, or pair it with Cache::None so the node is not
 *  paying to invalidate a cache it never keeps. */
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
// Preset bundles — LayerStyle values for Element::style()

/** Knobs the gel bundle exposes; the defaults dress a pill. */
struct AquaGelOptions {
  float lensAlphaTop = 0.72f;   ///< lens ramp: white at the top, clear below
  float lensBottomFrac = 0.52f; ///< lens ends this far down the box
  float lensInsetXFrac = 0.05f; ///< lens inset each side; ~0.16 on spheres
  float bottomGlow = 0.85f;     ///< strength of the light from below
  bool halo = true;             ///< luminous tint drop beneath the shape
  /** `bleed()` runs before the node has a layout size, so the halo's cull
   *  reserve has to be declared here: set this to the tallest the gel will
   *  be. The halo reaches about 0.65 of that height beyond the box, and
   *  under-declaring it truncates the halo at the cached picture's edge. */
  float expectedHeight = 64.0f;
  bool operator==(const AquaGelOptions &) const = default;
};

/** The gel pill body, everything sized as a FRACTION of the node's height:
 *  a deep→light vertical ramp, a dark band recessed from the top, a
 *  screen-blended glow rising from the bottom edge, and a luminous halo of
 *  the tint underneath. Because it reads the size at paint, one value
 *  dresses a pill of any dimensions; because it is value-comparable, a
 *  static button wearing it prunes without a memo. */
struct AquaBody {
  SkColor4f tint = detail::rgb(0x1E8FFF);
  AquaGelOptions opts;

  bool operator==(const AquaBody &) const = default;
  float bleed() const {
    return opts.halo ? opts.expectedHeight * 0.65f : 0.0f;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float H = ctx.size.height();
    if (opts.halo) { // a lightened, half-transparent cast of the tint
      util::Shadow{detail::toward(tint, {1, 1, 1, 1}, 0.30f, 0.5f),
                   {0, H * 0.25f},
                   H * 0.40f}
          .paint(c, ctx);
    }
    SkPaint body; // deep at the top, saturated in the middle, light below
    body.setAntiAlias(true);
    body.setShader(detail::vRamp(
        0, H,
        {detail::scale(tint, 0.55f, 0.9f), tint,
         detail::toward(tint, {1, 1, 1, 1}, 0.35f, 0.95f)},
        {0.0f, 0.55f, 1.0f}));
    c.drawPath(ctx.outline, body);
    InnerShadow{detail::scale(tint, 0.30f, 0.45f), {0, H * 0.08f}, H * 0.25f}
        .paint(c, ctx);
    if (opts.bottomGlow > 0) { // screen-blended, fading out by mid-height
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

/** The highlight lens: a white ramp across the top half of the shape,
 *  clipped inside it — the wet-looking specular that sells the gel. */
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

/** The drop-in gel bundle: body, gloss and a hairline keyline. Use it on a
 *  pill — `box().corners({h/2}).style(styles::aquaGel(tint))` — and give
 *  the node NO fill: the body decoration paints the surface, and a fill
 *  would cover it. Pass options to retune the lens and the glow. */
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

/** The sphere-tuned bundle: a domed lens inset further from the edges and
 *  confined to the upper half, over a hotter bottom glow — what reads as
 *  round rather than as a pill. Pass the diameter so the halo reserves the
 *  right cull reach. */
inline LayerStyle aquaOrb(SkColor4f tint = detail::rgb(0x1E8FFF),
                          float expectedDiameter = 128.0f) {
  AquaGelOptions opts;
  opts.lensInsetXFrac = 0.16f;
  opts.lensBottomFrac = 0.50f;
  opts.bottomGlow = 0.95f;
  opts.expectedHeight = expectedDiameter;
  return aquaGel(tint, opts);
}

/** Which chrome the bundle wears. */
struct ChromeOptions {
  enum class Palette : uint8_t {
    Steel, ///< the dark ramp — heavy contrast, for plates and wordmarks
    Silver ///< the light ramp — window and control chrome
  };
  Palette palette = Palette::Steel;
  bool horizonSliver = true; ///< white specular sliver straddling 50%
  float keylineWidth = 2.0f;
  SkColor4f keyline = detail::rgb(0x10141A);
  float bevelDepth = 3.0f, bevelSize = 5.0f;
  bool operator==(const ChromeOptions &) const = default;
};

/** Where the chrome ramp's hard stop sits, as a fraction of the node's
 *  height. Position hand-added glints against `kChromeHorizonFrac * H` so
 *  they stay on the horizon at any size. */
inline constexpr float kChromeHorizonFrac = 0.50f;

/** The chrome body: the palette's vertical ramp, with its hard stop at the
 *  horizon, drawn through the shape's outline. */
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

/** The finishing pass: a 1 px white top edge plus the white specular sliver
 *  straddling the horizon, both clipped inside the shape.
 *
 *  The sliver FADES OUT at both ends, and must. A specular band drawn as a
 *  hard rectangle ends in two blunt vertical stubs, and on a chrome
 *  wordmark — where the glyphs already chop the band into segments — those
 *  stubs read as an unfinished strikethrough rather than as light. */
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

/** The drop-in chrome bundle: drop shadow, palette ramp, horizon sliver,
 *  chisel bevel, and a dark keyline stroked OUTSIDE the silhouette. The
 *  Silver palette skips the dark inner top band, which would fight its
 *  white top edge. */
inline LayerStyle y2kChrome(ChromeOptions opts = {}) {
  PathFormat keyline;
  keyline.width = opts.keylineWidth;
  keyline.strokeFill = Fill::color(opts.keyline);
  keyline.align = PathFormat::Align::Outer;
  LayerStyle bundle;
  bundle.under = {Decoration(util::Shadow{{0, 0, 0, 0.45f}, {0, 6}, 10}),
                  Decoration(ChromeBody{opts.palette})};
  // The sliver goes UNDER the content, with the plate. As a foreground it
  // would cross the node's own type, where a horizontal white band at half
  // height reads as a strikethrough instead of as a sheen on the plate.
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
// Chrome type — unit-space Materials for Element::textFill()

/** The sunset-chrome ramp in UNIT space: hand it straight to textFill()
 *  and the hard horizon crosses the capitals at half cap height, whatever
 *  the size —
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

/** The silver-chrome ramp in unit space, for textFill(). */
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
// Gloss contour — the satin band that follows the shape

/** The shape's blurred coverage remapped through a 256-entry CONTOUR
 *  table, tinted and clipped inside the shape.
 *
 *  This is the light band in gel and chrome that a plain gradient cannot
 *  fake, because it follows the SHAPE's own distance field rather than a
 *  screen axis: on a blob it curves with the blob. It is one image-filter
 *  chain (blur, then an alpha table), so it composes with the node's other
 *  decorations inside a single paint rather than forcing a layer. */
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
 *  `center` — 0 is the rim, 1 the deep interior — over a band `width`
 *  wide, giving one bright ring inside the silhouette. */
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
