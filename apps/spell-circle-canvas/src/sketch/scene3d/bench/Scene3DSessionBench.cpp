/** @file
 * The 3D frame loop: describe, reconcile, extract and draw, on the device
 * a scene3d sketch is drawn on. An arm with no Vulkan runtime to ask says so
 * and measures nothing.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/GraphiteTypes.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/PaintOrder.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilworld/World.h>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "support/Devices.h"

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
struct Ring {
  void setup(Scene3DContext& ctx) {
    ctx.canvas(480, 320);
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 200, 520};
    ctx.camera(lens);
  }
  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    return world::group(
               {world::directionalLight({-0.4f, -0.8f, -0.3f}).key("sun"),
                world::each(
                    24,
                    [seconds](size_t i) {
                      const float angle = (float)i * 15.0f + seconds * 30.0f;
                      return world::mesh(
                                 gm::superellipsoid({20, 20, 20}, 0.2f, 16, 12))
                          .key("body" + std::to_string(i))
                          .rotateY(angle)
                          .at({160.0f, 0, 0})
                          .fill(sigil::material::surface::program());
                    })})
        .key("set");
  }
};

void Scene3DFrame(benchmark::State& state) {
  std::string why;
  if (!sigil::sketch::test::installDeviceRuntime(&why)) {
    state.SkipWithError(("no Vulkan device: " + why).c_str());
    return;
  }
  std::unique_ptr<Session> session = kindOf<Ring>()->open(fonts(), assets());
  useRuntime({});
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(480, 320));
  SkCanvas canvas(bitmap);
  for (int i = 0; i < 8; ++i) session->frame(canvas, 1.0 / 60.0);
  for (auto&& _ : state) session->frame(canvas, 1.0 / 60.0);
}
BENCHMARK(Scene3DFrame)->Unit(benchmark::kMicrosecond);

/** THE RING THROUGH A PANE, magnified by the argument about its centre,
 *  the way a window zoomed into a scene3d sketch
 *  draws it: the pane is the declared canvas's size at every zoom, so
 *  the work a frame does is meant to stay where it is at one while the
 *  picture it is a piece of grows with the square of the zoom. */
void Scene3DFrameThroughPane(benchmark::State& state) {
  const float zoom = (float)state.range(0);
  std::string why;
  if (!sigil::sketch::test::installDeviceRuntime(&why)) {
    state.SkipWithError(("no Vulkan device: " + why).c_str());
    return;
  }
  std::unique_ptr<Session> session = kindOf<Ring>()->open(fonts(), assets());
  useRuntime({});
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(480, 320));
  SkCanvas canvas(bitmap);
  canvas.translate(240.0f, 160.0f);
  canvas.scale(zoom, zoom);
  canvas.translate(-240.0f, -160.0f);
  for (int i = 0; i < 8; ++i) session->frame(canvas, 1.0 / 60.0);
  for (auto&& _ : state) session->frame(canvas, 1.0 / 60.0);
}
BENCHMARK(Scene3DFrameThroughPane)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Unit(benchmark::kMicrosecond);

/** SIX SCREENS WORN BY CARDS, each a compose scene painted on the device
 *  — painted once at setup and left standing, or painted again every
 *  frame, as the arm says. */
struct WearingScreens {
  static inline bool repainting = false;
  std::array<std::shared_ptr<sigil::compose::TextureScene>, 6> screens;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(480, 320);
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 520};
    ctx.camera(lens);
    for (auto& screen : screens) {
      screen = ctx.textureScene({256, 256});
      screen->render(face(0.0));
    }
  }
  static sigil::compose::Element face(double seconds) {
    return sigil::compose::box()
        .width(256)
        .height(256)
        .fill(sigil::compose::Fill::color(
            {0.2f, 0.4f, 0.5f + 0.5f * (float)std::fmod(seconds, 1.0), 1.0f}));
  }
  world::Frame describe(sigil::motion::Duration now) {
    std::vector<world::Node> cards;
    for (size_t i = 0; i < screens.size(); ++i) {
      if (repainting) screens[i]->render(face(now.count()), now.count());
      sigil::material::Material surface = sigil::material::surface::unlit(
          {.baseColor = {1, 1, 1, 1}});
      surface.slot(sigil::material::surface::kBaseColorSlot,
                   screens[i]->texture());
      cards.push_back(world::mesh(gm::quad(120, 120))
                          .key("card" + std::to_string(i))
                          .at({(float)i * 130.0f - 325.0f, 0, 0})
                          .fill(std::move(surface)));
    }
    return world::group(std::move(cards)).key("cards");
  }
};

/** A SCENE3D FRAME WEARING SIX DEVICE SCREENS, drawn the way a window
 *  draws one: through the paint-order canvas on the device's Graphite,
 *  its picture presented where the device drew it, and the frame
 *  submitted and finished before the next. Screens standing still
 *  (`repainted:0`) are worn as they stand — nothing read back to place
 *  them, nothing waited for and no levels filled again — so what remains
 *  is the frame itself; screens painted every frame (`repainted:1`) pay
 *  their painting, one wait and one fill each. */
void Scene3DFrameWearingScreens(benchmark::State& state) {
  std::string why;
  if (!sigil::sketch::test::installDeviceRuntime(&why)) {
    state.SkipWithError(("no Vulkan device: " + why).c_str());
    return;
  }
  const sigil::geometry::device::test::OnDevice on =
      sigil::geometry::device::test::onDevice();
  sigil::skia::GraphiteContext* graphite = on ? on->graphite() : nullptr;
  if (!graphite) {
    useRuntime({});
    state.SkipWithError("the device carries no Graphite");
    return;
  }
  useDevice(on.device);
  WearingScreens::repainting = state.range(0) != 0;
  {
    std::unique_ptr<Session> session =
        kindOf<WearingScreens>()->open(fonts(), assets(), false);
    sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
        graphite->recorder(), SkImageInfo::MakeN32Premul(480, 320));
    const auto frame = [&] {
      {
        sigil::skia::PaintOrderCanvas ordered(*graphite, surface->getCanvas());
        session->frame(ordered, 1.0 / 60.0);
      }
      (void)graphite->submit(*graphite->recorder(),
                             skgpu::graphite::SyncToCpu::kYes);
    };
    for (int i = 0; i < 8; ++i) frame();
    for (auto&& _ : state) frame();
  }
  useDevice(nullptr);
  useRuntime({});
}
BENCHMARK(Scene3DFrameWearingScreens)
    ->Arg(0)
    ->Arg(1)
    ->ArgName("repainted")
    ->Unit(benchmark::kMicrosecond);

/** A SCENE2D SKETCH BAKING THE RING into a panel of its page every frame,
 *  at the density the arm states: the panel is the same canvas units at
 *  every density, so what grows is the pixels the bake lands on — the
 *  price of a bake taken at the device's own scale. */
struct BakingRing {
  static inline float density = 1.0f;
  Ring ring;
  sk_sp<SkImage> picture;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(480, 320);
    ctx.composer.render(sigil::compose::box().width(10).height(10));
  }
  void update(double seconds, Scene2DContext& ctx) {
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 200, 520};
    picture = ctx.bakeScene3D(
        ring.describe(sigil::motion::Duration(seconds)), {320, 200},
        {.camera = lens, .density = density});
  }
};

void Scene2DBakeAtDensity(benchmark::State& state) {
  std::string why;
  if (!sigil::sketch::test::installDeviceRuntime(&why)) {
    state.SkipWithError(("no Vulkan device: " + why).c_str());
    return;
  }
  BakingRing::density = (float)state.range(0);
  std::unique_ptr<Session> session =
      kindOf<BakingRing>()->open(fonts(), assets());
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(480, 320));
  SkCanvas canvas(bitmap);
  for (int i = 0; i < 4; ++i) session->frame(canvas, 1.0 / 60.0);
  for (auto&& _ : state) session->frame(canvas, 1.0 / 60.0);
  session.reset();
  useRuntime({});
}
BENCHMARK(Scene2DBakeAtDensity)
    ->Arg(1)
    ->Arg(2)
    ->Arg(3)
    ->ArgName("density")
    ->Unit(benchmark::kMillisecond);

}  // namespace
