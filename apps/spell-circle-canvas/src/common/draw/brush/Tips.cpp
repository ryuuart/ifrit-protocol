/** @file
 * The image tip and the custom tip: one stamp per dab through the canvas.
 */

#include "DabStyle.h"
#include "Executors.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>

#include <utility>

namespace sigil::draw::brush {

SkPaint imageTipPaint(Pen& pen, const Tool& tool) {
  pen.stroke(tool.color);
  SkPaint paint;
  if (const SkPaint* stroke = pen.strokePaint()) paint = *stroke;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAlphaf(1.0f);
  return paint;
}

void depositImage(Pen& pen, const Tool& tool, const SkPaint& base,
                  const DabStyle& style) {
  SkCanvas* canvas = pen.canvas();
  if (!tool.imageTip || !canvas) return;
  sk_sp<SkColorFilter> tint = SkColorFilters::Blend(
      pigment(tool, style.opacity), nullptr, SkBlendMode::kSrcIn);
  if (tool.imageMask == ImageMask::InvertedLuminance) {
    constexpr float invertedLuminance[20] = {
        0, 0, 0, 0, 1, 0,        0,        0,        0, 1,
        0, 0, 0, 0, 1, -0.2126f, -0.7152f, -0.0722f, 1, 0,
    };
    tint = SkColorFilters::Compose(tint,
                                   SkColorFilters::Matrix(invertedLuminance));
  }
  SkPaint paint = base;
  paint.setColorFilter(std::move(tint));
  SkAutoCanvasRestore restore(canvas, true);
  canvas->translate(style.position.fX, style.position.fY);
  canvas->rotate(degrees(style.angle));
  const float halfWidth = style.size * 0.5f;
  const float halfHeight = style.size * style.aspect * 0.5f;
  canvas->drawImageRect(
      tool.imageTip,
      SkRect::MakeLTRB(-halfWidth, -halfHeight, halfWidth, halfHeight),
      SkSamplingOptions(SkFilterMode::kLinear), &paint);
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
