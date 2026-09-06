// SigilData decode benchmarks: a delimiter-separated file and a JSON
// document of the same shape, both read into a Table from bytes already
// in memory, so the disk stays out of the timed loop.

#include <benchmark/benchmark.h>
#include <sigildata/decode/Csv.h>
#include <sigildata/decode/Json.h>

#include <string>

static std::string csvOf(size_t rows) {
  std::string text = "name,value,when\n";
  for (size_t i = 0; i < rows; ++i)
    text += "row" + std::to_string(i) + "," + std::to_string(i * 3 % 997) +
            ",2019-03-08\n";
  return text;
}

static std::string jsonOf(size_t rows) {
  std::string text = "[";
  for (size_t i = 0; i < rows; ++i) {
    if (i) text += ',';
    text += R"({"name":"row)" + std::to_string(i) + R"(","value":)" +
            std::to_string(i * 3 % 997) + "}";
  }
  return text + "]";
}

static void BM_DecodeCsv(benchmark::State& state) {
  const std::string text = csvOf((size_t)state.range(0));
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(sigil::data::decodeCsv(text));
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_DecodeCsv)->Arg(100)->Arg(10000);

static void BM_DecodeJsonTable(benchmark::State& state) {
  const std::string text = jsonOf((size_t)state.range(0));
  for ([[maybe_unused]] auto _ : state) {
    const auto document = sigil::data::decodeJson(text);
    benchmark::DoNotOptimize(sigil::data::tableFromJson(*document));
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_DecodeJsonTable)->Arg(100)->Arg(10000);
