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
    colors.push_back(material::skia::toSkColor(s.color));
    stops.push_back(s.pos);
  }
  return vRamp(y0, y1, std::move(colors), std::move(stops));
}
/** The kit's unit-space ramp as a compose gradient. */
material::skia::Paint unitRamp(
    const std::vector<sigil::material::kit::RampStop>& ramp) {
  std::vector<material::skia::Stop> stops;
  stops.reserve(ramp.size());
  for (const auto& s : ramp)
    stops.push_back({s.pos, material::skia::toSkColor(s.color)});
  return material::skia::Paint::linear({0, 0}, {0, 1}, std::move(stops));
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

}  // namespace sigil::compose::styles
