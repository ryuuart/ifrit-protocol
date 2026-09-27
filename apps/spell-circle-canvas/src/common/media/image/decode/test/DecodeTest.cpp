/** @file
 * The image route — the Skia codecs reached through
 * media::decode<media::Image>() and the image probe on raw bytes, the KTX reader, the DDS cube
 * map through OpenImageIO when it is built in, and the SVG backend when
 * it is. The file reads here are the test's own; the library takes no
 * paths.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkSize.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/image/Decode.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <type_traits>

#include <fstream>
#include <string>
#include <vector>

#include "CubeContainers.h"
#include "Pixels.h"

namespace {

using sigil::media::test::assetPath;
using sigil::media::test::expectNearColor;
using sigil::media::test::pixelAt;
using sigil::media::Channels;
using sigil::media::Image;
using sigil::media::ImageOptions;
using sigil::media::Metadata;

std::shared_ptr<const Image> decodeImage(std::span<const std::byte> bytes,
                                         ImageOptions options = {},
                                         const std::filesystem::path& name = {}) {
  return sigil::media::decode<Image>(bytes, options, name);
}

std::optional<Metadata> probeImage(std::span<const std::byte> bytes,
                                   const std::filesystem::path& name = {}) {
  return probeDocument(std::type_identity<Image>{}, bytes, name);
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

// The optional backends are a build-time fact, so a claim about one is
// carried here whether or not it is built in: without the backend the
// case says which backend it wanted rather than vanishing from the run.
#ifdef SIGILMEDIA_HAS_OIIO
constexpr bool kOiioBackend = true;
#else
constexpr bool kOiioBackend = false;
#endif
#ifdef SIGILMEDIA_HAS_SVG
constexpr bool kSvgBackend = true;
#else
constexpr bool kSvgBackend = false;
#endif

TEST(ImageDecode, RoutesRasterBytesThroughTheSkiaCodecs) {
  const auto bytes = readFile(assetPath("anim.gif"));
  ASSERT_FALSE(bytes.empty());
  auto asset = decodeImage(bytes, {},
                                         assetPath("anim.gif"));
  ASSERT_TRUE(asset);
  EXPECT_TRUE(asset->isRunning());
  ASSERT_EQ(asset->frames().size(), 3u);
  expectNearColor(pixelAt(asset->frames()[1].image, 2, 2), SK_ColorGREEN, 0,
                  "second frame");
  auto info = probeImage(bytes);
  ASSERT_TRUE(info);
  EXPECT_EQ(info->format, "gif");
  EXPECT_EQ(info->frames, 3);
}

TEST(ImageDecode, LdrChannelsNormalizeToPremultipliedFloats) {
  const auto bytes = readFile(assetPath("still.png"));
  ASSERT_FALSE(bytes.empty());
  auto channels = sigil::media::decode<Channels>(bytes);
  ASSERT_TRUE(channels);
  EXPECT_EQ(channels->width, 4);
  EXPECT_EQ(channels->height, 4);
  EXPECT_FALSE(channels->floatingPoint);
  ASSERT_EQ(channels->names.size(), 4u);
  EXPECT_FLOAT_EQ(channels->at(1, 1, channels->index("R")), 1.0f);
  EXPECT_FLOAT_EQ(channels->at(1, 1, channels->index("G")), 0.0f);
  EXPECT_FLOAT_EQ(channels->at(1, 1, channels->index("A")), 1.0f);
}

TEST(ImageDecode, DecodesEveryStillFormat) {
  // JPEG is lossy and GIF/AVIF quantize, so compare with a small tolerance.
  const struct {
    const char* file;
    int tolerance;
  } kCases[] = {
      {"still.png", 0}, {"still.jpg", 12}, {"still.webp", 0},
      {"still.gif", 0}, {"still.avif", 2},
  };
  for (const auto& testCase : kCases) {
    const auto image = decodeImage(readFile(assetPath(testCase.file)));
    ASSERT_TRUE(image) << testCase.file;
    EXPECT_EQ(image->size(), glm::ivec2(4, 4)) << testCase.file;
    EXPECT_FALSE(image->isRunning()) << testCase.file;
    ASSERT_EQ(image->frames().size(), 1u) << testCase.file;
    EXPECT_EQ(image->duration().count(), 0.0) << testCase.file;
    expectNearColor(pixelAt(image->frames()[0].image, 1, 1), SK_ColorRED,
                    testCase.tolerance, testCase.file);
  }
}

TEST(ImageDecode, DecodesAnimationsWithCompositedFrames) {
  const SkColor kFrameColors[] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE};
  const struct {
    const char* file;
    int tolerance;
  } kCases[] = {{"anim.gif", 0}, {"anim.webp", 0}, {"anim.avif", 2}};
  for (const auto& testCase : kCases) {
    const auto image = decodeImage(readFile(assetPath(testCase.file)));
    ASSERT_TRUE(image) << testCase.file;
    EXPECT_TRUE(image->isRunning()) << testCase.file;
    ASSERT_EQ(image->frames().size(), 3u) << testCase.file;
    EXPECT_EQ(image->repetitions(), -1) << testCase.file;
    EXPECT_DOUBLE_EQ(image->duration().count(), 0.3) << testCase.file;
    for (size_t index = 0; index < 3; ++index) {
      EXPECT_DOUBLE_EQ(image->frames()[index].duration.count(), 0.1)
          << testCase.file << " frame " << index;
      expectNearColor(pixelAt(image->frames()[index].image, 2, 2),
                      kFrameColors[index], testCase.tolerance, testCase.file);
    }
  }
}

TEST(ImageDecode, AnAnimationPlaysAsItsFileSays) {
  using namespace std::chrono_literals;
  const auto image = decodeImage(readFile(assetPath("anim.gif")));
  ASSERT_TRUE(image);
  const auto& frames = image->frames();
  EXPECT_EQ(image->frameAt(0ms).image, frames[0].image);
  EXPECT_EQ(image->frameAt(99ms).image, frames[0].image);
  EXPECT_EQ(image->frameAt(150ms).image, frames[1].image);
  EXPECT_EQ(image->frameAt(250ms).image, frames[2].image);
  EXPECT_EQ(image->frameAt(310ms).image, frames[0].image);  // looped
  EXPECT_EQ(image->frameAt(-5ms).image, frames[0].image);   // clamped
}

TEST(ImageDecode, RejectsBytesThatAreNotAnImage) {
  EXPECT_FALSE(decodeImage(readFile(assetPath("missing.png"))));
}

// The options a hub's load takes for an image are this library's own,
// named through the hook a hub finds by argument-dependent lookup.
static_assert(std::is_same_v<decltype(loadOptions(std::type_identity<Image>{})),
                             ImageOptions>);

TEST(ImageDecode, RejectsUnsupportedBytes) {
  const char kGarbage[] = "definitely not an image";
  const std::span<const std::byte> garbage(
      reinterpret_cast<const std::byte*>(kGarbage), sizeof(kGarbage));
  EXPECT_FALSE(decodeImage(garbage));
  EXPECT_FALSE(probeImage(garbage));
  EXPECT_FALSE(decodeImage({}));
}

// The six faces of a cube map, each its own colour in the +x -x +y -y
// +z -z order, so where a face landed in the column is legible from the
// texel.
constexpr sigil::media::test::CubeFaces kCubeFaces = {
    SK_ColorRED,    SK_ColorGREEN, SK_ColorBLUE,
    SK_ColorYELLOW, SK_ColorCYAN,  SK_ColorMAGENTA};

/** The decoded image must be the faces stacked into a 1:6 column. */
void expectCubeColumn(const std::vector<std::byte>& bytes, const char* name) {
  auto asset = decodeImage(bytes, {}, name);
  ASSERT_TRUE(asset) << name;
  ASSERT_EQ(asset->frames().size(), 1u) << name;
  const sk_sp<SkImage>& image = asset->frames()[0].image;
  EXPECT_EQ(image->width(), 8) << name;
  EXPECT_EQ(image->height(), 48) << name;
  for (int face = 0; face < 6; ++face)
    expectNearColor(pixelAt(image, 4, face * 8 + 4), kCubeFaces[(size_t)face],
                    0, name);
  auto info = probeImage(bytes, name);
  ASSERT_TRUE(info) << name;
  EXPECT_EQ(info->width, 8) << name;
  EXPECT_EQ(info->height, 48) << name;
  EXPECT_EQ(info->channels, 4) << name;
  EXPECT_FALSE(info->floatingPoint) << name;
}

TEST(KtxDecode, ACubeMapInEitherContainerIsTheSixFacesAsAColumn) {
  const auto ktx1 = sigil::media::test::cubeKtx1(kCubeFaces, 8);
  expectCubeColumn(ktx1, "cube.ktx");
  auto info = probeImage(ktx1);
  ASSERT_TRUE(info);
  EXPECT_EQ(info->format, "ktx");
  const auto ktx2 = sigil::media::test::cubeKtx2(kCubeFaces, 8);
  expectCubeColumn(ktx2, "cube.ktx2");
  info = probeImage(ktx2);
  ASSERT_TRUE(info);
  EXPECT_EQ(info->format, "ktx2");
}

TEST(KtxDecode, ATruncatedFileIsRefused) {
  auto ktx2 = sigil::media::test::cubeKtx2(kCubeFaces, 8);
  ktx2.resize(ktx2.size() - 1);  // the last face is one byte short
  EXPECT_FALSE(decodeImage(ktx2));
  EXPECT_FALSE(probeImage(ktx2));
}

// A crafted header, byte by byte over a good file: the fields a reader
// takes its lengths from are the ones a hostile file lies about.
void writeWord(std::vector<std::byte>& bytes, size_t at, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    bytes[at + i] = (std::byte)((value >> (8 * i)) & 0xFFu);
}

TEST(KtxDecode, AHeaderWhoseLengthsOverflowIsRefused) {
  // 2^30 by 2^30 at sixteen bytes a texel: the face length is a product
  // of header fields that wraps to zero, so the length the file states
  // covers it, the level is taken, and the image the reader is then
  // asked to hold is 2^62 floats — from a file of a hundred-odd bytes.
  auto ktx2 = sigil::media::test::cubeKtx2(kCubeFaces, 8);
  writeWord(ktx2, 12, 109);       // vkFormat R32G32B32A32_SFLOAT
  writeWord(ktx2, 20, 1u << 30);  // pixelWidth
  writeWord(ktx2, 24, 1u << 30);  // pixelHeight
  writeWord(ktx2, 36, 1);         // faceCount
  EXPECT_FALSE(decodeImage(ktx2));
  EXPECT_FALSE(probeImage(ktx2));

  // The same lie in a KTX 1 header, where the width alone is past what
  // any image holds.
  auto ktx1 = sigil::media::test::cubeKtx1(kCubeFaces, 8);
  writeWord(ktx1, 16, 0x1406);    // glType GL_FLOAT
  writeWord(ktx1, 36, 1u << 30);  // pixelWidth
  writeWord(ktx1, 40, 1u << 30);  // pixelHeight
  writeWord(ktx1, 52, 1);         // numberOfFaces
  EXPECT_FALSE(decodeImage(ktx1));
  EXPECT_FALSE(probeImage(ktx1));
}

TEST(OiioDecode, ADdsCubeMapIsTheSixFacesAsAColumn) {
  if (!kOiioBackend) GTEST_SKIP() << "built without the OpenImageIO backend";
  expectCubeColumn(sigil::media::test::cubeDds(kCubeFaces, 8), "cube.dds");
}

// An 8x4 document, red left half, blue right half.
constexpr char kTwoRectSvg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='4'>"
    "<rect x='0' y='0' width='4' height='4' fill='#ff0000'/>"
    "<rect x='4' y='0' width='4' height='4' fill='#0000ff'/>"
    "</svg>";

std::span<const std::byte> svgBytes(const char* svg) {
  return {reinterpret_cast<const std::byte*>(svg),
          std::char_traits<char>::length(svg)};
}

TEST(SvgDecode, RendersAtExplicitSize) {
  if (!kSvgBackend) GTEST_SKIP() << "built without the Skia SVG backend";
  auto asset = decodeImage(svgBytes(kTwoRectSvg),
      {.width = 64, .height = 32});
  ASSERT_TRUE(asset);
  EXPECT_EQ(asset->size().x, 64);
  EXPECT_EQ(asset->size().y, 32);
  ASSERT_EQ(asset->frames().size(), 1u);
  expectNearColor(pixelAt(asset->frames()[0].image, 16, 16), SK_ColorRED, 0,
                  "left rect");
  expectNearColor(pixelAt(asset->frames()[0].image, 48, 16), SK_ColorBLUE, 0,
                  "right rect");
}

TEST(SvgDecode, WidthOnlyDerivesHeightFromAspect) {
  if (!kSvgBackend) GTEST_SKIP() << "built without the Skia SVG backend";
  auto asset = decodeImage(svgBytes(kTwoRectSvg),
      {.width = 100});
  ASSERT_TRUE(asset);
  EXPECT_EQ(asset->size().x, 100);
  EXPECT_EQ(asset->size().y, 50);  // 8x4 intrinsic aspect
  // And with no size at all, the intrinsic size wins.
  auto intrinsic = decodeImage(svgBytes(kTwoRectSvg));
  ASSERT_TRUE(intrinsic);
  EXPECT_EQ(intrinsic->size().x, 8);
  EXPECT_EQ(intrinsic->size().y, 4);
}

TEST(SvgDecode, ProbeReportsFormatAndIntrinsicSize) {
  if (!kSvgBackend) GTEST_SKIP() << "built without the Skia SVG backend";
  auto info = probeImage(svgBytes(kTwoRectSvg));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->format, "svg");
  EXPECT_EQ(info->width, 8);
  EXPECT_EQ(info->height, 4);
  EXPECT_EQ(info->channels, 4);
  EXPECT_EQ(info->frames, 1);
  EXPECT_FALSE(info->floatingPoint);
}

}  // namespace
