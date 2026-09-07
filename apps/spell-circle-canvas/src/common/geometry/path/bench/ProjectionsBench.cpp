/** @file
 * Each projection scheme by the one read a chart makes per star — the way
 * out and the way home — with the two costs beside them: the closed-form
 * image of a circle on the sphere, which is what a whole family of them is
 * struck from, and the turn of the sphere a catalogue is carried through
 * before any of it is drawn.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/path/Projection.h>

#include <vector>

using sigil::geometry::path::PlaneCircle;
using sigil::geometry::path::Projection;
using sigil::geometry::path::Rotation;
using sigil::geometry::path::Scheme;
using sigil::geometry::path::Spherical;

namespace {

/** A patch of sky, walked in the order a catalogue arrives in. */
std::vector<Spherical> sky(int count) {
  std::vector<Spherical> out;
  out.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    out.push_back({(float)i * 137.508f, -60.0f + (float)(i % 121)});
  return out;
}

Projection of(Scheme scheme) {
  return {
      .scheme = scheme, .centre = {.lonDeg = 20, .latDeg = 35}, .scale = 240};
}

void BM_ProjectionAt(benchmark::State& state) {
  const Projection map = of((Scheme)state.range(0));
  const std::vector<Spherical> stars = sky(1024);
  for ([[maybe_unused]] auto iteration : state)
    for (const Spherical& star : stars) benchmark::DoNotOptimize(map.at(star));
  state.counters["stars/s"] = benchmark::Counter(
      (double)stars.size(), benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_ProjectionAt)
    ->ArgName("scheme")
    ->Arg((int)Scheme::Stereographic)
    ->Arg((int)Scheme::Orthographic)
    ->Arg((int)Scheme::AzimuthalEquidistant)
    ->Arg((int)Scheme::Equirectangular)
    ->Arg((int)Scheme::Mercator)
    ->Unit(benchmark::kMicrosecond);

void BM_ProjectionFrom(benchmark::State& state) {
  const Projection map = of((Scheme)state.range(0));
  std::vector<glm::vec2> plane;
  for (const Spherical& star : sky(1024)) plane.push_back(map.at(star));
  for ([[maybe_unused]] auto iteration : state)
    for (const glm::vec2& p : plane) benchmark::DoNotOptimize(map.from(p));
  state.counters["points/s"] = benchmark::Counter(
      (double)plane.size(), benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_ProjectionFrom)
    ->ArgName("scheme")
    ->Arg((int)Scheme::Stereographic)
    ->Arg((int)Scheme::Orthographic)
    ->Arg((int)Scheme::AzimuthalEquidistant)
    ->Arg((int)Scheme::Equirectangular)
    ->Arg((int)Scheme::Mercator)
    ->Unit(benchmark::kMicrosecond);

/** The image of a circle on the sphere: what an instrument covered in
 *  concentric families of them is drawn by, one closed form per ring. */
void BM_ProjectionCircleOf(benchmark::State& state) {
  const Projection plate{
      .scheme = Scheme::Stereographic, .centre = {.latDeg = 90}, .scale = 235};
  const Spherical zenith{.lonDeg = 90, .latDeg = 51.83f};
  for ([[maybe_unused]] auto iteration : state)
    for (int h = 0; h < 90; ++h)
      benchmark::DoNotOptimize(plate.circleOf(zenith, 90.0f - (float)h));
  state.counters["circles/s"] =
      benchmark::Counter(90.0, benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_ProjectionCircleOf)->Unit(benchmark::kMicrosecond);

/** A catalogue turned from one epoch to another — the read that stands in
 *  front of every one above when a chart steps through time. */
void BM_RotationOfSky(benchmark::State& state) {
  const Rotation turn = Rotation::zyz(-4.3055f, 3.7543f, -4.3155f);
  const std::vector<Spherical> stars = sky(1024);
  for ([[maybe_unused]] auto iteration : state)
    for (const Spherical& star : stars) benchmark::DoNotOptimize(turn(star));
  state.counters["stars/s"] = benchmark::Counter(
      (double)stars.size(), benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_RotationOfSky)->Unit(benchmark::kMicrosecond);

}  // namespace
