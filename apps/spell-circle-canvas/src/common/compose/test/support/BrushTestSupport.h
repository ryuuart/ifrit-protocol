#pragma once
// Stock decorations and material fixtures used by the brush cases.
// Tests import the kit and typography vocabulary they use explicitly.

#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Presets.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Paint.h>
#ifdef SIGILMATERIAL_ENABLE_OCIO
#include <sigilmaterial/ocio/Ocio.h>
#endif

#include "ShapeTestSupport.h"

namespace {

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
