#pragma once
// Support for compose_brush_test: lines, rails, hatches and brushes.

#include <include/core/SkColorFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/Brushes.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Layouts.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/TextFx.h>
#include <sigilcompose/Util.h>
#include <sigilcompose/kit/Strokes.h>

#include <algorithm>
#include <cmath>

#include "Effects.h"
#include "Host.h"
#include "Profile.h"
#include "Strokes.h"

namespace {

/** Counts distinct painted runs in a vertical scan column. */
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
