// A page that keeps its pixels and repaints only what changed: the region a
// composer answers for a draw, and a page repainted region by region frame
// after frame against the same page painted whole once.

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "support/Host.h"
#include "support/Rows.h"
#include "support/SurfaceSceneFixture.h"

namespace {

using sigil::material::texture::Role;

/** A ground with a small square on it and a second square that never
 *  changes. */
Element twoSquares(material::Color changing) {
  return box()
      .width(200)
      .height(200)
      .fill(material::Color{.1f, .1f, .1f, 1})
      .children({
          box().absolute().rect(10, 10, 30, 30).fill(changing),
          box()
              .absolute()
              .rect(100, 100, 50, 50)
              .fill(material::Color{.2f, .6f, .2f, 1}),
      });
}

/** One frame of a page with something of everything on it: a lit ground of
 *  noise, a passage of a stated size whose words change, a square under a
 *  blur whose colour changes, a square that moves, a square that comes and
 *  goes, and lit type that holds still. */
Element busyPage(int frame) {
  static const char8_t* const kWords[] = {u8"10.25", u8"10.50", u8"11.75",
                                          u8"8.00", u8"12.25"};
  const material::Material ground =
      material::from(material::noise(.08f)).surface({.roughness = .7f});
  const material::Material lit =
      material::from(material::Color{.8f, .7f, .3f, 1})
          .surface({.metallic = 1.0f, .roughness = .3f});
  const float shade = .2f + .15f * (float)frame;
  std::vector<Element> children{
      text(kWords[frame % 5], whiteStyle(18)).absolute().rect(120, 8, 60, 22),
      box()
          .absolute()
          .rect(20, 60, 60, 60)
          .filter(material::Filter::blur(5))
          .children({box()
                         .absolute()
                         .rect(15, 15, 30, 30)
                         .fill(material::Color{shade, .3f, 1 - shade, 1})}),
      box().absolute().rect(100 + 6.5f * (float)frame, 70, 24, 24).fill(lit),
      text(u8"HH", whiteStyle(30))
          .absolute()
          .rect(10, 130, 80, 40)
          .ink(lit, PaintBox::Glyph),
  };
  if (frame % 2 == 1)
    children.push_back(box()
                           .absolute()
                           .rect(140, 140, 30, 30)
                           .fill(material::Color{.9f, .9f, .9f, 1}));
  return box().width(200).height(180).fill(ground).children(
      std::move(children));
}

/** The pixels of @p scene's map of @p role as they stand on the device,
 *  read back through a target on @p context. */
SkBitmap devicePixelsOf(const SurfaceScene& scene, Role role,
                        sigil::skia::GraphiteContext& context) {
  SkBitmap pixels;
  const TextureScene* map = scene.scene(role);
  if (!map || !map->image()) return pixels;
  const SkImageInfo info =
      SkImageInfo::MakeN32Premul(map->image()->width(), map->image()->height());
  const sk_sp<SkSurface> target =
      SkSurfaces::RenderTarget(context.recorder(), info);
  if (!target) return pixels;
  SkPaint copy;
  copy.setBlendMode(SkBlendMode::kSrc);
  target->getCanvas()->drawImage(map->image(), 0, 0, SkSamplingOptions(),
                                 &copy);
  pixels.allocPixels(info);
  if (!sigil::skia::readbackPixels(context, *target, pixels.pixmap()))
    return SkBitmap();
  return pixels;
}

/** How many pixels of @p a differ from @p b. */
int differing(const SkBitmap& a, const SkBitmap& b) {
  if (a.dimensions() != b.dimensions()) return -1;
  int count = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x)
      if (*a.getAddr32(x, y) != *b.getAddr32(x, y)) ++count;
  return count;
}

}  // namespace

TEST(ComposeRepaint, TheChangedRegionHoldsWhereAChangedNodeStoodAndStands) {
  sigil::compose::test::checkRows({
      {"AChangedNodeAndNothingWhenStill",
       [&] {
         Host host(200, 200);
         host.composer.setSurfaceMap(Role::BaseColor);
         host.composer.render(twoSquares({.9f, .1f, .1f, 1}));
         // Nothing has been kept yet, so the first draw is the whole canvas.
         EXPECT_FALSE(host.composer.changedRegion().has_value());
         host.frame();

         host.composer.render(twoSquares({.9f, .1f, .1f, 1}));
         const auto still = host.composer.changedRegion();
         ASSERT_TRUE(still.has_value());
         EXPECT_TRUE(still->empty());
         host.frame();

         host.composer.render(twoSquares({.1f, .1f, .9f, 1}));
         const auto changed = host.composer.changedRegion();
         ASSERT_TRUE(changed.has_value());
         ASSERT_EQ(changed->size(), 1u);
         const SkRect region = geometry::path::toSk(changed->front());
         EXPECT_TRUE(region.contains(SkRect::MakeXYWH(10, 10, 30, 30)));
         EXPECT_FALSE(region.intersects(SkRect::MakeXYWH(100, 100, 50, 50)));
         host.frame();
         EXPECT_TRUE(host.composer.drewWithinChangedRegion());

         // A composer that bakes pixels cuts its bakes to the clip they were
         // taken under, so it answers the whole canvas.
         host.composer.setSurfaceMap(std::nullopt);
         host.frame();
         host.composer.render(twoSquares({.1f, .9f, .1f, 1}));
         EXPECT_FALSE(host.composer.changedRegion().has_value());
       }},
      {"AMovedNodeWhereItStoodAndWhereItStands",
       [&] {
         Host host(200, 200);
         host.composer.setSurfaceMap(Role::BaseColor);
         const auto at = [](float left) {
           return box().width(200).height(200).children(
               {box()
                    .absolute()
                    .rect(left, 20, 20, 20)
                    .fill(material::Color{1, 1, 1, 1})});
         };
         host.composer.render(at(10));
         EXPECT_FALSE(host.composer.changedRegion().has_value());
         host.frame();
         host.composer.render(at(150));
         const auto changed = host.composer.changedRegion();
         ASSERT_TRUE(changed.has_value());
         // Where it stood and where it stands are two parts, not the span
         // between.
         ASSERT_EQ(changed->size(), 2u);
         const auto holds = [&](SkRect rect) {
           for (const geometry::path::Rect& part : *changed)
             if (geometry::path::toSk(part).contains(rect)) return true;
           return false;
         };
         EXPECT_TRUE(holds(SkRect::MakeXYWH(10, 20, 20, 20)));
         EXPECT_TRUE(holds(SkRect::MakeXYWH(150, 20, 20, 20)));
       }},
  });
}

TEST(ComposeRepaint, APageRepaintedInRegionsIsThePageDrawnWhole) {
  const auto kept = SurfaceScene::make({200, 180}, fonts());
  ASSERT_NE(kept, nullptr);
  constexpr int kFrames = 5;
  for (int frame = 0; frame < kFrames; ++frame)
    kept->render(busyPage(frame), frame / 60.0);

  const auto whole = SurfaceScene::make({200, 180}, fonts());
  ASSERT_NE(whole, nullptr);
  whole->render(busyPage(kFrames - 1), (kFrames - 1) / 60.0);

  for (Role role : {Role::BaseColor, Role::Normal, Role::Packed, Role::Emissive,
                    Role::Clearcoat}) {
    const SkBitmap repainted = pixelsOf(*kept, role);
    const SkBitmap painted = pixelsOf(*whole, role);
    ASSERT_FALSE(repainted.empty());
    EXPECT_EQ(differing(repainted, painted), 0)
        << "map " << static_cast<int>(role);
  }
}

TEST(ComposeRepaint, APageRepaintedInRegionsOnTheDeviceIsThePageDrawnWhole) {
  std::string error;
  auto made = sigil::core::hardware::GpuDevice::createOwned();
  if (!made) GTEST_SKIP() << made.error().message;
  const auto device = std::move(*made);
  const auto context = sigil::skia::GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context on this device";
  const auto kept = SurfaceScene::make({200, 180}, fonts());
  const auto whole = SurfaceScene::make({200, 180}, fonts());
  ASSERT_TRUE(kept && whole);
  ASSERT_TRUE(kept->useDevice(*device, *context));
  ASSERT_TRUE(whole->useDevice(*device, *context));
  constexpr int kFrames = 5;
  for (int frame = 0; frame < kFrames; ++frame)
    kept->render(busyPage(frame), frame / 60.0);
  whole->render(busyPage(kFrames - 1), (kFrames - 1) / 60.0);
  for (Role role : {Role::BaseColor, Role::Normal, Role::Packed}) {
    const SkBitmap repainted = devicePixelsOf(*kept, role, *context);
    const SkBitmap painted = devicePixelsOf(*whole, role, *context);
    ASSERT_FALSE(repainted.empty());
    EXPECT_EQ(differing(repainted, painted), 0)
        << "map " << static_cast<int>(role);
  }
}
