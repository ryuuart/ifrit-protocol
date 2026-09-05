/** @file
 * The 3D frame loop: describe, reconcile, extract and draw, on the CPU
 * mesh executor — the floor a set stands on before any device.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilworld/element/Element.h>

namespace {

using namespace sigil::sketch;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

/** A ring of bodies, all turning: the description changes every frame,
 *  so what is measured is reconcile plus draw rather than a still tree. */
struct Ring : Set {
  void setup(SetContext& ctx) override {
    ctx.canvas(480, 320);
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 200, 520};
    ctx.camera(lens);
  }
  world::Frame describe(float seconds) override {
    world::Element root =
        world::Element().key("set").child(world::Element().key("sun").light(
            world::light::sun({-0.4f, -0.8f, -0.3f}, {1, 1, 1, 1}, 1.0f)));
    for (int i = 0; i < 24; ++i) {
      const float angle = (float)i * 15.0f + seconds * 30.0f;
      root =
          root.child(world::Element()
                         .key("body" + std::to_string(i))
                         .rotateY(angle)
                         .at({160.0f, 0, 0})
                         .mesh(gm::superellipsoid({20, 20, 20}, 0.2f, 16, 12))
                         .fill(sigil::material::kit::surface()));
    }
    return root;
  }
};

void Frame(benchmark::State& state) {
  std::unique_ptr<Session> session = kindOf<Ring>()->open(fonts(), assets());
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(480, 320));
  SkCanvas canvas(bitmap);
  for (int i = 0; i < 8; ++i) session->frame(canvas, 1.0 / 60.0);
  for (auto&& _ : state) session->frame(canvas, 1.0 / 60.0);
}
BENCHMARK(Frame)->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
