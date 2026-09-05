#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/video/Video.h>
#include <sigilvideo/encode/Encode.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cstddef>
#include <memory>

using namespace sigil::compose;

namespace {

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

std::shared_ptr<sigil::video::Video> redClip() {
  constexpr int kSize = 64;
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(kSize, kSize));
  pixels.eraseColor(SK_ColorRED);
  auto encoder = sigil::video::Encoder::make(
      sigil::video::Format::Mp4,
      {.width = kSize,
       .height = kSize,
       .framesPerSecond = 10,
       .bitRate = 500'000,
       .hardware = sigil::video::HardwarePreference::Disabled});
  if (!encoder || !encoder->append(pixels.pixmap())) return nullptr;
  sk_sp<SkData> bytes = encoder->finish();
  if (!bytes) return nullptr;
  sigil::video::DecodeOptions options;
  options.hardware = sigil::video::HardwarePreference::Disabled;
  return sigil::video::decodeVideo(static_cast<const std::byte*>(bytes->data()),
                                   bytes->size(), options, "compose.mp4");
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
  std::shared_ptr<sigil::video::Video> clip = redClip();
  ASSERT_NE(clip, nullptr);
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({128, 128});
  composer.render(box()
                      .fill(Fill::color({0, 0, 1, 1}))
                      .alignItems(Align::Center)
                      .justify(Justify::Center)
                      .child(video(clip)));

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

TEST(ComposeVideo, SharedPlaybackSuppliesTheLeafAndRegistersItsClipOnce) {
  std::shared_ptr<sigil::video::Video> clip = redClip();
  ASSERT_NE(clip, nullptr);
  // No worker: the leaf's request decodes inside paint, so the frame it
  // reads back is the one it asked for. The production pool differs only
  // in where the decode runs.
  auto playback = std::make_shared<sigil::video::Playback>(
      sigil::video::Playback::Options{.workerThreads = 0});
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({64, 64});
  composer.render(video(clip, playback));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 64));
  surface->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*surface->getCanvas());
  EXPECT_TRUE(redAt(*surface, 32, 32));

  // Describing the scene again registers no second clock for the clip.
  composer.render(video(clip, playback));
  composer.render(video(clip, playback));
  EXPECT_EQ(playback->size(), 1u);
}

TEST(ComposeVideo, RegisteredHandleFansOnePlayerOutToSeveralLeaves) {
  std::shared_ptr<sigil::video::Video> clip = redClip();
  ASSERT_NE(clip, nullptr);
  auto playback = std::make_shared<sigil::video::Playback>(
      sigil::video::Playback::Options{.workerThreads = 0});
  const sigil::video::Playback::Handle handle = playback->add(clip);
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({128, 64});
  composer.render(stack()
                      .child(video(clip, playback, handle)
                                 .rect(SkRect::MakeXYWH(0, 0, 64, 64)))
                      .child(video(clip, playback, handle)
                                 .rect(SkRect::MakeXYWH(64, 0, 64, 64))));
  EXPECT_EQ(playback->size(), 1u);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(128, 64));
  surface->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*surface->getCanvas());
  EXPECT_TRUE(redAt(*surface, 32, 32));
  EXPECT_TRUE(redAt(*surface, 96, 32));
}

TEST(ComposeVideo, LeafCompositesItsSingleDrawWithoutAGroupingNode) {
  std::shared_ptr<sigil::video::Video> clip = redClip();
  ASSERT_NE(clip, nullptr);
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({64, 64});
  composer.render(box()
                      .fill(Fill::color({0, 0, 1, 1}))
                      .child(video(clip, {.fit = VideoFit::Cover,
                                          .opacity = 0.5f,
                                          .blend = SkBlendMode::kPlus})));

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
