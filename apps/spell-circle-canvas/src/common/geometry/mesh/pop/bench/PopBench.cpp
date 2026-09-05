/** @file
 * Benchmarks of the point operators: the cook, each operator over a
 * thousand points, whole chains by count and operator mix, and the swept
 * operator by tessellation and by profile — plus the ring seam a device
 * executor replaces on its own.
 */

// geometry_pop_bench — the CPU pop executor under load: what each
// operator costs per point on top of a seeded cloud, how the whole chain
// scales with count and with the operator mix, and what a mask, a
// deformer or a point-set seed costs relative to the plain chain. Run a
// Release build; Debug numbers say nothing.

#include <benchmark/benchmark.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/mesh/pop/Sweep.h>

#include <cmath>
#include <functional>
#include <numbers>
#include <vector>

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

namespace {

std::vector<glm::vec3> ring(int knots) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < knots; ++i) {
    const float a = (float)i / (float)knots * 2.0f * std::numbers::pi_v<float>;
    loop.emplace_back(200.0f * std::cos(a), 40.0f * std::sin(3.0f * a),
                      200.0f * std::sin(a));
  }
  return loop;
}

/** The plain chain: scatter, jitter, noise, colour, size — the shape a
 *  comet has. */
pop::Builder plain(int count) {
  return pop::on(ring(12))
      .count(count)
      .spread(30)
      .jitter(6)
      .noise(14, 0.012f)
      .fade({1, 0.3f, 0.2f, 1}, {0.2f, 0.6f, 1, 1})
      .vary(0.4f);
}

void countPoints(benchmark::State& state, int64_t points) {
  state.counters["points/s"] = benchmark::Counter(
      (double)points, benchmark::Counter::kIsIterationInvariantRate);
  state.SetItemsProcessed(state.iterations() * points);
}

/** One operator over a cloud that already exists: the copy-in of the
 *  seed is the floor every arm below shares, so an operator's own cost
 *  is its arm minus BM_PopOperator/seed. */
using Operator = std::function<pop::Builder(pop::Builder)>;

struct NamedOperator {
  const char* name;
  Operator apply;
};

const std::vector<NamedOperator>& operators() {
  static const std::vector<NamedOperator> ops = {
      {"seed", [](pop::Builder b) { return b; }},
      {"move", [](pop::Builder b) { return b.move({0, 10, 0}); }},
      {"jitter", [](pop::Builder b) { return b.jitter(6); }},
      {"noise", [](pop::Builder b) { return b.noise(14, 0.012f); }},
      {"vary", [](pop::Builder b) { return b.vary(0.4f); }},
      {"fade",
       [](pop::Builder b) {
         return b.fade({1, 0.3f, 0.2f, 1}, {0.2f, 0.6f, 1, 1});
       }},
      {"lookAt", [](pop::Builder b) { return b.lookAt({0, 0, 900}); }},
      {"twist", [](pop::Builder b) { return b.twist(90, {0, 1, 0}, -60, 60); }},
      {"bend",
       [](pop::Builder b) {
         return b.bend(40, {0, 1, 0}, {1, 0, 0}, -60, 60);
       }},
      {"peak", [](pop::Builder b) { return b.peak(8); }},
      {"select",
       [](pop::Builder b) { return b.select("band", {0, 0, 0}, 160, 0.4f); }},
      {"smooth", [](pop::Builder b) { return b.smooth(0.5f, 4); }},
  };
  return ops;
}

void BM_PopOperator(benchmark::State& state) {
  const int count = 1000;
  const NamedOperator& op = operators()[(size_t)state.range(0)];
  state.SetLabel(op.name);
  const Cloud seed = plain(count).cloud();
  const pop::Chain chain = op.apply(pop::on(seed));
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(pop::cook(chain));
  countPoints(state, count);
}
BENCHMARK(BM_PopOperator)->DenseRange(0, 11)->Unit(benchmark::kMicrosecond);

void BM_PopCook_Plain(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0));
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(pop::cook(chain));
  countPoints(state, state.range(0));
  state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_PopCook_Plain)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_PopCook_MaskedAndDeformed(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0))
                               .select("band", {0, 0, 0}, 160, 0.4f)
                               .twist(90, {0, 1, 0}, -60, 60)
                               .masked("band")
                               .bend(40, {0, 1, 0}, {1, 0, 0}, -60, 60)
                               .peak(8)
                               .masked("band")
                               .mixBy("Color", "Color", "Color", "band");
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(pop::cook(chain));
  countPoints(state, state.range(0));
  state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_PopCook_MaskedAndDeformed)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_PopCook_Relax(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0)).smooth(0.5f, 4);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(pop::cook(chain));
  countPoints(state, state.range(0));
  state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_PopCook_Relax)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_PopCook_PointSetSeed(benchmark::State& state) {
  // Seeding from an existing cloud with a few lanes: the copy-in cost
  // against the scatter it replaces.
  const Cloud seed = plain((int)state.range(0)).cloud();
  const pop::Chain chain =
      pop::on(seed).move({0, 10, 0}).vary(0.3f).lookAt({0, 0, 900});
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(pop::cook(chain));
  countPoints(state, state.range(0));
  state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_PopCook_PointSetSeed)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_PopSink_Stamps(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0)).lookAt({0, 0, 900});
  const Mesh stamp = mesh::quad(4, 4);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(pop::cookMesh(chain, stamp));
  countPoints(state, state.range(0));
  state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_PopSink_Stamps)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** WHAT THE SEAM COSTS. A cook through the runtime value — a capability
 *  question per operator, then a virtual call — against the same work
 *  reached with no indirection at all. The chain is deliberately tiny,
 *  so what is measured is the dispatch rather than the cook. */
void BM_PopRuntime_Dispatch(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0));
  const pop::Runtime runtime = pop::Runtime::cpu();
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(pop::cook(chain, runtime));
  countPoints(state, state.range(0));
}
BENCHMARK(BM_PopRuntime_Dispatch)
    ->Arg(1)
    ->Arg(1000)
    ->Unit(benchmark::kMicrosecond);

void BM_PopRuntime_Direct(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0));
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(pop::Runtime::cpu()->cook(chain));
  countPoints(state, state.range(0));
}
BENCHMARK(BM_PopRuntime_Direct)
    ->Arg(1)
    ->Arg(1000)
    ->Unit(benchmark::kMicrosecond);


/** A closed trefoil: curvature that turns in all three axes, so a rail
 *  read off it has inflections to carry through rather than a planar
 *  arc. */
curve::Spline3 knot(int knots) {
  curve::Spline3 spline;
  spline.closed = true;
  for (int i = 0; i < knots; ++i) {
    const float t = 2.0f * std::numbers::pi_v<float> * (float)i / (float)knots;
    spline.points.emplace_back((std::sin(t) + 2.0f * std::sin(2 * t)) * 60.0f,
                               (std::cos(t) - 2.0f * std::cos(2 * t)) * 60.0f,
                               -std::sin(3 * t) * 60.0f);
  }
  return spline;
}

void countVertices(benchmark::State& state, const Mesh& m) {
  state.counters["vertices/s"] = benchmark::Counter(
      (double)m.vertexCount(), benchmark::Counter::kIsIterationInvariantRate);
  state.counters["triangles"] = (double)m.triangleCount();
}

void BM_Sweep_Circle(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const path::Polyline profile = pop::profile::circle((int)state.range(1));
  const pop::SweepOptions options{.segments = (int)state.range(0),
                                    .scale = 6};
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = pop::sweep(spline, profile, options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Sweep_Circle)
    ->ArgsProduct({{32, 256, 1024}, {6, 24}})
    ->ArgNames({"segments", "sides"})
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** THE RING SEAM ALONE: what a rail and a profile become as a dispatch,
 *  and the executor's own call over it, without the topology, the caps or
 *  the pour into a mesh's lanes. It is the half a device replaces, so a
 *  device executor's arm has a host number to stand beside. */
void BM_Sweep_Rings(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const path::Polyline profile = pop::profile::circle(24);
  const pop::SweepOptions options{.segments = (int)state.range(0),
                                    .scale = 6};
  const std::vector<curve::Frame3> rail =
      curve::frames(spline, options.segments, options.up);
  pop::kernel::Dispatch work;
  pop::describe(rail, profile, options, &work);
  std::vector<glm::vec4> positions(work.vertices());
  std::vector<glm::vec4> normals(work.vertices());
  for ([[maybe_unused]] auto iteration : state) {
    options.runtime->rings(work, positions.data(), normals.data());
    benchmark::DoNotOptimize(positions.data());
  }
  state.counters["vertices/s"] =
      benchmark::Counter((double)state.iterations() * (double)work.vertices(),
                         benchmark::Counter::kIsRate);
}
BENCHMARK(BM_Sweep_Rings)
    ->Arg(32)
    ->Arg(256)
    ->Arg(1024)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_Sweep_Line(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const path::Polyline profile = pop::profile::line();
  const pop::SweepOptions options{
      .segments = (int)state.range(0),
      .scale = 24,
      .normals = pop::SweepOptions::Normals::Frame};
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = pop::sweep(spline, profile, options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Sweep_Line)
    ->RangeMultiplier(4)
    ->Range(32, 2048)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

// The hung rail, which walks the loop in parameter rather than by arc
// length and re-derives its own tangents: a different cost per section
// from the transported rail above.
void BM_Sweep_Hang(benchmark::State& state) {
  const curve::Spline3 spline = knot(9);
  const path::Polyline profile = pop::profile::line();
  const pop::SweepOptions options{
      .scale = 24, .normals = pop::SweepOptions::Normals::Frame};
  const int sections = (int)state.range(0);
  Mesh last;
  for ([[maybe_unused]] auto iteration : state) {
    last = pop::sweep(curve::hangFrames(spline, sections, 1, 0.4f), profile,
                        options);
    benchmark::DoNotOptimize(last.positions.data());
  }
  countVertices(state, last);
}
BENCHMARK(BM_Sweep_Hang)
    ->RangeMultiplier(4)
    ->Range(32, 2048)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

}  // namespace

BENCHMARK_MAIN();
