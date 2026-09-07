/** @file
 * The stepper under load: a field of free particles, the same field
 * flocking (which is where the cost of comparing every pair shows), and
 * a chain of sticks under its constraint passes.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/physics/Physics.h>

#include <cmath>
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

/** Free particles: one pass over the lanes per force, and one to move
 *  them. The argument is the count, and the cost is linear in it. */
void ParticleStep(benchmark::State& state) {
  Points points = cloud((int)state.range(0));
  const std::vector<Force> forces{gravity({0, 900}), drag(0.4f)};
  const Verlet stepper{.dt = 1.0f / 60.0f, .damping = 0.1f};
  for ([[maybe_unused]] auto iteration : state) stepper.step(points, forces);
}
BENCHMARK(ParticleStep)->Arg(256)->Arg(4096);

/** The same field flocking. Every point is compared with every other,
 *  so this arm is the one a neighbour index would move, and the gap
 *  between it and the arm above is what that index is worth. */
void FlockStep(benchmark::State& state) {
  Points points = cloud((int)state.range(0));
  const std::vector<Force> forces{boids({}, 80.0f), drag(0.4f)};
  const Verlet stepper{.dt = 1.0f / 60.0f};
  for ([[maybe_unused]] auto iteration : state) stepper.step(points, forces);
}
BENCHMARK(FlockStep)->Arg(256)->Arg(1024);

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
