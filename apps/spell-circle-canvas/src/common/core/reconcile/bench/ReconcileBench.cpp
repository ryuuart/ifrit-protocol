/** @file
 * The reconciler timed over a fake host, so the numbers are the kernel's
 * own: a cold mount, a warm identical re-describe (every node prunes), one
 * changed leaf among many, a keyed rotation of the child list, a churn
 * that retires and mounts a slice each pass, and a memo hit — each at
 * several node counts.
 */

#include <benchmark/benchmark.h>

#include <string>

#include "FakeHost.h"

using namespace sigil::core;
using namespace sigil::core::test::reconcile;

namespace {

Desc grid(int count, int changed = -1, int shift = 0) {
  std::vector<Desc> children;
  children.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    const int id = (i + shift) % count;
    children.push_back(desc("n" + std::to_string(id), id == changed ? 1 : 0));
  }
  return desc("root", 0, std::move(children));
}

void ColdMount(benchmark::State& state) {
  const int count = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    FakeHost host;
    host.render(grid(count));
    benchmark::DoNotOptimize(host.root);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

void WarmPrune(benchmark::State& state) {
  const int count = (int)state.range(0);
  FakeHost host;
  host.render(grid(count));
  for ([[maybe_unused]] auto iteration : state) {
    host.render(grid(count));
    benchmark::DoNotOptimize(host.root);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

void OneChangedLeaf(benchmark::State& state) {
  const int count = (int)state.range(0);
  FakeHost host;
  host.render(grid(count));
  int phase = 0;
  for ([[maybe_unused]] auto iteration : state) {
    host.render(grid(count, (phase++ * 7) % count));
    benchmark::DoNotOptimize(host.root);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

void KeyedRotation(benchmark::State& state) {
  const int count = (int)state.range(0);
  FakeHost host;
  host.render(grid(count));
  int shift = 0;
  for ([[maybe_unused]] auto iteration : state) {
    host.render(grid(count, -1, ++shift));
    benchmark::DoNotOptimize(host.root);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

void Churn(benchmark::State& state) {
  // A tenth of the children leave and a tenth arrive every pass.
  const int count = (int)state.range(0);
  FakeHost host;
  host.render(grid(count));
  int generation = 0;
  for ([[maybe_unused]] auto iteration : state) {
    ++generation;
    std::vector<Desc> children;
    for (int i = 0; i < count; ++i) {
      const int gen = (i % 10 == 0) ? generation : 0;
      children.push_back(
          desc("n" + std::to_string(i) + "g" + std::to_string(gen)));
    }
    host.render(desc("root", 0, std::move(children)));
    host.retired.clear();
    host.log.clear();
    benchmark::DoNotOptimize(host.root);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

void MemoHit(benchmark::State& state) {
  const int count = (int)state.range(0);
  const auto memoGrid = [count] {
    std::vector<Desc> children;
    for (int i = 0; i < count; ++i) {
      auto shell = desc("m" + std::to_string(i));
      Memo<Desc> memo;
      memo.props = i;
      memo.equal = [](const std::any& a, const std::any& b) {
        return std::any_cast<int>(a) == std::any_cast<int>(b);
      };
      memo.invoke = [i](const std::any&) {
        return desc("m" + std::to_string(i), i);
      };
      shell->memo = std::move(memo);
      children.push_back(std::move(shell));
    }
    return desc("root", 0, std::move(children));
  };
  FakeHost host;
  host.render(memoGrid());
  for ([[maybe_unused]] auto iteration : state) {
    host.render(memoGrid());
    benchmark::DoNotOptimize(host.root);
  }
  state.SetItemsProcessed(state.iterations() * count);
}

}  // namespace

BENCHMARK(ColdMount)->Arg(100)->Arg(1000)->Arg(10000);
BENCHMARK(WarmPrune)->Arg(100)->Arg(1000)->Arg(10000);
BENCHMARK(OneChangedLeaf)->Arg(1000)->Arg(10000);
BENCHMARK(KeyedRotation)->Arg(1000)->Arg(10000);
BENCHMARK(Churn)->Arg(1000)->Arg(10000);
BENCHMARK(MemoHit)->Arg(1000)->Arg(10000);

BENCHMARK_MAIN();
