/** @file
 * Layer styles: bevels, sheens and inner shadows built from gradients and
 * blurs, and the gel and chrome bundles over the kit's colour tables.
 */

#include <include/core/SkColorFilter.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRRect.h>
#include <include/core/SkString.h>
#include <include/effects/SkGradient.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sigil::compose::styles {

namespace detail {
namespace {
sk_sp<SkShader> vRamp(float y0, float y1, std::vector<SkColor4f> colors,
                      std::vector<float> stops) {
  SkPoint pts[2] = {{0, y0}, {0, y1}};
  return SkShaders::LinearGradient(pts,
                                   SkGradient({{colors.data(), colors.size()},
                                               {stops.data(), stops.size()},
                                               SkTileMode::kClamp},
                                              {}));
}
/** The kit's ramp over [y0, y1]. */
sk_sp<SkShader> vRamp(float y0, float y1,
                      const std::vector<sigil::material::kit::RampStop>& ramp) {
  std::vector<SkColor4f> colors;
  std::vector<float> stops;
  for (const auto& s : ramp) {
    colors.push_back(sk(s.color));
    stops.push_back(s.pos);
  }
  return vRamp(y0, y1, std::move(colors), std::move(stops));
}
/** The kit's unit-space ramp as a compose gradient. */
Material unitRamp(const std::vector<sigil::material::kit::RampStop>& ramp) {
  std::vector<Stop> stops;
  for (const auto& s : ramp) stops.push_back({s.pos, sk(s.color)});
  return Material::linear({0, 0}, {0, 1}, std::move(stops));
}
}  // namespace
}  // namespace detail

namespace kit = sigil::material::kit;

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

Effect ripple(float amplitudePx, float wavelengthPx, float phase,
              bool vertical) {
  // The field's recipe compiled through SigilMaterial's cache, spelled
  // as a shader effect so the recipe's float uniforms stay comparable and
  // a re-described equal ripple prunes.
  const sigil::material::Material m = sigil::material::field::ripple(
      amplitudePx, wavelengthPx, phase, vertical);
  sigil::material::skia::install();
  const sigil::material::Material::Resolved resolved =
      m.resolve(sigil::material::Target::SkSL, {});
  const auto* program =
      resolved.program
          ? resolved.program->as<sigil::material::skia::SkiaProgram>()
          : nullptr;
  if (!program) return {};
  return Effect::shader(program->effect(),
                        {{"uAmp", m.get<float>("uAmp")},
                         {"uFreq", m.get<float>("uFreq")},
                         {"uPhase", m.get<float>("uPhase")},
                         {"uVertical", m.get<float>("uVertical")}});
}

void AquaBody::paint(SkCanvas& c, const PaintContext& ctx) const {
  const float H = ctx.size.height();
  const sigil::material::Color t = detail::mat(tint);
  if (opts.halo) {  // a lightened, half-transparent cast of the tint
    Shadow{detail::sk(kit::aquaHalo(t)), {0, H * 0.25f}, H * 0.40f}.paint(c,
                                                                          ctx);
  }
  SkPaint body;  // deep at the top, saturated in the middle, light below
  body.setAntiAlias(true);
  body.setShader(detail::vRamp(0, H, kit::aquaBodyRamp(t)));
  c.drawPath(ctx.outline, body);
  InnerShadow{detail::sk(kit::aquaTopBand(t)), {0, H * 0.08f}, H * 0.25f}.paint(
      c, ctx);
  if (opts.bottomGlow > 0) {  // screen-blended, fading out by mid-height
    SkPaint glow;
    glow.setAntiAlias(true);
    glow.setBlendMode(SkBlendMode::kScreen);
    glow.setShader(
        detail::vRamp(H * 0.55f, H, kit::aquaGlowRamp(t, opts.bottomGlow)));
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
  hairline.strokeFill =
      Fill::color(detail::sk(kit::aquaHairline(detail::mat(tint))));
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
  p.setShader(detail::vRamp(0, ctx.size.height(), kit::chromeRamp(palette)));
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
  keyline.strokeFill = Fill::color(detail::sk(opts.keyline));
  keyline.align = PathFormat::Align::Outer;
  LayerStyle bundle;
  bundle.under = {Decoration(Shadow{{0, 0, 0, 0.45f}, {0, 6}, 10}),
                  Decoration(ChromeBody{opts.palette})};
  // The sliver goes UNDER the content, with the plate. As a foreground it
  // would cross the node's own type, where a horizontal white band at half
  // height reads as a strikethrough instead of as a sheen on the plate.
  if (opts.horizonSliver) bundle.under.push_back(Decoration(ChromeSliver{}));
  if (opts.palette == ChromeOptions::Palette::Steel)
    bundle.over.push_back(Decoration(
        InnerShadow{detail::sk(kit::chromeSteelTopBand()), {0, 3}, 4}));
  bundle.over.push_back(Decoration(BevelEmboss{opts.bevelDepth,
                                               opts.bevelSize,
                                               120,
                                               {1, 1, 1, 0.5f},
                                               {0, 0, 0, 0.65f}}));
  if (opts.keylineWidth > 0) bundle.over.push_back(Decoration(keyline));
  return bundle;
}

Material sunsetChromeText() {
  return detail::unitRamp(kit::sunsetChromeText());
}

Material silverChromeText() {
  return detail::unitRamp(kit::silverChromeText());
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
