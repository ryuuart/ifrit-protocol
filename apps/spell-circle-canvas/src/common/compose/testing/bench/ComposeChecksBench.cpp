// What a verification pass costs. The two read-back checks are the
// largest single cost in a study's setup — a sketch that audits its own
// geometry pays this on every open — and both are asked about the same
// shape: a band of hundreds of quadrilaterals, one per sampled step of a
// spine, all in one path.
//
// The arms are the two questions a study asks and the two terms each
// grows in: the lattice a coverage scan samples on, and the number of
// steps the band is built from. A check that reads the whole figure per
// sample is quadratic in the step count here; one that indexes it first
// is not, and these arms are where that shows.

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilgeometry/path/Profile.h>

#include <vector>

#include "BenchSupport.h"

namespace {

using sigil::compose::test::coverage;
using sigil::compose::test::widthAlong;
namespace geometry = sigil::geometry;

struct Width {
  float px = 0.0f;
  float across(float) const { return px; }
  float max() const { return px; }
  bool operator==(const Width&) const = default;
};

SkPath sCurve() {
  SkPathBuilder b;
  b.moveTo(80, 300);
  b.cubicTo(300, 190, 660, 410, 900, 300);
  return b.detach();
}

/** One quadrilateral per sampled step, consecutive steps sharing an edge,
 *  all in one path — the band a stroke grammar hands back. */
SkPath bandAlong(const SkPath& spine, float width, int steps) {
  SkPathBuilder b;
  SkContourMeasureIter it(spine, false);
  sk_sp<SkContourMeasure> contour = it.next();
  if (!contour || steps < 1) return b.detach();
  const float len = contour->length();
  const float half = width * 0.5f;
  SkPoint prevLeft{0, 0}, prevRight{0, 0};
  bool have = false;
  for (int i = 0; i <= steps; ++i) {
    SkPoint p;
    SkVector tangent;
    if (!contour->getPosTan(len * (float)i / (float)steps, &p, &tangent))
      continue;
    const SkPoint left{p.x() - tangent.y() * half, p.y() + tangent.x() * half};
    const SkPoint right{p.x() + tangent.y() * half, p.y() - tangent.x() * half};
    if (have) {
      b.moveTo(prevLeft);
      b.lineTo(left);
      b.lineTo(right);
      b.lineTo(prevRight);
      b.close();
    }
    prevLeft = left;
    prevRight = right;
    have = true;
  }
  return b.detach();
}

/** The region a study tests a band inside: the band's own bounds, taken
 *  as a path rather than a rect, so the arm prices the region gate too. */
SkPath boundsAsRegion(const SkPath& band) {
  SkPathBuilder b;
  b.addRect(band.getBounds());
  return b.detach();
}

void BM_Check_Coverage_Band(benchmark::State& state) {
  const SkPath band = bandAlong(sCurve(), 40.0f, 719);
  const std::vector<SkPath> pieces{band};
  const SkRect box = band.getBounds();
  const int grid = (int)state.range(0);
  for (auto _ : state) benchmark::DoNotOptimize(coverage(pieces, box, grid));
  state.counters["samples"] = (double)grid * grid;
  state.counters["quads"] = 719;
}
BENCHMARK(BM_Check_Coverage_Band)->Arg(128)->Arg(384)->Arg(512);

void BM_Check_Coverage_Region(benchmark::State& state) {
  const SkPath band = bandAlong(sCurve(), 40.0f, 719);
  const std::vector<SkPath> pieces{band};
  const SkPath region = boundsAsRegion(band);
  const int grid = (int)state.range(0);
  for (auto _ : state) benchmark::DoNotOptimize(coverage(pieces, region, grid));
  state.counters["samples"] = (double)grid * grid;
}
BENCHMARK(BM_Check_Coverage_Region)->Arg(384);

void BM_Check_Coverage_Steps(benchmark::State& state) {
  // The term the check has to be linear in: a band's step count. Four
  // times the steps is four times the edges and must not be sixteen
  // times the cost.
  const SkPath band = bandAlong(sCurve(), 40.0f, (int)state.range(0));
  const std::vector<SkPath> pieces{band};
  const SkRect box = band.getBounds();
  for (auto _ : state) benchmark::DoNotOptimize(coverage(pieces, box, 256));
  state.counters["quads"] = (double)state.range(0);
}
BENCHMARK(BM_Check_Coverage_Steps)->Arg(180)->Arg(719)->Arg(2876);

void BM_Check_WidthAlong_Band(benchmark::State& state) {
  const geometry::path::Profile flat{Width{40.0f}};
  const SkPath spine = sCurve();
  const SkPath band = bandAlong(spine, 40.0f, (int)state.range(0));
  for (auto _ : state)
    benchmark::DoNotOptimize(widthAlong(band, spine, flat, 4.0f, 90));
  state.counters["quads"] = (double)state.range(0);
}
BENCHMARK(BM_Check_WidthAlong_Band)->Arg(180)->Arg(719)->Arg(2876);

}  // namespace
