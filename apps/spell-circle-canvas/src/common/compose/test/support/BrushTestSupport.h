#pragma once
// Support for compose_brush_test: decorations, lines, rails, hatches and
// brushes, the kit's stroke grammar and the bordered feed plate, over the
// shape support. The Brush tier carries the typography headers with it, so
// text-fx presets are in reach here too.

#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Typography.h>

#include "ShapeTestSupport.h"

namespace {

/** A subtree the promoter will actually promote.
 *
 *  "Expensive" has to mean over the promotion time threshold. Child count
 *  alone does not get there — hundreds of thin hairline-stroked boxes are
 *  still far under the bar — so the panel carries a per-pixel shader across
 *  its whole area as well as its children. Drop the shader and every
 *  assertion about this node being promoted quietly becomes an assertion
 *  about a node that never could be. */
Element expensivePanel() {
  Element panel =
      box().width(180).height(180).fill(Material::sksl(heavyEffect(false)));
  for (int i = 0; i < 220; ++i) {
    const float t = (float)i / 220.0f;
    panel.child(box()
                    .absolute()
                    .left(4 + t * 170)
                    .top(2)
                    .width(2)
                    .height(176)
                    .fill(i % 2 ? green() : red())
                    .foreground(stroke(0.7f, Fill::color({1, 1, 1, 0.5f}))));
  }
  return panel;
}

/** How many separate runs of `color` a vertical scan crosses. */
int verticalRuns(Host& host, int x, int y0, int y1, SkColor color) {
  int runs = 0;
  bool in = false;
  for (int y = y0; y <= y1; ++y) {
    const bool hit = host.pixel(x, y) == color;
    if (hit && !in) ++runs;
    in = hit;
  }
  return runs;
}

}  // namespace
