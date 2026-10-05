/** @file
 * What the frame's resources cost between two frames: asking for the
 * stamping of a point set that stood still, which is a lookup, and of one
 * that moved, which forms it again.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilworld/advanced/Skia.h>

#include <cstdint>

using namespace sigil;
using namespace sigil::world;

namespace {

/** @p count points on a grid, as a compute pass would have written them. */
geometry::mesh::Cloud grid(int count) {
  geometry::mesh::Cloud cloud;
  cloud.positions.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    cloud.positions.push_back(
        {(float)(i % 64) * 4.0f, (float)(i / 64) * 4.0f, 0.0f});
  return cloud;
}

void StillStamping(benchmark::State& state) {
  const geometry::mesh::Cloud cloud = grid((int)state.range(0));
  const geometry::mesh::Mesh bead = geometry::mesh::quad(2.0f, 2.0f);
  Targets targets;
  benchmark::DoNotOptimize(targets.stamped(cloud, bead));
  for ([[maybe_unused]] auto iteration : state) {
    benchmark::DoNotOptimize(targets.stamped(cloud, bead));
    targets.endFrame();
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(StillStamping)->Arg(256)->Arg(4096);

void MovingStamping(benchmark::State& state) {
  geometry::mesh::Cloud cloud = grid((int)state.range(0));
  const geometry::mesh::Mesh bead = geometry::mesh::quad(2.0f, 2.0f);
  Targets targets;
  for ([[maybe_unused]] auto iteration : state) {
    cloud.positions.front().x += 1.0f;
    benchmark::DoNotOptimize(targets.stamped(cloud, bead));
    targets.endFrame();
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(MovingStamping)->Arg(256)->Arg(4096);

}  // namespace
