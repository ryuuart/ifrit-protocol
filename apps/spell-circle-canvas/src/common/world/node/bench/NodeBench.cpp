/** @file
 * What a describe costs: building a group of meshes at several node
 * counts, comparing two descriptions field by field, and filling a
 * node's lane list.
 */

#include <benchmark/benchmark.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Lanes.h>
#include <sigilworld/advanced/Node.h>

#include <string>
#include <vector>

using namespace sigil::world;

namespace {

Group describeTree(int nodes) {
  return group(each((size_t)nodes,
                    [](size_t i) {
                      return group()
                          .key("n" + std::to_string(i))
                          .at({(float)i, 0.0f, 0.0f})
                          .rotateY((float)i)
                          .scale(1.0f + (float)i * 0.01f)
                          .collection("lit");
                    }))
      .key("root");
}

void DescribeTree(benchmark::State& state) {
  const int nodes = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(describeTree(nodes));
  state.SetItemsProcessed(state.iterations() * nodes);
}
BENCHMARK(DescribeTree)->Arg(64)->Arg(512)->Arg(4096);

void PropsEqual(benchmark::State& state) {
  const Group a =
      group().key("n").at({1, 2, 3}).rotateY(30.0f).collection("lit");
  const Group b =
      group().key("n").at({1, 2, 3}).rotateY(30.0f).collection("lit");
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(propertiesEqual(*a.node(), *b.node()));
}
BENCHMARK(PropsEqual);

void LanesOf(benchmark::State& state) {
  const Group node = group().key("n").at({1, 2, 3}).rotateY(30.0f);
  std::vector<Lane> lanes;
  for ([[maybe_unused]] auto iteration : state) {
    lanesOf(*node.node(), lanes);
    benchmark::DoNotOptimize(lanes.data());
  }
}
BENCHMARK(LanesOf);

}  // namespace
