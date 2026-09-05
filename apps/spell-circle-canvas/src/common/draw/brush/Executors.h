#pragma once

/** @file
 * The per-tip executors `deposit` dispatches to. Private to the library.
 *
 * The round tips — grain, nib, scatter — describe every mark as a stamp
 * and draw the stroke as one sprite batch; fibres, image and custom tips
 * draw through the pen dab by dab.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPoint.h>
#include <sigildraw/Constants.h>
#include <sigildraw/brush/Dab.h>
#include <sigildraw/brush/Tool.h>

#include <span>
#include <vector>

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
void depositGrain(Pen& pen, const Tool& tool, const Dab& dab,
                  const DabStyle& style, std::vector<Stamp>& stamps);
void depositScatter(Pen& pen, const Tool& tool, const Dab& dab,
                    const DabStyle& style, std::vector<Stamp>& stamps);
/** Draws the stamps as one sprite batch under @p blend, or one ellipse
 *  each where the blend has no sprite form (SUBTRACT) or there is no
 *  canvas. */
void drawStamps(Pen& pen, Constant blend, std::span<const Stamp> stamps);

void depositFibres(Pen& pen, const Tool& tool, std::span<const Dab> dabs);

/** The paint an image tip is stamped with, taken from the pen once per
 *  stroke: its antialiasing and blend, with the pigment applied as a
 *  colour filter per dab. */
[[nodiscard]] SkPaint imageTipPaint(Pen& pen, const Tool& tool);
void depositImage(Pen& pen, const Tool& tool, const SkPaint& base,
                  const DabStyle& style);
void depositCustom(Pen& pen, const Tool& tool, const Dab& dab,
                   const DabStyle& style);

}  // namespace sigil::draw::brush
