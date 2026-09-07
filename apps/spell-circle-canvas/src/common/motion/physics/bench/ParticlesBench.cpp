/** @file
 * A cloud under load: ten thousand particles born, stepped, aged and
 * reaped — and each of the four measured on its own, because they are
 * paid for at different rates and a consumer that is slow is slow in one
 * of them.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/physics/Physics.h>

#include <string>
#include <vector>

using namespace sigil::motion::physics;
namespace chance = sigil::core::chance;

namespace {

constexpr size_t kCount = 10000;

/** A wall of flame: a segment throwing a wide cone, with the four attributes
 *  a stamping renderer reads. */
Emitter wall() {
  return Emitter{
      .from = EmitFrom::Segment,
      .at = {0.0f, 0.0f},
      .size = {400.0f, 0.0f},
      .aim = {0.0f, -1.0f},
      .cone = 0.6f,
      .speed = {.mean = 180.0f, .variation = 60.0f},
      .attributes = {{std::string(kLife), {.mean = 1.4f, .variation = 0.5f}},
                     {"size", {.mean = 4.0f, .variation = 1.6f}},
                     {"red", {.mean = 1.0f, .variation = 0.35f}},
                     {"green", {.mean = 0.22f, .variation = 0.35f}}},
      .rate = 6000.0f};
}

/** A steady cloud of the bench's size, with the two decaying attributes set
 *  so that `live` does the work a study's colours do. */
Particles crowd() {
  const Emitter mouth = wall();
  chance::Stream stream = chance::Stream::pcg(1982);
  Particles cloud;
  cloud.attribute("size").rate = 1.2f;
  Attribute& red = cloud.attribute("red");
  red.rate = -0.4f;
  red.least = 0.0f;
  Attribute& green = cloud.attribute("green");
  green.rate = -1.1f;
  green.least = 0.0f;
  mouth.burst(cloud, stream, kCount);
  return cloud;
}

}  // namespace

/** BIRTHS: seven draws off the stream and a push onto every attribute, per
 *  particle. The argument is how many arrive at once. */
void ParticleBirth(benchmark::State& state) {
  const Emitter mouth = wall();
  chance::Stream stream = chance::Stream::pcg(7);
  for ([[maybe_unused]] auto iteration : state) {
    Particles cloud;
    mouth.burst(cloud, stream, (size_t)state.range(0));
    benchmark::DoNotOptimize(cloud.points.position.data());
  }
}
BENCHMARK(ParticleBirth)->Arg(1000)->Arg((int64_t)kCount);

/** THE STEP: the same stepper over the same attributes the point set's own
 *  bench measures, so the two arms are comparable and the cost of being
 *  a particle rather than a point is the difference. */
void ParticleCloudStep(benchmark::State& state) {
  Particles cloud = crowd();
  const std::vector<Force> forces{gravity({0, 900}), drag(0.4f)};
  const Verlet stepper{.dt = 1.0f / 60.0f};
  for ([[maybe_unused]] auto iteration : state)
    stepper.step(cloud.points, forces);
}
BENCHMARK(ParticleCloudStep);

/** AGEING: one pass over the age attribute and one over each drifting
 * attribute, which is what an attribute changing at a rate costs. */
void ParticleLive(benchmark::State& state) {
  Particles cloud = crowd();
  for ([[maybe_unused]] auto iteration : state) {
    cloud.live(1.0f / 60.0f);
    benchmark::DoNotOptimize(cloud.age.data());
  }
}
BENCHMARK(ParticleLive);

/** DEATH: the whole cloud reaped and reborn, which is the compaction —
 *  every attribute's last value moved into every hole — at its worst, since
 *  nothing survives. */
void ParticleReap(benchmark::State& state) {
  const Emitter mouth = wall();
  chance::Stream stream = chance::Stream::pcg(11);
  Particles cloud;
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    cloud.clear();
    mouth.burst(cloud, stream, kCount);
    cloud.live(10.0f);
    state.ResumeTiming();
    benchmark::DoNotOptimize(cloud.reap());
  }
}
BENCHMARK(ParticleReap);
