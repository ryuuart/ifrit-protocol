/** @file
 * What a describe costs: building a tree of elements at several node
 * counts, comparing two descriptions field by field, and filling a
 * node's lane list.
 */

#include <benchmark/benchmark.h>
#include <sigilworld/element/Lanes.h>
#include <sigilworld/element/Node.h>

#include <string>
#include <vector>

using namespace sigil::world;

namespace {

Element describeTree(int nodes) {
  Element root;
  root.key("root");
  for (int i = 0; i < nodes; ++i)
    root.child(Element()
                   .key("n" + std::to_string(i))
                   .at({(float)i, 0.0f, 0.0f})
                   .rotateY((float)i)
                   .scale(1.0f + (float)i * 0.01f)
                   .tag("lit"));
  return root;
}

void DescribeTree(benchmark::State& state) {
  const int nodes = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(describeTree(nodes));
  state.SetItemsProcessed(state.iterations() * nodes);
}
BENCHMARK(DescribeTree)->Arg(64)->Arg(512)->Arg(4096);

void PropsEqual(benchmark::State& state) {
  const Element a = Element().key("n").at({1, 2, 3}).rotateY(30.0f).tag("lit");
  const Element b = Element().key("n").at({1, 2, 3}).rotateY(30.0f).tag("lit");
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(propsEqual(*a.node(), *b.node()));
}
BENCHMARK(PropsEqual);

void LanesOf(benchmark::State& state) {
  const Element e = Element().key("n").at({1, 2, 3}).rotateY(30.0f);
  std::vector<Lane> lanes;
  for ([[maybe_unused]] auto iteration : state) {
    lanesOf(*e.node(), lanes);
    benchmark::DoNotOptimize(lanes.data());
  }
}
BENCHMARK(LanesOf);

}  // namespace

BENCHMARK_MAIN();
