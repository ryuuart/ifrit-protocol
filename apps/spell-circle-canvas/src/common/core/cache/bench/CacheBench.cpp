/** @file
 * The settled-subtree proof timed over a fake host, so the numbers are the
 * kernel's own: a wholly still tree, one driven lane at the deepest leaf
 * (the worst case — every ancestor folds a volatile verdict), a tree where
 * every node is driven, and the hold's read side over a settled tree —
 * each at several node counts.
 */

#include <benchmark/benchmark.h>

#include <memory>
#include <string>
#include <vector>

#include "FakeCacheHost.h"

using namespace sigil::core;
using namespace sigil::core::test;

namespace {

/** A balanced binary tree of @p count nodes; `drive` names how many of
 *  them carry a connected lane, taken from the deepest leaves back. Held
 *  by pointer because the nodes carry parent links. */
std::unique_ptr<FakeNode> tree(int count, int drive = 0) {
  auto root = std::make_unique<FakeNode>();
  root->key = "n0";
  std::vector<FakeNode*> flat{root.get()};
  for (int i = 1; i < count; ++i)
    flat.push_back(&flat[(size_t)(i - 1) / 2]->add("n" + std::to_string(i)));
  for (int i = count - 1; i >= 0 && drive > 0; --i, --drive) {
    flat[(size_t)i]->driven = true;
    flat[(size_t)i]->lanes = {1.0f};
  }
  return root;
}

void StillTree(benchmark::State& state) {
  const int count = (int)state.range(0);
  FakeHost host;
  const std::unique_ptr<FakeNode> root = tree(count);
  for (auto&& _ : state) {
    host.proveTree(*root);
    benchmark::DoNotOptimize(root->verdict);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

void OneDrivenLeaf(benchmark::State& state) {
  const int count = (int)state.range(0);
  FakeHost host;
  const std::unique_ptr<FakeNode> root = tree(count, 1);
  for (auto&& _ : state) {
    host.proveTree(*root);
    benchmark::DoNotOptimize(root->verdict);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

void EveryNodeDriven(benchmark::State& state) {
  const int count = (int)state.range(0);
  FakeHost host;
  const std::unique_ptr<FakeNode> root = tree(count, count);
  for (auto&& _ : state) {
    host.proveTree(*root);
    benchmark::DoNotOptimize(root->verdict);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

/** The release's read side: every node warmed up and holding still, so
 *  every one of them resolves its lane and compares it. */
void SettledRelease(benchmark::State& state) {
  const int count = (int)state.range(0);
  FakeHost host;
  const std::unique_ptr<FakeNode> root = tree(count, count);
  for (int i = 0; i < host.hold + 2; ++i) {
    host.proveTree(*root);
    host.drawTree(*root);
    host.scanReleased();
  }
  for (auto&& _ : state) {
    host.proveTree(*root);
    benchmark::DoNotOptimize(root->verdict);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(StillTree)->Arg(64)->Arg(512)->Arg(4096);
BENCHMARK(OneDrivenLeaf)->Arg(64)->Arg(512)->Arg(4096);
BENCHMARK(EveryNodeDriven)->Arg(64)->Arg(512)->Arg(4096);
BENCHMARK(SettledRelease)->Arg(64)->Arg(512)->Arg(4096);

}  // namespace

BENCHMARK_MAIN();
