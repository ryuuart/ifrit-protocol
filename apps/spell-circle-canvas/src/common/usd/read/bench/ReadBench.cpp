/** @file
 * usd_read_bench — reading a stage into a Model, per triangle. The
 * stages are authored by the Writer into a temporary directory before
 * timing starts, so the bytes read are the same on every machine.
 * Registers nothing when the USD runtime is absent, so the ledger sees
 * an empty run rather than a failure. Run a Release build; Debug
 * numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilusd/read/Reader.h>
#include <sigilusd/runtime/Runtime.h>
#include <sigilusd/write/Writer.h>

#include <cstdio>
#include <filesystem>

using namespace sigil;

namespace {

std::filesystem::path scratch(const char* name) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "sigilusd_read_bench";
  std::filesystem::create_directories(dir);
  return dir / name;
}

/** A crate holding a torus of at least `triangles` triangles, written
 *  once per size; returns the path and the triangle count. */
std::pair<std::filesystem::path, size_t> stageOf(int triangles) {
  int segments = 8;
  while (segments * segments * 2 < triangles) ++segments;
  const geometry::Mesh mesh =
      geometry::mesh::torus(100, 40, segments, segments);
  const std::filesystem::path file =
      scratch(("torus_" + std::to_string(triangles) + ".usdc").c_str());
  usd::Writer writer(file);
  writer.mesh("prop", mesh, glm::mat4(1.0f), world::Material{});
  writer.save();
  return {file, mesh.triangleCount()};
}

void BM_ReadModel(benchmark::State& state) {
  const auto [file, triangles] = stageOf((int)state.range(0));
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(usd::readModel(file));
  state.SetItemsProcessed(state.iterations() * (int64_t)triangles);
}

}  // namespace

int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape): an
                                   // uncaught error ends the run
  std::string why;
  if (usd::runtime::available(&why)) {
    benchmark::RegisterBenchmark("BM_ReadModel", BM_ReadModel)
        ->Arg(128)
        ->Arg(2048)
        ->Arg(32768);
  } else {
    std::fprintf(stderr, "usd_read_bench: nothing to run — %s\n", why.c_str());
  }
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
