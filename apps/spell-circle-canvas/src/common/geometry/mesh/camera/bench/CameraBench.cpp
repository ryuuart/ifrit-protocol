/** @file
 * Benchmarks of the camera transforms: building a view-projection, the
 * matrix seam, and the two placement helpers.
 */

// geometry_mesh_camera_bench — the per-frame transform work every draw
// pays before it touches a vertex. These are small, fixed-cost matrix
// builds, so the numbers are per call rather than per element. Run a
// Release build; Debug numbers say nothing.

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/camera/Camera.h>

#include <glm/glm.hpp>

using namespace sigil::geometry::mesh;

namespace {

camera::Camera scene() {
  camera::Camera cam;
  cam.eye = {120, 180, 640};
  cam.target = {0, 20, 0};
  return cam;
}

void BM_ViewProjection(benchmark::State& state) {
  const camera::Camera cam = scene();
  for ([[maybe_unused]] auto iteration : state) {
    glm::mat4 vp = cam.viewProjection({1920, 1080});
    benchmark::DoNotOptimize(vp);
  }
}
BENCHMARK(BM_ViewProjection)->Unit(benchmark::kNanosecond);

void BM_View(benchmark::State& state) {
  const camera::Camera cam = scene();
  for ([[maybe_unused]] auto iteration : state) {
    glm::mat4 v = cam.view();
    benchmark::DoNotOptimize(v);
  }
}
BENCHMARK(BM_View)->Unit(benchmark::kNanosecond);

void BM_ToSkM44(benchmark::State& state) {
  const glm::mat4 m = scene().viewProjection({1920, 1080});
  for ([[maybe_unused]] auto iteration : state) {
    SkM44 out = camera::toSkM44(m);
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_ToSkM44)->Unit(benchmark::kNanosecond);

void BM_Place(benchmark::State& state) {
  for ([[maybe_unused]] auto iteration : state) {
    glm::mat4 m = camera::place({10, 20, 30}, 24, -12, 6, 1.4f);
    benchmark::DoNotOptimize(m);
  }
}
BENCHMARK(BM_Place)->Unit(benchmark::kNanosecond);

void BM_FaceCamera(benchmark::State& state) {
  for ([[maybe_unused]] auto iteration : state) {
    glm::mat4 m = camera::faceCamera({120, 180, 640}, {0, 20, 0});
    benchmark::DoNotOptimize(m);
  }
}
BENCHMARK(BM_FaceCamera)->Unit(benchmark::kNanosecond);

}  // namespace

BENCHMARK_MAIN();
