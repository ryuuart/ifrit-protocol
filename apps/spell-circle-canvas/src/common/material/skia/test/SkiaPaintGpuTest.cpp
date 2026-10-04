/** @file
 * A paint on a device: textures bound where they stand on each recorder,
 * a raster source uploaded once, and a replaced source reaching every
 * destination.
 */

#include "SkiaPaintTestSupport.h"

#if defined(__APPLE__)
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>

#include <chrono>

namespace {

constexpr int kDeviceTileSize = 16;
constexpr SkColor4f kFirstDeviceColor{4, 1, .5f, .5f};
constexpr SkColor4f kSecondDeviceColor{.5f, 1, 4, .5f};
constexpr SkColor4f kRasterColor{.5f, 3, 1, .25f};

SkImageInfo deviceTileInfo() {
  return SkImageInfo::Make(kDeviceTileSize, kDeviceTileSize,
                           kRGBA_F16_SkColorType, kPremul_SkAlphaType);
}

void paintDeviceTile(SkSurface& surface, const SkColor4f& color) {
  surface.getCanvas()->clear(SK_ColorTRANSPARENT);
  SkPaint ink;
  ink.setColor4f(color);
  surface.getCanvas()->drawRect(SkRect::MakeWH(8, kDeviceTileSize), ink);
}

class DeviceSourceBinding final : public sigil::media::DeviceBinding {
 public:
  struct Entry {
    skgpu::graphite::Recorder* recorder;
    sk_sp<SkImage> image;
    int reads = 0;
  };
  std::array<Entry, 2> devices;
  sk_sp<SkImage> raster;
  int frameReads = 0;
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

  void resetReads() {
    frameReads = rasterReads = unexpectedReads = 0;
    for (auto& entry : devices) entry.reads = 0;
  }
};

struct HeldDevicePicture {
  std::shared_ptr<DeviceSourceBinding> binding;
  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    ++binding->frameReads;
    sigil::media::Frame frame;
    frame.device = {.kind = sigil::media::DeviceFrame::Kind::Texture,
                    .width = kDeviceTileSize,
                    .height = kDeviceTileSize,
                    .binding = binding};
    return frame;
  }
  bool isRunning() const { return false; }
  glm::ivec2 size() const { return {kDeviceTileSize, kDeviceTileSize}; }
  bool operator==(const HeldDevicePicture&) const = default;
};

struct HeldRasterPicture {
  std::shared_ptr<DeviceSourceBinding> binding;
  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    ++binding->frameReads;
    sigil::media::Frame frame;
    frame.image = binding->raster;
    return frame;
  }
  bool isRunning() const { return false; }
  glm::ivec2 size() const { return {kDeviceTileSize, kDeviceTileSize}; }
  bool operator==(const HeldRasterPicture&) const = default;
};

enum class DevicePaintKind {
  Static,
  Geometry,
  Time,
  Nested,
  Raw,
  Layered,
  ReverseRaw,
  ReverseLayered
};
struct DevicePaintCase {
  const char* name;
  DevicePaintKind kind;
};

Texture heldDeviceTexture(const std::shared_ptr<DeviceSourceBinding>& binding) {
  return Texture(HeldDevicePicture{binding})
      .sampling(Sampling::Nearest)
      .tile(Repeat::None);
}

Paint heldDevicePaint(const std::shared_ptr<DeviceSourceBinding>& binding,
                      DevicePaintKind kind) {
  struct Parameters {
    float uGain = 1;
  };
  if (kind == DevicePaintKind::Raw) {
    const auto effect = effectFor(
        "uniform shader source; uniform float uGain;"
        "half4 main(float2 p) { return source.eval(p) * uGain; }");
    return skia::sksl(effect)
        .set("uGain", 1.f)
        .slot("source", Paint::recipe(image(heldDeviceTexture(binding))));
  }
  if (kind == DevicePaintKind::Layered)
    return Paint::recipe(
        from(Color{0, 0, 0, 0}).layer(image(heldDeviceTexture(binding))));
  auto recipe = Recipe::of<Parameters>("paint.device-source").slot("source");
  if (kind == DevicePaintKind::Geometry)
    recipe.frame(FrameInput::Resolution)
        .body(Target::SkSL,
              "half4 main(float2 p) { return source.eval(p * 16 / "
              "uResolution) * uGain; }");
  else if (kind == DevicePaintKind::Time)
    recipe.frame(FrameInput::Time)
        .body(Target::SkSL,
              "half4 main(float2 p) { return source.eval(p + float2(uTime, "
              "0)) * uGain; }");
  else
    recipe.body(Target::SkSL,
                "half4 main(float2 p) { return source.eval(p) * uGain; }");
  Material material(std::make_shared<const Recipe>(std::move(recipe)));
  material.set("uGain", 1.f);
  if (kind == DevicePaintKind::ReverseRaw ||
      kind == DevicePaintKind::ReverseLayered)
    return Paint::recipe(std::move(material))
        .slot("source",
              heldDevicePaint(binding, kind == DevicePaintKind::ReverseRaw
                                           ? DevicePaintKind::Raw
                                           : DevicePaintKind::Layered));
  if (kind == DevicePaintKind::Nested)
    material.slot("source", image(heldDeviceTexture(binding)));
  else
    material.slot("source", heldDeviceTexture(binding));
  return Paint::recipe(std::move(material));
}

class SkiaPaintGpu : public testing::Test {
 protected:
  void SetUp() override {
    device = sigil::core::hardware::GpuDevice::createOwned();
    if (!device) GTEST_SKIP() << "no GPU device";
    first = sigil::skia::GraphiteContext::create(*device);
    second = sigil::skia::GraphiteContext::create(*device);
    if (!first || !second) GTEST_SKIP() << "no Graphite context";
    ASSERT_NE(first->recorder(), second->recorder());
    binding = makeBinding(kFirstDeviceColor, kSecondDeviceColor, kRasterColor);
    ASSERT_TRUE(binding);
  }

  std::shared_ptr<DeviceSourceBinding> makeBinding(
      const SkColor4f& firstColor, const SkColor4f& secondColor,
      const SkColor4f& rasterColor) {
    const auto firstTile =
        SkSurfaces::RenderTarget(first->recorder(), deviceTileInfo());
    const auto secondTile =
        SkSurfaces::RenderTarget(second->recorder(), deviceTileInfo());
    const auto rasterTile = SkSurfaces::Raster(deviceTileInfo());
    if (!firstTile || !secondTile || !rasterTile) return nullptr;
    paintDeviceTile(*firstTile, firstColor);
    paintDeviceTile(*secondTile, secondColor);
    paintDeviceTile(*rasterTile, rasterColor);
    auto source = std::make_shared<DeviceSourceBinding>();
    source->devices = {{{first->recorder(), firstTile->makeImageSnapshot()},
                        {second->recorder(), secondTile->makeImageSnapshot()}}};
    source->raster = rasterTile->makeImageSnapshot();
    return source;
  }

  FrameData frame(sigil::skia::GraphiteContext* context = nullptr) const {
    return {.resolution = {kDeviceTileSize, kDeviceTileSize},
            .recorder = context ? context->recorder() : nullptr};
  }

  SkBitmap draw(const sk_sp<SkShader>& shader,
                sigil::skia::GraphiteContext* context = nullptr) {
    auto surface = context ? SkSurfaces::RenderTarget(context->recorder(),
                                                      deviceTileInfo())
                           : SkSurfaces::Raster(deviceTileInfo());
    if (!surface) return {};
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    if (shader) {
      SkPaint ink;
      ink.setShader(shader);
      surface->getCanvas()->drawPaint(ink);
    }
    SkBitmap pixels;
    pixels.allocPixels(deviceTileInfo());
    const bool read = context ? sigil::skia::readbackPixels(*context, *surface,
                                                            pixels.pixmap())
                              : surface->readPixels(pixels, 0, 0);
    if (!read) pixels.reset();
    return pixels;
  }

  void expectTile(const sk_sp<SkShader>& shader, const SkColor4f& expected,
                  sigil::skia::GraphiteContext* context = nullptr) {
    const SkBitmap pixels = draw(shader, context);
    ASSERT_FALSE(pixels.empty());
    const SkColor4f actual = pixels.getColor4f(4, 4);
    for (int channel = 0; channel < 4; ++channel)
      EXPECT_NEAR(actual[channel], expected[channel], .005f)
          << "channel " << channel;
    EXPECT_EQ(pixels.getColor4f(12, 4).fA, 0);
  }

  std::unique_ptr<sigil::core::hardware::GpuDevice> device;
  std::unique_ptr<sigil::skia::GraphiteContext> first, second;
  std::shared_ptr<DeviceSourceBinding> binding;
};

class SkiaPaintGpuDeviceSources
    : public SkiaPaintGpu,
      public testing::WithParamInterface<DevicePaintCase> {};

class SkiaPaintGpuSourceReplacement
    : public SkiaPaintGpu,
      public testing::WithParamInterface<DevicePaintCase> {};

}  // namespace

TEST_F(SkiaPaintGpu,
       PreparedMappedSurfaceBindsItsFirstDeviceWithoutRasterReads) {
  const auto normals =
      makeBinding({.5f, .5f, 1, 1}, {.5f, .5f, 1, 1}, {1, .5f, .5f, 1});
  ASSERT_TRUE(normals);
  const glm::mat3 sourceToUnit{1.f / 16, 0, 0, 0, 1.f / 16, 0, 0, 0, 1};
  const glm::mat3 unitToNode{16, 0, 0, 0, 16, 0, 0, 0, 1};
  const Material pixels = image(heldDeviceTexture(binding).uv(sourceToUnit));
  const Material normal = image(heldDeviceTexture(normals).uv(sourceToUnit));
  Material layered =
      from(Color{0, 0, 0, 0}).layer(pixels, {.mask = Mask{normal}});
  Material sampled =
      sigil::material::shader("half4 main(float2 p) { return source.eval(p); }",
                              ShaderOptions{.textures = {{"source", {}}}});
  sampled.slot("source", layered);
  const Lighting lighting(Light{.intensity = 0, .ambient = 1});
  for (Material material : {pixels, layered, sampled}) {
    binding->resetReads();
    normals->resetReads();
    material.surface({.roughness = 1.f, .normal = normal});
    const skia::LitSurface prepared(material);
    EXPECT_EQ(binding->frameReads, 0);
    EXPECT_EQ(normals->frameReads, 0);
    EXPECT_EQ(binding->rasterReads, 0);
    EXPECT_EQ(normals->rasterReads, 0);
    const auto shader =
        prepared.shader(lighting, frame(first.get()), unitToNode);
    ASSERT_TRUE(shader);
    expectTile(shader, kFirstDeviceColor, first.get());
    EXPECT_GT(binding->devices[0].reads, 0);
    EXPECT_GT(normals->devices[0].reads, 0);
    EXPECT_EQ(binding->rasterReads, 0);
    EXPECT_EQ(normals->rasterReads, 0);
    const int colorReads = binding->devices[0].reads;
    const int normalReads = normals->devices[0].reads;
    expectTile(prepared.shader(lighting, frame(first.get()), unitToNode),
               kFirstDeviceColor, first.get());
    EXPECT_EQ(binding->devices[0].reads, colorReads);
    EXPECT_EQ(normals->devices[0].reads, normalReads);
    EXPECT_EQ(binding->rasterReads, 0);
    EXPECT_EQ(normals->rasterReads, 0);
    const Paint snapshot = prepared.under(lighting);
    ASSERT_TRUE(skia::staticShader(snapshot));
    expectTile(skia::staticShader(snapshot)->makeWithLocalMatrix(
                   skia::toSkMatrix(unitToNode)),
               kRasterColor);
    EXPECT_GT(binding->rasterReads, 0);
    EXPECT_GT(normals->rasterReads, 0);
    const int rasterColorReads = binding->rasterReads;
    const int rasterNormalReads = normals->rasterReads;
    prepared.under(lighting);
    EXPECT_EQ(binding->rasterReads, rasterColorReads);
    EXPECT_EQ(normals->rasterReads, rasterNormalReads);
  }
}

TEST_P(SkiaPaintGpuDeviceSources,
       HeldSourceFollowsRasterAndReplacementRecorders) {
  const DevicePaintKind kind = GetParam().kind;
  const Paint paint = heldDevicePaint(binding, kind);
  EXPECT_EQ(paint.isRunning(), kind == DevicePaintKind::Time);
  EXPECT_EQ(paint.geometryDependent(), kind == DevicePaintKind::Geometry);
  const auto snapshot = skia::shader(paint);
  const auto snapshotColor =
      kind == DevicePaintKind::Geometry ? SkColors::kTransparent : kRasterColor;
  expectTile(snapshot, snapshotColor);
  binding->resetReads();

  const auto gpu = skia::shader(paint, frame(first.get()));
  ASSERT_TRUE(gpu);
  expectTile(gpu, kFirstDeviceColor, first.get());
  EXPECT_GT(binding->devices[0].reads, 0);
  EXPECT_EQ(binding->rasterReads, 0);
  const int firstReads = binding->devices[0].reads;
  EXPECT_EQ(skia::shader(paint, frame(first.get())), gpu);
  EXPECT_EQ(binding->devices[0].reads, firstReads);

  const auto raster = skia::shader(paint, frame());
  expectTile(raster, kRasterColor);
  EXPECT_EQ(binding->rasterReads,
            kind == DevicePaintKind::Geometry || kind == DevicePaintKind::Time
                ? 1
                : 0);
  const int rasterReads = binding->rasterReads;
  EXPECT_EQ(skia::shader(paint, frame()), raster);
  EXPECT_EQ(binding->rasterReads, rasterReads);
  const auto gpuAgain = skia::shader(paint, frame(first.get()));
  expectTile(gpuAgain, kFirstDeviceColor, first.get());
  if (kind != DevicePaintKind::Layered) EXPECT_EQ(gpuAgain, gpu);
  EXPECT_EQ(binding->devices[0].reads, firstReads);

  const auto otherGpu = skia::shader(paint, frame(second.get()));
  expectTile(otherGpu, kSecondDeviceColor, second.get());
  EXPECT_NE(otherGpu, gpuAgain);
  EXPECT_GT(binding->devices[1].reads, 0);
  const int secondReads = binding->devices[1].reads;
  EXPECT_EQ(skia::shader(paint, frame(second.get())), otherGpu);
  EXPECT_EQ(binding->devices[1].reads, secondReads);
  const auto restored = skia::shader(paint, frame(first.get()));
  expectTile(restored, kFirstDeviceColor, first.get());
  EXPECT_EQ(skia::shader(paint, frame(first.get())), restored);
  const auto rasterAgain = skia::shader(paint, frame());
  expectTile(rasterAgain, kRasterColor);
  if (kind != DevicePaintKind::Layered) EXPECT_EQ(rasterAgain, raster);
  EXPECT_EQ(binding->rasterReads, rasterReads);
  EXPECT_EQ(binding->unexpectedReads, 0);
  expectTile(skia::shader(paint), snapshotColor);
  if (kind != DevicePaintKind::Time) EXPECT_EQ(skia::shader(paint), snapshot);
  EXPECT_EQ(binding->frameReads, 0);
}

TEST_F(SkiaPaintGpu,
       StaticRasterCaptureKeepsItsSamplerAndDescriptorAcrossDestinations) {
  const Texture texture = Texture(HeldRasterPicture{binding})
                              .region({2, 0, 12, kDeviceTileSize})
                              .at({2.25f, 1})
                              .tile(Repeat::Pad, Repeat::None)
                              .sampling(Sampling::Linear);
  const Paint paint = Paint::recipe(image(texture));
  ASSERT_TRUE(paint.recipeMaterial());
  const auto* description =
      dynamic_cast<const Texture*>(paint.recipeMaterial()->leaf("image"));
  ASSERT_TRUE(description);
  EXPECT_EQ(*description, texture);
  EXPECT_EQ(binding->frameReads, 1);
  binding->resetReads();
  const auto snapshot = skia::shader(paint);
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(skia::shader(paint, frame()), snapshot);
  EXPECT_EQ(skia::shader(paint, frame(first.get())), snapshot);
  EXPECT_EQ(skia::shader(paint, frame(second.get())), snapshot);
  const auto expectSampler = [&](const SkBitmap& pixels) {
    ASSERT_FALSE(pixels.empty());
    const auto left = pixels.getColor4f(0, 4);
    const auto edge = pixels.getColor4f(8, 4);
    for (int channel = 0; channel < 4; ++channel)
      EXPECT_NEAR(left[channel], kRasterColor[channel], .005f)
          << "clamped channel " << channel;
    for (int channel = 0; channel < 3; ++channel)
      EXPECT_NEAR(edge[channel], kRasterColor[channel], .005f)
          << "filtered channel " << channel;
    EXPECT_NEAR(edge.fA, kRasterColor.fA * .25f, .005f);
    EXPECT_EQ(pixels.getColor4f(12, 4).fA, 0);
    EXPECT_EQ(pixels.getColor4f(4, 0).fA, 0);
  };
  expectSampler(draw(snapshot));
  expectSampler(draw(snapshot, first.get()));
  expectSampler(draw(snapshot, second.get()));
  EXPECT_EQ(binding->frameReads, 0);
  EXPECT_EQ(binding->rasterReads, 0);
  EXPECT_EQ(binding->devices[0].reads, 0);
  EXPECT_EQ(binding->devices[1].reads, 0);
}

TEST_F(SkiaPaintGpu, UnavailableFramelessSourceResolvesOnItsRecorder) {
  binding->raster.reset();
  const Paint paint = Paint::recipe(image(heldDeviceTexture(binding)));
  EXPECT_FALSE(paint.isNone());
  EXPECT_FALSE(paint.isRunning());
  EXPECT_FALSE(paint.geometryDependent());
  expectTile(skia::shader(paint), SkColors::kTransparent);
  binding->resetReads();
  const auto gpu = skia::shader(paint, frame(first.get()));
  ASSERT_TRUE(gpu);
  expectTile(gpu, kFirstDeviceColor, first.get());
  expectTile(skia::shader(paint, frame()), SkColors::kTransparent);
  EXPECT_EQ(skia::shader(paint, frame(first.get())), gpu);
  expectTile(skia::shader(paint), SkColors::kTransparent);
  EXPECT_EQ(binding->unexpectedReads, 0);
}

TEST_P(SkiaPaintGpuSourceReplacement,
       CachedReplacementDoesNotChangeCopiesOrReuseOldSources) {
  Paint original = heldDevicePaint(binding, GetParam().kind);
  const auto originalGpu = skia::shader(original, frame(first.get()));
  const auto originalRaster = skia::shader(original, frame());
  Paint changed = original;
  binding->resetReads();
  changed.set("uGain", .5f);
  EXPECT_FALSE(changed == original);
  auto dimmedGpu = kFirstDeviceColor;
  dimmedGpu.fA *= .5f;
  auto dimmedRaster = kRasterColor;
  dimmedRaster.fA *= .5f;
  expectTile(skia::shader(changed), dimmedRaster);
  expectTile(skia::shader(changed, frame()), dimmedRaster);
  expectTile(skia::shader(changed, frame(first.get())), dimmedGpu, first.get());
  EXPECT_EQ(binding->frameReads, 0);
  EXPECT_EQ(skia::shader(original, frame(first.get())), originalGpu);
  EXPECT_EQ(skia::shader(original, frame()), originalRaster);
  changed.set("uGain", 1.f);
  EXPECT_TRUE(changed == original);
  EXPECT_EQ(binding->frameReads, 0);
  constexpr SkColor4f replacementGpu{1, 4, 2, .25f};
  constexpr SkColor4f replacementRaster{3, .5f, 2, .5f};
  const auto replacement =
      makeBinding(replacementGpu, kSecondDeviceColor, replacementRaster);
  ASSERT_TRUE(replacement);
  changed.slot("source", Paint::recipe(image(heldDeviceTexture(replacement))));
  EXPECT_FALSE(changed == original);
  EXPECT_EQ(skia::shader(original, frame(first.get())), originalGpu);
  EXPECT_EQ(skia::shader(original, frame()), originalRaster);
  expectTile(originalGpu, kFirstDeviceColor, first.get());
  expectTile(originalRaster, kRasterColor);
  const auto changedGpu = skia::shader(changed, frame(first.get()));
  expectTile(changedGpu, replacementGpu, first.get());
  expectTile(skia::shader(changed, frame()), replacementRaster);
  EXPECT_EQ(skia::shader(changed, frame(first.get())), changedGpu);

  changed.slot("source", Paint::recipe(image(heldDeviceTexture(binding))));
  EXPECT_TRUE(changed == original);
  expectTile(skia::shader(changed, frame(first.get())), kFirstDeviceColor,
             first.get());
  expectTile(skia::shader(changed, frame()), kRasterColor);
  expectTile(skia::shader(changed), kRasterColor);
  EXPECT_EQ(skia::shader(original, frame(first.get())), originalGpu);
  EXPECT_EQ(binding->unexpectedReads, 0);
  EXPECT_EQ(replacement->unexpectedReads, 0);
}

INSTANTIATE_TEST_SUITE_P(
    SourceKinds, SkiaPaintGpuDeviceSources,
    testing::Values(
        DevicePaintCase{"StaticRecipe", DevicePaintKind::Static},
        DevicePaintCase{"GeometryRecipe", DevicePaintKind::Geometry},
        DevicePaintCase{"TimeRecipe", DevicePaintKind::Time},
        DevicePaintCase{"NestedRecipe", DevicePaintKind::Nested},
        DevicePaintCase{"RawChild", DevicePaintKind::Raw},
        DevicePaintCase{"LayeredMaterial", DevicePaintKind::Layered},
        DevicePaintCase{"RawInRecipe", DevicePaintKind::ReverseRaw},
        DevicePaintCase{"LayersInRecipe", DevicePaintKind::ReverseLayered}),
    [](const testing::TestParamInfo<DevicePaintCase>& test) {
      return test.param.name;
    });

INSTANTIATE_TEST_SUITE_P(
    SourceKinds, SkiaPaintGpuSourceReplacement,
    testing::Values(DevicePaintCase{"NestedRecipe", DevicePaintKind::Nested},
                    DevicePaintCase{"RawChild", DevicePaintKind::Raw}),
    [](const testing::TestParamInfo<DevicePaintCase>& test) {
      return test.param.name;
    });
#endif
