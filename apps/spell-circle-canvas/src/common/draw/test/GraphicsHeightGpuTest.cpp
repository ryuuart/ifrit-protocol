/** @file
 * A seeded fibre deposit read at its own resolution, then differentiated
 * into normals with shared and separately drawn height sources. What holds
 * on every backend is asserted: each paints a finite, grey, opaque height
 * and repeats it exactly, a raster height uploaded to the device keeps
 * every pixel, and every source differentiates into finite unit normals,
 * flat where the source is. Where the raster and the device's own heights
 * stand apart is left open, because the two rasterize overlapping bristle
 * coverage differently and no common contract is chosen yet.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Tool.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace {

namespace draw = sigil::draw;
namespace brush = sigil::draw::brush;
namespace material = sigil::material;
using sigil::skia::GraphiteContext;

constexpr SkISize kHeightSize{1024, 128};
constexpr SkISize kNormalSize{964, 116};

struct Pixels {
  SkISize size;
  std::vector<float> rgba;

  SkPixmap pixmap() {
    return {SkImageInfo::Make(size, kRGBA_F32_SkColorType, kPremul_SkAlphaType),
            rgba.data(), size_t(size.width()) * 4 * sizeof(float)};
  }
};

Pixels read(SkSurface& surface, GraphiteContext* context = nullptr) {
  Pixels pixels{
      surface.imageInfo().dimensions(),
      std::vector<float>(size_t(surface.width()) * surface.height() * 4)};
  const bool ok =
      context ? sigil::skia::readbackPixels(*context, surface, pixels.pixmap())
              : surface.readPixels(pixels.pixmap(), 0, 0);
  EXPECT_TRUE(ok);
  if (!ok) return {};
  return pixels;
}

sk_sp<SkSurface> target(SkISize size, SkColorType type,
                        GraphiteContext* context = nullptr) {
  const auto info = SkImageInfo::Make(size, type, kPremul_SkAlphaType);
  auto surface = context ? SkSurfaces::RenderTarget(context->recorder(), info)
                         : SkSurfaces::Raster(info);
  EXPECT_TRUE(surface);
  return surface;
}

sk_sp<SkImage> height(SkSurface& host) {
  draw::Pen pen;
  pen.begin(*host.getCanvas(), {.width = 1024, .height = 128});
  draw::Graphics buffer{1024, 128};
  auto& p = buffer.begin(pen);
  p.push();
  p.colorMode(draw::RGB);
  p.blendMode(draw::BLEND);
  p.rectMode(draw::CORNER);
  p.background(0);
  p.randomSeed(0x510beu);
  p.clip([&] { p.rect(0, 0, 1024, 128); });
  auto tool = brush::watercolor({.88f, .88f, .88f, 1}, 58);
  tool.blend = draw::BLEND;
  tool.opacity = .72f;
  tool.bristles = 36;
  tool.spacing = 2.2f;
  tool.scatter = 1.6f;
  tool.pressure = {.18f, .97f, .22f};
  const std::array<brush::Sample, 5> samples{{{{-32, 92}, .24f},
                                              {{228, 28}, .91f},
                                              {{482, 87}, .76f},
                                              {{746, 39}, 1.f},
                                              {{1074, 83}, .17f}}};
  brush::spline(p, tool, samples, .8f);
  tool.width = 27;
  tool.opacity = .55f;
  tool.bristles = 19;
  const std::array<brush::Sample, 4> crossing{{{{30, 122}, .2f},
                                               {{298, 72}, .82f},
                                               {{699, 110}, .7f},
                                               {{1010, 62}, .13f}}};
  brush::spline(p, tool, crossing, .8f);
  p.pop();
  buffer.end();
  pen.end();
  EXPECT_EQ(buffer.extent(), kHeightSize);
  return buffer.image();
}

Pixels readImage(const sk_sp<SkImage>& image,
                 GraphiteContext* context = nullptr) {
  if (!image) return {};
  if (!context) {
    Pixels pixels{
        image->dimensions(),
        std::vector<float>(size_t(image->width()) * image->height() * 4)};
    const bool ok = image->readPixels(nullptr, pixels.pixmap(), 0, 0);
    EXPECT_TRUE(ok);
    return ok ? std::move(pixels) : Pixels{};
  }
  // Nearest sampling at the original extent keeps placement and preview
  // minification out of the height comparison.
  auto surface = target(image->dimensions(), kN32_SkColorType, context);
  if (!surface) return {};
  SkPaint ink;
  ink.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawImage(
      image, 0, 0, SkSamplingOptions(SkFilterMode::kNearest), &ink);
  return read(*surface, context);
}

material::Material placed(const sk_sp<SkImage>& image, SkISize size) {
  material::Texture texture(image);
  glm::mat3 placement(1);
  placement[0][0] = float(size.width()) / image->width();
  placement[1][1] = float(size.height()) / image->height();
  texture.uv(placement);
  return material::image(std::move(texture));
}

Pixels paint(const material::Material& value, SkISize size, SkColorType type,
             GraphiteContext* context = nullptr) {
  auto surface = target(size, type, context);
  if (!surface) return {};
  material::FrameData frame{
      .resolution = {float(size.width()), float(size.height())},
      .recorder = context ? context->recorder() : nullptr};
  SkPaint ink;
  ink.setBlendMode(SkBlendMode::kSrc);
  const auto lowered = material::skia::paint(value);
  if (lowered.isSolid()) {
    ink.setColor4f(material::skia::toSkColor(lowered.solidColor()), nullptr);
  } else {
    ink.setShader(material::skia::shader(lowered, frame));
    EXPECT_TRUE(ink.getShader());
    if (!ink.getShader()) return {};
  }
  surface->getCanvas()->drawPaint(ink);
  return read(*surface, context);
}

void checkHeight(const Pixels& pixels) {
  ASSERT_EQ(pixels.size, kHeightSize);
  ASSERT_EQ(pixels.rgba.size(), size_t(1024 * 128 * 4));
  size_t nonfinite = 0, marked = 0, clear = 0;
  float peak = 0, greyError = 0, alphaError = 0;
  for (size_t i = 0; i < pixels.rgba.size(); i += 4) {
    for (size_t channel = 0; channel < 4; ++channel)
      nonfinite += !std::isfinite(pixels.rgba[i + channel]);
    peak = std::max(peak, pixels.rgba[i]);
    marked += pixels.rgba[i] > 0;
    clear += pixels.rgba[i] == 0;
    greyError =
        std::max({greyError, std::abs(pixels.rgba[i] - pixels.rgba[i + 1]),
                  std::abs(pixels.rgba[i] - pixels.rgba[i + 2])});
    alphaError = std::max(alphaError, std::abs(pixels.rgba[i + 3] - 1));
  }
  EXPECT_EQ(nonfinite, 0u);
  EXPECT_GT(marked, 0u);
  EXPECT_GT(clear, 0u);
  EXPECT_GT(peak, .05f);
  EXPECT_LE(peak, 1);
  EXPECT_FLOAT_EQ(greyError, 0);
  EXPECT_FLOAT_EQ(alphaError, 0);
}

void checkNormals(const Pixels& pixels, bool flat = false) {
  ASSERT_EQ(pixels.size, kNormalSize);
  ASSERT_EQ(pixels.rgba.size(), size_t(964 * 116 * 4));
  size_t nonfinite = 0, tilted = 0;
  float lengthError = 0, alphaError = 0, flatError = 0, minimumZ = 1;
  for (size_t i = 0; i < pixels.rgba.size(); i += 4) {
    for (size_t channel = 0; channel < 4; ++channel)
      nonfinite += !std::isfinite(pixels.rgba[i + channel]);
    const float x = pixels.rgba[i] * 2 - 1;
    const float y = pixels.rgba[i + 1] * 2 - 1;
    const float z = pixels.rgba[i + 2] * 2 - 1;
    tilted += std::abs(x) + std::abs(y) > .01f;
    minimumZ = std::min(minimumZ, z);
    lengthError =
        std::max(lengthError, std::abs(std::sqrt(x * x + y * y + z * z) - 1));
    alphaError = std::max(alphaError, std::abs(pixels.rgba[i + 3] - 1));
    flatError =
        std::max({flatError, std::abs(x), std::abs(y), std::abs(z - 1)});
  }
  EXPECT_EQ(nonfinite, 0u);
  EXPECT_GE(minimumZ, 0);
  EXPECT_LT(lengthError, .006f);
  EXPECT_FLOAT_EQ(alphaError, 0);
  if (flat)
    EXPECT_FLOAT_EQ(flatError, 0);
  else
    EXPECT_GT(tilted, 0u);
}

/** How many pixels of two reads of one size differ at all, and by how
 *  much on the widest channel. */
struct Difference {
  float maximum = 0;
  size_t changedPixels = 0;
};

Difference difference(const Pixels& actual, const Pixels& expected) {
  EXPECT_EQ(actual.size, expected.size);
  EXPECT_EQ(actual.rgba.size(), expected.rgba.size());
  Difference result;
  if (actual.rgba.size() != expected.rgba.size()) return result;
  for (size_t index = 0; index < actual.rgba.size(); index += 4) {
    bool changed = false;
    for (size_t channel = 0; channel < 4; ++channel) {
      const float error = std::abs(actual.rgba[index + channel] -
                                   expected.rgba[index + channel]);
      result.maximum = std::max(result.maximum, error);
      changed |= error != 0;
    }
    result.changedPixels += changed;
  }
  return result;
}

}  // namespace

TEST(GraphicsHeightGpu,
     EachBackendRepeatsItsHeightKeepsAnUploadAndDerivesUnitNormals) {
  auto device = sigil::core::hardware::GpuDevice::createOwned();
  if (!device) GTEST_SKIP() << "no GPU device";
  auto context = GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context";
  auto rasterHost = target(kHeightSize, kRGBA_F32_SkColorType);
  auto deviceHost = target(kHeightSize, kRGBA_F16_SkColorType, context.get());
  ASSERT_TRUE(rasterHost);
  ASSERT_TRUE(deviceHost);
  const auto rasterImage = height(*rasterHost);
  const auto deviceImage = height(*deviceHost);
  ASSERT_TRUE(rasterImage);
  ASSERT_TRUE(deviceImage);
  EXPECT_FALSE(rasterImage->isTextureBacked());
  EXPECT_TRUE(deviceImage->isTextureBacked());
  EXPECT_EQ(rasterImage->colorType(), kN32_SkColorType);
  EXPECT_EQ(deviceImage->colorType(), kN32_SkColorType);
  const auto rasterHeight = readImage(rasterImage);
  const auto deviceHeight = readImage(deviceImage, context.get());
  const auto copiedHeight = readImage(rasterImage, context.get());
  checkHeight(rasterHeight);
  checkHeight(deviceHeight);
  checkHeight(copiedHeight);
  const auto repeatedRaster = height(*rasterHost);
  const auto repeatedDevice = height(*deviceHost);
  ASSERT_TRUE(repeatedRaster);
  ASSERT_TRUE(repeatedDevice);
  EXPECT_EQ(rasterHeight.rgba, readImage(repeatedRaster).rgba);
  EXPECT_EQ(deviceHeight.rgba, readImage(repeatedDevice, context.get()).rgba);

  const auto sharedHeight = placed(rasterImage, kNormalSize);
  const auto nativeHeight = placed(deviceImage, kNormalSize);
  const auto sharedNormal = material::surface::normalFromHeight(
      sharedHeight, {.depth = 7, .step = .8f});
  const auto nativeNormal = material::surface::normalFromHeight(
      nativeHeight, {.depth = 7, .step = .8f});
  const auto rasterNormal =
      paint(sharedNormal, kNormalSize, kRGBA_F32_SkColorType);
  const auto rasterHalfNormal =
      paint(sharedNormal, kNormalSize, kRGBA_F16_SkColorType);
  const auto sharedDeviceNormal =
      paint(sharedNormal, kNormalSize, kRGBA_F16_SkColorType, context.get());
  const auto nativeDeviceNormal =
      paint(nativeNormal, kNormalSize, kRGBA_F16_SkColorType, context.get());
  checkNormals(rasterNormal);
  checkNormals(rasterHalfNormal);
  checkNormals(sharedDeviceNormal);
  checkNormals(nativeDeviceNormal);
  const auto constantNormal = material::surface::normalFromHeight(
      material::Color{.4f, .4f, .4f, 1}, {.depth = 7, .step = .8f});
  const auto zeroDepthNormal = material::surface::normalFromHeight(
      sharedHeight, {.depth = 0, .step = .8f});
  for (const auto& flat : {constantNormal, zeroDepthNormal}) {
    checkNormals(paint(flat, kNormalSize, kRGBA_F32_SkColorType), true);
    checkNormals(paint(flat, kNormalSize, kRGBA_F16_SkColorType, context.get()),
                 true);
  }
  // The raster height drawn unchanged onto the device keeps every pixel,
  // so a difference between the two backends' own heights is in how each
  // drew the fibres, never in the upload.
  const Difference uploaded = difference(copiedHeight, rasterHeight);
  EXPECT_EQ(uploaded.changedPixels, 0u);
  EXPECT_FLOAT_EQ(uploaded.maximum, 0);
}
