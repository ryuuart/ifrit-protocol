/** @file
 * A material's effects stage on a node: shadows, glows, strokes and
 * bevels drawn around the node's own outline, and a hard outer shadow as
 * the misprint echo that re-stamps the fill and the text beneath the real
 * pass.
 */

#include "MaterialEffects.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/pathops/SkPathOps.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/skia/Bevel.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SurfaceMaps.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>

#include <algorithm>
#include <cmath>

#include "SurfaceMap.h"
#include "UnitSurface.h"
#include "description/ComposeInternal.h"

namespace sigil::compose::detail {

struct MaterialStrokeCache {
  explicit MaterialStrokeCache(const material::Material& input)
      : material(input), prepared(input) {}

  material::Material material;
  material::skia::SurfaceMaps prepared;
  /** Where the material reads its unit: the band the keyline covers and
   *  the tangent painted along it, made again when the outline, its place
   *  in the page or the density moves. */
  SkPath outline;
  glm::mat3 toPage{1.0f};
  float density = 0;
  std::optional<UnitShape> unit;
  UnitSurfaces units;
};

/** The shoulder a bevel's face was last shaded from, and what it was cut
 *  from: the outline, the bevel's width and height, and the density. */
struct BevelFaceCache {
  SkPath outline;
  float size = 0;
  float depth = 0;
  float density = 0;
  material::Texture shoulder;
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

/** An outline's contours parted by whether they close. Only a closed
 *  contour has an inside, so only a closed contour has sides: the region
 *  a fill's implicit close would give an open one is a chord across empty
 *  space, not the inside of anything drawn. */
struct ContoursBySide {
  SkPath closed;
  SkPath open;
};

ContoursBySide contoursBySide(const SkPath& outline) {
  SkPathBuilder closed(outline.getFillType());
  SkPathBuilder open;
  SkPathBuilder contour;
  bool closes = false;
  const auto finish = [&] {
    if (contour.isEmpty()) return;
    (closes ? closed : open).addPath(contour.detach());
    contour = SkPathBuilder();
  };
  SkPath::Iter iter(outline, false);
  SkPoint points[4];
  for (SkPath::Verb verb; (verb = iter.next(points)) != SkPath::kDone_Verb;) {
    switch (verb) {
      case SkPath::kMove_Verb:
        finish();
        closes = iter.isClosedContour();
        contour.moveTo(points[0]);
        break;
      case SkPath::kLine_Verb:
        contour.lineTo(points[1]);
        break;
      case SkPath::kQuad_Verb:
        contour.quadTo(points[1], points[2]);
        break;
      case SkPath::kConic_Verb:
        contour.conicTo(points[1], points[2], iter.conicWeight());
        break;
      case SkPath::kCubic_Verb:
        contour.cubicTo(points[1], points[2], points[3]);
        break;
      case SkPath::kClose_Verb:
        contour.close();
        break;
      default:
        break;
    }
  }
  finish();
  return {closed.detach(), open.detach()};
}

/** What a keyline's side is read against: the silhouette when the node
 *  carries one, otherwise the closed contours of its outline. */
SkPath insideOf(const PaintContext& context, const SkPath& closed) {
  return context.silhouette.empty() ? closed
                                    : geometry::path::toSk(context.silhouette);
}

/** Strokes @p outline with @p paint on the side @p position names. An open
 *  contour has no side, so it is stroked centred at the stated width. */
void strokeSide(SkCanvas& canvas, const PaintContext& context, SkPaint paint,
                float width, material::StrokePosition position) {
  paint.setStyle(SkPaint::kStroke_Style);
  const ContoursBySide contours =
      contoursBySide(geometry::path::toSk(context.outline));
  if (!contours.open.isEmpty()) {
    SkPaint centred = paint;
    centred.setStrokeWidth(width);
    canvas.drawPath(contours.open, centred);
  }
  if (contours.closed.isEmpty()) return;
  canvas.save();
  // Inside or outside: clip to that side and stroke at double width, so
  // the half that is kept is the stated width.
  switch (position) {
    case material::StrokePosition::Inside:
      canvas.clipPath(insideOf(context, contours.closed), true);
      paint.setStrokeWidth(width * 2);
      break;
    case material::StrokePosition::Outside:
      canvas.clipPath(insideOf(context, contours.closed), SkClipOp::kDifference,
                      true);
      paint.setStrokeWidth(width * 2);
      break;
    case material::StrokePosition::Center:
      paint.setStrokeWidth(width);
      break;
  }
  canvas.drawPath(contours.closed, paint);
  canvas.restore();
}

/** The band a keyline @p width wide on the side @p position names covers
 *  around @p outline, whose inside is @p shape when it is not empty and
 *  otherwise the outline's closed contours. An open contour's band is
 *  centred on it, as `strokeSide` draws it. */
SkPath strokeBand(const SkPath& outline, const SkPath& shape, float width,
                  material::StrokePosition position) {
  const ContoursBySide contours = contoursBySide(outline);
  SkPaint centred;
  centred.setStyle(SkPaint::kStroke_Style);
  centred.setStrokeWidth(width);
  const SkPath openBand =
      skpathutils::FillPathWithPaint(contours.open, centred);
  if (contours.closed.isEmpty()) return openBand;
  SkPaint stroke = centred;
  stroke.setStrokeWidth(
      position == material::StrokePosition::Center ? width : width * 2);
  SkPath band = skpathutils::FillPathWithPaint(contours.closed, stroke);
  if (position != material::StrokePosition::Center) {
    const SkPath& inside = shape.isEmpty() ? contours.closed : shape;
    band =
        Op(band, inside,
           position == material::StrokePosition::Inside ? kIntersect_SkPathOp
                                                        : kDifference_SkPathOp)
            .value_or(band);
  }
  if (openBand.isEmpty()) return band;
  return Op(band, openBand, kUnion_SkPathOp).value_or(band);
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
      if (bevel.shadesFace()) {
        paintBevelFace(pen, context);
        return;
      }
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

void CoverageMark::paintBevelFace(draw::Pen& pen,
                                  const PaintContext& context) const {
  const material::BevelOptions& bevel = effect.bevel;
  const SkPath outline = geometry::path::toSk(context.outline);
  if (outline.isEmpty() || !(bevel.size > 0)) return;
  const float density =
      context.bakeDensity > 0 ? context.bakeDensity : context.contentScale;
  if (!face) face = std::make_shared<BevelFaceCache>();
  if (face->outline != outline || face->size != bevel.size ||
      face->depth != bevel.depth || face->density != density) {
    face->outline = outline;
    face->size = bevel.size;
    face->depth = bevel.depth;
    face->density = density;
    face->shoulder = material::skia::reliefNormals(
        outline, {.shoulder = bevel.size, .depth = bevel.depth / bevel.size},
        density > 0 ? density : 1.0f);
  }
  const sk_sp<SkShader> shader =
      material::skia::bevelFace(face->shoulder, bevel, frameOf(context));
  if (!shader) return;
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setShader(shader);
  pen.canvas()->drawPath(shapeOf(context), paint);
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
  // On a page painted as a surface map the keyline paints that map of its
  // surface; every other draw paints its colours flat.
  if (surfaced && context.surfaceMap) {
    if (!cache || cache->material != *surfaced)
      cache = std::make_shared<MaterialStrokeCache>(*surfaced);
    // A keyline that reads its unit reads the band it covers: the relief
    // cut from it, the brushing along the outline's tangent.
    const material::skia::SurfaceMaps* maps = &cache->prepared;
    if (material::readsUnit(*surfaced)) {
      const SkPath outline = geometry::path::toSk(context.outline);
      const float density =
          context.bakeDensity > 0 ? context.bakeDensity : context.contentScale;
      const glm::mat3& toPage = context.toRoot.matrix;
      if (!cache->unit || cache->outline != outline ||
          cache->toPage != toPage || cache->density != density) {
        UnitShape unit;
        unit.outline =
            strokeBand(outline,
                       context.silhouette.empty()
                           ? SkPath()
                           : geometry::path::toSk(context.silhouette),
                       options.width, options.position);
        unit.density = density;
        if (surfaced->surface()->anisotropyAlongUnit)
          unit.directionMap = tangentAlong(
              outline, unit.outline, options.width * 2 + 2, toPage, density);
        cache->outline = outline;
        cache->toPage = toPage;
        cache->density = density;
        cache->unit = std::move(unit);
      }
      maps = &cache->units.mapsFor(*surfaced, *cache->unit);
    }
    const sk_sp<SkShader> shader = material::skia::shader(
        maps->asMap(*context.surfaceMap), frameOf(context));
    if (!shader) return;
    stroke.setShader(shader);
    markSurfaceMap(stroke);
    strokeSide(canvas, context, std::move(stroke), options.width,
               options.position);
    return;
  }
  if (source.isSolid()) {
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
