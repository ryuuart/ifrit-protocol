#pragma once

/** @file
 * The per-tip executors `deposit` dispatches to. Private to the library.
 *
 * The round tips — dust, nib, scatter — describe every mark as a stamp
 * and draw the stroke as one sprite batch; fibres, shape and custom tips
 * draw through the pen dab by dab. The grain stands beside all of them:
 * a texture that takes coverage away, riding one stamp or standing still
 * over the whole deposit.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <sigildraw/Constants.h>
#include <sigildraw/brush/Dab.h>
#include <sigildraw/brush/Grain.h>
#include <sigildraw/brush/Tool.h>

#include <span>
#include <vector>

class SkMatrix;
class SkShader;

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

struct DabStyle;

/** One round mark: an ellipse of the width and height at the angle. */
struct Stamp {
  SkPoint position;
  float width;
  float height;
  float angle;
  SkColor color;
};

void depositNib(const Tool& tool, const DabStyle& style,
                std::vector<Stamp>& stamps);
void depositDust(Pen& pen, const Tool& tool, const Dab& dab,
                 const DabStyle& style, std::vector<Stamp>& stamps);
void depositScatter(Pen& pen, const Tool& tool, const Dab& dab,
                    const DabStyle& style, std::vector<Stamp>& stamps);
/** Draws the stamps as one sprite batch under @p blend, or one ellipse
 *  each where the blend has no sprite form (SUBTRACT) or there is no
 *  canvas. */
void drawStamps(Pen& pen, Constant blend, std::span<const Stamp> stamps);

void depositFibres(Pen& pen, const Tool& tool, std::span<const Dab> dabs);

/** The paint a shape tip is stamped with, taken from the pen once per
 *  stroke: its antialiasing and blend, with the pigment and the artwork
 *  arriving as the paint's shader per dab. */
[[nodiscard]] SkPaint shapeTipPaint(Pen& pen, const Tool& tool);
void depositShape(Pen& pen, const Tool& tool, const SkPaint& base,
                  const DabStyle& style);
void depositCustom(Pen& pen, const Tool& tool, const Dab& dab,
                   const DabStyle& style);

/** The grain as coverage: the texture tiled through @p local, its
 *  luminance read as how much of a mark survives. Null when the grain
 *  carries no image. */
[[nodiscard]] sk_sp<SkShader> grainShader(const Grain& grain,
                                          const SkMatrix& local);
/** @p mark with @p grain taken out of its coverage; @p mark unchanged
 *  when the grain carries no image. */
[[nodiscard]] sk_sp<SkShader> throughGrain(sk_sp<SkShader> mark,
                                           const Grain& grain,
                                           const SkMatrix& local);
/** Takes @p grain out of the coverage of everything already drawn into
 *  the pen's current layer, in the pen's space. */
void layerGrain(Pen& pen, const Grain& grain);

}  // namespace sigil::draw::brush
