// SigilData table benchmarks: the reshapings a redraw runs — reading a
// numeric column, ordering by one, and gathering by a text key.

#include <benchmark/benchmark.h>
#include <sigildata/table/Table.h>

#include <string>
#include <vector>

using sigil::data::Order;
using sigil::data::Table;

static Table built(size_t rows) {
  std::vector<double> values;
  std::vector<std::string> keys;
  values.reserve(rows);
  keys.reserve(rows);
  for (size_t i = 0; i < rows; ++i) {
    values.push_back((double)((i * 7919) % 1000) / 10.0);
    keys.push_back("k" + std::to_string(i % 8));
  }
  Table table;
  table.add("value", std::move(values));
  table.add("key", std::move(keys));
  return table;
}

static void BM_TableColumnSum(benchmark::State& state) {
  const Table table = built((size_t)state.range(0));
  for ([[maybe_unused]] auto _ : state) {
    double total = 0;
    for (double v : table.column<double>("value")) total += v;
    benchmark::DoNotOptimize(total);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_TableColumnSum)->Arg(100)->Arg(10000);

static void BM_TableSort(benchmark::State& state) {
  const Table table = built((size_t)state.range(0));
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(table.sort("value", Order::Descending));
}
BENCHMARK(BM_TableSort)->Arg(100)->Arg(10000);

static void BM_TableGroup(benchmark::State& state) {
  const Table table = built((size_t)state.range(0));
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(table.group("key"));
}
BENCHMARK(BM_TableGroup)->Arg(100)->Arg(10000);
