/** @file
 * What an ordering costs: a chain of passes each reading the one before
 * it, at several pass counts, and a fan of independent passes over one
 * source at the same counts.
 */

#include <benchmark/benchmark.h>
#include <sigilworld/graph/Plan.h>

#include <string>

using namespace sigil;
using namespace sigil::world;

namespace {

constexpr SkISize kExtent{256, 256};

/** A chain: every pass reads what the one before it wrote, so the order
 *  is fully determined and the transients take turns. */
Frame chained(int passes) {
  Frame frame;
  frame.extent(kExtent).pass(geometryPass("main").writes("stage0"));
  for (int i = 1; i < passes; ++i)
    frame.pass(postPass("stage" + std::to_string(i))
                   .reads("stage" + std::to_string(i - 1))
                   .writes("stage" + std::to_string(i)));
  return frame;
}

/** A fan: every pass reads the one source, so nothing orders them
 *  against each other and the declaration order is the answer. */
Frame fanned(int passes) {
  Frame frame;
  frame.extent(kExtent).pass(geometryPass("main").writes("colour"));
  for (int i = 1; i < passes; ++i)
    frame.pass(postPass("leaf" + std::to_string(i))
                   .reads("colour")
                   .writes("leaf" + std::to_string(i)));
  return frame;
}

void OrderChain(benchmark::State& state) {
  const int passes = (int)state.range(0);
  const Frame frame = chained(passes);
  for ([[maybe_unused]] auto iteration : state) {
    graph::Plan plan = graph::build(frame);
    benchmark::DoNotOptimize(plan);
  }
  state.SetItemsProcessed(state.iterations() * passes);
}
BENCHMARK(OrderChain)->Arg(4)->Arg(16)->Arg(64)->Arg(256);

void OrderFan(benchmark::State& state) {
  const int passes = (int)state.range(0);
  const Frame frame = fanned(passes);
  for ([[maybe_unused]] auto iteration : state) {
    graph::Plan plan = graph::build(frame);
    benchmark::DoNotOptimize(plan);
  }
  state.SetItemsProcessed(state.iterations() * passes);
}
BENCHMARK(OrderFan)->Arg(4)->Arg(16)->Arg(64)->Arg(256);

}  // namespace
