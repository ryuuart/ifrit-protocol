/** @file
 * The shape tip and the custom tip: one stamp per dab through the canvas.
 */

#include "DabStyle.h"
#include "Executors.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>

#include <algorithm>
#include <utility>

namespace sigil::draw::brush {

namespace {

/** The artwork's coverage: its own alpha, or one minus its luminance for
 *  the dark-mark-on-white artwork a tip is usually drawn as. */
sk_sp<SkColorFilter> shapeCoverage(ImageMask mask) {
  if (mask == ImageMask::Alpha) return nullptr;
  constexpr float invertedLuminance[20] = {
      0, 0, 0, 0, 1, 0,        0,        0,        0, 1,
      0, 0, 0, 0, 1, -0.2126f, -0.7152f, -0.0722f, 1, 0,
  };
  return SkColorFilters::Matrix(invertedLuminance);
}

/** Where the artwork's pixels land: its top-left pixel to the top-left
 *  corner of a stamp of the style's size, turned about the stamp's
 *  centre and set down at the dab. */
SkMatrix stampPlacement(const SkImage& image, const DabStyle& style) {
  const float width = std::max(0.01f, style.size);
  const float height = std::max(0.01f, style.size * style.aspect);
  SkMatrix placement = SkMatrix::Translate(style.position.fX,
                                           style.position.fY);
  placement.preRotate(degrees(style.angle));
  placement.preTranslate(-width * 0.5f, -height * 0.5f);
  placement.preScale(width / (float)std::max(1, image.width()),
                     height / (float)std::max(1, image.height()));
  return placement;
}

}  // namespace

SkPaint shapeTipPaint(Pen& pen, const Tool& tool) {
  pen.stroke(tool.color);
  SkPaint paint;
  if (const SkPaint* stroke = pen.strokePaint()) paint = *stroke;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAlphaf(1.0f);
  return paint;
}

void depositShape(Pen& pen, const Tool& tool, const SkPaint& base,
                  const DabStyle& style) {
  SkCanvas* canvas = pen.canvas();
  if (!tool.shape || !tool.shape->image || !canvas) return;
  const SkImage& artwork = *tool.shape->image;
  const SkMatrix placement = stampPlacement(artwork, style);

  // The stamp is a shader rather than a drawn image, because that is
  // what lets the grain multiply into its coverage in one draw; the
  // rect is the artwork's own bounds mapped out, and the decal tiling
  // keeps everything outside them empty.
  sk_sp<SkShader> shape = tool.shape->image->makeShader(
      SkTileMode::kDecal, SkTileMode::kDecal,
      SkSamplingOptions(SkFilterMode::kLinear), &placement);
  if (!shape) return;
  if (sk_sp<SkColorFilter> mask = shapeCoverage(tool.shape->mask))
    shape = shape->makeWithColorFilter(std::move(mask));

  sk_sp<SkShader> mark = SkShaders::Blend(
      SkBlendMode::kDstIn,
      SkShaders::Color(pigment(tool, style.opacity), nullptr),
      std::move(shape));
  if (tool.grain && tool.grain->space == GrainSpace::Dab) {
    // The grain travels and turns with the stamp but keeps its size in
    // the pen's space, so one scale means the same thing whichever space
    // the grain stands in.
    SkMatrix carried = SkMatrix::Translate(style.position.fX,
                                           style.position.fY);
    carried.preRotate(degrees(style.angle));
    mark = throughGrain(std::move(mark), *tool.grain, carried);
  }

  SkPaint paint = base;
  paint.setShader(std::move(mark));
  paint.setAntiAlias(true);
  canvas->drawRect(
      placement.mapRect(SkRect::MakeIWH(artwork.width(), artwork.height())),
      paint);
}

void depositCustom(Pen& pen, const Tool& tool, const Dab& dab,
                   const DabStyle& style) {
  SkCanvas* canvas = pen.canvas();
  if (!tool.customTip || !canvas) return;
  // The transform is the dab's and is restored after it; the style is
  // set to the tip's contract — the pigment as fill and stroke, the
  // default modes — rather than saved and restored, since a full style
  // copy per dab is what a stroke of a thousand dabs cannot afford.
  const SkColor4f color = pigment(tool, style.opacity);
  pen.fill(color);
  pen.stroke(color);
  pen.rectMode(CORNER);
  pen.ellipseMode(CENTER);
  SkAutoCanvasRestore restore(canvas, true);
  canvas->translate(style.position.fX, style.position.fY);
  canvas->rotate(degrees(style.angle));
  canvas->scale(style.size, style.size * style.aspect);
  tool.customTip(pen, dab);
}

}  // namespace sigil::draw::brush
