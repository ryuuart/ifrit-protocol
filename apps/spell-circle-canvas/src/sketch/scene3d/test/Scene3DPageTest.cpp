/** @file
 * A scene3d sketch's pages and screens: a compose tree painted into a
 * texture or a surface's maps and worn by a body, on the device it is drawn
 * on or on the CPU where there is none, each body wearing its own region.
 *
 * Every case that opens a scene3d sketch draws it on the device and SKIPS where
 * the machine has no Vulkan runtime; its suite carries the `gpu` label.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <memory>
#include <vector>

#include "Scene3DTestSupport.h"
#include "support/Fixtures.h"
#include "support/Pixels.h"

namespace {

using namespace sigil::sketch;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;
using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::sketch::test::oneFrame;
using sigil::sketch::test::samePicture;
using sigil::sketch::test::Scene3DDoors;
using sigil::sketch::test::shadedFrame;

/** A scene3d sketch that paints a compose sheet into a texture the session
 *  keeps, and wears it on one unlit card facing the camera. */
struct Screened {
  std::shared_ptr<sigil::compose::TextureScene> screen;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
    screen = ctx.textureScene({64, 64});
  }
  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    using namespace sigil::compose;
    screen->render(box().width(64).height(64).fill(Fill::color({0, 1, 0, 1})),
                   seconds);
    sigil::material::Material surface =
        sigil::material::surface::unlit({.baseColor = {1, 1, 1, 1}});
    surface.slot(sigil::material::surface::kBaseColorSlot, screen->texture());
    return world::group(
               {world::mesh(gm::quad(120, 90)).key("card").fill(surface)})
        .key("set");
  }
};

/** A scene3d sketch that paints a compose page as a surface's maps the session
 *  keeps — a lit panel under a flat green label — and wears it on one card
 *  under a sun. */
struct Surfaced {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    return world::Frame(
               world::group({world::directionalLight({0, 0, -1}).key("sun"),
                             world::mesh(gm::quad(120, 90))
                                 .key("card")
                                 .fill(page.surface())})
                   .key("set"))
        .pass(page.render(
            box()
                .width(64)
                .height(64)
                .fill(material::from(material::Color{.6f, .6f, .6f, 1})
                          .surface({.roughness = .9f}))
                .children({box()
                               .absolute()
                               .rect(16, 16, 32, 32)
                               .fill(material::Color{0, 1, 0, 1})})));
  }
};

/** One page laid out as two halves, red and blue, each stated unlit, and
 *  two cards side by side wearing a half each. */
struct HalvedPage {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
    page = ctx.page({64, 32});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    const auto unlit = [](material::Color colour) {
      return material::from(colour).surface({.unlit = true});
    };
    return world::Frame(
               world::group(
                   {world::mesh(gm::quad(60, 60))
                        .key("left card")
                        .at({-40, 0, 0})
                        .fill(page.surface(material::PixelRect{0, 0, 32, 32})),
                    world::mesh(gm::quad(60, 60))
                        .key("right card")
                        .at({40, 0, 0})
                        .fill(
                            page.surface(material::PixelRect{32, 0, 32, 32}))})
                   .key("set"))
        .pass(page.render(box().width(64).height(32).children({
            box().absolute().rect(0, 0, 32, 32).fill(unlit({1, 0, 0, 1})),
            box().absolute().rect(32, 0, 32, 32).fill(unlit({0, 0, 1, 1})),
        })));
  }
};

TEST_F(Scene3DDoors, PaintsATextureSceneOntoABody) {
  std::unique_ptr<Session> session =
      kindOf<Screened>()->open(fonts(), assets());
  // The card is unlit and wears the sheet, so what the middle of the
  // picture shows is what the compose tree painted: green, and nothing
  // of the black ground or of any lighting.
  const SkBitmap picture = oneFrame(*session);
  const SkColor centre = picture.getColor(80, 60);
  EXPECT_GT(SkColorGetG(centre), 200u);
  EXPECT_LT(SkColorGetR(centre), 40u);
  EXPECT_LT(SkColorGetB(centre), 40u);
}

/** The screen the last `Screened` session made, so a case can ask where
 *  it stands. */
std::weak_ptr<sigil::compose::TextureScene>& lastScreen() {
  static std::weak_ptr<sigil::compose::TextureScene> screen;
  return screen;
}

/** `Screened`, telling the case which screen it was handed. */
struct ScreenedAndSeen : Screened {
  void setup(Scene3DContext& ctx) {
    Screened::setup(ctx);
    lastScreen() = screen;
  }
};

TEST_F(Scene3DDoors, ALiveScene3DPaintsItsScreensOnTheDeviceItIsDrawnOn) {
  // On the CPU every material a screen or a page paints is interpreted
  // pixel by pixel, which for a page of material is seconds a frame; a
  // live scene3d sketch's screens stand on the device it draws on, and a
  // capture that will be diffed keeps the CPU's pixels.
  const sigil::geometry::device::test::OnDevice on =
      sigil::geometry::device::test::onDevice();
  ASSERT_TRUE(on);
  useDevice(on.device);
  for (const bool deterministic : {false, true}) {
    SCOPED_TRACE(deterministic ? "a capture" : "a window");
    std::unique_ptr<Session> session =
        kindOf<ScreenedAndSeen>()->open(fonts(), assets(), deterministic);
    const SkBitmap picture = oneFrame(*session);
    const std::shared_ptr<sigil::compose::TextureScene> screen =
        lastScreen().lock();
    ASSERT_NE(screen, nullptr);
    EXPECT_EQ((bool)screen->deviceImage(), !deterministic)
        << (deterministic ? "a capture" : "a window");
    const SkColor centre = picture.getColor(80, 60);
    EXPECT_GT(SkColorGetG(centre), 200u);
    EXPECT_LT(SkColorGetR(centre), 40u);
  }
  useDevice(nullptr);
}

TEST_F(Scene3DDoors, APageCapturedOnTheDeviceIsThePageTheWindowPaints) {
  // A capture that will be diffed paints its page on the device the window
  // paints it on, so the frame a capture takes is the frame the window
  // shows, pixel for pixel.
  const sigil::geometry::device::test::OnDevice on =
      sigil::geometry::device::test::onDevice();
  ASSERT_TRUE(on);
  useDevice(on.device);
  const SkBitmap window = shadedFrame(
      *kindOf<Surfaced>()->open(fonts(), assets(), /*deterministic=*/false));
  const SkBitmap capture = shadedFrame(
      *kindOf<Surfaced>()->open(fonts(), assets(), /*deterministic=*/true));
  EXPECT_TRUE(samePicture(window, capture));
  // The page a capture asks for stands on that device.
  std::vector<std::shared_ptr<sigil::compose::SurfaceScene>> pages;
  Scene3DContext ctx{.assets = assets(),
                     .fonts = fonts(),
                     .pages = &pages,
                     .deterministic = true};
  (void)ctx.page({16, 16});
  ASSERT_EQ(pages.size(), 1u);
  pages.front()->render(sigil::compose::box().width(16).height(16).fill(
      sigil::compose::Fill::color({0, 1, 0, 1})));
  const auto* base =
      pages.front()->scene(sigil::material::texture::Role::BaseColor);
  ASSERT_NE(base, nullptr);
  EXPECT_TRUE((bool)base->deviceImage());
  useDevice(nullptr);
}

TEST(Scene3DPages, ACaptureWithNoDevicePaintsItsPageOnTheCpu) {
  // Where the process holds no device a page paints on the CPU, and paints.
  useDevice(nullptr);
  std::vector<std::shared_ptr<sigil::compose::SurfaceScene>> pages;
  Scene3DContext ctx{.assets = assets(),
                     .fonts = fonts(),
                     .pages = &pages,
                     .deterministic = true};
  (void)ctx.page({16, 16});
  ASSERT_EQ(pages.size(), 1u);
  pages.front()->render(sigil::compose::box().width(16).height(16).fill(
      sigil::compose::Fill::color({0, 1, 0, 1})));
  const auto* base =
      pages.front()->scene(sigil::material::texture::Role::BaseColor);
  ASSERT_NE(base, nullptr);
  EXPECT_FALSE((bool)base->deviceImage());
  ASSERT_NE(base->image(), nullptr);
  EXPECT_FALSE(base->image()->isTextureBacked());
  SkBitmap pixels;
  pixels.allocN32Pixels(16, 16);
  ASSERT_TRUE(base->image()->readPixels(nullptr, pixels.pixmap(), 0, 0));
  EXPECT_EQ(pixels.getColor(8, 8), SK_ColorGREEN);
}

TEST_F(Scene3DDoors, PaintsASurfaceSceneThatTheScene3DLights) {
  std::unique_ptr<Session> session =
      kindOf<Surfaced>()->open(fonts(), assets());
  const SkBitmap picture = shadedFrame(*session);
  // The flat label takes the sun as the page's base colour and comes out
  // green; the lit panel around it is grey under the same sun.
  const SkColor label = picture.getColor(80, 60);
  EXPECT_GT(SkColorGetG(label), 100u);
  EXPECT_GT(SkColorGetG(label), SkColorGetR(label) + 60);
  EXPECT_GT(SkColorGetG(label), SkColorGetB(label) + 60);
  const SkColor panel = picture.getColor(48, 60);
  EXPECT_GT(SkColorGetR(panel), 15u);
  EXPECT_NEAR(int(SkColorGetR(panel)), int(SkColorGetG(panel)), 12);
}

TEST_F(Scene3DDoors, EachBodyWearsItsOwnRegionOfOnePage) {
  std::unique_ptr<Session> session =
      kindOf<HalvedPage>()->open(fonts(), assets());
  const SkBitmap picture = shadedFrame(*session);
  // The left card wears the page's red half and the right card its blue
  // half, each whole across the card.
  const SkColor left = picture.getColor(55, 60);
  EXPECT_GT(SkColorGetR(left), 150u);
  EXPECT_LT(SkColorGetB(left), 60u);
  const SkColor right = picture.getColor(105, 60);
  EXPECT_GT(SkColorGetB(right), 150u);
  EXPECT_LT(SkColorGetR(right), 60u);
}

}  // namespace
