/** @file
 * What a pass costs on the CPU: a geometry pass at several body counts,
 * the same pass culled to a tenth of them, and the three post ops over
 * one target.
 */

#include <benchmark/benchmark.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/frame/Runtime.h>

#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::world;

namespace {

constexpr SkISize kExtent{512, 512};

Mesh triangle() {
  Mesh m;
  m.positions = {{-6, -6, 0}, {6, -6, 0}, {0, 6, 0}};
  m.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  m.uvs = {{0, 0}, {1, 0}, {0.5f, 1}};
  m.indices = {0, 1, 2};
  return m;
}

/** The bodies a pass runs over, with every tenth of them tagged, and the
 *  storage their spans point into. */
struct Bodies {
  Mesh mesh = triangle();
  std::vector<std::string> keys;
  std::vector<std::vector<std::string>> tags;
  std::vector<Draw> draws;
  std::vector<Light> lights;

  explicit Bodies(int count) {
    keys.reserve((size_t)count);
    tags.reserve((size_t)count);
    for (int i = 0; i < count; ++i) {
      keys.push_back("n" + std::to_string(i));
      tags.push_back(i % 10 == 0 ? std::vector<std::string>{"glow"}
                                 : std::vector<std::string>{});
    }
    draws.reserve((size_t)count);
    for (int i = 0; i < count; ++i) {
      const int column = i % 32;
      const int row = i / 32;
      Draw draw;
      draw.mesh = &mesh;
      draw.world = glm::translate(
          glm::mat4(1.0f),
          {(float)column * 14.0f - 220.0f, (float)row * 14.0f - 120.0f, 0.0f});
      draw.key = keys[(size_t)i];
      draw.tags = tags[(size_t)i];
      draws.push_back(draw);
    }
  }

  View view() const {
    View v;
    v.draws = draws;
    v.lights = lights;
    v.camera.eye = {0, 0, 620};
    v.extent = kExtent;
    return v;
  }
};

PassWork workOf(const Pass& pass, Selection realisation) {
  PassWork work;
  work.pass = &pass;
  work.realisation = realisation;
  return work;
}

Targets targets() {
  Targets t;
  t.extent(kExtent);
  return t;
}

void GeometryPass(benchmark::State& state) {
  const int bodies = (int)state.range(0);
  const Bodies scene(bodies);
  const Pass pass = geometryPass("main").writes("colour");
  Targets into = targets();
  for ([[maybe_unused]] auto iteration : state)
    Runtime::cpu()->execute(workOf(pass, Selection::None), scene.view(), into);
  state.SetItemsProcessed(state.iterations() * bodies);
}
BENCHMARK(GeometryPass)->Arg(64)->Arg(512)->Arg(2048);

void CulledPass(benchmark::State& state) {
  const int bodies = (int)state.range(0);
  const Bodies scene(bodies);
  const Pass pass =
      geometryPass("glow").only(sel::tag("glow")).writes("colour");
  Targets into = targets();
  for ([[maybe_unused]] auto iteration : state)
    Runtime::cpu()->execute(workOf(pass, Selection::Cull), scene.view(), into);
  state.SetItemsProcessed(state.iterations() * bodies);
}
BENCHMARK(CulledPass)->Arg(64)->Arg(512)->Arg(2048);

void PostBlur(benchmark::State& state) {
  const Bodies scene(64);
  Targets into = targets();
  const Pass main = geometryPass("main").writes("colour");
  Runtime::cpu()->execute(workOf(main, Selection::None), scene.view(), into);
  const Pass blur = postPass("blur").reads("colour").writes("soft").blur(6.0f);
  for ([[maybe_unused]] auto iteration : state)
    Runtime::cpu()->execute(workOf(blur, Selection::None), scene.view(), into);
}
BENCHMARK(PostBlur);

void PostComposite(benchmark::State& state) {
  const Bodies scene(64);
  Targets into = targets();
  const Pass main = geometryPass("main").writes("colour");
  Runtime::cpu()->execute(workOf(main, Selection::None), scene.view(), into);
  into.keep("trail");
  const Pass trail = postPass("trail")
                         .reads("colour")
                         .previous("trail")
                         .writes("trail")
                         .composite(SkBlendMode::kPlus, 0.85f);
  for ([[maybe_unused]] auto iteration : state) {
    Runtime::cpu()->execute(workOf(trail, Selection::None), scene.view(), into);
    into.endFrame();
  }
}
BENCHMARK(PostComposite);

}  // namespace

BENCHMARK_MAIN();
