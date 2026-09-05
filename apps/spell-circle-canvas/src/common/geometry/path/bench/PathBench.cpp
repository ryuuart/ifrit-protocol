/** @file
 * Benchmarks of the path leaf: flattening and resampling by point count,
 * corner detection and the parallel and displaced constructions by
 * contour length, resampling by spacing (the walk that lays a mark every
 * so many pixels, and the smooth curve through a chain of controls), the
 * scanline lattice by the area it fills, and the noise hashes per call.
 */

// geometry_path_bench — the path leaf under load: flattening and
// resampling by point count, corner detection and the parallel and
// displaced constructions by contour length (the outline scaled up, its
// shape and curvature per unit length kept), and the hash every seeded
// jitter starts from. Run a Release build; Debug numbers say nothing.

#include <benchmark/benchmark.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Lattice.h>
#include <sigilgeometry/path/Noise.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Pose.h>
#include <sigilgeometry/path/Stride.h>

#include <cmath>
#include <numbers>
#include <vector>

using namespace sigil::geometry::path;

namespace {

/** A closed ring of `segments` cubic arcs with a gentle radial ripple, so
 *  every segment is a real curve the flattener has to subdivide. */
SkPath rippledRing(int segments, float radius = 200.0f) {
  SkPathBuilder builder;
  const float step = 2.0f * std::numbers::pi_v<float> / (float)segments;
  auto point = [&](float a) {
    const float r = radius * (1.0f + 0.15f * std::sin(6.0f * a));
    return SkPoint{r * std::cos(a), r * std::sin(a)};
  };
  builder.moveTo(point(0));
  for (int i = 0; i < segments; ++i) {
    const float a0 = step * (float)i, a1 = step * (float)(i + 1);
    const SkPoint p0 = point(a0), p3 = point(a1);
    const float tangent = radius * step / 3.0f;
    const SkPoint p1 = {p0.fX - tangent * std::sin(a0),
                        p0.fY + tangent * std::cos(a0)};
    const SkPoint p2 = {p3.fX + tangent * std::sin(a1),
                        p3.fY - tangent * std::cos(a1)};
    builder.cubicTo(p1, p2, p3);
  }
  builder.close();
  return builder.detach();
}

/** A closed sawtooth polygon of `teeth` sharp corners, its radius grown
 *  with the count so the teeth stay the same size: every vertex is a
 *  corner and the corner density per unit length is constant, so the
 *  walk's cost should grow with the length alone. */
SkPath sawtooth(int teeth) {
  const float radius = 4.0f * (float)teeth;
  SkPathBuilder builder;
  const float step = 2.0f * std::numbers::pi_v<float> / (float)teeth;
  for (int i = 0; i < teeth; ++i) {
    const float a = step * (float)i;
    const float r = (i % 2 == 1) ? radius : radius * 0.8f;
    const SkPoint p = {r * std::cos(a), r * std::sin(a)};
    if (i == 0)
      builder.moveTo(p);
    else
      builder.lineTo(p);
  }
  builder.close();
  return builder.detach();
}

float pathLength(const SkPath& path) {
  float total = 0;
  for (const Contour& contour : Contour::of(path)) total += contour.length();
  return total;
}

void BM_Flatten(benchmark::State& state) {
  const SkPath path = rippledRing((int)state.range(0));
  size_t points = 0;
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Polyline> lines = flatten(path, 0.25f);
    points = lines.front().points.size();
    benchmark::DoNotOptimize(lines.data());
  }
  state.counters["points/s"] = benchmark::Counter(
      (double)points, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN((int64_t)points);
}
BENCHMARK(BM_Flatten)
    ->RangeMultiplier(4)
    ->Range(16, 4096)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Resample(benchmark::State& state) {
  const int count = (int)state.range(0);
  const Polyline line = flatten(rippledRing(64), 0.1f).front();
  for ([[maybe_unused]] auto iteration : state) {
    Sampled sampled = resample(line, count);
    benchmark::DoNotOptimize(sampled.points.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Resample)
    ->RangeMultiplier(4)
    ->Range(64, 16384)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** THE WALK A STROKE IS LAID DOWN BY: a centreline resampled so no step
 *  is longer than the spacing, which is one mark per step for anything
 *  that stamps along a curve. Measured per source vertex. */
void BM_Subdivide(benchmark::State& state) {
  const int count = (int)state.range(0);
  const Sampled even = resample(flatten(rippledRing(64), 0.5f).front(), count);
  Polyline line;
  line.points = even.points;
  line.closed = true;
  line.lane.assign(line.points.size(), 0.5f);
  for ([[maybe_unused]] auto iteration : state) {
    Polyline cut = subdivide(line, 2.0f);
    benchmark::DoNotOptimize(cut.points.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Subdivide)
    ->RangeMultiplier(4)
    ->Range(64, 4096)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** The smooth centreline a chain of placed controls is read as. */
void BM_CatmullRom(benchmark::State& state) {
  const int count = (int)state.range(0);
  Polyline controls;
  controls.points.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    const float t = (float)i;
    controls.points.push_back({t * 7.0f, 60.0f * std::sin(t * 0.4f)});
  }
  controls.lane.assign(controls.points.size(), 0.75f);
  for ([[maybe_unused]] auto iteration : state) {
    Polyline curve = catmullRom(controls, 1.0f, 0.5f);
    benchmark::DoNotOptimize(curve.points.data());
  }
  state.counters["controls/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_CatmullRom)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** The walk fed one piece at a time, which is what a device reporting
 *  input costs per event. */
void BM_Stride(benchmark::State& state) {
  for ([[maybe_unused]] auto iteration : state) {
    Stride stride;
    float last = 0;
    for (int piece = 0; piece < 128; ++piece)
      stride.advance(4.0f, 0.8f,
                     [&](Stride::Step step) { last = step.distance; });
    benchmark::DoNotOptimize(last);
  }
  state.counters["pieces/s"] =
      benchmark::Counter(128, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_Stride);

/** The scanline fill, measured by the number of lines it lays: the
 *  crossing pass is every edge once per line. */
void BM_Lattice(benchmark::State& state) {
  const int lines = (int)state.range(0);
  const std::vector<Polyline> rings = {flatten(rippledRing(64), 0.5f).front()};
  const float height = rings.front().bounds().height();
  const LatticeOptions options{.spacing = height / (float)lines, .angle = 0.4f};
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<LatticeMark> marks = lattice(rings, options);
    benchmark::DoNotOptimize(marks.data());
  }
  state.counters["lines/s"] =
      benchmark::Counter(lines, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(lines);
}
BENCHMARK(BM_Lattice)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_ContourCorners(benchmark::State& state) {
  const SkPath path = sawtooth((int)state.range(0));
  const std::vector<Contour> contours = Contour::of(path);
  const float length = pathLength(path);
  size_t found = 0;
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Contour::Corner> corners = contours.front().corners(30.0f);
    found = corners.size();
    benchmark::DoNotOptimize(corners.data());
  }
  state.counters["px/s"] =
      benchmark::Counter(length, benchmark::Counter::kIsIterationInvariantRate);
  state.counters["corners"] = (double)found;
  state.SetComplexityN((int64_t)length);
}
BENCHMARK(BM_ContourCorners)
    ->RangeMultiplier(4)
    ->Range(8, 512)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Parallel(benchmark::State& state) {
  const SkPath path = rippledRing(64, (float)state.range(0));
  const float length = pathLength(path);
  for ([[maybe_unused]] auto iteration : state) {
    SkPath offset = parallel(path, 12.0f);
    benchmark::DoNotOptimize(offset);
  }
  state.counters["px/s"] =
      benchmark::Counter(length, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN((int64_t)length);
}
BENCHMARK(BM_Parallel)
    ->ArgName("radius")
    ->RangeMultiplier(4)
    ->Range(50, 3200)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Displace(benchmark::State& state) {
  const SkPath path = rippledRing(64, (float)state.range(0));
  const float length = pathLength(path);
  const bool zigzag = state.range(1) != 0;
  for ([[maybe_unused]] auto iteration : state) {
    SkPath wave = displace(path, 6.0f, 24.0f, zigzag);
    benchmark::DoNotOptimize(wave);
  }
  state.counters["px/s"] =
      benchmark::Counter(length, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_Displace)
    ->ArgsProduct({{50, 800, 3200}, {0, 1}})
    ->ArgNames({"radius", "zigzag"})
    ->Unit(benchmark::kMicrosecond);

void BM_NoiseHash(benchmark::State& state) {
  uint32_t i = 0;
  float sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += sigil::core::noise::hash(7u, i++);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["calls/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_NoiseHash);

void BM_NoisePcgHash(benchmark::State& state) {
  uint32_t i = 0, sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink ^= sigil::core::noise::pcgHash(i++);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["calls/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_NoisePcgHash);

void BM_NoiseValue3(benchmark::State& state) {
  float t = 0, sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    t += 0.37f;
    sink += valueNoise({t, t * 0.5f, -t}, 11u);
    benchmark::DoNotOptimize(sink);
  }
  state.counters["calls/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_NoiseValue3);

/** The read a motion path makes every frame: one pose at a fraction of
 *  the total arc length, over a path already measured into contours. */
void BM_PoseAlong(benchmark::State& state) {
  const std::vector<Contour> contours = Contour::of(rippledRing(64));
  const float total = totalLength(contours);
  float u = 0;
  for ([[maybe_unused]] auto iteration : state) {
    u += 0.013f;
    if (u > 1.0f) u -= 1.0f;
    benchmark::DoNotOptimize(poseAlong(contours, u * total).position);
  }
  state.counters["poses/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_PoseAlong);

/** The same read over a path cut into several contours, which is what
 *  the walk across them costs. */
void BM_PoseAlongManyContours(benchmark::State& state) {
  SkPathBuilder builder;
  for (int i = 0; i < 8; ++i)
    builder.addPath(rippledRing(16, 40.0f + 20.0f * (float)i));
  const std::vector<Contour> contours = Contour::of(builder.detach());
  const float total = totalLength(contours);
  float u = 0;
  for ([[maybe_unused]] auto iteration : state) {
    u += 0.013f;
    if (u > 1.0f) u -= 1.0f;
    benchmark::DoNotOptimize(poseAlong(contours, u * total).position);
  }
  state.counters["poses/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_PoseAlongManyContours);

}  // namespace
