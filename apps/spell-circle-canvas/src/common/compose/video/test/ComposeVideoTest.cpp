#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/video/Video.h>
#include <sigilmedia/video/Encoder.h>

#include <cstddef>
#include <memory>
#include <vector>

#include "Fonts.h"

using namespace sigil::compose;

// The fit a frame meets its box under is SigilMaterial's, spelled at its
// own origin.
namespace material = sigil::material;

namespace {

using sigil::test::fonts;

/** A one-frame red clip, opened on @p pool when one is given. */
std::shared_ptr<const sigil::media::Video> redClip(
    std::shared_ptr<sigil::media::Playback> pool = nullptr) {
  constexpr int kSize = 64;
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(kSize, kSize));
  pixels.eraseColor(SK_ColorRED);
  sigil::media::Encoder encoder(
      {.width = kSize,
       .height = kSize,
       .framesPerSecond = 10,
       .bitRate = 500'000,
       .hardware = sigil::media::HardwarePreference::Disabled});
  if (!encoder || !encoder.append(pixels.pixmap())) return nullptr;
  const std::vector<std::byte> bytes = encoder.finish();
  if (bytes.empty()) return nullptr;
  return sigil::media::decode<sigil::media::Video>(
      bytes,
      {.playback = std::move(pool),
       .hardware = sigil::media::HardwarePreference::Disabled},
      "compose.mp4");
}

/** Whether the pixel at (x, y) of @p surface is the clip's red. */
bool redAt(SkSurface& surface, int x, int y) {
  SkBitmap sample;
  sample.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  if (!surface.readPixels(sample.pixmap(), x, y)) return false;
  const SkColor color = sample.getColor(0, 0);
  return SkColorGetR(color) > 220 && SkColorGetG(color) < 40 &&
         SkColorGetB(color) < 40;
}

}  // namespace

TEST(ComposeVideo, ClipIsALiveSizedLeaf) {
  const auto clip = redClip();
  ASSERT_NE(clip, nullptr);
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({128, 128});
  composer.render(box()
                      .fill(Fill::color({0, 0, 1, 1}))
                      .alignItems(Align::Center)
                      .justifyContent(Justify::Center)
                      .children({video(clip)}));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(128, 128));
  composer.draw(*surface->getCanvas());
  SkBitmap sample;
  sample.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  ASSERT_TRUE(surface->readPixels(sample.pixmap(), 64, 64));
  const SkColor center = sample.getColor(0, 0);
  EXPECT_GT(SkColorGetR(center), 220);
  EXPECT_LT(SkColorGetG(center), 40);
  EXPECT_LT(SkColorGetB(center), 40);
  ASSERT_TRUE(surface->readPixels(sample.pixmap(), 4, 4));
  EXPECT_EQ(sample.getColor(0, 0), SK_ColorBLUE);
}

TEST(ComposeVideo, AClipOnAPoolPaintsEveryLeafThatShowsIt) {
  // No worker: the leaf's ask decodes inside paint, so the frame it reads
  // back is the one it asked for. The production pool differs only in
  // where the decode runs.
  auto pool = std::make_shared<sigil::media::Playback>(
      sigil::media::Playback::Options{.workers = 0});
  const auto clip = redClip(pool);
  ASSERT_NE(clip, nullptr);
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({128, 64});
  composer.render(stack().children(
      {video(clip).rect(SkRect::MakeXYWH(0, 0, 64, 64)),
       video(clip).rect(SkRect::MakeXYWH(64, 0, 64, 64))}));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(128, 64));
  surface->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*surface->getCanvas());
  EXPECT_TRUE(clip->hasFrame());
  EXPECT_TRUE(redAt(*surface, 32, 32));
  EXPECT_TRUE(redAt(*surface, 96, 32));
}

TEST(ComposeVideo, LeafCompositesItsSingleDrawWithoutAGroupingNode) {
  const auto clip = redClip();
  ASSERT_NE(clip, nullptr);
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({64, 64});
  composer.render(box()
                      .fill(Fill::color({0, 0, 1, 1}))
                      .children({video(clip, {.fit = material::Fit::Cover,
                                              .opacity = 0.5f,
                                              .blend = SkBlendMode::kPlus})}));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 64));
  composer.draw(*surface->getCanvas());
  SkBitmap sample;
  sample.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  ASSERT_TRUE(surface->readPixels(sample.pixmap(), 32, 32));
  const SkColor center = sample.getColor(0, 0);
  EXPECT_NEAR(SkColorGetR(center), 128, 3);
  EXPECT_LT(SkColorGetG(center), 3);
  EXPECT_GT(SkColorGetB(center), 250);
}
