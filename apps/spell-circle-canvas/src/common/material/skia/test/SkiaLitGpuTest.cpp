/** @file
 * A lit surface on a device: mapped inputs bound per destination, float
 * targets keeping their range, and live and positioned lights, height
 * relief and normal strength matching the raster result.
 */

#include "SkiaLitTestSupport.h"

#if defined(__APPLE__)
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include "GraphiteReadback.h"

namespace {

class MappedDeviceBinding final : public sigil::media::DeviceBinding {
 public:
  struct Entry {
    skgpu::graphite::Recorder* recorder;
    sk_sp<SkImage> image;
    int reads = 0;
  };
  std::array<Entry, 2> devices;
  sk_sp<SkImage> raster;
  int rasterReads = 0;
  int unexpectedReads = 0;

  sk_sp<SkImage> image(skgpu::graphite::Recorder* recorder) override {
    if (!recorder) {
      ++rasterReads;
      return raster;
    }
    for (auto& entry : devices)
      if (entry.recorder == recorder) {
        ++entry.reads;
        return entry.image;
      }
    ++unexpectedReads;
    return nullptr;
  }
};

struct MappedDevicePicture {
  std::shared_ptr<MappedDeviceBinding> binding;
  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    sigil::media::Frame frame;
    frame.device = {.kind = sigil::media::DeviceFrame::Kind::Texture,
                    .width = 8,
                    .height = 8,
                    .binding = binding};
    return frame;
  }
  bool isRunning() const { return false; }
  SkISize size() const { return {8, 8}; }
  bool operator==(const MappedDevicePicture&) const = default;
};

}  // namespace

TEST(SkiaLitGpu, MappedLightingCopiesBindEachDestinationAndPreserveCoverage) {
  auto device = sigil::core::hardware::GpuDevice::createOwned();
  if (!device) GTEST_SKIP() << "no GPU device";
  auto first = sigil::skia::GraphiteContext::create(*device);
  auto second = sigil::skia::GraphiteContext::create(*device);
  if (!first || !second) GTEST_SKIP() << "no Graphite context";
  ASSERT_NE(first->recorder(), second->recorder());
  const SkImageInfo info =
      SkImageInfo::Make(8, 8, kRGBA_F16_SkColorType, kPremul_SkAlphaType);
  const std::array<SkColor4f, 3> colors{SkColor4f{2, .5f, .25f, .5f},
                                        SkColor4f{.25f, .5f, 2, .5f},
                                        SkColor4f{.25f, 1, .5f, .25f}};
  const auto sourceImage = [&](sigil::skia::GraphiteContext* context,
                               const SkColor4f& color) {
    auto surface = context ? SkSurfaces::RenderTarget(context->recorder(), info)
                           : SkSurfaces::Raster(info);
    if (!surface) return sk_sp<SkImage>{};
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    SkPaint ink;
    ink.setColor4f(color);
    surface->getCanvas()->drawPaint(ink);
    return surface->makeImageSnapshot();
  };
  const auto binding = std::make_shared<MappedDeviceBinding>();
  binding->devices = {
      {{first->recorder(), sourceImage(first.get(), colors[0])},
       {second->recorder(), sourceImage(second.get(), colors[1])}}};
  binding->raster = sourceImage(nullptr, colors[2]);
  ASSERT_TRUE(binding->devices[0].image);
  ASSERT_TRUE(binding->devices[1].image);
  ASSERT_TRUE(binding->raster);
  const Material material =
      image(Texture(sigil::media::PixelSource(MappedDevicePicture{binding}))
                .tile(Repeat::Pad)
                .sampling(Sampling::Nearest))
          .surface({.roughness = 1.f, .normal = from(Color{.5f, .5f, 1, 1})});
  const skia::LitSurface prepared(material);
  const skia::LitSurface copied = prepared;
  const Lighting lighting(Light{.intensity = 0, .ambient = 1});
  const auto draw = [&](const sk_sp<SkShader>& shader,
                        sigil::skia::GraphiteContext* context) {
    auto target = context ? SkSurfaces::RenderTarget(context->recorder(), info)
                          : SkSurfaces::Raster(info);
    if (!target) return SkBitmap{};
    target->getCanvas()->clear(SK_ColorTRANSPARENT);
    SkPaint ink;
    ink.setShader(shader);
    target->getCanvas()->drawPaint(ink);
    if (context)
      return sigil::skia::test::readGraphiteSurface(*context, target.get());
    SkBitmap pixels;
    pixels.allocPixels(info);
    if (!target->readPixels(pixels, 0, 0)) pixels.reset();
    return pixels;
  };
  sk_sp<SkShader> original;
  const std::array<sigil::skia::GraphiteContext*, 4> destinations{
      first.get(), nullptr, second.get(), first.get()};
  const std::array<size_t, 4> expectedIndex{0, 2, 1, 0};
  for (size_t step = 0; step < destinations.size(); ++step) {
    SCOPED_TRACE(step);
    auto* destination = destinations[step];
    const FrameData frame{
        .resolution = {8, 8},
        .recorder = destination ? destination->recorder() : nullptr};
    const glm::mat3 mapping{12.f + float(step) * 8, 0, 0, 0, 16, 0, 2, 3, 1};
    const auto built = copied.shader(lighting, frame, mapping);
    ASSERT_TRUE(built);
    if (step == 0) original = built;
    const SkBitmap pixels = draw(built, destination);
    ASSERT_FALSE(pixels.empty());
    const auto actual = pixels.getColor4f(4, 4);
    const auto& expected = colors[expectedIndex[step]];
    for (int channel = 0; channel < 4; ++channel)
      EXPECT_NEAR(actual[channel], expected[channel], .005f);
    EXPECT_FLOAT_EQ(actual.fA, expected.fA);
  }
  for (const float cutoff : {.25f, .5f, .75f}) {
    SCOPED_TRACE(cutoff);
    const skia::LitSurface cutout(
        from(material).surface({.roughness = 1.f, .alphaCutoff = cutoff}));
    for (size_t step = 0; step < destinations.size(); ++step) {
      SCOPED_TRACE(step);
      auto* destination = destinations[step];
      const FrameData frame{
          .resolution = {8, 8},
          .recorder = destination ? destination->recorder() : nullptr};
      const auto built = cutout.shader(lighting, frame, glm::mat3{1});
      ASSERT_TRUE(built);
      const SkBitmap pixels = draw(built, destination);
      ASSERT_FALSE(pixels.empty());
      const auto actual = pixels.getColor4f(4, 4);
      const auto source = colors[expectedIndex[step]];
      const auto expected = source.fA < cutoff ? SkColor4f{0, 0, 0, 0} : source;
      for (int channel = 0; channel < 4; ++channel)
        EXPECT_NEAR(actual[channel], expected[channel], .005f);
      EXPECT_FLOAT_EQ(actual.fA, expected.fA);
    }
  }
  const SkBitmap held = draw(original, first.get());
  ASSERT_FALSE(held.empty());
  const auto actual = held.getColor4f(4, 4);
  for (int channel = 0; channel < 4; ++channel)
    EXPECT_NEAR(actual[channel], colors[0][channel], .005f);
  EXPECT_GT(binding->devices[0].reads, 0);
  EXPECT_GT(binding->devices[1].reads, 0);
  EXPECT_GT(binding->rasterReads, 0);
  EXPECT_EQ(binding->unexpectedReads, 0);
}

TEST(SkiaLitGpu, FloatTargetsPreserveHdrThroughPartialCoverage) {
  auto device = sigil::core::hardware::GpuDevice::createOwned();
  if (!device) GTEST_SKIP() << "no GPU device";
  auto graphite = sigil::skia::GraphiteContext::create(*device);
  if (!graphite) GTEST_SKIP() << "no Graphite context";
  constexpr int width = 64, height = 24;
  const Light dark{.intensity = 0, .ambient = 0};
  auto emitted = [&](Color color) {
    return skia::lit(from(Color{0, 0, 0, 1})
                         .surface({.emission = color, .emissionStrength = 1}),
                     dark);
  };
  const Paint unit = emitted({1, 1, 1, 1});
  const Paint hdr = emitted({4, 2, 1, 1});
  const SkPath curve = SkPathBuilder()
                           .moveTo(20.25f, 18.5f)
                           .cubicTo(24.75f, -4.5f, 35.25f, 27.5f, 41.5f, 4.25f)
                           .detach();
  auto draw = [&](bool gpu, const Paint& material, SkColor4f ground) {
    const SkImageInfo info = SkImageInfo::Make(
        width, height, gpu ? kRGBA_F16_SkColorType : kRGBA_F32_SkColorType,
        kPremul_SkAlphaType);
    auto target = gpu ? SkSurfaces::RenderTarget(graphite->recorder(), info)
                      : SkSurfaces::Raster(info);
    EXPECT_TRUE(target);
    if (!target) return SkBitmap{};
    SkCanvas& canvas = *target->getCanvas();
    canvas.clear(ground);
    FrameData frame{.resolution = {float(width), float(height)},
                    .recorder = gpu ? graphite->recorder() : nullptr};
    SkPaint ink;
    ink.setAntiAlias(true);
    ink.setShader(skia::shader(material, frame));
    EXPECT_TRUE(ink.getShader());
    canvas.drawRect(SkRect::MakeLTRB(2.25f, 2.5f, 14.75f, 20.25f), ink);
    canvas.drawOval(SkRect::MakeLTRB(46.25f, 2.75f, 62.5f, 20.25f), ink);
    ink.setStyle(SkPaint::kStroke_Style);
    ink.setStrokeWidth(3.5f);
    ink.setStrokeCap(SkPaint::kRound_Cap);
    canvas.drawPath(curve, ink);
    if (gpu)
      return sigil::skia::test::readGraphiteSurface(*graphite, target.get());
    SkBitmap pixels;
    EXPECT_TRUE(pixels.tryAllocPixels(info));
    EXPECT_TRUE(target->readPixels(pixels.pixmap(), 0, 0));
    return pixels;
  };
  for (bool gpu : {false, true}) {
    SCOPED_TRACE(gpu ? "Graphite F16" : "raster F32");
    const SkBitmap coverage = draw(gpu, unit, {0, 0, 0, 0});
    ASSERT_FALSE(coverage.isNull());
    for (SkColor4f ground :
         {SkColor4f{0, 0, 0, 0}, SkColor4f{.05f, .1f, .15f, 1}}) {
      SCOPED_TRACE(ground.fA);
      const SkBitmap actual = draw(gpu, hdr, ground);
      ASSERT_FALSE(actual.isNull());
      std::array<int, 3> fractional{}, interior{};
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
          const float alpha = coverage.getColor4f(x, y).fA;
          const SkColor4f pixel = actual.getColor4f(x, y);
          const float outputAlpha = alpha + ground.fA * (1 - alpha);
          // getColor4f reads straight color; compare the stored premultiplied
          // energy against this executor's own geometry coverage.
          EXPECT_NEAR(pixel.fR * pixel.fA,
                      4 * alpha + ground.fR * ground.fA * (1 - alpha), .012f);
          EXPECT_NEAR(pixel.fG * pixel.fA,
                      2 * alpha + ground.fG * ground.fA * (1 - alpha), .008f);
          EXPECT_NEAR(pixel.fB * pixel.fA,
                      alpha + ground.fB * ground.fA * (1 - alpha), .004f);
          EXPECT_NEAR(pixel.fA, outputAlpha, .001f);
          const size_t region = x < 16 ? 0 : x < 44 ? 1 : 2;
          if (alpha > .02f && alpha < .98f) ++fractional[region];
          if (alpha > .98f) ++interior[region];
        }
      for (size_t region = 0; region < fractional.size(); ++region) {
        SCOPED_TRACE(region);
        EXPECT_GT(fractional[region], 10);
        EXPECT_GT(interior[region], 20);
      }
      EXPECT_NEAR(actual.getColor4f(8, 8).fR, 4, .004f);
      EXPECT_NEAR(actual.getColor4f(8, 8).fG, 2, .002f);
      EXPECT_NEAR(actual.getColor4f(54, 11).fR, 4, .004f);
    }
  }
}

TEST(SkiaLitGpu, LiveLightColorMatchesRasterAndRetainsPreparedNormals) {
  auto device = sigil::core::hardware::GpuDevice::createOwned();
  if (!device) GTEST_SKIP() << "no GPU device";
  auto graphite = sigil::skia::GraphiteContext::create(*device);
  if (!graphite) GTEST_SKIP() << "no Graphite context";
  constexpr int width = 24, height = 12;
  const auto reads = std::make_shared<int>(0);
  const Material normals = image(sigil::media::PixelSource(
      CountedPicture{reads, twoFacedNormals(width, height)}));
  const Material material = from(Color{.6f, .6f, .6f, 1})
                                .surface({.roughness = 1.f, .normal = normals});
  const skia::LitSurface prepared(material);
  EXPECT_EQ(*reads, 0);
  auto color = sigil::motion::animatable(Color{3, 0, 0, 1});
  const Light light{.elevation = 90, .color = color, .ambient = 0};
  const Paint live = prepared.under(light);
  const int readOnce = *reads;
  ASSERT_GT(readOnce, 0);
  EXPECT_TRUE(live.isRunning());
  for (Color endpoint :
       {Color{3, 0, 0, 1}, Color{0, 3, 0, 1}, Color{0, 0, 3, 1}}) {
    color = endpoint;
    const SkBitmap expected = drawnFloat(live, width, height);
    auto target = SkSurfaces::RenderTarget(
        graphite->recorder(),
        SkImageInfo::Make(width, height, kRGBA_F16_SkColorType,
                          kPremul_SkAlphaType));
    ASSERT_TRUE(target);
    target->getCanvas()->clear(SK_ColorTRANSPARENT);
    FrameData frame{.resolution = {float(width), float(height)},
                    .recorder = graphite->recorder()};
    auto shader = skia::shader(live, frame);
    ASSERT_TRUE(shader);
    SkPaint fill;
    fill.setShader(std::move(shader));
    target->getCanvas()->drawRect(SkRect::MakeWH(width, height), fill);
    const SkBitmap actual =
        sigil::skia::test::readGraphiteSurface(*graphite, target.get());
    ASSERT_FALSE(actual.isNull());
    for (int x : {2, 12, 22}) {
      const SkColor4f a = actual.getColor4f(x, 6);
      const SkColor4f e = expected.getColor4f(x, 6);
      EXPECT_NEAR(a.fR, e.fR, .005f);
      EXPECT_NEAR(a.fG, e.fG, .005f);
      EXPECT_NEAR(a.fB, e.fB, .005f);
      EXPECT_FLOAT_EQ(a.fA, e.fA);
      EXPECT_GT(std::max({a.fR, a.fG, a.fB}), 1);
    }
    EXPECT_EQ(*reads, readOnce);
    EXPECT_FLOAT_EQ(light.intensity.value(), 1);
  }
}

TEST(SkiaLitGpu, PositionedLightsMatchRasterUnderAffinePlacement) {
  auto device = sigil::core::hardware::GpuDevice::createOwned();
  if (!device) GTEST_SKIP() << "no GPU device";
  auto graphite = sigil::skia::GraphiteContext::create(*device);
  if (!graphite) GTEST_SKIP() << "no Graphite context";
  constexpr int width = 32, height = 16;
  const Material material =
      from(Color{.3f, .4f, .5f, 1})
          .surface({.roughness = .7f,
                    .normal = image(twoFacedNormals(width, height))});
  FrameData frame;
  frame.resolution = {width, height};
  frame.world = glm::mat3{0, 1, 0, -1, 0, 0, 20, 8, 1};
  Light point;
  point.kind = LightKind::Point;
  point.position = {12, 24, 20};
  point.range = 100;
  point.intensity = .3f;
  point.ambient = .08f;
  Light spot = point;
  spot.kind = LightKind::Spot;
  spot.elevation = 90;
  spot.innerAngle = 20;
  spot.outerAngle = 45;
  Light behind = point;
  behind.position.z = -20;
  Light zeroRange = spot;
  zeroRange.range = 0;
  const Light key = studio({.direction = 0,
                            .elevation = 35,
                            .color = Color{.1f, .2f, .3f, 1},
                            .intensity = .2f,
                            .ambient = .04f});
  Lighting sceneDirectional = key;
  sceneDirectional.frame = LightingFrame::Scene;
  Lighting sceneEnvironment = environment(from(Color{.1f, .2f, .3f, 1}),
                                          {.intensity = .5f, .size = {1, 1}});
  sceneEnvironment.frame = LightingFrame::Scene;
  for (const Lighting& lighting :
       {Lighting(point), Lighting(spot), Lighting(behind), Lighting(zeroRange),
        Lighting(std::vector{key, point, spot}),
        Lighting(std::vector{zeroRange, point, key}), sceneDirectional,
        sceneEnvironment}) {
    SCOPED_TRACE(lighting.lights.size());
    const Paint fill = skia::lit(material, lighting);
    ASSERT_FALSE(fill.isSolid());
    const SkBitmap expected = drawn(fill, width, height, frame);
    auto target = SkSurfaces::RenderTarget(
        graphite->recorder(), SkImageInfo::MakeN32Premul(width, height));
    ASSERT_TRUE(target);
    FrameData gpuFrame = frame;
    gpuFrame.recorder = graphite->recorder();
    auto shader = skia::shader(fill, gpuFrame);
    ASSERT_TRUE(shader);
    SkPaint paint;
    paint.setShader(std::move(shader));
    target->getCanvas()->drawRect(SkRect::MakeWH(width, height), paint);
    const SkBitmap actual =
        sigil::skia::test::readGraphiteSurface(*graphite, target.get());
    ASSERT_FALSE(actual.isNull());
    for (int y : {2, 8, 13}) {
      for (int x : {2, 10, 22, 29}) {
        SCOPED_TRACE(x);
        SCOPED_TRACE(y);
        const SkColor a = actual.getColor(x, y), e = expected.getColor(x, y);
        EXPECT_NEAR(SkColorGetR(a), SkColorGetR(e), 2);
        EXPECT_NEAR(SkColorGetG(a), SkColorGetG(e), 2);
        EXPECT_NEAR(SkColorGetB(a), SkColorGetB(e), 2);
        EXPECT_EQ(SkColorGetA(a), SkColorGetA(e));
      }
    }
  }
}

TEST(SkiaLitGpu, UnitHeightReliefKeepsPixelMetricAcrossAffineMappings) {
  auto device = sigil::core::hardware::GpuDevice::createOwned();
  if (!device) GTEST_SKIP() << "no GPU device";
  auto graphite = sigil::skia::GraphiteContext::create(*device);
  if (!graphite) GTEST_SKIP() << "no Graphite context";
  constexpr int width = 160, height = 80;
  const FrameData frame{.resolution = {width, height},
                        .world = glm::mat3{1, 0, 0, .15f, 1, 0, 9, 3, 1},
                        .recorder = graphite->recorder()};
  const Lighting light(Light{.intensity = .5f,
                             .ambient = 0,
                             .kind = LightKind::Point,
                             .position = {110, 34, 80},
                             .range = 240});
  const Material ramp = shader(R"(
half4 main(float2 p) { return half4(half3(.2 + .4 * p.x + .2 * p.y), 1); }
)");
  const auto finish = [](Material normal) {
    return from(Color{.2f, .3f, .4f, 1})
        .surface({.roughness = .8f, .normal = std::move(normal)});
  };
  const auto draw = [&](sk_sp<SkShader> shader) {
    auto target = SkSurfaces::RenderTarget(
        graphite->recorder(),
        SkImageInfo::Make(width, height, kRGBA_F16_SkColorType,
                          kPremul_SkAlphaType));
    EXPECT_TRUE(target);
    EXPECT_TRUE(shader);
    if (!target || !shader) return SkBitmap{};
    target->getCanvas()->clear(SK_ColorTRANSPARENT);
    SkPaint paint;
    paint.setShader(std::move(shader));
    target->getCanvas()->drawRect(SkRect::MakeWH(width, height), paint);
    return sigil::skia::test::readGraphiteSurface(*graphite, target.get());
  };
  struct Sample {
    glm::mat3 mapping;
    glm::vec2 slope;
  };
  // These node-space gradients follow directly from the authored linear
  // height, independently of the height-normal shader's sampling steps.
  const std::array samples{
      Sample{{24, 0, 0, 0, 40, 0, 12, 8, 1}, {.4f / 24, .2f / 40}},
      Sample{{120, 0, 0, 0, 40, 0, 12, 8, 1}, {.4f / 120, .2f / 40}},
      Sample{{0, 24, 0, -40, 0, 0, 80, 8, 1}, {-.2f / 40, .4f / 24}},
      Sample{{-24, 0, 0, 0, 40, 0, 80, 8, 1}, {-.4f / 24, .2f / 40}},
      Sample{{40, 0, 0, 20, 40, 0, 12, 8, 1}, {.01f, 0}},
  };
  for (float depth : {-6.f, 0.f, 6.f}) {
    SCOPED_TRACE(depth);
    const skia::LitSurface prepared(finish(surface::blendNormals(
        surface::normalFromHeight(ramp, {.depth = depth, .step = 1}),
        Color{.5f, .5f, 1, 1})));
    for (size_t index = 0; index < samples.size(); ++index) {
      SCOPED_TRACE(index);
      const Sample& sample = samples[index];
      const glm::vec3 normal = glm::normalize(
          glm::vec3{-depth * sample.slope.x, depth * sample.slope.y, 1});
      const skia::LitSurface reference(
          finish(Color{normal.x * .5f + .5f, normal.y * .5f + .5f,
                       normal.z * .5f + .5f, 1}));
      const SkBitmap expected =
          draw(skia::shader(reference.under(light), frame));
      const SkBitmap actual =
          draw(prepared.shader(light, frame, sample.mapping));
      ASSERT_FALSE(expected.isNull());
      ASSERT_FALSE(actual.isNull());
      EXPECT_EQ(actual.dimensions(), expected.dimensions());
      for (float u : {.3f, .5f, .7f}) {
        const glm::vec3 at = sample.mapping * glm::vec3{u, .5f, 1};
        const int x = static_cast<int>(at.x), y = static_cast<int>(at.y);
        const SkColor4f a = actual.getColor4f(x, y);
        const SkColor4f b = expected.getColor4f(x, y);
        for (int channel = 0; channel < 3; ++channel) {
          EXPECT_TRUE(std::isfinite(a[channel]));
          EXPECT_NEAR(a[channel], b[channel], .001f);
        }
        EXPECT_FLOAT_EQ(a.fA, 1);
      }
    }
  }
  const skia::LitSurface derived(
      finish(surface::normalFromHeight(ramp, {.depth = 6})));
  glm::mat3 projective = samples.front().mapping;
  projective[0][2] = .002f;
  const SkBitmap flattened = draw(derived.shader(light, frame, projective));
  const skia::LitSurface flat(finish(Color{.5f, .5f, 1, 1}));
  const SkBitmap expected = draw(skia::shader(flat.under(light), frame));
  ASSERT_FALSE(flattened.isNull());
  ASSERT_FALSE(expected.isNull());
  for (int x : {20, 28, 36}) {
    const auto a = flattened.getColor4f(x, 24);
    const auto b = expected.getColor4f(x, 24);
    for (int channel = 0; channel < 3; ++channel)
      EXPECT_NEAR(a[channel], b[channel], .001f);
    EXPECT_FLOAT_EQ(a.fA, 1);
  }
  glm::mat3 singular = samples.front().mapping;
  singular[0] = glm::vec3{0};
  EXPECT_FALSE(derived.shader(light, frame, singular));
}

TEST(SkiaLitGpu, ZeroNormalStrengthMatchesAFlatSurfaceAcrossMapZ) {
  auto device = sigil::core::hardware::GpuDevice::createOwned();
  if (!device) GTEST_SKIP() << "no GPU device";
  auto graphite = sigil::skia::GraphiteContext::create(*device);
  if (!graphite) GTEST_SKIP() << "no Graphite context";

  constexpr int mapWidth = 128, mapHeight = 32;
  constexpr int width = 192, height = 48;
  SkBitmap normals;
  normals.allocN32Pixels(mapWidth, mapHeight, true);
  for (int y = 0; y < mapHeight; ++y)
    for (int x = 0; x < mapWidth; ++x)
      *normals.getAddr32(x, y) =
          SkPreMultiplyARGB(255, 128, 128, 128 + (x + y * 3) % 128);
  normals.setImmutable();
  glm::mat3 placement(1);
  placement[0][0] = 1.371f;
  placement[1][1] = .913f;
  placement[2][0] = .375f;
  placement[2][1] = -.625f;
  const Material normal = image(Texture(normals.asImage())
                                    .uv(placement)
                                    .tile(Repeat::Pad)
                                    .sampling(Sampling::Linear));
  const auto draw = [&](const Material& material, const Lighting& lighting) {
    auto surface = SkSurfaces::RenderTarget(
        graphite->recorder(), SkImageInfo::MakeN32Premul(width, height));
    if (!surface) return SkBitmap();
    FrameData frame;
    frame.resolution = {float(width), float(height)};
    auto shader = skia::shader(skia::lit(material, lighting), frame);
    if (!shader) return SkBitmap();
    SkPaint fill;
    fill.setShader(std::move(shader));
    surface->getCanvas()->drawRect(SkRect::MakeWH(width, height), fill);
    return sigil::skia::test::readGraphiteSurface(*graphite, surface.get());
  };
  const Light key = studio(
      {.direction = 110, .elevation = 46, .intensity = .62f, .ambient = .42f});
  const EnvironmentMap panorama =
      EnvironmentMap::baked(32, [](float u, float v) {
        return glm::vec3{.66f + 4.8f * u, .64f + 2.8f * v, .60f + 3.4f * u * v};
      });
  const Environment around =
      environment(panorama.texture().source(), {.intensity = .52f});
  for (const auto& lighting :
       {Lighting(key), Lighting(around), Lighting(key, around)}) {
    for (float clearcoat : {0.0f, 1.0f}) {
      SCOPED_TRACE(clearcoat);
      const Material flat =
          from(Color{.4f, .65f, .35f, 1})
              .surface({.roughness = .17f, .clearcoat = clearcoat});
      const Material mapped = from(Color{.4f, .65f, .35f, 1})
                                  .surface({.roughness = .17f,
                                            .normal = normal,
                                            .normalScale = 0,
                                            .clearcoat = clearcoat});
      const SkBitmap expected = draw(flat, lighting);
      const SkBitmap actual = draw(mapped, lighting);
      ASSERT_FALSE(expected.isNull());
      ASSERT_FALSE(actual.isNull());
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const SkColor reference = expected.getColor(x, y);
          const SkColor result = actual.getColor(x, y);
          EXPECT_NEAR(SkColorGetR(result), SkColorGetR(reference), 1) << x;
          EXPECT_NEAR(SkColorGetG(result), SkColorGetG(reference), 1) << x;
          EXPECT_NEAR(SkColorGetB(result), SkColorGetB(reference), 1) << x;
          EXPECT_EQ(SkColorGetA(result), SkColorGetA(reference)) << x;
        }
      }
    }
  }
}
#endif
