/** @file
 * A material's effects stage on a node: shadows, glows, strokes and
 * bevels drawn around the node's own outline, and a hard outer shadow as
 * the misprint echo that re-stamps the fill and the text beneath the real
 * pass.
 */

#include "MaterialEffects.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <cmath>

#include "description/ComposeInternal.h"

namespace sigil::compose::detail {

struct MaterialStrokeCache {
  explicit MaterialStrokeCache(const material::Material& input)
      : material(input), prepared(input) {}

  material::Material material;
  material::skia::LitSurface prepared;
  std::optional<material::Lighting> lighting;
  material::Paint paint;
};

namespace {

using Step = material::CoverageEffect::Kind;

SkPaint painted(material::Color color) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor4f(material::skia::toSkColor(color), nullptr);
  return paint;
}

void blurred(SkPaint& paint, float blur) {
  if (blur > 0)
    paint.setMaskFilter(
        SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur * 0.5f));
}

SkPath shapeOf(const PaintContext& context) {
  return context.silhouette.empty() ? geometry::path::toSk(context.outline)
                                    : geometry::path::toSk(context.silhouette);
}

/** A band hugging the inner edge, displaced along @p offset. */
void innerShadow(SkCanvas& canvas, const PaintContext& context,
                 material::Color color, glm::vec2 offset, float blur,
                 float spread) {
  canvas.save();
  canvas.clipPath(shapeOf(context), true);
  SkPaint paint = painted(color);
  paint.setStyle(SkPaint::kStroke_Style);
  const float reach = std::max(blur, 1.0f) + 2 * spread +
                      std::max(std::abs(offset.x), std::abs(offset.y));
  paint.setStrokeWidth(reach);
  blurred(paint, blur);
  canvas.translate(offset.x, offset.y);
  canvas.drawPath(geometry::path::toSk(context.outline), paint);
  canvas.restore();
}

}  // namespace

namespace {

/** Strokes @p outline with @p paint on the side @p position names. */
void strokeSide(SkCanvas& canvas, const PaintContext& context, SkPaint paint,
                float width, material::StrokePosition position) {
  paint.setStyle(SkPaint::kStroke_Style);
  canvas.save();
  // Inside or outside: clip to that side and stroke at double width, so
  // the half that is kept is the stated width.
  switch (position) {
    case material::StrokePosition::Inside:
      canvas.clipPath(shapeOf(context), true);
      paint.setStrokeWidth(width * 2);
      break;
    case material::StrokePosition::Outside:
      canvas.clipPath(shapeOf(context), SkClipOp::kDifference, true);
      paint.setStrokeWidth(width * 2);
      break;
    case material::StrokePosition::Center:
      paint.setStrokeWidth(width);
      break;
  }
  canvas.drawPath(geometry::path::toSk(context.outline), paint);
  canvas.restore();
}

}  // namespace

float CoverageMark::bleed() const {
  switch (effect.kind) {
    case Step::Shadow:
      if (effect.shadow.inside) return 0;
      return effect.shadow.blur * 1.5f + effect.shadow.spread +
             std::max(std::abs(effect.shadow.offset.x),
                      std::abs(effect.shadow.offset.y));
    case Step::Stroke:
      return effect.stroke.position == material::StrokePosition::Inside
                 ? 0
                 : effect.stroke.width;
    case Step::Bevel:
      return 0;
  }
  return 0;
}

void CoverageMark::paint(draw::Pen& pen, const PaintContext& context) const {
  SkCanvas& canvas = *pen.canvas();
  switch (effect.kind) {
    case Step::Shadow: {
      const material::ShadowOptions& shadow = effect.shadow;
      if (shadow.inside) {
        innerShadow(canvas, context, effect.color, shadow.offset, shadow.blur,
                    shadow.spread);
        return;
      }
      SkPaint paint = painted(effect.color);
      if (shadow.spread > 0) {
        paint.setStyle(SkPaint::kStrokeAndFill_Style);
        paint.setStrokeWidth(shadow.spread * 2);
      }
      blurred(paint, shadow.blur);
      canvas.save();
      canvas.translate(shadow.offset.x, shadow.offset.y);
      canvas.drawPath(geometry::path::toSk(context.outline), paint);
      canvas.restore();
      return;
    }
    case Step::Stroke:
      strokeSide(canvas, context, painted(effect.color), effect.stroke.width,
                 effect.stroke.position);
      return;
    case Step::Bevel: {
      const material::BevelOptions& bevel = effect.bevel;
      const float radians = bevel.angleDegrees * 3.14159265358979f / 180.0f;
      // Canvas y grows downward, and an inner shadow's visible edge is the
      // one opposite its offset: the lit band is cast away from the light.
      const glm::vec2 away = {-std::cos(radians) * bevel.depth,
                              std::sin(radians) * bevel.depth};
      innerShadow(canvas, context, bevel.highlight, away, bevel.size, 0);
      innerShadow(canvas, context, bevel.shadow, -away, bevel.size, 0);
      return;
    }
  }
}

bool MaterialStroke::isRunning() const {
  return surfaced ? surfaced->isRunning() : source.isRunning();
}

bool MaterialStroke::usesWorldSpace() const {
  return surfaced ? material::skia::usesWorldSpace(*surfaced)
                  : source.usesWorldSpace();
}

void MaterialStroke::paint(draw::Pen& pen, const PaintContext& context) const {
  SkCanvas& canvas = *pen.canvas();
  SkPaint stroke;
  stroke.setAntiAlias(true);
  const material::Lighting under =
      surfaced ? material::skia::lightingFor(
                     *surfaced, context.lighting ? *context.lighting
                                                 : material::Lighting{})
               : material::Lighting{};
  if (under) {
    if (!cache || cache->material != *surfaced)
      cache = std::make_shared<MaterialStrokeCache>(*surfaced);
    if (!cache->lighting || *cache->lighting != under) {
      cache->lighting = under;
      cache->paint = cache->prepared.under(under);
    }
    const sk_sp<SkShader> shader =
        material::skia::shader(cache->paint, frameOf(context));
    if (!shader) return;
    stroke.setShader(shader);
  } else if (source.isSolid()) {
    stroke.setColor4f(material::skia::toSkColor(source.solidColor()), nullptr);
  } else if (sk_sp<SkShader> shader =
                 material::skia::shader(source, frameOf(context))) {
    stroke.setShader(std::move(shader));
  } else {
    return;
  }
  strokeSide(canvas, context, std::move(stroke), options.width,
             options.position);
}

EffectMarks effectMarksOf(const material::Filter& effects) {
  EffectMarks marks;
  for (const material::CoverageEffect& step : effects.coverage()) {
    const bool hardEcho = step.kind == Step::Shadow && !step.shadow.inside &&
                          step.shadow.blur <= 0 && step.shadow.spread <= 0;
    if (hardEcho) {
      marks.echoes.push_back(
          Echo{{step.shadow.offset.x, step.shadow.offset.y}, step.color});
      continue;
    }
    const bool beneath = step.kind == Step::Shadow && !step.shadow.inside;
    (beneath ? marks.beneath : marks.over)
        .push_back(Decoration(CoverageMark{step}));
  }
  material::Filter pixels = effects.withoutCoverage();
  if (!pixels.isNone()) marks.pixels = std::move(pixels);
  return marks;
}

void applyEffects(ElementNode& node, const material::Filter& effects) {
  EffectMarks marks = effectMarksOf(effects);
  if (!marks.echoes.empty()) {
    std::vector<Echo>& echoes = node.fxData.ensure().echoes;
    echoes.insert(echoes.end(), marks.echoes.begin(), marks.echoes.end());
  }
  node.backgrounds.insert(node.backgrounds.end(), marks.beneath.begin(),
                          marks.beneath.end());
  node.foregrounds.insert(node.foregrounds.end(), marks.over.begin(),
                          marks.over.end());
  if (marks.pixels) node.fxData.ensure().layerEffect = std::move(marks.pixels);
}

}  // namespace sigil::compose::detail
