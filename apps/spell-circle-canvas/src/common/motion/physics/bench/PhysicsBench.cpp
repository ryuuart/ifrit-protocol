/** @file
 * The stepper under load: a field of free particles, the same field
 * flocking over the grid beside the same flock comparing every pair, the
 * grid apart from what asks it, and a chain of sticks under its
 * constraint passes.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/physics/Physics.h>

#include <cmath>
#include <cstdint>
#include <vector>

using namespace sigil::motion::physics;

namespace {

Points cloud(int count) {
  Points points;
  for (int i = 0; i < count; ++i) {
    const float angle = (float)i * 0.61f;
    points.add({std::cos(angle) * 200.0f, std::sin(angle) * 200.0f},
               {std::sin(angle) * 30.0f, std::cos(angle) * 30.0f});
  }
  return points;
}

/** A FIELD rather than a ring: the points spread over a box whose side
 *  grows with the count, so the density stays the same at every arm and
 *  a flock's reach holds about the same number of birds whatever the
 *  size of the flock. That is what makes the two flock arms below a
 *  measurement of the search and not of how crowded a shape happens to
 *  be — a ring is a line, and every point on one is near a sixteenth of
 *  the set however many there are. */
Points field(int count) {
  Points points;
  const float side = std::sqrt((float)count) * 20.0f;
  uint32_t bits = 0x9e3779b9u;
  const auto next = [&bits] {
    bits ^= bits << 13u;
    bits ^= bits >> 17u;
    bits ^= bits << 5u;
    return (float)(bits >> 8u) * (1.0f / 16777216.0f);
  };
  for (int i = 0; i < count; ++i)
    points.add({next() * side, next() * side},
               {next() * 60.0f - 30.0f, next() * 60.0f - 30.0f});
  return points;
}

/** Free particles: one pass over the lanes per force, and one to move
 *  them. The argument is the count, and the cost is linear in it. */
void ParticleStep(benchmark::State& state) {
  Points points = cloud((int)state.range(0));
  const std::vector<Force> forces{gravity({0, 900}), drag(0.4f)};
  const Verlet stepper{.dt = 1.0f / 60.0f, .damping = 0.1f};
  for ([[maybe_unused]] auto iteration : state) stepper.step(points, forces);
}
BENCHMARK(ParticleStep)->Arg(256)->Arg(4096);

/** The same field flocking, over the grid: one index built per step and
 *  one query per point, so the cost follows how many birds are NEAR one
 *  rather than how many there are. */
void FlockStep(benchmark::State& state) {
  Points points = field((int)state.range(0));
  const std::vector<Force> forces{boids({}, 80.0f), drag(0.4f)};
  const Verlet stepper{.dt = 1.0f / 60.0f};
  for ([[maybe_unused]] auto iteration : state) stepper.step(points, forces);
}
BENCHMARK(FlockStep)->Arg(256)->Arg(1024)->Arg(10000);

/** THE SAME THREE STEERINGS, FOUND BY COMPARING EVERY PAIR — the shape
 *  the flock has without an index, written as a caller's own force so
 *  the two stand in one bench and the gap between them is the index's
 *  whole worth. Its answer is the arm above's: the grid hands its
 *  neighbours back in index order, so both sums are the same sum. */
void flockByWalking(Points& points, float, const Force& force) {
  const float reachSquared = force.radius * force.radius;
  const size_t count = points.size();
  for (size_t i = 0; i < count; ++i) {
    if (!points.movable(i)) continue;
    Vec2 away{}, heading{}, centre{};
    int neighbours = 0;
    for (size_t j = 0; j < count; ++j) {
      if (j == i) continue;
      const Vec2 offset = points.position[j] - points.position[i];
      const float distanceSquared = offset.lengthSquared();
      if (distanceSquared > reachSquared || distanceSquared <= 0.0f) continue;
      ++neighbours;
      heading += points.velocity[j];
      centre += points.position[j];
      away -= offset * (1.0f / distanceSquared);
    }
    if (neighbours == 0) continue;
    const float share = 1.0f / (float)neighbours;
    const Vec2 alignment = heading * share - points.velocity[i];
    const Vec2 cohesion = centre * share - points.position[i];
    const Vec2 steering = away * force.flock.separation +
                          alignment * force.flock.alignment +
                          cohesion * force.flock.cohesion;
    points.force[i] += steering * (force.strength * points.mass[i]);
  }
}

void PairwiseFlockStep(benchmark::State& state) {
  Points points = field((int)state.range(0));
  const std::vector<Force> forces{Force{.kind = ForceKind::Body,
                                        .strength = 1.0f,
                                        .radius = 80.0f,
                                        .body = &flockByWalking},
                                  drag(0.4f)};
  const Verlet stepper{.dt = 1.0f / 60.0f};
  for ([[maybe_unused]] auto iteration : state) stepper.step(points, forces);
}
BENCHMARK(PairwiseFlockStep)->Arg(256)->Arg(1024)->Arg(10000);

/** The index alone, apart from what asks it: the two counting passes a
 *  build is, and then one radius query per point over the grid that came
 *  out — which is where a consumer that indexes for something other than
 *  a flock reads its cost. */
void NeighbourhoodBuild(benchmark::State& state) {
  const Points points = field((int)state.range(0));
  Neighbourhood near;
  for ([[maybe_unused]] auto iteration : state) {
    near.build(points.position, 80.0f);
    benchmark::DoNotOptimize(near.cell());
  }
}
BENCHMARK(NeighbourhoodBuild)->Arg(1024)->Arg(10000);

void NeighbourhoodQuery(benchmark::State& state) {
  const Points points = field((int)state.range(0));
  const Neighbourhood near(points.position, 80.0f);
  std::vector<uint32_t> found;
  for ([[maybe_unused]] auto iteration : state) {
    size_t total = 0;
    for (const Vec2 at : points.position) {
      near.within(at, 80.0f, found);
      total += found.size();
    }
    benchmark::DoNotOptimize(total);
  }
}
BENCHMARK(NeighbourhoodQuery)->Arg(1024)->Arg(10000);

/** A hanging chain: the constraint list walked `iterations` times per
 *  step, which is the cost a stiffer structure is bought with. */
void ChainStep(benchmark::State& state) {
  Points points;
  std::vector<Constraint> sticks;
  const int links = 512;
  points.add({0, 0}, {}, 1.0f, true);
  for (int i = 1; i < links; ++i) {
    points.add({(float)i * 8.0f, 0.0f});
    sticks.push_back(distance((size_t)i - 1, (size_t)i, 8.0f));
  }
  const std::vector<Force> forces{gravity({0, 900})};
  const Verlet stepper{.dt = 1.0f / 60.0f, .iterations = (int)state.range(0)};
  for ([[maybe_unused]] auto iteration : state)
    stepper.step(points, forces, sticks);
}
BENCHMARK(ChainStep)->Arg(1)->Arg(8);

}  // namespace
