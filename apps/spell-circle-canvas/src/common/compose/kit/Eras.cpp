/** @file
 * The era looks: the gel bundle, the y2k chrome bundle, the two chrome
 * ramps a wordmark is filled with, and the satin contour band. Every one
 * is a composition of the brush tier's mechanisms over SigilMaterial's
 * colour tables — no new kind of decoration, which is what keeps a look
 * in the kit.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRRect.h>
#include <include/core/SkShader.h>
#include <include/effects/SkGradient.h>
#include <include/effects/SkImageFilters.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Gel.h>
#include <sigilcompose/kit/Gloss.h>
#include <sigilmaterial/kit/LayerStyles.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Ramp.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

void AquaBody::paint(SkCanvas& c, const PaintContext& ctx) const {
  const float H = ctx.size.height();
  const sigil::material::Color t = material::skia::toColor(tint);
  if (opts.halo) {  // a lightened, half-transparent cast of the tint
    Shadow{material::skia::toSkColor(material::kit::aquaHalo(t)),
           {0, H * 0.25f},
           H * 0.40f}
        .paint(c, ctx);
  }
  SkPaint body;  // deep at the top, saturated in the middle, light below
  body.setAntiAlias(true);
  body.setShader(
      material::skia::verticalRamp(0, H, material::kit::aquaBodyRamp(t)));
  c.drawPath(ctx.outline, body);
  if (opts.topBand > 0) {  // the recess under the top edge
    sigil::material::Color band = material::kit::aquaTopBand(t);
    band.a *= opts.topBand;
    styles::InnerShadow{
        material::skia::toSkColor(band), {0, H * 0.08f}, H * 0.25f}
        .paint(c, ctx);
  }
  if (opts.bottomGlow > 0) {  // screen-blended, fading out by mid-height
    SkPaint glow;
    glow.setAntiAlias(true);
    glow.setBlendMode(SkBlendMode::kScreen);
    glow.setShader(
        material::skia::verticalRamp(H * 0.55f, H,
                                     material::kit::aquaGlowRamp(t, opts.bottomGlow)));
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
  const float fade = std::clamp(fadeEnd, 0.05f, 1.0f);
  p.setShader(material::skia::verticalRamp(lens.top(), lens.bottom(),
                                           {{0.0f, {1, 1, 1, alphaTop}},
                                            {fade, {1, 1, 1, alphaBottom}},
                                            {1.0f, {1, 1, 1, alphaBottom}}}));
  c.save();
  c.clipPath(ctx.outline, true);
  c.drawRRect(SkRRect::MakeRectXY(lens, lens.height() / 2, lens.height() / 2),
              p);
  c.restore();
}

LayerStyle aquaGel(SkColor4f tint, AquaGelOptions opts) {
  PathFormat hairline;
  hairline.width = 1.0f;
  hairline.strokeFill = Fill::color(material::skia::toSkColor(
      material::kit::aquaHairline(material::skia::toColor(tint))));
  hairline.align = PathFormat::Align::Inner;
  return LayerStyle{
      {Decoration(AquaBody{tint, opts})},
      {Decoration(AquaGloss{opts.lensInsetXFrac, 0.04f, opts.lensBottomFrac,
                            opts.lensAlphaTop, 0.0f, opts.lensFadeEnd}),
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
  p.setShader(material::skia::verticalRamp(0, ctx.size.height(),
                                           material::kit::chromeRamp(palette)));
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
  keyline.strokeFill = Fill::color(material::skia::toSkColor(opts.keyline));
  keyline.align = PathFormat::Align::Outer;
  LayerStyle bundle;
  bundle.under = {Decoration(Shadow{{0, 0, 0, 0.45f}, {0, 6}, 10}),
                  Decoration(ChromeBody{opts.palette})};
  // The sliver goes UNDER the content, with the plate. As a foreground it
  // would cross the node's own type, where a horizontal white band at half
  // height reads as a strikethrough instead of as a sheen on the plate.
  if (opts.horizonSliver) bundle.under.emplace_back(ChromeSliver{});
  if (opts.palette == ChromeOptions::Palette::Steel)
    bundle.over.emplace_back(styles::InnerShadow{
        material::skia::toSkColor(material::kit::chromeSteelTopBand()),
        {0, 3},
        4});
  bundle.over.emplace_back(styles::BevelEmboss{
      opts.bevelDepth, opts.bevelSize, 120, {1, 1, 1, 0.5f}, {0, 0, 0, 0.65f}});
  if (opts.keylineWidth > 0) bundle.over.emplace_back(keyline);
  return bundle;
}

material::skia::Paint sunsetChromeType() {
  return material::skia::unitRamp(material::kit::sunsetChromeText());
}

material::skia::Paint silverChromeType() {
  return material::skia::unitRamp(material::kit::silverChromeText());
}

void GlossContour::paint(SkCanvas& c, const PaintContext& ctx) const {
  SkPaint p;
  p.setAntiAlias(true);
  // The ring table reads blurred COVERAGE, so the outline is drawn opaque
  // and the colour's alpha is applied after the table: scaling the
  // coverage first would move the ring, and a translucent colour whose
  // alpha sits at the ring's centre would put the whole interior on the
  // peak.
  p.setColor4f({color.fR, color.fG, color.fB, 1.0f}, nullptr);
  const float alphaScale[20] = {1, 0, 0, 0,        0,  //
                                0, 1, 0, 0,        0,  //
                                0, 0, 1, 0,        0,  //
                                0, 0, 0, color.fA, 0};
  p.setImageFilter(SkImageFilters::ColorFilter(
      SkColorFilters::Compose(
          SkColorFilters::Matrix(alphaScale),
          SkColorFilters::TableARGB(table.data(), nullptr, nullptr, nullptr)),
      SkImageFilters::Blur(sigma, sigma, nullptr)));
  c.save();
  c.clipPath(ctx.outline, true);  // satin lives INSIDE the shape
  c.translate(offset.fX, offset.fY);
  c.drawPath(ctx.outline, p);
  c.restore();
}

GlossContour gloss(SkColor4f color, float sigma, SkVector offset,
                   float ringCenter, float ringWidth) {
  GlossContour g;
  g.color = color;
  g.sigma = sigma;
  g.offset = offset;
  g.table = material::kit::contourRing(ringCenter, ringWidth);
  return g;
}

}  // namespace sigil::compose::kit
