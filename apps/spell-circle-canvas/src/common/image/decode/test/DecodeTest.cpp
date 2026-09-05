/** @file
 * The format-routing decode surface — the Skia codecs reached through
 * decodeImage()/probeImage() on raw bytes, the KTX reader, the DDS cube
 * map through OpenImageIO when it is built in, and the SVG backend when
 * it is. The file reads here are the test's own; the library takes no
 * paths.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilimage/decode/Decode.h>

#include <fstream>
#include <string>
#include <vector>

#include "CubeContainers.h"
#include "Pixels.h"

namespace {

using sigil::image::test::assetPath;
using sigil::image::test::expectNearColor;
using sigil::image::test::pixelAt;

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

// The six faces of a cube map, each its own colour in the +x -x +y -y
// +z -z order, so where a face landed in the column is legible from the
// texel.
constexpr sigil::image::test::CubeFaces kCubeFaces = {
    SK_ColorRED,    SK_ColorGREEN, SK_ColorBLUE,
    SK_ColorYELLOW, SK_ColorCYAN,  SK_ColorMAGENTA};

/** The decoded image must be the faces stacked into a 1:6 column. */
void expectCubeColumn(const std::vector<std::byte>& bytes, const char* name) {
  auto asset = sigil::image::decodeImage(bytes.data(), bytes.size(), {}, name);
  ASSERT_TRUE(asset.has_value()) << name;
  ASSERT_EQ(asset->frames().size(), 1u) << name;
  const sk_sp<SkImage>& image = asset->frames()[0].image;
  EXPECT_EQ(image->width(), 8) << name;
  EXPECT_EQ(image->height(), 48) << name;
  for (int face = 0; face < 6; ++face)
    expectNearColor(pixelAt(image, 4, face * 8 + 4), kCubeFaces[(size_t)face],
                    0, name);
  auto info = sigil::image::probeImage(bytes.data(), bytes.size(), name);
  ASSERT_TRUE(info.has_value()) << name;
  EXPECT_EQ(info->width, 8) << name;
  EXPECT_EQ(info->height, 48) << name;
  EXPECT_EQ(info->channels, 4) << name;
  EXPECT_FALSE(info->floatingPoint) << name;
}

TEST(KtxDecode, ACubeMapInEitherContainerIsTheSixFacesAsAColumn) {
  const auto ktx1 = sigil::image::test::cubeKtx1(kCubeFaces, 8);
  expectCubeColumn(ktx1, "cube.ktx");
  auto info = sigil::image::probeImage(ktx1.data(), ktx1.size());
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->format, "ktx");
  const auto ktx2 = sigil::image::test::cubeKtx2(kCubeFaces, 8);
  expectCubeColumn(ktx2, "cube.ktx2");
  info = sigil::image::probeImage(ktx2.data(), ktx2.size());
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->format, "ktx2");
}

TEST(KtxDecode, ATruncatedFileIsRefused) {
  auto ktx2 = sigil::image::test::cubeKtx2(kCubeFaces, 8);
  ktx2.resize(ktx2.size() - 1);  // the last face is one byte short
  EXPECT_FALSE(sigil::image::decodeImage(ktx2.data(), ktx2.size()).has_value());
  EXPECT_FALSE(sigil::image::probeImage(ktx2.data(), ktx2.size()).has_value());
}

#ifdef SIGILIMAGE_HAS_OIIO

TEST(OiioDecode, ADdsCubeMapIsTheSixFacesAsAColumn) {
  expectCubeColumn(sigil::image::test::cubeDds(kCubeFaces, 8), "cube.dds");
}

#endif  // SIGILIMAGE_HAS_OIIO

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
