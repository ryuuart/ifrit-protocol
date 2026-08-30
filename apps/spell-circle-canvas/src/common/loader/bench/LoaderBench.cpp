// loader_bench — the hub's hot paths per call, with the disk kept out
// of the loop: a blob served from the entry cache, a typed load whose
// view is already decoded (the registry dispatch and cache lookup that
// every frame-rate consumer pays), URI resolution against the mount
// table, and the key a network URL is cached under. The one file each
// URI names is written once, before timing. Run a Release build; Debug
// numbers say nothing.

#include <benchmark/benchmark.h>
#include <sigilloader/Loader.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace sigil::loader;

namespace {

/** A mounted directory holding `files` small resources, torn down with
 *  the fixture. */
struct Mounted {
  std::filesystem::path dir;
  Hub hub;
  std::vector<std::string> uris;

  explicit Mounted(int files) {
    dir = std::filesystem::temp_directory_path() /
          ("loader_bench_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    for (int i = 0; i < files; ++i) {
      const std::string name = "res_" + std::to_string(i) + ".txt";
      std::ofstream(dir / name) << std::string(4096, 'x');
      uris.push_back("res://bench/" + name);
    }
    hub.mount("res://bench/", dir);
  }
  ~Mounted() { std::filesystem::remove_all(dir); }
};

void countCalls(benchmark::State& state, int64_t calls) {
  state.counters["calls/s"] = benchmark::Counter(
      (double)calls, benchmark::Counter::kIsIterationInvariantRate);
}

/** A cache hit: the bytes were fetched once before timing, so each call
 *  is the key construction and the entry lookup. */
void BM_Blob_CacheHit(benchmark::State& state) {
  Mounted fixture((int)state.range(0));
  for (const std::string& uri : fixture.uris) (void)fixture.hub.blob(uri);
  for ([[maybe_unused]] auto iteration : state) {
    for (const std::string& uri : fixture.uris) {
      std::shared_ptr<const Bytes> bytes = fixture.hub.blob(uri);
      benchmark::DoNotOptimize(bytes.get());
    }
  }
  countCalls(state, (int64_t)fixture.uris.size());
}
BENCHMARK(BM_Blob_CacheHit)
    ->Arg(1)
    ->Arg(64)
    ->Arg(1024)
    ->Unit(benchmark::kMicrosecond);

struct Length {
  size_t value = 0;
};

/** A typed load whose view is already decoded: the type-indexed
 *  decoder lookup, the entry lookup and the view lookup. */
void BM_Load_ViewHit(benchmark::State& state) {
  Mounted fixture((int)state.range(0));
  fixture.hub.registerDecoder<Length>(
      [](const Bytes& bytes, std::string_view) -> std::optional<Length> {
        return Length{bytes.bytes.size()};
      });
  for (const std::string& uri : fixture.uris)
    (void)fixture.hub.load<Length>(uri);
  for ([[maybe_unused]] auto iteration : state) {
    for (const std::string& uri : fixture.uris) {
      std::shared_ptr<const Length> length = fixture.hub.load<Length>(uri);
      benchmark::DoNotOptimize(length.get());
    }
  }
  countCalls(state, (int64_t)fixture.uris.size());
}
BENCHMARK(BM_Load_ViewHit)
    ->Arg(1)
    ->Arg(64)
    ->Arg(1024)
    ->Unit(benchmark::kMicrosecond);

/** The mount-table walk that turns a URI into a path, against a table
 *  of `mounts` prefixes where the last and longest wins. */
void BM_Resolve(benchmark::State& state) {
  const int mounts = (int)state.range(0);
  Hub hub;
  std::string prefix = "res://";
  for (int i = 0; i < mounts; ++i) {
    prefix += "m" + std::to_string(i) + "/";
    hub.mount(prefix, std::filesystem::path("/tmp") / std::to_string(i));
  }
  const std::string uri = prefix + "leaf.png";
  for ([[maybe_unused]] auto iteration : state) {
    std::filesystem::path path = hub.resolve(uri);
    benchmark::DoNotOptimize(path);
  }
  countCalls(state, 1);
}
BENCHMARK(BM_Resolve)->Arg(1)->Arg(8)->Arg(64);

void BM_NetworkCacheKey(benchmark::State& state) {
  const std::string url =
      "https://example.invalid/assets/textures/albedo_4k_v03.png?rev=17";
  for ([[maybe_unused]] auto iteration : state) {
    std::string key = networkCacheKey(url);
    benchmark::DoNotOptimize(key.data());
  }
  countCalls(state, 1);
}
BENCHMARK(BM_NetworkCacheKey);

}  // namespace

BENCHMARK_MAIN();
