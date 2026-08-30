/** @file
 * Layer styles: bevels, sheens and inner shadows built from gradients and
 * blurs, as presets over keylines and shadows.
 */

#include <include/core/SkColorFilter.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRRect.h>
#include <include/core/SkString.h>
#include <include/effects/SkGradient.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/paint/LayerStyles.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sigil::compose::styles {

namespace detail {
namespace {
SkColor4f scale(SkColor4f c, float k, float a) {
  return {c.fR * k, c.fG * k, c.fB * k, a};
}
SkColor4f toward(SkColor4f c, SkColor4f target, float t, float a) {
  return {c.fR + (target.fR - c.fR) * t, c.fG + (target.fG - c.fG) * t,
          c.fB + (target.fB - c.fB) * t, a};
}
sk_sp<SkShader> vRamp(float y0, float y1, std::vector<SkColor4f> colors,
                      std::vector<float> stops) {
  SkPoint pts[2] = {{0, y0}, {0, y1}};
  return SkShaders::LinearGradient(pts,
                                   SkGradient({{colors.data(), colors.size()},
                                               {stops.data(), stops.size()},
                                               SkTileMode::kClamp},
                                              {}));
}
}  // namespace
}  // namespace detail

void InnerShadow::paint(SkCanvas& c, const PaintContext& ctx) const {
  c.save();
  c.clipPath(ctx.outline, true);
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(color, nullptr);
  p.setStyle(SkPaint::kStroke_Style);
  const float reach =
      std::max(size, 1.0f) + std::max(std::abs(offset.fX), std::abs(offset.fY));
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

void OuterGlow::paint(SkCanvas& c, const PaintContext& ctx) const {
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

void BevelEmboss::paint(SkCanvas& c, const PaintContext& ctx) const {
  const float rad = angleDeg * 3.1415927f / 180.0f;
  // Canvas y grows downward: light FROM angle → the vector pointing away
  // from the light. An inner shadow's visible edge is OPPOSITE its offset.
  const SkVector away = {-std::cos(rad) * depth, std::sin(rad) * depth};
  InnerShadow{highlight, away, size}.paint(c, ctx);               // lit edges
  InnerShadow{shadow, {-away.fX, -away.fY}, size}.paint(c, ctx);  // far edges
}

void Overlay::paint(SkCanvas& c, const PaintContext& ctx) const {
  SkPaint p;
  p.setAntiAlias(true);
  if (material.isSolid())
    p.setColor4f(material.solidColor(), nullptr);
  else if (sk_sp<SkShader> s = material.asShader())
    p.setShader(std::move(s));
  else
    return;
  p.setBlendMode(blend);
  if (opacity < 1.0f) p.setAlphaf(p.getAlphaf() * opacity);
  c.drawPath(ctx.outline, p);
}

Effect textGlow(SkColor4f color, float sigma) {
  return Effect::filter(SkImageFilters::DropShadow(0, 0, sigma, sigma,
                                                   color.toSkColor(), nullptr));
}

Effect ripple(float amplitudePx, float wavelengthPx, float phase,
              bool vertical) {
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
    if (!effect) SkDebugf("sigilcompose ripple shader: %s\n", err.c_str());
    return effect;
  }();
  if (!fx) return {};
  return Effect::shader(fx,
                        {{"uAmp", amplitudePx},
                         {"uFreq", 6.2831853f / std::max(wavelengthPx, 1.0f)},
                         {"uPhase", phase},
                         {"uVertical", vertical ? 1.0f : 0.0f}});
}

void AquaBody::paint(SkCanvas& c, const PaintContext& ctx) const {
  const float H = ctx.size.height();
  if (opts.halo) {  // a lightened, half-transparent cast of the tint
    Shadow{detail::toward(tint, {1, 1, 1, 1}, 0.30f, 0.5f),
           {0, H * 0.25f},
           H * 0.40f}
        .paint(c, ctx);
  }
  SkPaint body;  // deep at the top, saturated in the middle, light below
  body.setAntiAlias(true);
  body.setShader(
      detail::vRamp(0, H,
                    {detail::scale(tint, 0.55f, 0.9f), tint,
                     detail::toward(tint, {1, 1, 1, 1}, 0.35f, 0.95f)},
                    {0.0f, 0.55f, 1.0f}));
  c.drawPath(ctx.outline, body);
  InnerShadow{detail::scale(tint, 0.30f, 0.45f), {0, H * 0.08f}, H * 0.25f}
      .paint(c, ctx);
  if (opts.bottomGlow > 0) {  // screen-blended, fading out by mid-height
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

void AquaGloss::paint(SkCanvas& c, const PaintContext& ctx) const {
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

LayerStyle aquaGel(SkColor4f tint, AquaGelOptions opts) {
  PathFormat hairline;
  hairline.width = 1.0f;
  hairline.strokeFill = Fill::color(detail::scale(tint, 0.45f, 0.6f));
  hairline.align = PathFormat::Align::Inner;
  return LayerStyle{
      {Decoration(AquaBody{tint, opts})},
      {Decoration(AquaGloss{opts.lensInsetXFrac, 0.04f, opts.lensBottomFrac,
                            opts.lensAlphaTop, 0.0f}),
       Decoration(hairline)}};
}

LayerStyle aquaOrb(SkColor4f tint, float expectedDiameter) {
  AquaGelOptions opts;
  opts.lensInsetXFrac = 0.16f;
  opts.lensBottomFrac = 0.50f;
  opts.bottomGlow = 0.95f;
  opts.expectedHeight = expectedDiameter;
  return aquaGel(tint, opts);
}

void ChromeBody::paint(SkCanvas& c, const PaintContext& ctx) const {
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

void ChromeSliver::paint(SkCanvas& c, const PaintContext& ctx) const {
  const float W = ctx.size.width(), H = ctx.size.height();
  c.save();
  c.clipPath(ctx.outline, true);
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f({1, 1, 1, 0.9f}, nullptr);
  c.drawRect(SkRect::MakeXYWH(0, 0, W, 1), p);  // the top edge

  // One horizontal alpha ramp reused for the hot line and the bloom
  // under it; the bloom also falls off vertically, so the pair reads as
  // light gathering along the horizon rather than as two drawn rules.
  const float horizon = H * horizonFrac;
  const float fade = std::clamp(falloff, 0.02f, 0.49f);
  const float mid[4] = {0.0f, fade, 1.0f - fade, 1.0f};
  auto band = [&](float y, float height, float alpha) {
    const SkColor4f colors[4] = {
        {1, 1, 1, 0}, {1, 1, 1, alpha}, {1, 1, 1, alpha}, {1, 1, 1, 0}};
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

LayerStyle y2kChrome(ChromeOptions opts) {
  PathFormat keyline;
  keyline.width = opts.keylineWidth;
  keyline.strokeFill = Fill::color(opts.keyline);
  keyline.align = PathFormat::Align::Outer;
  LayerStyle bundle;
  bundle.under = {Decoration(Shadow{{0, 0, 0, 0.45f}, {0, 6}, 10}),
                  Decoration(ChromeBody{opts.palette})};
  // The sliver goes UNDER the content, with the plate. As a foreground it
  // would cross the node's own type, where a horizontal white band at half
  // height reads as a strikethrough instead of as a sheen on the plate.
  if (opts.horizonSliver) bundle.under.push_back(Decoration(ChromeSliver{}));
  if (opts.palette == ChromeOptions::Palette::Steel)
    bundle.over.push_back(
        Decoration(InnerShadow{detail::rgb(0x001020, 0.30f), {0, 3}, 4}));
  bundle.over.push_back(Decoration(BevelEmboss{opts.bevelDepth,
                                               opts.bevelSize,
                                               120,
                                               {1, 1, 1, 0.5f},
                                               {0, 0, 0, 0.65f}}));
  if (opts.keylineWidth > 0) bundle.over.push_back(Decoration(keyline));
  return bundle;
}

Material sunsetChromeText() {
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

Material silverChromeText() {
  return Material::linear({0, 0}, {0, 1},
                          {{0.0f, detail::rgb(0xFDFDFD)},
                           {0.2f, detail::rgb(0xD2D8DD)},
                           {0.48f, detail::rgb(0xA5ADB5)},
                           {0.5f, detail::rgb(0x6F7880)},
                           {0.52f, detail::rgb(0xE9ECEF)},
                           {0.8f, detail::rgb(0xC6CDD3)},
                           {1.0f, detail::rgb(0x9BA3AC)}});
}

void GlossContour::paint(SkCanvas& c, const PaintContext& ctx) const {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(color, nullptr);
  p.setImageFilter(SkImageFilters::ColorFilter(
      SkColorFilters::TableARGB(table.data(), nullptr, nullptr, nullptr),
      SkImageFilters::Blur(sigma, sigma, nullptr)));
  c.save();
  c.clipPath(ctx.outline, true);  // satin lives INSIDE the shape
  c.translate(offset.fX, offset.fY);
  c.drawPath(ctx.outline, p);
  c.restore();
}

std::array<uint8_t, 256> glossRing(float center, float width) {
  std::array<uint8_t, 256> t{};
  for (int i = 0; i < 256; ++i) {
    const float a = (float)i / 255.0f;
    const float d = std::abs(a - center) / std::max(0.05f, width * 0.5f);
    const float peak = std::max(0.0f, 1.0f - d);
    t[(size_t)i] = (uint8_t)std::lround(255.0f * peak * peak *
                                        (3.0f - 2.0f * peak));  // smoothstep
  }
  return t;
}

GlossContour gloss(SkColor4f color, float sigma, SkVector offset,
                   float ringCenter, float ringWidth) {
  GlossContour g;
  g.color = color;
  g.sigma = sigma;
  g.offset = offset;
  g.table = glossRing(ringCenter, ringWidth);
  return g;
}

}  // namespace sigil::compose::styles
