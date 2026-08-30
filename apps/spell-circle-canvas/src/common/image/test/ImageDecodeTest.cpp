// SigilImageDecode: the format-routing decode surface — the Skia codecs
// reached through decodeImage()/probeImage() on raw bytes, and the SVG
// backend when it is built in. The file reads here are the test's own;
// the library takes no paths.

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilimage/Decode.h>

#include <fstream>
#include <string>
#include <vector>

namespace {

std::string assetPath(const char* name) {
  return std::string(IFRIT_IMAGE_TEST_ASSET_DIR "/") + name;
}

std::vector<std::byte> readFile(const std::string& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  std::vector<std::byte> bytes;
  if (!stream) return bytes;
  bytes.resize((size_t)stream.tellg());
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()),
              (std::streamsize)bytes.size());
  return bytes;
}

/** Reads the pixel at (x, y) of a decoded frame as unpremultiplied color. */
SkColor pixelAt(const sk_sp<SkImage>& image, int x, int y) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(image->width(), image->height(),
                                          kUnpremul_SkAlphaType));
  EXPECT_TRUE(image->readPixels(nullptr, bitmap.pixmap(), 0, 0));
  return bitmap.getColor(x, y);
}

void expectNearColor(SkColor actual, SkColor expected, int tolerance,
                     const char* what) {
  EXPECT_NEAR(int(SkColorGetR(actual)), int(SkColorGetR(expected)), tolerance)
      << what;
  EXPECT_NEAR(int(SkColorGetG(actual)), int(SkColorGetG(expected)), tolerance)
      << what;
  EXPECT_NEAR(int(SkColorGetB(actual)), int(SkColorGetB(expected)), tolerance)
      << what;
  EXPECT_EQ(SkColorGetA(actual), SkColorGetA(expected)) << what;
}

TEST(ImageDecode, RoutesRasterBytesThroughTheSkiaCodecs) {
  const auto bytes = readFile(assetPath("anim.gif"));
  ASSERT_FALSE(bytes.empty());
  auto asset = sigil::image::decodeImage(bytes.data(), bytes.size(), {},
                                         assetPath("anim.gif"));
  ASSERT_TRUE(asset.has_value());
  EXPECT_TRUE(asset->animated());
  ASSERT_EQ(asset->frames().size(), 3u);
  expectNearColor(pixelAt(asset->frames()[1].image, 2, 2), SK_ColorGREEN, 0,
                  "second frame");
  auto info = sigil::image::probeImage(bytes.data(), bytes.size());
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->format, "gif");
  EXPECT_EQ(info->frames, 3);
}

TEST(ImageDecode, LdrChannelsNormalizeToPremultipliedFloats) {
  const auto bytes = readFile(assetPath("still.png"));
  ASSERT_FALSE(bytes.empty());
  auto channels = sigil::image::decodeChannels(bytes.data(), bytes.size());
  ASSERT_TRUE(channels.has_value());
  EXPECT_EQ(channels->width, 4);
  EXPECT_EQ(channels->height, 4);
  EXPECT_FALSE(channels->floatingPoint);
  ASSERT_EQ(channels->names.size(), 4u);
  EXPECT_FLOAT_EQ(channels->at(1, 1, channels->index("R")), 1.0f);
  EXPECT_FLOAT_EQ(channels->at(1, 1, channels->index("G")), 0.0f);
  EXPECT_FLOAT_EQ(channels->at(1, 1, channels->index("A")), 1.0f);
}

TEST(ImageDecode, RejectsUnsupportedBytes) {
  const char kGarbage[] = "definitely not an image";
  EXPECT_FALSE(
      sigil::image::decodeImage(reinterpret_cast<const std::byte*>(kGarbage),
                                sizeof(kGarbage))
          .has_value());
  EXPECT_FALSE(
      sigil::image::probeImage(reinterpret_cast<const std::byte*>(kGarbage),
                               sizeof(kGarbage))
          .has_value());
}

#ifdef SIGILIMAGE_HAS_SVG

// An 8x4 document, red left half, blue right half.
constexpr char kTwoRectSvg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='4'>"
    "<rect x='0' y='0' width='4' height='4' fill='#ff0000'/>"
    "<rect x='4' y='0' width='4' height='4' fill='#0000ff'/>"
    "</svg>";

const std::byte* svgBytes(const char* svg) {
  return reinterpret_cast<const std::byte*>(svg);
}

TEST(SvgDecode, RendersAtExplicitSize) {
  auto asset = sigil::image::decodeImage(
      svgBytes(kTwoRectSvg), std::char_traits<char>::length(kTwoRectSvg),
      {.width = 64, .height = 32});
  ASSERT_TRUE(asset.has_value());
  EXPECT_EQ(asset->width(), 64);
  EXPECT_EQ(asset->height(), 32);
  ASSERT_EQ(asset->frames().size(), 1u);
  expectNearColor(pixelAt(asset->frames()[0].image, 16, 16), SK_ColorRED, 0,
                  "left rect");
  expectNearColor(pixelAt(asset->frames()[0].image, 48, 16), SK_ColorBLUE, 0,
                  "right rect");
}

TEST(SvgDecode, WidthOnlyDerivesHeightFromAspect) {
  auto asset = sigil::image::decodeImage(
      svgBytes(kTwoRectSvg), std::char_traits<char>::length(kTwoRectSvg),
      {.width = 100});
  ASSERT_TRUE(asset.has_value());
  EXPECT_EQ(asset->width(), 100);
  EXPECT_EQ(asset->height(), 50);  // 8x4 intrinsic aspect
  // And with no size at all, the intrinsic size wins.
  auto intrinsic = sigil::image::decodeImage(
      svgBytes(kTwoRectSvg), std::char_traits<char>::length(kTwoRectSvg));
  ASSERT_TRUE(intrinsic.has_value());
  EXPECT_EQ(intrinsic->width(), 8);
  EXPECT_EQ(intrinsic->height(), 4);
}

TEST(SvgDecode, ProbeReportsFormatAndIntrinsicSize) {
  auto info = sigil::image::probeImage(
      svgBytes(kTwoRectSvg), std::char_traits<char>::length(kTwoRectSvg));
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->format, "svg");
  EXPECT_EQ(info->width, 8);
  EXPECT_EQ(info->height, 4);
  EXPECT_EQ(info->channels, 4);
  EXPECT_EQ(info->frames, 1);
  EXPECT_FALSE(info->floatingPoint);
}

#endif  // SIGILIMAGE_HAS_SVG

}  // namespace
