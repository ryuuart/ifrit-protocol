#pragma once
// Support for compose_core_test: the kernel — elements, the reconciler,
// layout, paint, transitions, text, the feed and the instanced leaf — and
// the field walks. Only kernel headers: the binary links SigilComposeCore
// alone, which is what proves the kernel stands without its catalogs.

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
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmedia/core/Image.h>
#include <sigilmaterial/skia/Paint.h>
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

#include <sigilgeometry/path/Skia.h>

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

/** Skia rects as the layout's own rectangles, for a scheme a test writes
 *  in Skia's terms. */
inline std::vector<geometry::path::Rect> rectanglesOf(
    const std::vector<SkRect>& rects) {
  std::vector<geometry::path::Rect> out;
  out.reserve(rects.size());
  for (const SkRect& rect : rects) out.push_back(geometry::path::fromSk(rect));
  return out;
}

}  // namespace sigil::compose
