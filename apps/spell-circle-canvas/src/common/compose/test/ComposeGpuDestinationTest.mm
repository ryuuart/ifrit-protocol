// Graphite cases for paint whose source is bound to a device: a material,
// a fill, an ink and a span's ink each draw through the destination they
// are replayed into, retain their pixels while it holds, and rebind when a
// raster canvas or another recorder takes its place.

#include "support/GpuTestSupport.h"

using namespace sigil::compose::graphiteTesting;

namespace {

class RecorderImageBinding final : public sigil::media::DeviceBinding {
 public:
  RecorderImageBinding(sk_sp<SkImage> device, sk_sp<SkImage> raster,
                       skgpu::graphite::Recorder *owner)
      : devices{{owner, std::move(device)}}, raster(std::move(raster)) {}

  void addDevice(skgpu::graphite::Recorder *owner, sk_sp<SkImage> image) {
    devices.push_back({owner, std::move(image)});
  }

  sk_sp<SkImage> image(skgpu::graphite::Recorder *recorder) override {
    if (!recorder) {
      ++rasterReads;
      return raster;
    }
    ++deviceReads;
    const auto found = std::find_if(devices.begin(), devices.end(), [recorder](const auto &entry) {
      return entry.first == recorder;
    });
    unexpectedRecorder |= found == devices.end();
    return found == devices.end() ? nullptr : found->second;
  }

  void resetReads() {
    deviceReads = rasterReads = 0;
    unexpectedRecorder = false;
  }

  int deviceReads = 0;
  int rasterReads = 0;
  bool unexpectedRecorder = false;

 private:
  std::vector<std::pair<skgpu::graphite::Recorder *, sk_sp<SkImage>>> devices;
  sk_sp<SkImage> raster;
};

struct DevicePicture {
  std::shared_ptr<RecorderImageBinding> binding;

  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    sigil::media::Frame frame;
    frame.device = {.kind = sigil::media::DeviceFrame::Kind::Texture,
                    .width = 16,
                    .height = 16,
                    .binding = binding};
    return frame;
  }
  bool isRunning() const { return false; }
  glm::ivec2 size() const { return {16, 16}; }
  bool operator==(const DevicePicture &) const = default;
};

struct MaterialDestinationCase {
  const char *name;
  bool live;
  bool localBake;
  bool plain = false;
};

class ComposeGpuMaterialDestination : public testing::TestWithParam<MaterialDestinationCase> {};

sigil::material::Material deviceMaterial(const std::shared_ptr<RecorderImageBinding> &binding,
                                         bool live, bool plain = false) {
  struct NoParameters {};
  using namespace sigil::material;
  if (plain) return image(Texture(DevicePicture{binding}).sampling(Sampling::Nearest));
  auto recipe = Recipe::of<NoParameters>("compose.device-picture")
                    .slot("source")
                    .frame(FrameInput::Resolution);
  if (live)
    recipe.frame(FrameInput::Time)
        .body(Target::SkSL, "half4 main(float2 p) { return source.eval("
                            "p * 16 / uResolution + float2(uTime, 0)); }");
  else
    recipe.body(Target::SkSL, "half4 main(float2 p) { return source.eval("
                              "p * 16 / uResolution); }");
  Material material(std::make_shared<const Recipe>(std::move(recipe)));
  material.slot("source", Texture(DevicePicture{binding}).sampling(Sampling::Nearest));
  return material;
}

Element deviceMaterialTree(const sigil::material::Material &material, bool localBake) {
  Element content =
      box().absolute().left(8).top(8).width(96).height(56).cache(Cache::Picture).fill(material);
  if (localBake)
    content = box()
                  .width(88)
                  .height(72)
                  .overflow(Overflow::Clip)
                  .cache(Cache::Texture)
                  .cacheScale(.5f)
                  .children({content});
  return box()
      .cache(Cache::None)
      .children({box()
                     .absolute()
                     .left(12)
                     .top(12)
                     .width(88)
                     .height(72)
                     .overflow(Overflow::Clip)
                     .cache(Cache::Picture)
                     .children({content})});
}

struct SpanInkDomain {
  const char *name;
  PaintBox box;
};

class ComposeGpuSpanInk : public testing::TestWithParam<SpanInkDomain> {};

constexpr int kSpanWidth = 352, kSpanHeight = 136;

sk_sp<SkImage> splitInkTile(SkSurface &surface, SkColor left, SkColor right) {
  surface.getCanvas()->clear(right);
  SkPaint paint;
  paint.setColor(left);
  surface.getCanvas()->drawRect(SkRect::MakeWH(8, 16), paint);
  return surface.makeImageSnapshot();
}

sigil::material::Material spanDeviceMaterial(const std::shared_ptr<RecorderImageBinding> &binding,
                                             PaintBox domain) {
  glm::mat3 placement(1.0f);
  if (domain == PaintBox::Glyph) {
    placement[0][0] = placement[1][1] = 1.0f / 16;
  } else {
    // The instrument's 64 px "HH " advances 96 px; the selected
    // 72 px "HH" advances 86.4 px. A passage ink keeps these coordinates.
    placement[0][0] = 86.4f / 16;
    placement[2][0] = 96;
  }
  return sigil::material::image(sigil::material::Texture(DevicePicture{binding})
                                    .uv(placement)
                                    .sampling(sigil::material::Sampling::Nearest));
}

Element spanInkTree(const sigil::material::Material *source, PaintBox domain) {
  sigil::weave::TextStyle base;
  base.shaping.typeface = sigil::test::instrument::sans();
  base.shaping.fontSize = 64;
  base.paint.foreground.setColor(SK_ColorWHITE);
  Text passage = text(u8"HH HH HH", base);
  if (source)
    passage.span(sigil::weave::selectors::regex(u8"HH HH$"), SpanStyle().ink(*source, domain));
  passage.span(sigil::weave::selectors::word(1), SpanStyle().fontSize(72));
  if (source)
    passage.span(sigil::weave::selectors::word(2),
                 SpanStyle().ink(sigil::material::Color{1, 0, 0, 1}));
  return box()
      .cache(Cache::Picture)
      .children(
          {box()
               .absolute()
               .left(12)
               .top(8)
               .width(328)
               .height(120)
               .cache(Cache::Picture)
               .children(
                   {passage.key("spans").absolute().left(8).top(8).width(312).height(104).cache(
                       Cache::Picture)})});
}

std::vector<std::pair<int, int>> glyphColumns(const SkBitmap &pixels) {
  std::vector<std::pair<int, int>> columns;
  const auto inked = [&](int x) {
    for (int y = 0; y < pixels.height(); ++y) {
      const SkColor color = pixels.getColor(x, y);
      if (SkColorGetR(color) + SkColorGetG(color) + SkColorGetB(color) > 90) return true;
    }
    return false;
  };
  for (int x = 0; x < pixels.width();) {
    if (!inked(x)) {
      ++x;
      continue;
    }
    const int left = x;
    while (x < pixels.width() && inked(x)) ++x;
    columns.emplace_back(left, x);
  }
  return columns;
}

SkColor columnInk(const SkBitmap &pixels, int x) {
  SkColor brightest = SK_ColorBLACK;
  int maximum = -1;
  for (int y = 0; y < pixels.height(); ++y) {
    const SkColor color = pixels.getColor(x, y);
    const int sum = SkColorGetR(color) + SkColorGetG(color) + SkColorGetB(color);
    if (sum > maximum) {
      maximum = sum;
      brightest = color;
    }
  }
  return brightest;
}

void expectSpanInk(const SkBitmap &pixels, const std::vector<std::pair<int, int>> &columns,
                   PaintBox domain, SkColor left, SkColor right) {
  ASSERT_FALSE(pixels.empty());
  const auto actual = glyphColumns(pixels);
  ASSERT_EQ(actual.size(), 6u);
  ASSERT_EQ(columns.size(), 6u);
  for (size_t glyph = 0; glyph < columns.size(); ++glyph) {
    SCOPED_TRACE(glyph);
    EXPECT_NEAR(actual[glyph].first, columns[glyph].first, 1);
    EXPECT_NEAR(actual[glyph].second, columns[glyph].second, 1);
    SkColor expectedLeft = glyph < 2 ? SK_ColorWHITE : SK_ColorRED;
    SkColor expectedRight = expectedLeft;
    if (glyph == 2 || glyph == 3) {
      expectedLeft = domain == PaintBox::Glyph || glyph == 2 ? left : right;
      expectedRight = domain == PaintBox::Glyph || glyph == 3 ? right : left;
    }
    EXPECT_EQ(columnInk(pixels, columns[glyph].first + 2), expectedLeft);
    EXPECT_EQ(columnInk(pixels, columns[glyph].second - 3), expectedRight);
  }
}

struct SpanRun {
  const sigil::weave::ShapedWord *shaped;
  glm::vec2 origin;
  float advance;
  bool operator==(const SpanRun &) const = default;
};

std::vector<SpanRun> spanRuns(const Composer &composer) {
  std::vector<SpanRun> runs;
  if (const auto *layout = composer.paragraphLayout("spans"))
    for (const auto &run : layout->runs) runs.push_back({run.shaped, run.origin, run.advance});
  return runs;
}

sigil::material::Color materialColor(SkColor color) {
  return {SkColorGetR(color) / 255.f, SkColorGetG(color) / 255.f, SkColorGetB(color) / 255.f,
          SkColorGetA(color) / 255.f};
}

// A uniform source covers each mapped unit, while its light response still
// depends on the unit's actual position in the page.
sigil::material::Material constantDeviceInk(const std::shared_ptr<RecorderImageBinding> &binding) {
  return sigil::material::image(sigil::material::Texture(DevicePicture{binding})
                                    .tile(sigil::material::Repeat::Pad)
                                    .sampling(sigil::material::Sampling::Nearest));
}

void expectSurfacedSpanInk(const SkBitmap &actual, const SkBitmap &coverage,
                           const SkBitmap *lightingReference,
                           const std::vector<std::pair<int, int>> &columns) {
  ASSERT_FALSE(actual.empty());
  ASSERT_EQ(columns.size(), 6u);
  size_t neighbors = 0, selected = 0, replaced = 0;
  size_t selectedBrightness = 0;
  size_t changed = 0;
  int maximum = 0;
  for (int y = 1; y + 1 < coverage.height(); ++y)
    for (int x = 1; x + 1 < coverage.width(); ++x) {
      bool interior = true;
      for (int dy = -1; interior && dy <= 1; ++dy)
        for (int dx = -1; interior && dx <= 1; ++dx)
          interior = coverage.getColor(x + dx, y + dy) == SK_ColorWHITE;
      if (!interior) continue;
      SkColor expected;
      if (x < columns[2].first) {
        ++neighbors;
        expected = SK_ColorWHITE;
      } else if (x < columns[4].first) {
        ++selected;
        expected = lightingReference ? lightingReference->getColor(x, y) : SK_ColorBLACK;
        selectedBrightness += SkColorGetR(expected) + SkColorGetG(expected) + SkColorGetB(expected);
      } else {
        ++replaced;
        expected = SK_ColorRED;
      }
      const SkColor pixel = actual.getColor(x, y);
      int difference = 0;
      for (int shift : {0, 8, 16})
        difference = std::max(
            difference, std::abs(int((pixel >> shift) & 255) - int((expected >> shift) & 255)));
      maximum = std::max(maximum, difference);
      changed += difference > 2;
    }
  EXPECT_GT(neighbors, 400u);
  EXPECT_GT(selected, 400u);
  EXPECT_GT(replaced, 400u);
  if (lightingReference) EXPECT_GT(selectedBrightness, selected * 20);
  EXPECT_EQ(changed, 0u) << "largest interior channel difference: " << maximum;
}

}  // namespace

TEST_P(ComposeGpuMaterialDestination, DeviceSourceUsesTheOwningDestinationAndRetainsItsPixels) {
  REQUIRE_GPU();
  const MaterialDestinationCase &test = GetParam();
  sigil::skia::GraphiteContext &context = *graphite();
  const SkImageInfo tileInfo = SkImageInfo::MakeN32Premul(16, 16);
  const auto deviceTile = SkSurfaces::RenderTarget(context.recorder(), tileInfo);
  const auto rasterTile = SkSurfaces::Raster(tileInfo);
  ASSERT_TRUE(deviceTile);
  ASSERT_TRUE(rasterTile);
  SkPaint cyan;
  cyan.setColor(SK_ColorCYAN);
  deviceTile->getCanvas()->clear(SK_ColorWHITE);
  deviceTile->getCanvas()->drawRect(SkRect::MakeWH(8, 16), cyan);
  cyan.setColor(SK_ColorMAGENTA);
  rasterTile->getCanvas()->clear(SK_ColorYELLOW);
  rasterTile->getCanvas()->drawRect(SkRect::MakeWH(8, 16), cyan);
  const auto binding = std::make_shared<RecorderImageBinding>(
      deviceTile->makeImageSnapshot(), rasterTile->makeImageSnapshot(), context.recorder());
  constexpr int width = 112, height = 96;
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({width, height});
  composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  composer.render(
      deviceMaterialTree(deviceMaterial(binding, test.live, test.plain), test.localBake));
  // A plain static source retains its eager raster snapshot. Other
  // destination bindings must use the canvas behind the picture recorder.
  if (test.plain) EXPECT_GT(binding->rasterReads, 0);
  binding->resetReads();
  const SkBitmap first = drawOnGpu(composer, width, height);
  ASSERT_FALSE(first.empty());
  EXPECT_GT(composer.stats().picturesRecorded, 0u);
  if (test.localBake) {
    EXPECT_EQ(composer.stats().texturesBaked, 1u);
    if (test.plain)
      EXPECT_EQ(binding->rasterReads, 0);
    else
      EXPECT_GT(binding->rasterReads, 0);
    EXPECT_EQ(binding->deviceReads, 0);
  } else {
    EXPECT_EQ(composer.stats().texturesBaked, 0u);
    EXPECT_GT(binding->deviceReads, 0);
    EXPECT_EQ(binding->rasterReads, 0);
  }
  EXPECT_FALSE(binding->unexpectedRecorder);
  EXPECT_EQ(first.getColor(24, 40), test.localBake ? SK_ColorMAGENTA : SK_ColorCYAN);
  EXPECT_EQ(first.getColor(84, 40), test.localBake ? SK_ColorYELLOW : SK_ColorWHITE);
  EXPECT_EQ(first.getColor(104, 40), SK_ColorBLACK);
  EXPECT_EQ(first.getColor(28, 84), SK_ColorBLACK);

  const int firstReads = binding->deviceReads + binding->rasterReads;
  const SkBitmap held = drawOnGpu(composer, width, height);
  ASSERT_FALSE(held.empty());
  EXPECT_EQ(binding->deviceReads + binding->rasterReads, firstReads);
  EXPECT_EQ(composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(composer.stats().texturesBaked, 0u);
  EXPECT_EQ(mismatchedPixels(first, held), 0u);
  if (test.live) {
    engine.advance(std::chrono::duration<double>(.25));
    const SkBitmap advanced = drawOnGpu(composer, width, height);
    ASSERT_FALSE(advanced.empty());
    EXPECT_GT(binding->deviceReads, firstReads);
    EXPECT_EQ(binding->rasterReads, 0);
    EXPECT_EQ(advanced.getColor(24, 40), SK_ColorCYAN);
    EXPECT_EQ(advanced.getColor(84, 40), SK_ColorWHITE);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Destinations, ComposeGpuMaterialDestination,
    testing::Values(MaterialDestinationCase{"Picture", false, false},
                    MaterialDestinationCase{"LivePicture", true, false},
                    MaterialDestinationCase{"LocalRasterPicture", false, true},
                    MaterialDestinationCase{"PlainPicture", false, false, true},
                    MaterialDestinationCase{"PlainLocalRasterPicture", false, true, true}),
    [](const auto &info) { return info.param.name; });

TEST(ComposeGpu, StaticDeviceFillAndInkRebindHeldPicturesAcrossDestinations) {
  REQUIRE_GPU();
  auto replacementDevice = sigil::core::hardware::GpuDevice::createOwned();
  ASSERT_TRUE(replacementDevice);
  auto replacement = sigil::skia::GraphiteContext::create(*replacementDevice);
  ASSERT_TRUE(replacement);
  const SkImageInfo tileInfo = SkImageInfo::MakeN32Premul(16, 16);
  const auto deviceTile = SkSurfaces::RenderTarget(graphite()->recorder(), tileInfo);
  const auto otherTile = SkSurfaces::RenderTarget(replacement->recorder(), tileInfo);
  const auto rasterTile = SkSurfaces::Raster(tileInfo);
  ASSERT_TRUE(deviceTile);
  ASSERT_TRUE(otherTile);
  ASSERT_TRUE(rasterTile);
  deviceTile->getCanvas()->clear(SK_ColorCYAN);
  otherTile->getCanvas()->clear(SK_ColorGREEN);
  rasterTile->getCanvas()->clear(SK_ColorMAGENTA);
  const auto binding = std::make_shared<RecorderImageBinding>(
      deviceTile->makeImageSnapshot(), rasterTile->makeImageSnapshot(), graphite()->recorder());
  binding->addDevice(replacement->recorder(), otherTile->makeImageSnapshot());
  const auto material = deviceMaterial(binding, false, true);
  constexpr int width = 128, height = 96;
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({width, height});
  composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  composer.render(
      box()
          .cache(Cache::Picture)
          .children({box().absolute().left(8).top(12).width(40).height(72).fill(material),
                     text("M")
                         .absolute()
                         .left(58)
                         .top(18)
                         .width(62)
                         .height(70)
                         .font({.size = 48})
                         .ink(material)}));
  EXPECT_GT(binding->rasterReads, 0);
  binding->resetReads();
  const auto expectSource = [&](const SkBitmap &pixels, SkColor color) {
    ASSERT_FALSE(pixels.empty());
    EXPECT_EQ(pixels.getColor(24, 40), color);
    size_t inkPixels = 0;
    for (int y = 18; y < 88; ++y)
      for (int x = 58; x < 120; ++x) inkPixels += pixels.getColor(x, y) == color;
    EXPECT_GT(inkPixels, 30u);
  };
  expectSource(drawOnGpu(composer, width, height), SK_ColorCYAN);
  EXPECT_GT(binding->deviceReads, 0);
  EXPECT_EQ(binding->rasterReads, 0);
  const int heldReads = binding->deviceReads;
  expectSource(drawOnGpu(composer, width, height), SK_ColorCYAN);
  EXPECT_EQ(binding->deviceReads, heldReads);
  EXPECT_EQ(composer.stats().picturesRecorded, 0u);
  expectSource(drawOnRaster(composer, width, height), SK_ColorMAGENTA);
  EXPECT_EQ(binding->rasterReads, 0);
  EXPECT_GT(composer.stats().picturesRecorded, 0u);
  expectSource(drawOnGpu(composer, width, height), SK_ColorCYAN);
  expectSource(drawOnGpu(composer, *replacement, width, height), SK_ColorGREEN);
  EXPECT_FALSE(binding->unexpectedRecorder);
}

TEST(ComposeGpu, StaticDeviceFillRetainsItsSourceWithoutAFramelessFallback) {
  REQUIRE_GPU();
  const auto tile =
      SkSurfaces::RenderTarget(graphite()->recorder(), SkImageInfo::MakeN32Premul(16, 16));
  ASSERT_TRUE(tile);
  tile->getCanvas()->clear(SK_ColorCYAN);
  const auto binding = std::make_shared<RecorderImageBinding>(tile->makeImageSnapshot(), nullptr,
                                                              graphite()->recorder());
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({48, 48});
  composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  composer.render(
      box().width(48).height(48).cache(Cache::Picture).fill(deviceMaterial(binding, false, true)));
  binding->resetReads();
  const SkBitmap gpu = drawOnGpu(composer, 48, 48);
  ASSERT_FALSE(gpu.empty());
  EXPECT_EQ(gpu.getColor(24, 24), SK_ColorCYAN);
  EXPECT_GT(binding->deviceReads, 0);
  EXPECT_EQ(binding->rasterReads, 0);
  const SkBitmap raster = drawOnRaster(composer, 48, 48);
  ASSERT_FALSE(raster.empty());
  EXPECT_EQ(raster.getColor(24, 24), SK_ColorBLACK);
  const SkBitmap rebound = drawOnGpu(composer, 48, 48);
  ASSERT_FALSE(rebound.empty());
  EXPECT_EQ(rebound.getColor(24, 24), SK_ColorCYAN);
  EXPECT_FALSE(binding->unexpectedRecorder);
}

TEST_P(ComposeGpuSpanInk, StaticSpanInkRebindsCachedParagraphsAcrossDestinations) {
  REQUIRE_GPU();
  ASSERT_TRUE(sigil::test::instrument::sans());
  auto replacementDevice = sigil::core::hardware::GpuDevice::createOwned();
  ASSERT_TRUE(replacementDevice);
  auto replacement = sigil::skia::GraphiteContext::create(*replacementDevice);
  ASSERT_TRUE(replacement);
  const auto info = SkImageInfo::MakeN32Premul(16, 16);
  const auto deviceTile = SkSurfaces::RenderTarget(graphite()->recorder(), info);
  const auto otherTile = SkSurfaces::RenderTarget(replacement->recorder(), info);
  const auto rasterTile = SkSurfaces::Raster(info);
  ASSERT_TRUE(deviceTile);
  ASSERT_TRUE(otherTile);
  ASSERT_TRUE(rasterTile);
  const auto binding = std::make_shared<RecorderImageBinding>(
      splitInkTile(*deviceTile, SK_ColorCYAN, SK_ColorBLUE),
      splitInkTile(*rasterTile, SK_ColorMAGENTA, SK_ColorYELLOW), graphite()->recorder());
  binding->addDevice(replacement->recorder(),
                     splitInkTile(*otherTile, SK_ColorGREEN, SK_ColorBLUE));
  const PaintBox domain = GetParam().box;

  sigil::motion::Engine engine;
  Composer reference(engine, fonts());
  reference.setSize({kSpanWidth, kSpanHeight});
  reference.render(spanInkTree(nullptr, domain));
  const auto white = drawOnRaster(reference, kSpanWidth, kSpanHeight);
  ASSERT_FALSE(white.empty());
  const auto columns = glyphColumns(white);
  ASSERT_EQ(columns.size(), 6u);
  EXPECT_GT(columns[2].second - columns[2].first, columns[0].second - columns[0].first);

  Composer composer(engine, fonts());
  composer.setSize({kSpanWidth, kSpanHeight});
  composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  const auto describe = [&] {
    const auto source = spanDeviceMaterial(binding, domain);
    return spanInkTree(&source, domain);
  };
  composer.render(describe());
  binding->resetReads();
  const auto first = drawOnGpu(composer, kSpanWidth, kSpanHeight);
  expectSpanInk(first, columns, domain, SK_ColorCYAN, SK_ColorBLUE);
  EXPECT_GT(binding->deviceReads, 0);
  EXPECT_EQ(binding->rasterReads, 0);
  EXPECT_GT(composer.stats().picturesRecorded, 0u);
  const auto runs = spanRuns(composer);
  ASSERT_FALSE(runs.empty());
  const int reads = binding->deviceReads;

  const auto held = drawOnGpu(composer, kSpanWidth, kSpanHeight);
  ASSERT_FALSE(held.empty());
  EXPECT_EQ(mismatchedPixels(first, held), 0u);
  EXPECT_EQ(binding->deviceReads, reads);
  EXPECT_EQ(composer.stats().picturesRecorded, 0u);

  // Equal descriptions have fresh Fill wrappers while the paragraph
  // retains the source values that gave its spans their paint ownership.
  composer.render(describe());
  EXPECT_EQ(composer.stats().patchedNodes, 0u);
  const int rasterReads = binding->rasterReads;
  const auto redescribed = drawOnGpu(composer, kSpanWidth, kSpanHeight);
  ASSERT_FALSE(redescribed.empty());
  EXPECT_EQ(mismatchedPixels(first, redescribed), 0u);
  EXPECT_EQ(binding->deviceReads, reads);
  EXPECT_EQ(binding->rasterReads, rasterReads);
  EXPECT_EQ(composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(spanRuns(composer), runs);

  expectSpanInk(drawOnRaster(composer, kSpanWidth, kSpanHeight), columns, domain, SK_ColorMAGENTA,
                SK_ColorYELLOW);
  EXPECT_GT(composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(spanRuns(composer), runs);
  expectSpanInk(drawOnGpu(composer, kSpanWidth, kSpanHeight), columns, domain, SK_ColorCYAN,
                SK_ColorBLUE);
  EXPECT_EQ(spanRuns(composer), runs);
  expectSpanInk(drawOnGpu(composer, *replacement, kSpanWidth, kSpanHeight), columns, domain,
                SK_ColorGREEN, SK_ColorBLUE);
  EXPECT_EQ(spanRuns(composer), runs);
  EXPECT_FALSE(binding->unexpectedRecorder);
}

TEST_P(ComposeGpuSpanInk, SpanInkWithoutRasterFallbackSurvivesRecorderReplacement) {
  REQUIRE_GPU();
  ASSERT_TRUE(sigil::test::instrument::sans());
  auto replacementDevice = sigil::core::hardware::GpuDevice::createOwned();
  ASSERT_TRUE(replacementDevice);
  auto replacement = sigil::skia::GraphiteContext::create(*replacementDevice);
  ASSERT_TRUE(replacement);
  const auto info = SkImageInfo::MakeN32Premul(16, 16);
  const auto deviceTile = SkSurfaces::RenderTarget(graphite()->recorder(), info);
  const auto otherTile = SkSurfaces::RenderTarget(replacement->recorder(), info);
  ASSERT_TRUE(deviceTile);
  ASSERT_TRUE(otherTile);
  const auto binding = std::make_shared<RecorderImageBinding>(
      splitInkTile(*deviceTile, SK_ColorCYAN, SK_ColorBLUE), nullptr, graphite()->recorder());
  binding->addDevice(replacement->recorder(),
                     splitInkTile(*otherTile, SK_ColorGREEN, SK_ColorBLUE));
  const PaintBox domain = GetParam().box;

  sigil::motion::Engine engine;
  Composer reference(engine, fonts());
  reference.setSize({kSpanWidth, kSpanHeight});
  reference.render(spanInkTree(nullptr, domain));
  const auto white = drawOnRaster(reference, kSpanWidth, kSpanHeight);
  ASSERT_FALSE(white.empty());
  const auto columns = glyphColumns(white);
  ASSERT_EQ(columns.size(), 6u);

  Composer composer(engine, fonts());
  composer.setSize({kSpanWidth, kSpanHeight});
  composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  const auto source = spanDeviceMaterial(binding, domain);
  composer.render(spanInkTree(&source, domain));
  binding->resetReads();
  const auto first = drawOnGpu(composer, kSpanWidth, kSpanHeight);
  expectSpanInk(first, columns, domain, SK_ColorCYAN, SK_ColorBLUE);
  EXPECT_GT(binding->deviceReads, 0);
  EXPECT_EQ(binding->rasterReads, 0);
  const int reads = binding->deviceReads;
  const auto held = drawOnGpu(composer, kSpanWidth, kSpanHeight);
  ASSERT_FALSE(held.empty());
  EXPECT_EQ(mismatchedPixels(first, held), 0u);
  EXPECT_EQ(binding->deviceReads, reads);
  EXPECT_EQ(composer.stats().picturesRecorded, 0u);
  const auto runs = spanRuns(composer);
  const auto unavailable = drawOnRaster(composer, kSpanWidth, kSpanHeight);
  ASSERT_FALSE(unavailable.empty());
  EXPECT_EQ(glyphColumns(unavailable).size(), 4u);
  for (size_t glyph = 0; glyph < columns.size(); ++glyph) {
    const SkColor expected = glyph < 2 ? SK_ColorWHITE : glyph < 4 ? SK_ColorBLACK : SK_ColorRED;
    EXPECT_EQ(columnInk(unavailable, columns[glyph].first + 2), expected);
    EXPECT_EQ(columnInk(unavailable, columns[glyph].second - 3), expected);
  }
  EXPECT_EQ(spanRuns(composer), runs);
  expectSpanInk(drawOnGpu(composer, *replacement, kSpanWidth, kSpanHeight), columns, domain,
                SK_ColorGREEN, SK_ColorBLUE);
  EXPECT_EQ(spanRuns(composer), runs);
  EXPECT_FALSE(binding->unexpectedRecorder);
}

INSTANTIATE_TEST_SUITE_P(Domains, ComposeGpuSpanInk,
                         testing::Values(SpanInkDomain{"Passage", PaintBox::Element},
                                         SpanInkDomain{"Glyph", PaintBox::Glyph}),
                         [](const auto &info) { return info.param.name; });

TEST(ComposeGpu, SurfacedUnitSpanInkRebindsItsColorAndNormalAcrossDestinations) {
  REQUIRE_GPU();
  ASSERT_TRUE(sigil::test::instrument::sans());
  auto replacementDevice = sigil::core::hardware::GpuDevice::createOwned();
  ASSERT_TRUE(replacementDevice);
  auto replacement = sigil::skia::GraphiteContext::create(*replacementDevice);
  ASSERT_TRUE(replacement);
  namespace material = sigil::material;
  const auto info = SkImageInfo::MakeN32Premul(16, 16);
  const SkColor deviceColor = SkColorSetRGB(150, 110, 60);
  const SkColor rasterColor = SkColorSetRGB(110, 150, 80);
  const SkColor otherColor = SkColorSetRGB(70, 130, 170);
  const SkColor deviceNormal = SkColorSetRGB(37, 128, 218);
  const SkColor rasterNormal = SkColorSetRGB(180, 128, 218);
  const SkColor otherNormal = SkColorSetRGB(200, 128, 218);
  const auto picture = [&](sigil::skia::GraphiteContext *context, SkColor color) {
    auto surface =
        context ? SkSurfaces::RenderTarget(context->recorder(), info) : SkSurfaces::Raster(info);
    if (!surface) return sk_sp<SkImage>{};
    surface->getCanvas()->clear(color);
    return surface->makeImageSnapshot();
  };
  struct Case {
    PaintBox domain;
    material::LightKind kind;
    bool rasterFallback;
  };
  for (const Case &test : {Case{PaintBox::Glyph, material::LightKind::Point, true},
                           Case{PaintBox::Word, material::LightKind::Spot, false}}) {
    SCOPED_TRACE(test.domain == PaintBox::Glyph ? "Glyph/Point" : "Word/Spot");
    const auto deviceColorImage = picture(graphite(), deviceColor);
    const auto deviceNormalImage = picture(graphite(), deviceNormal);
    const auto otherColorImage = picture(replacement.get(), otherColor);
    const auto otherNormalImage = picture(replacement.get(), otherNormal);
    ASSERT_TRUE(deviceColorImage);
    ASSERT_TRUE(deviceNormalImage);
    ASSERT_TRUE(otherColorImage);
    ASSERT_TRUE(otherNormalImage);
    const auto rasterColorImage = test.rasterFallback ? picture(nullptr, rasterColor) : nullptr;
    const auto rasterNormalImage = test.rasterFallback ? picture(nullptr, rasterNormal) : nullptr;
    if (test.rasterFallback) {
      ASSERT_TRUE(rasterColorImage);
      ASSERT_TRUE(rasterNormalImage);
    }
    const auto colorBinding = std::make_shared<RecorderImageBinding>(
        deviceColorImage, rasterColorImage, graphite()->recorder());
    const auto normalBinding = std::make_shared<RecorderImageBinding>(
        deviceNormalImage, rasterNormalImage, graphite()->recorder());
    colorBinding->addDevice(replacement->recorder(), otherColorImage);
    normalBinding->addDevice(replacement->recorder(), otherNormalImage);
    const auto ink = constantDeviceInk(colorBinding)
                         .surface({.roughness = .8f, .normal = constantDeviceInk(normalBinding)});
    const material::Light source{.elevation = 90.f,
                                 .intensity = .85f,
                                 .ambient = 0.f,
                                 .kind = test.kind,
                                 .position = {140, 50, 90},
                                 .range = 440,
                                 .innerAngle = 25,
                                 .outerAngle = 80};
    const auto tree = [&] {
      return scene()
          .width(kSpanWidth)
          .height(kSpanHeight)
          .children({light(source).translateX(17).translateY(9), spanInkTree(&ink, test.domain)});
    };
    sigil::motion::Engine engine;
    Composer mask(engine, fonts());
    mask.setSize({kSpanWidth, kSpanHeight});
    mask.render(spanInkTree(nullptr, test.domain));
    const auto coverage = drawOnRaster(mask, kSpanWidth, kSpanHeight);
    ASSERT_FALSE(coverage.empty());
    const auto columns = glyphColumns(coverage);
    ASSERT_EQ(columns.size(), 6u);
    Composer expected(engine, fonts());
    expected.setSize({kSpanWidth, kSpanHeight});
    expected.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
    const auto reference = [&](sigil::skia::GraphiteContext *context, SkColor color,
                               SkColor normal) {
      auto placed = source;
      placed.position += glm::vec3{17, 9, 0};
      material::Lighting lighting(placed);
      lighting.frame = material::LightingFrame::Scene;
      expected.render(box()
                          .width(kSpanWidth)
                          .height(kSpanHeight)
                          .fill(material::from(materialColor(color))
                                    .surface({.roughness = .8f, .normal = materialColor(normal)}))
                          .lighting(lighting));
      return context ? drawOnGpu(expected, *context, kSpanWidth, kSpanHeight)
                     : drawOnRaster(expected, kSpanWidth, kSpanHeight);
    };
    Composer composer(engine, fonts());
    composer.setSize({kSpanWidth, kSpanHeight});
    composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
    composer.render(tree());
    colorBinding->resetReads();
    normalBinding->resetReads();
    const auto first = drawOnGpu(composer, kSpanWidth, kSpanHeight);
    ASSERT_FALSE(first.empty());
    const auto firstExpected = reference(graphite(), deviceColor, deviceNormal);
    ASSERT_FALSE(firstExpected.empty());
    expectSurfacedSpanInk(first, coverage, &firstExpected, columns);
    EXPECT_GT(colorBinding->deviceReads, 0);
    EXPECT_GT(normalBinding->deviceReads, 0);
    EXPECT_EQ(colorBinding->rasterReads, 0);
    EXPECT_EQ(normalBinding->rasterReads, 0);
    const auto runs = spanRuns(composer);
    ASSERT_FALSE(runs.empty());
    const int colorReads = colorBinding->deviceReads, normalReads = normalBinding->deviceReads;
    const auto held = drawOnGpu(composer, kSpanWidth, kSpanHeight);
    ASSERT_FALSE(held.empty());
    EXPECT_EQ(mismatchedPixels(first, held), 0u);
    EXPECT_EQ(colorBinding->deviceReads, colorReads);
    EXPECT_EQ(normalBinding->deviceReads, normalReads);
    EXPECT_EQ(composer.stats().picturesRecorded, 0u);
    const auto raster = drawOnRaster(composer, kSpanWidth, kSpanHeight);
    const auto rasterExpected = reference(nullptr, rasterColor, rasterNormal);
    ASSERT_FALSE(rasterExpected.empty());
    expectSurfacedSpanInk(raster, coverage, test.rasterFallback ? &rasterExpected : nullptr,
                          columns);
    EXPECT_GT(colorBinding->rasterReads, 0);
    if (test.rasterFallback) EXPECT_GT(normalBinding->rasterReads, 0);
    EXPECT_EQ(spanRuns(composer), runs);
    const auto returned = drawOnGpu(composer, kSpanWidth, kSpanHeight);
    expectSurfacedSpanInk(returned, coverage, &firstExpected, columns);
    EXPECT_EQ(spanRuns(composer), runs);
    const auto other = drawOnGpu(composer, *replacement, kSpanWidth, kSpanHeight);
    const auto otherExpected = reference(replacement.get(), otherColor, otherNormal);
    ASSERT_FALSE(otherExpected.empty());
    expectSurfacedSpanInk(other, coverage, &otherExpected, columns);
    EXPECT_EQ(spanRuns(composer), runs);
    EXPECT_FALSE(colorBinding->unexpectedRecorder);
    EXPECT_FALSE(normalBinding->unexpectedRecorder);
  }
}
