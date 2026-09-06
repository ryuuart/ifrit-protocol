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
#include <sigilgeometry/path/Extremes.h>
#include <sigilgeometry/path/Fit.h>
#include <sigilgeometry/path/Interpolate.h>
#include <sigilgeometry/path/Ops.h>
#include <sigilgeometry/path/Segments.h>
#include <sigilgeometry/path/Tidy.h>
#include <sigilcore/compute/Chance.h>
#include <sigilgeometry/path/Lattice.h>
#include <sigilgeometry/path/Neighbours.h>
#include <sigilgeometry/path/Noise.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Cells.h>
#include <sigilgeometry/path/Hull.h>
#include <sigilgeometry/path/Scatter.h>
#include <sigilgeometry/path/Symmetry.h>
#include <sigilgeometry/path/Trace.h>
#include <sigilgeometry/path/Triangulate.h>
#include <sigilgeometry/path/Pose.h>
#include <sigilgeometry/path/Stride.h>

#include <cmath>
#include <numbers>
#include <optional>
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

// ---------------------------------------------------------------------------
// The uniform grid

/** A cloud of `count` points in a box whose edge grows with the cube root
 *  of the count, so the DENSITY is constant and a query at a fixed radius
 *  sweeps the same number of neighbours whatever the size. That is what
 *  makes the build linear and the query flat across the range. */
std::vector<glm::vec3> box(int count) {
  const float edge = 100.0f * std::cbrt((float)count);
  sigil::core::chance::Stream stream = sigil::core::chance::Stream::pcg(5);
  std::vector<glm::vec3> points;
  points.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    points.push_back(
        {stream.range(0, edge), stream.range(0, edge), stream.range(0, edge)});
  return points;
}

/** BUILDING THE INDEX: two counting passes over the points. */
void BM_NeighboursBuild(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec3> points = box(count);
  for ([[maybe_unused]] auto iteration : state) {
    Neighbours index(points);
    benchmark::DoNotOptimize(index.size());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_NeighboursBuild)
    ->RangeMultiplier(10)
    ->Range(10000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

/** ONE RADIUS QUERY, at a radius holding a handful of points. The whole
 *  claim of the grid is that this does not grow with the count. */
void BM_NeighboursWithin(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec3> points = box(count);
  const Neighbours index(points);
  std::vector<uint32_t> found;
  size_t at = 0;
  for ([[maybe_unused]] auto iteration : state) {
    index.within(points[at++ % points.size()], 200.0f, found);
    benchmark::DoNotOptimize(found.size());
  }
  state.counters["queries/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_NeighboursWithin)
    ->RangeMultiplier(10)
    ->Range(10000, 100000)
    ->Unit(benchmark::kNanosecond);

/** ONE K-NEAREST QUERY: the ring walk plus the partial sort. */
void BM_NeighboursNearestK(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec3> points = box(count);
  const Neighbours index(points);
  size_t at = 0;
  for ([[maybe_unused]] auto iteration : state) {
    benchmark::DoNotOptimize(
        index.nearest(points[at++ % points.size()], 8).size());
  }
  state.counters["queries/s"] =
      benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_NeighboursNearestK)
    ->RangeMultiplier(10)
    ->Range(10000, 100000)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// Filling a shape with points

/** The region every scatter arm fills: a ring with a hole, so the
 *  containment test is the even-odd walk a real outline costs and not a
 *  rect test. */
Region ringRegion() {
  SkPathBuilder builder;
  builder.addPath(rippledRing(64, 400.0f));
  builder.addPath(rippledRing(64, 160.0f));
  return Region::of(builder.detach(), 0.5f);
}

/** INDEPENDENT DRAWS: one containment test per accepted point plus the
 *  rejections the bounding box costs. */
void BM_ScatterRandom(benchmark::State& state) {
  const int count = (int)state.range(0);
  const Region region = ringRegion();
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<glm::vec2> points = sample(region, uniform(count));
    benchmark::DoNotOptimize(points.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_ScatterRandom)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

/** THE POISSON FILL: the same region filled to a separation that yields
 *  about the same count, which is the arm that says what a minimum
 *  distance costs over independence. */
void BM_ScatterPoisson(benchmark::State& state) {
  const int count = (int)state.range(0);
  const Region region = ringRegion();
  const float radius = std::sqrt(region.area() / (float)count) * 0.8f;
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<glm::vec2> points = sample(region, poisson(radius));
    benchmark::DoNotOptimize(points.data());
  }
  state.SetComplexityN(count);
}
BENCHMARK(BM_ScatterPoisson)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

/** THE RELAXATION, which is one index build and one gather per pass. */
void BM_ScatterBlueNoise(benchmark::State& state) {
  const int count = (int)state.range(0);
  const Region region = ringRegion();
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<glm::vec2> points = sample(region, blueNoise(count, 1, 4));
    benchmark::DoNotOptimize(points.data());
  }
  state.SetComplexityN(count);
}
BENCHMARK(BM_ScatterBlueNoise)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

// ---------------------------------------------------------------------------
// The triangulation, its dual and the outline

/** `count` points spread evenly over a square whose side grows with the
 *  square root of the count, so the density is constant and the arms
 *  measure the construction rather than the crowding. */
std::vector<glm::vec2> sheet(int count) {
  const float edge = 20.0f * std::sqrt((float)count);
  return sample(Region::of(SkRect::MakeWH(edge, edge)), uniform(count, 3));
}

/** THE TRIANGULATION. */
void BM_Delaunay(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec2> points = sheet(count);
  for ([[maybe_unused]] auto iteration : state) {
    Triangulation mesh = delaunay(points);
    benchmark::DoNotOptimize(mesh.triangles.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Delaunay)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oNLogN);

/** THE DUAL, over a triangulation already built: one polygon clipped
 *  once per neighbour. */
void BM_Voronoi(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec2> points = sheet(count);
  const Triangulation mesh = delaunay(points);
  const SkRect box = SkRect::MakeWH(20.0f * std::sqrt((float)count),
                                    20.0f * std::sqrt((float)count));
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Polyline> cells = voronoi(mesh, box);
    benchmark::DoNotOptimize(cells.data());
  }
  state.counters["cells/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Voronoi)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);

/** THE CONVEX HULL: a sort and two passes, and no triangulation at all. */
void BM_HullConvex(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec2> points = sheet(count);
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Polyline> rings = hull(points);
    benchmark::DoNotOptimize(rings.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_HullConvex)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oNLogN);

/** THE ALPHA SHAPE, which is the triangulation plus the stitch: what a
 *  bound costs over no bound at all. */
void BM_HullAlpha(benchmark::State& state) {
  const int count = (int)state.range(0);
  const std::vector<glm::vec2> points = sheet(count);
  for ([[maybe_unused]] auto iteration : state) {
    std::vector<Polyline> rings = hull(points, 40.0f);
    benchmark::DoNotOptimize(rings.data());
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_HullAlpha)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oNLogN);

// ---------------------------------------------------------------------------
// Walking, repeating and stepping a field

/** THE STREAMLINE: four field reads per step, so this arm is really what
 *  the field costs, which is the honest thing to measure — the walk
 *  itself is four adds. */
void BM_Streamline(benchmark::State& state) {
  const int steps = (int)state.range(0);
  sigil::core::noise::Field field;
  field.frequency = 0.004f;
  field.octaves = 3;
  const VectorField angled = flow(field, Flow::Angle, 2.0f);
  TraceOptions options;
  options.step = 2.0f;
  options.length = 2.0f * (float)steps;
  float at = 0;
  for ([[maybe_unused]] auto iteration : state) {
    at += 7.0f;
    Polyline line = streamline(angled, {at, at * 0.5f}, options);
    benchmark::DoNotOptimize(line.points.data());
  }
  state.counters["steps/s"] =
      benchmark::Counter(steps, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(steps);
}
BENCHMARK(BM_Streamline)
    ->RangeMultiplier(8)
    ->Range(64, 4096)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** THE COPIES A SYMMETRY MAKES, over a path: the matrices, and the
 *  concatenation of the figure under each of them. */
void BM_SymmetryCopies(benchmark::State& state) {
  const int order = (int)state.range(0);
  Symmetry symmetry;
  symmetry.order = order;
  symmetry.mirror = true;
  const SkPath figure = rippledRing(32, 120.0f);
  for ([[maybe_unused]] auto iteration : state) {
    SkPath all = copies(symmetry, figure);
    benchmark::DoNotOptimize(all.countPoints());
  }
  state.counters["copies/s"] = benchmark::Counter(
      order * 2, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(order);
}
BENCHMARK(BM_SymmetryCopies)
    ->RangeMultiplier(4)
    ->Range(2, 128)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** ONE STEP OF A CELL SHEET, with a nine-cell rule: what the substrate
 *  costs a reaction-diffusion or an automaton per cell per frame. */
void BM_CellsStep(benchmark::State& state) {
  const int edge = (int)state.range(0);
  Cells<float> sheet(edge, edge, 0.0f);
  sheet.setEdge(Edge::Wrap);
  for (int i = 0; i < edge; ++i) sheet.at(i, i) = 1.0f;
  for ([[maybe_unused]] auto iteration : state) {
    sheet.step([](const Cells<float>& from, int x, int y) {
      float total = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) total += from.read(x + dx, y + dy);
      return total * (1.0f / 9.0f);
    });
  }
  const auto cells = (int64_t)edge * (int64_t)edge;
  state.counters["cells/s"] =
      benchmark::Counter((double)cells,
                         benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(cells);
}
BENCHMARK(BM_CellsStep)
    ->RangeMultiplier(4)
    ->Range(64, 1024)
    ->Unit(benchmark::kMillisecond)
    ->Complexity(benchmark::oN);


/** The segment reader and the way back, by node count: every node once
 *  in each direction, and the floor everything below stands on. */
void BM_SegmentsRoundTrip(benchmark::State& state) {
  const int count = (int)state.range(0);
  const SkPath path = rippledRing(count);
  for ([[maybe_unused]] auto iteration : state) {
    SkPath back = toPath(segments(path), path.getFillType());
    benchmark::DoNotOptimize(back);
  }
  state.counters["nodes/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_SegmentsRoundTrip)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** Nodes added where the curve turns: two cubic solves per piece. */
void BM_Extremes(benchmark::State& state) {
  const int count = (int)state.range(0);
  const SkPath path = rippledRing(count);
  for ([[maybe_unused]] auto iteration : state) {
    SkPath split = extremes(path);
    benchmark::DoNotOptimize(split);
  }
  state.counters["nodes/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Extremes)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** Nodes taken away: the passes repeat while anything moves, so the
 *  worst case is a straight run cut into every node there is. */
void BM_Tidy(benchmark::State& state) {
  const int count = (int)state.range(0);
  SkPathBuilder builder;
  builder.moveTo(0, 0);
  for (int i = 1; i <= count; ++i) builder.lineTo((float)i, 0);
  const SkPath path = builder.detach();
  for ([[maybe_unused]] auto iteration : state) {
    SkPath tidied = tidy(path);
    benchmark::DoNotOptimize(tidied);
  }
  state.counters["nodes/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Tidy)
    ->RangeMultiplier(4)
    ->Range(16, 512)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity();

/** The least-squares fit by point count: every pass is the whole run,
 *  and a split is two of them. */
void BM_FitCurve(benchmark::State& state) {
  const int count = (int)state.range(0);
  std::vector<glm::vec2> wave;
  wave.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    wave.push_back({(float)i, 30.0f * std::sin((float)i * 0.08f)});
  for ([[maybe_unused]] auto iteration : state) {
    SkPath fitted = fitCurve(wave, 0.5f);
    benchmark::DoNotOptimize(fitted);
  }
  state.counters["points/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_FitCurve)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity();

/** The exact in-between: the compatibility check and one weighted
 *  average of two point arrays. */
void BM_Interpolate(benchmark::State& state) {
  const int count = (int)state.range(0);
  const SkPath a = rippledRing(count, 200.0f);
  const SkPath b = rippledRing(count, 240.0f);
  for ([[maybe_unused]] auto iteration : state) {
    std::optional<SkPath> half = interpolate(a, b, 0.5f);
    benchmark::DoNotOptimize(half);
  }
  state.counters["nodes/s"] =
      benchmark::Counter(count, benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN(count);
}
BENCHMARK(BM_Interpolate)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** The offset at either end of its position dial: the stroker where the
 *  band straddles, the contour walk where it does not. */
void BM_Offset(benchmark::State& state) {
  const SkPath path = rippledRing(64);
  const float position = (float)state.range(0) / 100.0f;
  for ([[maybe_unused]] auto iteration : state) {
    SkPath grown = ops::offset(path, 8.0f, {.position = position});
    benchmark::DoNotOptimize(grown);
  }
}
BENCHMARK(BM_Offset)->Arg(0)->Arg(50)->Unit(benchmark::kMicrosecond);

}  // namespace
