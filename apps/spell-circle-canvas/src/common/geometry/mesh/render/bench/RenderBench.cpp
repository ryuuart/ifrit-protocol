/** @file
 * Benchmarks of the mesh draw: the built-in runtime by triangle count
 * and by shading mode, and the panel concat.
 */

// geometry_mesh_render_bench — what one drawn frame costs on the
// built-in runtime: the per-vertex shade and the per-triangle sort as
// the mesh grows, the three shading modes against each other, and the
// panel path that only concats a matrix. Run a Release build; Debug
// numbers say nothing.

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/render/Painter.h>

#include <glm/glm.hpp>

using namespace sigil::geometry::mesh;

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 480;

sk_sp<SkSurface> target() {
  return SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWidth, kHeight));
}

camera::Camera scene() {
  camera::Camera cam;
  cam.eye = {0, 120, 520};
  return cam;
}

void countTriangles(benchmark::State& state, const Mesh& m) {
  state.counters["triangles/s"] = benchmark::Counter(
      (double)m.triangleCount(), benchmark::Counter::kIsIterationInvariantRate);
  state.SetComplexityN((int64_t)m.triangleCount());
}

void BM_DrawMesh(benchmark::State& state) {
  const int n = (int)state.range(0);
  const Mesh m = torus(120, 40, n, n / 2);
  sk_sp<SkSurface> surface = target();
  const camera::Camera cam = scene();
  render::MeshStyle style;
  for ([[maybe_unused]] auto iteration : state) {
    render::drawMesh(*surface->getCanvas(), m, glm::mat4(1.0f), cam,
                     {kWidth, kHeight}, style);
  }
  countTriangles(state, m);
}
BENCHMARK(BM_DrawMesh)
    ->RangeMultiplier(4)
    ->Range(16, 256)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_DrawMeshMode(benchmark::State& state) {
  const Mesh m = torus(120, 40, 96, 48);
  sk_sp<SkSurface> surface = target();
  const camera::Camera cam = scene();
  render::MeshStyle style;
  style.mode = (render::MeshStyle::Mode)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    render::drawMesh(*surface->getCanvas(), m, glm::mat4(1.0f), cam,
                     {kWidth, kHeight}, style);
  }
  countTriangles(state, m);
}
BENCHMARK(BM_DrawMeshMode)
    ->DenseRange(0, 2, 1)
    ->ArgNames({"mode"})
    ->Unit(benchmark::kMicrosecond);

// Unsorted and unculled: the sort and the cull are the two per-triangle
// passes a runtime can skip, so their cost is visible as the difference
// from BM_DrawMesh at the same size.
void BM_DrawMeshUnsorted(benchmark::State& state) {
  const Mesh m = torus(120, 40, 96, 48);
  sk_sp<SkSurface> surface = target();
  const camera::Camera cam = scene();
  render::MeshStyle style;
  style.depthSort = false;
  style.backfaceCull = false;
  for ([[maybe_unused]] auto iteration : state) {
    render::drawMesh(*surface->getCanvas(), m, glm::mat4(1.0f), cam,
                     {kWidth, kHeight}, style);
  }
  countTriangles(state, m);
}
BENCHMARK(BM_DrawMeshUnsorted)->Unit(benchmark::kMicrosecond);

void BM_DrawPanel(benchmark::State& state) {
  sk_sp<SkSurface> surface = target();
  const camera::Camera cam = scene();
  const glm::mat4 model = camera::place({0, 0, 0}, 24, -10);
  for ([[maybe_unused]] auto iteration : state) {
    render::drawPanel(*surface->getCanvas(), model, cam, {kWidth, kHeight},
                      [](SkCanvas& local) {
                        SkPaint paint;
                        paint.setColor4f({0.9f, 0.4f, 0.2f, 1});
                        local.drawRect(SkRect::MakeXYWH(-120, -80, 240, 160),
                                       paint);
                      });
  }
}
BENCHMARK(BM_DrawPanel)->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
