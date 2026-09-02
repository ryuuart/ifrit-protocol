/** @file
 * usd_write_bench — authoring a mesh into a stage, per triangle, and
 * saving a stage as crate and as ASCII, into a temporary directory.
 * Registers nothing when the USD runtime is absent, so the ledger sees
 * an empty run rather than a failure. Run a Release build; Debug
 * numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilusd/runtime/Runtime.h>
#include <sigilusd/write/Writer.h>

#include <cstdio>
#include <filesystem>

using namespace sigil;

namespace {

std::filesystem::path scratch(const char* name) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "sigilusd_write_bench";
  std::filesystem::create_directories(dir);
  return dir / name;
}

/** A torus of at least `triangles` triangles. */
geometry::mesh::Mesh torusOf(int triangles) {
  int segments = 8;
  while (segments * segments * 2 < triangles) ++segments;
  return geometry::mesh::torus(100, 40, segments, segments);
}

void BM_Mesh(benchmark::State& state) {
  const geometry::mesh::Mesh mesh = torusOf((int)state.range(0));
  const material::Material surface = material::kit::surface();
  for ([[maybe_unused]] auto _ : state) {
    usd::Writer writer(scratch("mesh.usdc"));
    benchmark::DoNotOptimize(
        writer.mesh("prop", mesh, glm::mat4(1.0f), surface));
  }
  state.SetItemsProcessed(state.iterations() * (int64_t)mesh.triangleCount());
}

void BM_Save(benchmark::State& state) {
  const geometry::mesh::Mesh mesh = torusOf(2048);
  const bool ascii = state.range(0) != 0;
  const std::filesystem::path file = scratch(ascii ? "save.usda" : "save.usdc");
  for ([[maybe_unused]] auto _ : state) {
    usd::Writer writer(file);
    writer.mesh("prop", mesh, glm::mat4(1.0f), material::kit::surface());
    benchmark::DoNotOptimize(writer.save());
  }
}

}  // namespace

int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape): an
                                   // uncaught error ends the run
  std::string why;
  if (usd::runtime::available(&why)) {
    benchmark::RegisterBenchmark("BM_Mesh", BM_Mesh)
        ->Arg(128)
        ->Arg(2048)
        ->Arg(32768);
    benchmark::RegisterBenchmark("BM_Save", BM_Save)
        ->Arg(0)
        ->Arg(1)
        ->Unit(benchmark::kMillisecond);
  } else {
    std::fprintf(stderr, "usd_write_bench: nothing to run — %s\n", why.c_str());
  }
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
