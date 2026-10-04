#pragma once
// Shared fixtures for element, layout, paint and retained-runtime cases.
// These headers include the stock values the cases use. The library's
// aggregate test binary does not prove an individual feature's link closure.

#include <include/core/SkColorFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkStream.h>
#include <include/core/SkString.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/effects/SkTrimPathEffect.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/kit/Feed.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmedia/core/Image.h>
#include <sigilweave/choreograph/Choreograph.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <type_traits>

#include "Atlas.h"
#include "Effects.h"
#include "Host.h"
#include "Profile.h"
#include "Strokes.h"

namespace sigil::compose {

/** Placed rectangles read back as Skia rects, which a test compares with
 *  `SkRect::MakeXYWH` and prints on a failure. */
struct SkiaRects : std::vector<SkRect> {
  SkiaRects(const std::vector<geometry::path::Rect>& rects) {  // NOLINT
    reserve(rects.size());
    for (const geometry::path::Rect& rect : rects)
      push_back(geometry::path::toSk(rect));
  }
};

}  // namespace sigil::compose
