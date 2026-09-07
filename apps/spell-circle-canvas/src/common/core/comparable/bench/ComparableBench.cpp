/** @file
 * What comparable type erasure costs, against the same question asked
 * of the model directly.
 *
 * The comparison is what a prune spends: two values that are copies of
 * one (the shared-state answer, which is the common case for a value
 * carried through a frame), two separately built values of one type
 * (the type check plus the model's own `==`), two values of different
 * types, and the escape hatch. The direct arm compares the models
 * themselves, so the difference between the two is what erasure adds.
 */

#include <benchmark/benchmark.h>
#include <sigilcore/comparable/Erased.h>

using sigil::core::Erased;

namespace {

struct Ops {
  virtual ~Ops() = default;
  virtual int answer() const = 0;
};

struct Model : Ops {
  explicit Model(int n) : n(n) {}
  int n;
  int answer() const override { return n; }
  bool operator==(const Model& o) const { return n == o.n; }
};

struct OtherModel : Ops {
  explicit OtherModel(int n) : n(n) {}
  int n;
  int answer() const override { return -n; }
  bool operator==(const OtherModel& o) const { return n == o.n; }
};

struct Opaque : Ops {
  explicit Opaque(int n) : n(n) {}
  int n;
  int answer() const override { return n; }
};

benchmark::Counter perCall() {
  return benchmark::Counter(1, benchmark::Counter::kIsIterationInvariantRate);
}

void BM_DirectEqual(benchmark::State& state) {
  const Model a{3}, b{3};
  for ([[maybe_unused]] auto iteration : state) {
    bool same = a == b;
    benchmark::DoNotOptimize(same);
  }
  state.counters["compares/s"] = perCall();
}
BENCHMARK(BM_DirectEqual);

void BM_ErasedCopiesEqual(benchmark::State& state) {
  const Erased<Ops> a = Model{3};
  // the copy is what the benchmark compares
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const Erased<Ops> copy = a;
  for ([[maybe_unused]] auto iteration : state) {
    bool same = a == copy;
    benchmark::DoNotOptimize(same);
  }
  state.counters["compares/s"] = perCall();
}
BENCHMARK(BM_ErasedCopiesEqual);

void BM_ErasedSameTypeEqual(benchmark::State& state) {
  const Erased<Ops> a = Model{3}, b = Model{3};
  for ([[maybe_unused]] auto iteration : state) {
    bool same = a == b;
    benchmark::DoNotOptimize(same);
  }
  state.counters["compares/s"] = perCall();
}
BENCHMARK(BM_ErasedSameTypeEqual);

void BM_ErasedDifferentTypeEqual(benchmark::State& state) {
  const Erased<Ops> a = Model{3}, b = OtherModel{3};
  for ([[maybe_unused]] auto iteration : state) {
    bool same = a == b;
    benchmark::DoNotOptimize(same);
  }
  state.counters["compares/s"] = perCall();
}
BENCHMARK(BM_ErasedDifferentTypeEqual);

void BM_ErasedEscapeHatchEqual(benchmark::State& state) {
  const Erased<Ops> a{Opaque{3}}, b{Opaque{3}};
  for ([[maybe_unused]] auto iteration : state) {
    bool same = a == b;
    benchmark::DoNotOptimize(same);
  }
  state.counters["compares/s"] = perCall();
}
BENCHMARK(BM_ErasedEscapeHatchEqual);

/** Construction, which a description pays once per frame per seam value
 *  it carries: a copy of the model into shared immutable state. */
void BM_ErasedConstruct(benchmark::State& state) {
  int n = 0;
  for ([[maybe_unused]] auto iteration : state) {
    Erased<Ops> value = Model{n++};
    benchmark::DoNotOptimize(value);
  }
  state.counters["values/s"] = perCall();
}
BENCHMARK(BM_ErasedConstruct);

/** Carrying one on: a refcount bump, which is what makes a copy-on-write
 *  node copy cheap. */
void BM_ErasedCopy(benchmark::State& state) {
  const Erased<Ops> a = Model{3};
  for ([[maybe_unused]] auto iteration : state) {
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Erased<Ops> copy = a;
    benchmark::DoNotOptimize(copy);
  }
  state.counters["copies/s"] = perCall();
}
BENCHMARK(BM_ErasedCopy);

/** A call through the operations, the other thing the value exists for. */
void BM_ErasedDispatch(benchmark::State& state) {
  const Erased<Ops> a = Model{3};
  int sink = 0;
  for ([[maybe_unused]] auto iteration : state) {
    sink += a->answer();
    benchmark::DoNotOptimize(sink);
  }
  state.counters["calls/s"] = perCall();
}
BENCHMARK(BM_ErasedDispatch);

}  // namespace
