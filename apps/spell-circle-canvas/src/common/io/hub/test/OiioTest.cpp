/** @file
 * The EXR cases: a layered float image decoded through the hub, the
 * layer a decode option selects, what a probe says the file means, and
 * the raw channels behind it. The fixtures are written with OpenImageIO
 * — the library itself encodes nothing — so this file has cases only
 * where OpenImageIO was found.
 */

#include "MountedHub.h"

#ifdef SIGILIO_HAS_OIIO

#include <OpenImageIO/imageio.h>
#include <gtest/gtest.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <sigilio/hub/Hub.h>

#include <filesystem>
#include <vector>

using namespace sigil::io;
namespace fs = std::filesystem;

namespace {

/** Writes a tiny EXR with layered channels: default RGBA plus a
 *  "glow" layer whose red channel is 2.5 (HDR range). */
void writeLayeredExr(const fs::path& path) {
  using namespace OIIO;
  constexpr int kSize = 4;
  ImageSpec spec(kSize, kSize, 8, TypeDesc::FLOAT);
  spec.channelnames = {"R",      "G",      "B",      "A",
                       "glow.R", "glow.G", "glow.B", "glow.A"};
  auto out = ImageOutput::create(path.string());
  ASSERT_TRUE(out);
  ASSERT_TRUE(out->open(path.string(), spec));
  std::vector<float> pixels(static_cast<size_t>(kSize) * kSize * 8);
  for (int px = 0; px < kSize * kSize; ++px) {
    float* p = pixels.data() + static_cast<ptrdiff_t>(px) * 8;
    p[0] = 0.25f;
    p[1] = 0.5f;
    p[2] = 0.75f;
    p[3] = 1.0f;  // base RGBA
    p[4] = 2.5f;
    p[5] = 0.125f;
    p[6] = 0.0f;
    p[7] = 1.0f;  // glow.*
  }
  ASSERT_TRUE(out->write_image(TypeDesc::FLOAT, pixels.data()));
  ASSERT_TRUE(out->close());
}

}  // namespace

TEST_F(IOOiio, ExrDecodesToFloatImage) {
  writeLayeredExr(dir.path / "probe.exr");
  auto image = hub.image("res://probe.exr");
  ASSERT_NE(image, nullptr);
  ASSERT_FALSE(image->frames().empty());
  const sk_sp<SkImage>& sk = image->frames().front().image;
  EXPECT_EQ(sk->width(), 4);
  EXPECT_EQ(sk->colorType(), kRGBA_F32_SkColorType);
}

TEST_F(IOOiio, ExrLayerSelectionReadsHdrChannels) {
  writeLayeredExr(dir.path / "probe.exr");
  auto glow = hub.image("res://probe.exr", {.layer = "glow"});
  ASSERT_NE(glow, nullptr);
  const sk_sp<SkImage>& sk = glow->frames().front().image;
  SkPixmap pixmap;
  ASSERT_TRUE(sk->peekPixels(&pixmap));
  const float* px = (const float*)pixmap.addr(0, 0);
  EXPECT_FLOAT_EQ(px[0], 2.5f);  // HDR value survives (F32)
  EXPECT_FLOAT_EQ(px[1], 0.125f);
}

TEST_F(IOOiio, ProbeListsLayersAndChannels) {
  writeLayeredExr(dir.path / "probe.exr");
  auto info = hub.probe<sigil::image::ImageProbe>("res://probe.exr");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->format, "openexr");
  EXPECT_EQ(info->width, 4);
  EXPECT_TRUE(info->floatingPoint);
  EXPECT_EQ(info->channels, 8);
  ASSERT_EQ(info->layers.size(), 1u);
  EXPECT_EQ(info->layers[0], "glow");
}

TEST_F(IOOiio, ChannelsExposeRawFloatData) {
  writeLayeredExr(dir.path / "probe.exr");
  auto channels = hub.channels("res://probe.exr");
  ASSERT_NE(channels, nullptr);
  EXPECT_EQ(channels->width, 4);
  EXPECT_TRUE(channels->floatingPoint);
  ASSERT_EQ(channels->names.size(), 8u);
  const int glowR = channels->index("glow.R");
  ASSERT_GE(glowR, 0);
  EXPECT_FLOAT_EQ(channels->at(0, 0, glowR), 2.5f);
  // And the Skia composition helper agrees.
  sk_sp<SkImage> composed = channels->makeImage("glow");
  ASSERT_NE(composed, nullptr);
  EXPECT_EQ(composed->colorType(), kRGBA_F32_SkColorType);
}

#endif  // SIGILIO_HAS_OIIO
