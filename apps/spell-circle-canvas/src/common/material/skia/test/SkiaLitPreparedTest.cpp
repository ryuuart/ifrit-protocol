/** @file
 * A prepared lit surface: its colour stack and maps retained across
 * lighting changes, the pass it publishes, unit-mapped inputs sampled
 * through a node's frame, mapped constant channels, and which surfaces
 * depend on where the node stands.
 */

#include "SkiaLitTestSupport.h"

TEST(SkiaLit, APreparedSurfaceRetainsItsColorAndEveryMapAcrossLightChanges) {
  const std::array colors{
      SkColorSetRGB(153, 153, 153), SkColorSetRGB(128, 128, 255),
      SkColorSetRGB(191, 191, 191), SkColorSetRGB(76, 76, 76),
      SkColorSetRGB(204, 204, 204), SkColorSetRGB(26, 51, 77)};
  std::array<std::shared_ptr<int>, 6> reads;
  std::array<sk_sp<SkImage>, 6> pictures;
  for (size_t i = 0; i < colors.size(); ++i) {
    reads[i] = std::make_shared<int>(0);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(8, 8, true);
    bitmap.eraseColor(colors[i]);
    bitmap.setImmutable();
    pictures[i] = bitmap.asImage();
  }
  const auto input = [&](size_t i) {
    return image(
        sigil::media::PixelSource(CountedPicture{reads[i], pictures[i]}),
        {.repeat = Repeat::Pad});
  };
  const Material material = from(input(0)).surface({.metallic = input(3),
                                                    .roughness = input(2),
                                                    .occlusion = input(4),
                                                    .normal = input(1),
                                                    .emission = {1, 1, 1, 1},
                                                    .emissionStrength = .1f,
                                                    .emissionMap = input(5)});
  const auto referenceInput = [&](size_t i) {
    return image(pictures[i], {.repeat = Repeat::Pad});
  };
  const Material reference = from(referenceInput(0))
                                 .surface({.metallic = referenceInput(3),
                                           .roughness = referenceInput(2),
                                           .occlusion = referenceInput(4),
                                           .normal = referenceInput(1),
                                           .emission = {1, 1, 1, 1},
                                           .emissionStrength = .1f,
                                           .emissionMap = referenceInput(5)});
  const skia::LitSurface prepared(material);
  const skia::LitSurface copied = prepared;
  for (size_t i = 0; i < reads.size(); ++i) EXPECT_EQ(*reads[i], 0) << i;
  Light light;
  light.kind = LightKind::Point;
  light.position = {4.5f, 4.5f, 20};
  light.range = 100;
  light.intensity = .2f;
  light.ambient = .1f;
  const Paint initial = prepared.under(light);
  ASSERT_NE(skia::staticShader(initial), nullptr);
  std::array<int, 6> readOnce;
  for (size_t i = 0; i < reads.size(); ++i) {
    readOnce[i] = *reads[i];
    ASSERT_GT(readOnce[i], 0) << i;
  }
  float near = 0;
  for (int step = 0; step < 12; ++step) {
    light.position.x = 4.5f + 8.f * step;
    const SkColor4f actual = sampled(copied.under(light));
    const SkColor4f expected = sampled(skia::lit(reference, light));
    EXPECT_NEAR(actual.fR, expected.fR, .00001f);
    EXPECT_NEAR(actual.fG, expected.fG, .00001f);
    EXPECT_NEAR(actual.fB, expected.fB, .00001f);
    if (step == 0) near = actual.fR;
    if (step == 11) EXPECT_LT(actual.fR, near - .01f);
    for (size_t i = 0; i < reads.size(); ++i)
      EXPECT_EQ(*reads[i], readOnce[i]) << i << ": " << step;
    EXPECT_FALSE(skia::usesWorldSpace(material));
    for (size_t i = 0; i < reads.size(); ++i)
      EXPECT_EQ(*reads[i], readOnce[i]) << i << ": " << step;
    for (float width : {24.f, 80.f}) {
      const glm::mat3 mapping{width, 0, 0, 0, 32, 0, 12, 9, 1};
      const auto mapped = copied.shader(light, {}, mapping);
      ASSERT_NE(mapped, nullptr);
      const SkColor4f unit = sampled(skia::paint(mapped));
      EXPECT_NEAR(unit.fR, expected.fR, .00001f);
      EXPECT_NEAR(unit.fG, expected.fG, .00001f);
      EXPECT_NEAR(unit.fB, expected.fB, .00001f);
      for (size_t i = 0; i < reads.size(); ++i)
        EXPECT_EQ(*reads[i], readOnce[i]) << i << ": " << step;
    }
  }
  EXPECT_EQ(sampled(prepared.under(Lighting{})),
            sampled(skia::paint(reference)));
  for (size_t i = 0; i < reads.size(); ++i)
    EXPECT_EQ(*reads[i], readOnce[i]) << i;
}

TEST(SkiaLit, APreparedPassPublishesItsCompleteStaticSnapshot) {
  const Material material = from(Color{.4f, .6f, .8f, 1})
                                .surface({.roughness = 1.0f,
                                          .occlusion = .5f,
                                          .emission = {.1f, .2f, .3f, 1},
                                          .emissionStrength = .4f});
  const skia::LitSurface prepared(material);
  const Lighting lighting(
      std::vector<Light>{Light{.intensity = 0, .ambient = .2f},
                         Light{.intensity = 0, .ambient = .3f}});
  const Paint pass = prepared.under(lighting);
  EXPECT_FALSE(pass.isRunning());
  EXPECT_FALSE(pass.geometryDependent());
  const sk_sp<SkShader> snapshot = skia::staticShader(pass);
  ASSERT_NE(snapshot, nullptr);
  const SkColor4f pixel = sampled(skia::paint(snapshot));
  EXPECT_NEAR(pixel.fR, .14f, .0001f);
  EXPECT_NEAR(pixel.fG, .23f, .0001f);
  EXPECT_NEAR(pixel.fB, .32f, .0001f);
  EXPECT_FLOAT_EQ(pixel.fA, 1);
}

TEST_P(PreparedPositionedPass, PlacementMemoAndCopyOnWriteSurvivePassAssembly) {
  const skia::LitSurface prepared(
      from(Color{.6f, .4f, .2f, 1}).surface({.roughness = .8f}));
  const Light light{.elevation = 90,
                    .intensity = .25f,
                    .ambient = 0,
                    .kind = GetParam(),
                    .position = {4.5f, 4.5f, 12},
                    .range = 80,
                    .innerAngle = 30,
                    .outerAngle = 60};
  Paint pass = prepared.under(light);
  EXPECT_FALSE(pass.isRunning());
  EXPECT_TRUE(pass.geometryDependent());
  EXPECT_TRUE(pass.usesWorldSpace());
  FrameData frame{.resolution = {8, 8}};
  const sk_sp<SkShader> first = skia::shader(pass, frame);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(skia::shader(pass, frame), first);
  const SkColor4f near = drawnFloat(pass, 8, 8, frame).getColor4f(4, 4);
  frame.world[2][0] = 24;
  const sk_sp<SkShader> moved = skia::shader(pass, frame);
  ASSERT_NE(moved, nullptr);
  EXPECT_NE(moved, first);
  EXPECT_EQ(skia::shader(pass, frame), moved);
  const SkColor4f far = drawnFloat(pass, 8, 8, frame).getColor4f(4, 4);
  EXPECT_LT(far.fR, near.fR - .01f);
  const Paint original = pass;
  pass.set("uAmbient", .5f);
  EXPECT_EQ(skia::shader(original, frame), moved);
  EXPECT_EQ(drawnFloat(original, 8, 8, frame).getColor4f(4, 4), far);
  EXPECT_GT(drawnFloat(pass, 8, 8, frame).getColor4f(4, 4).fR, far.fR + .2f);
}

TEST_P(PreparedPositionedPass, UnitSamplingKeepsActualPositionAndNormalSlopes) {
  const Material material =
      from(Color{.2f, .3f, .4f, 1})
          .surface({.roughness = 1.0f, .normal = from(Color{1, .5f, 1, 1})});
  const skia::LitSurface prepared(material);
  const Light light{.direction = 0,
                    .elevation = 45,
                    .intensity = .5f,
                    .ambient = 0,
                    .kind = GetParam(),
                    .position = {100, 25, 60},
                    .range = 240,
                    .innerAngle = 50,
                    .outerAngle = 80};
  for (const glm::mat3 world :
       {glm::mat3{1}, glm::mat3{0, 2, 0, -1, 0, 0, 30, 12, 1}}) {
    const FrameData frame{.resolution = {160, 64}, .world = world};
    const SkBitmap reference =
        drawnFloat(prepared.under(light), 160, 64, frame);
    for (float width : {24.f, 120.f}) {
      const glm::mat3 mapping{width, 0, 0, 0, 40, 0, 12, 9, 1};
      const auto shader = prepared.shader(light, frame, mapping);
      ASSERT_NE(shader, nullptr);
      const SkBitmap actual = drawnFloat(skia::paint(shader), 160, 64);
      for (int x : {16, 44, 100}) {
        const SkColor4f pixel = actual.getColor4f(x, 24);
        const SkColor4f expected = reference.getColor4f(x, 24);
        EXPECT_NEAR(pixel.fR, expected.fR, .00001f);
        EXPECT_NEAR(pixel.fG, expected.fG, .00001f);
        EXPECT_NEAR(pixel.fB, expected.fB, .00001f);
        EXPECT_FLOAT_EQ(pixel.fA, expected.fA);
      }
    }
  }
}

TEST(SkiaLit, MappedConstantChannelsPreserveOrdinaryLightingAndCoverage) {
  const Material material =
      from(Color{.2f, .3f, .4f, .5f})
          .surface({.metallic = from(Color{.4f, 0, 0, 1}),
                    .roughness = from(Color{.7f, 0, 0, 1}),
                    .occlusion = from(Color{.8f, 0, 0, 1}),
                    .normal = from(Color{.75f, .5f, .9f, 1}),
                    .emission = {1, 1, 1, 1},
                    .emissionStrength = 2,
                    .emissionMap = from(Color{1, .25f, .125f, 1})});
  const skia::LitSurface prepared(material);
  const Lighting light(Light{.color = Color{3, 2, 1, 1},
                             .intensity = .6f,
                             .ambient = .1f,
                             .kind = LightKind::Point,
                             .position = {32, 12, 60},
                             .range = 240});
  const FrameData frame{.resolution = {64, 32},
                        .world = glm::mat3{0, 2, 0, -1, 0, 0, 30, 12, 1}};
  const glm::mat3 mapping{-28, 0, 0, 0, 12, 0, 40, 7, 1};
  const auto mapped = prepared.shader(light, frame, mapping);
  ASSERT_TRUE(mapped);
  const SkBitmap actual = drawnFloat(skia::paint(mapped), 64, 32);
  const SkBitmap expected = drawnFloat(prepared.under(light), 64, 32, frame);
  float maximum = 0;
  size_t mismatches = 0;
  for (int y = 0; y < actual.height(); ++y)
    for (int x = 0; x < actual.width(); ++x) {
      const SkColor4f a = actual.getColor4f(x, y);
      const SkColor4f e = expected.getColor4f(x, y);
      for (int channel = 0; channel < 4; ++channel) {
        const float delta = std::abs(a[channel] - e[channel]);
        if (!std::isfinite(delta) || delta > .00001f) ++mismatches;
        maximum = std::max(maximum, delta);
      }
    }
  EXPECT_EQ(mismatches, 0u) << "largest channel difference: " << maximum;
  const SkColor4f pixel = actual.getColor4f(24, 16);
  EXPECT_GT(pixel.fR, 1.0f);
  EXPECT_FLOAT_EQ(pixel.fA, .5f);
}

TEST_P(PreparedPositionedPass,
       MappedLightingCopiesReadCurrentBindingsAndKeepPriorShaders) {
  const Material material =
      from(Color{.2f, .3f, .4f, .5f})
          .surface({.roughness = .8f, .normal = Color{.75f, .5f, .9f, 1}});
  const skia::LitSurface prepared(material);
  const skia::LitSurface copied = prepared;
  auto color = sigil::motion::animatable(Color{3, 0, 0, 1});
  auto strength = sigil::motion::animatable(.5f);
  Light light{.elevation = 65,
              .color = color,
              .intensity = strength,
              .ambient = .1f,
              .kind = GetParam(),
              .position = {32, 12, 60},
              .range = 240,
              .innerAngle = 45,
              .outerAngle = 80};
  FrameData frame{.resolution = {64, 32}};
  sk_sp<SkShader> first;
  SkColor4f firstPixel;
  for (int step = 0; step < 6; ++step) {
    SCOPED_TRACE(step);
    color = step % 2 ? Color{0, 0, 3, 1} : Color{3, 0, 0, 1};
    strength = .5f + .1f * step;
    if (step >= 2) light.position.x = 32 + 8.f * step;
    Lighting lighting(light);
    if (step >= 4) lighting.frame = LightingFrame::Scene;
    frame.world[2][0] = step >= 4 ? 12 : 0;
    const glm::mat3 mapping{24.f + 12.f * step, 0, 0, 0, 20, 0, 4, 5, 1};
    const auto built = copied.shader(lighting, frame, mapping);
    ASSERT_TRUE(built);
    const SkBitmap actual = drawnFloat(skia::paint(built), 64, 32);
    const SkBitmap expected =
        drawnFloat(skia::lit(material, lighting), 64, 32, frame);
    for (int x : {8, 24, 48}) {
      const auto a = actual.getColor4f(x, 16), e = expected.getColor4f(x, 16);
      EXPECT_NEAR(a.fR, e.fR, .00001f);
      EXPECT_NEAR(a.fG, e.fG, .00001f);
      EXPECT_NEAR(a.fB, e.fB, .00001f);
      EXPECT_FLOAT_EQ(a.fA, e.fA);
    }
    if (step == 0) {
      first = built;
      firstPixel = actual.getColor4f(24, 16);
    }
  }
  EXPECT_EQ(drawnFloat(skia::paint(first), 64, 32).getColor4f(24, 16),
            firstPixel);
  const Paint original = prepared.under(light);
  const SkColor4f originalPixel = sampled(original);
  Paint changed = copied.under(light);
  changed.set("uAmbient", .8f);
  EXPECT_GT(sampled(changed).fG, originalPixel.fG + .1f);
  EXPECT_EQ(sampled(prepared.under(light)), originalPixel);
}

TEST(SkiaLit, MappedLightingCopiesResolveConcurrentContexts) {
  const Material material =
      from(Color{.2f, .3f, .4f, .5f})
          .surface({.roughness = .8f, .normal = Color{.75f, .5f, .9f, 1}});
  const skia::LitSurface prepared(material);
  FrameData frame{.resolution = {8, 8},
                  .world = glm::mat3{1, 0, 0, .2f, 1, 0, 3, 2, 1}};
  std::array<Lighting, 3> contexts;
  std::array<SkColor4f, 3> expected;
  for (size_t i = 0; i < contexts.size(); ++i) {
    Light light{.elevation = 70,
                .color = Color{.5f + float(i), .75f, 1, 1},
                .intensity = .3f,
                .ambient = .1f,
                .kind = static_cast<LightKind>(i),
                .position = {12, 12, 40},
                .range = 160,
                .innerAngle = 50,
                .outerAngle = 80};
    contexts[i] = light;
    expected[i] =
        drawnFloat(skia::lit(material, light), 8, 8, frame).getColor4f(4, 4);
    ASSERT_TRUE(prepared.shader(light, frame, glm::mat3{1}));
  }
  std::array<std::future<std::array<SkColor4f, 8>>, 3> workers;
  for (size_t worker = 0; worker < workers.size(); ++worker)
    workers[worker] = std::async(std::launch::async, [&, worker, prepared] {
      std::array<SkColor4f, 8> pixels;
      for (size_t step = 0; step < pixels.size(); ++step) {
        const Lighting& lighting = contexts[(worker + step) % contexts.size()];
        if (step % 2) {
          pixels[step] = drawnFloat(prepared.under(lighting), 8, 8, frame)
                             .getColor4f(4, 4);
        } else {
          const glm::mat3 mapping{12.f + float(step), 0, 0, 0, 16, 0, 2, 3, 1};
          const auto built = prepared.shader(lighting, frame, mapping);
          if (!built) {
            pixels[step] = {std::numeric_limits<float>::quiet_NaN(), 0, 0, 0};
            continue;
          }
          pixels[step] = drawnFloat(skia::paint(built)).getColor4f(4, 4);
        }
      }
      return pixels;
    });
  for (size_t worker = 0; worker < workers.size(); ++worker) {
    const auto pixels = workers[worker].get();
    for (size_t step = 0; step < pixels.size(); ++step) {
      SCOPED_TRACE(worker);
      SCOPED_TRACE(step);
      const auto& reference = expected[(worker + step) % contexts.size()];
      for (int channel = 0; channel < 4; ++channel)
        EXPECT_NEAR(pixels[step][channel], reference[channel], .00001f);
    }
  }
}

TEST_P(PreparedPositionedPass,
       UnitHeightReliefUsesPixelDepthAcrossAffineMappings) {
  const Material height = shader(R"(
half4 main(float2 p) { return half4(half3(.2 + .4 * p.x + .2 * p.y), 1); }
)");
  const Light light{.direction = 0,
                    .elevation = 45,
                    .intensity = .5f,
                    .ambient = 0,
                    .kind = GetParam(),
                    .position = {100, 25, 60},
                    .range = 240,
                    .innerAngle = 50,
                    .outerAngle = 80};
  struct Sample {
    glm::mat3 mapping;
    glm::vec2 slope;
  };
  const std::array samples{
      Sample{{24, 0, 0, 0, 40, 0, 12, 8, 1}, {.4f / 24, .2f / 40}},
      Sample{{120, 0, 0, 0, 40, 0, 12, 8, 1}, {.4f / 120, .2f / 40}},
      Sample{{0, 24, 0, -40, 0, 0, 80, 8, 1}, {-.2f / 40, .4f / 24}},
      Sample{{-24, 0, 0, 0, 40, 0, 80, 8, 1}, {-.4f / 24, .2f / 40}},
      Sample{{40, 0, 0, 20, 40, 0, 12, 8, 1}, {.01f, 0}},
  };
  const auto finish = [](Material normal) {
    return from(Color{.2f, .3f, .4f, 1})
        .surface({.roughness = .8f, .normal = std::move(normal)});
  };
  for (float depth : {-6.0f, 6.0f})
    for (float step : {.25f, 1.0f, 4.0f}) {
      const Material normal = surface::normalFromHeight(height, {depth, step});
      const skia::LitSurface prepared(
          finish(surface::blendNormals(normal, Color{.5f, .5f, 1, 1})));
      for (const auto& sample : samples) {
        const auto n = glm::normalize(
            glm::vec3{-depth * sample.slope.x, depth * sample.slope.y, 1});
        const skia::LitSurface reference(finish(
            Color{n.x * .5f + .5f, n.y * .5f + .5f, n.z * .5f + .5f, 1}));
        const auto built = prepared.shader(light, {}, sample.mapping);
        ASSERT_TRUE(built);
        const SkBitmap actual = drawnFloat(skia::paint(built), 160, 80);
        const SkBitmap expected = drawnFloat(reference.under(light), 160, 80);
        for (float u : {.3f, .5f, .7f}) {
          const auto at = sample.mapping * glm::vec3{u, .5f, 1};
          const int x = static_cast<int>(at.x), y = static_cast<int>(at.y);
          const auto a = actual.getColor4f(x, y), b = expected.getColor4f(x, y);
          EXPECT_NEAR(a.fR, b.fR, .0001f);
          EXPECT_NEAR(a.fG, b.fG, .0001f);
          EXPECT_NEAR(a.fB, b.fB, .0001f);
          EXPECT_FLOAT_EQ(a.fA, b.fA);
        }
      }
    }
}

TEST(SkiaLit, UnitSamplingRetainsActualWorldAnchoredInputCoordinates) {
  struct Parameters {
    float offset = .2f;
  };
  const Material material =
      shader(
          "half4 main(float2 p) { return half4(offset + p.x * .001, "
          "offset + p.y * .001, .1, 1); }",
          Parameters{})
          .worldSpace()
          .surface({.roughness = 1.0f});
  const skia::LitSurface prepared(material);
  const Lighting lighting(Light{.intensity = 0, .ambient = 1});
  const FrameData frame{.resolution = {80, 40},
                        .rootResolution = {300, 200},
                        .world = glm::mat3{2, 0, 0, 0, 1, 0, 24, 15, 1}};
  const SkBitmap reference =
      drawnFloat(prepared.under(lighting), 80, 40, frame);
  const glm::mat3 mapping{64, 0, 0, 0, 24, 0, 12, 8, 1};
  const auto mapped = prepared.shader(lighting, frame, mapping);
  ASSERT_NE(mapped, nullptr);
  const SkBitmap actual = drawnFloat(skia::paint(mapped), 80, 40);
  for (int x : {20, 40, 60}) {
    const SkColor4f pixel = actual.getColor4f(x, 20);
    const SkColor4f expected = reference.getColor4f(x, 20);
    EXPECT_NEAR(pixel.fR, expected.fR, .00001f);
    EXPECT_NEAR(pixel.fG, expected.fG, .00001f);
    EXPECT_NEAR(pixel.fB, expected.fB, .00001f);
    EXPECT_NEAR(pixel.fR, .2f + (2 * (x + .5f) + 24) * .001f, .0001f);
    EXPECT_NEAR(pixel.fG, .2f + 35.5f * .001f, .0001f);
  }
}

TEST(SkiaLit, WorldDependencyQueriesDoNotFetchMaterialImages) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(8, 8, true);
  bitmap.eraseColor(SK_ColorWHITE);
  bitmap.setImmutable();
  const auto reads = std::make_shared<int>(0);
  const Material input =
      image(sigil::media::PixelSource(CountedPicture{reads, bitmap.asImage()}));
  const Material all = from(input)
                           .layer(input, {.mask = Mask{input}})
                           .surface({.metallic = input,
                                     .roughness = input,
                                     .occlusion = input,
                                     .normal = input,
                                     .emissionMap = input,
                                     .lighting = environment(input)});
  Material anchored = input;
  anchored.worldSpace();
  Material sampled = shader("half4 main(float2 p) { return source.eval(p); }",
                            ShaderOptions{.textures = {{"source", {}}}});
  sampled.slot("source", anchored);
  const Material masked = from(input).layer(input, {.mask = Mask{anchored}});
  const Material channel = from(input).surface({.normal = anchored});
  const Material around =
      from(input).surface({.lighting = environment(anchored)});
  const Material paintPart =
      skia::base(Paint::solid({1, 1, 1, 1}).worldSpace());
  const Material frameReader = shader(
      "half4 main(float2 p) { return half4(uWorld[2].xy * .001, 0, 1); }");
  ASSERT_EQ(*reads, 0);
  for (int repeat = 0; repeat < 4; ++repeat) {
    EXPECT_FALSE(skia::usesWorldSpace(all));
    for (const Material& material :
         {anchored, sampled, masked, channel, around, paintPart, frameReader})
      EXPECT_TRUE(skia::usesWorldSpace(material));
    EXPECT_EQ(*reads, 0);
  }
}

TEST(SkiaLit, BareAndComposedWorldFieldsAnchorOnceAndUseRootResolution) {
  EXPECT_TRUE(shader("half4 main(float2 p) { return half4(p / 256, 0, 1); }")
                  .worldSpace()
                  .geometryDependent());
  const Material bare = shader(
                            "half4 main(float2 p) { return half4(p / 256, "
                            "uResolution.x / 1024, 1); }")
                            .worldSpace();
  Material composed = bare;
  composed.layer(Color{0, 0, 0, 0});
  const FrameData frame{.resolution = {8, 8},
                        .rootResolution = {256, 128},
                        .world = glm::mat3{2, 0, 0, 0, 1, 0, 32, 16, 1}};
  const auto expectPixel = [](const SkColor4f& pixel) {
    EXPECT_NEAR(pixel.fR, 37.f / 256, .00001f);
    EXPECT_NEAR(pixel.fG, 19.5f / 256, .00001f);
    EXPECT_FLOAT_EQ(pixel.fB, .25f);
    EXPECT_FLOAT_EQ(pixel.fA, 1);
  };
  const auto direct = skia::shader(bare, frame);
  ASSERT_NE(direct, nullptr);
  expectPixel(drawnFloat(skia::paint(direct), 8, 8).getColor4f(2, 3));
  for (const Material& material : {bare, composed}) {
    EXPECT_TRUE(material.geometryDependent());
    EXPECT_TRUE(skia::usesWorldSpace(material));
    const Paint paint = skia::paint(material);
    EXPECT_TRUE(paint.worldSpace());
    const auto first = skia::shader(paint, frame);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(skia::shader(paint, frame), first);
    expectPixel(drawnFloat(skia::paint(first), 8, 8).getColor4f(2, 3));
    Paint local = paint;
    local.worldSpace(false);
    EXPECT_FALSE(local.usesWorldSpace());
    const auto unanchored = skia::shader(local, frame);
    ASSERT_NE(unanchored, nullptr);
    const SkColor4f pixel =
        drawnFloat(skia::paint(unanchored), 8, 8).getColor4f(2, 3);
    EXPECT_NEAR(pixel.fR, 2.5f / 256, .00001f);
    EXPECT_NEAR(pixel.fG, 3.5f / 256, .00001f);
    EXPECT_FLOAT_EQ(pixel.fB, 8.f / 1024);
    expectPixel(drawnFloat(paint, 8, 8, frame).getColor4f(2, 3));
    expectPixel(drawnFloat(skia::paint(first), 8, 8).getColor4f(2, 3));
  }
}

TEST(SkiaLit, NestedWorldFieldsTrackPlacementAndRootExtentInAHeldPaint) {
  const Material field = shader(
                             "half4 main(float2 p) { return half4(p / 256, "
                             "uResolution.x / 1024, 1); }")
                             .worldSpace();
  Material sampler = shader("half4 main(float2 p) { return source.eval(p); }",
                            ShaderOptions{.textures = {{"source", {}}}});
  sampler.slot("source", field);
  const Paint paint = skia::paint(sampler);
  EXPECT_FALSE(paint.worldSpace());
  EXPECT_TRUE(paint.usesWorldSpace());
  EXPECT_TRUE(paint.geometryDependent());
  FrameData frame{.resolution = {8, 8},
                  .rootResolution = {256, 128},
                  .world = glm::mat3{2, 0, 0, 0, 1, 0, 32, 16, 1}};
  const auto first = skia::shader(paint, frame);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(skia::shader(paint, frame), first);
  const SkColor4f near = drawnFloat(skia::paint(first), 8, 8).getColor4f(2, 3);
  EXPECT_NEAR(near.fR, 37.f / 256, .00001f);
  EXPECT_NEAR(near.fG, 19.5f / 256, .00001f);
  EXPECT_FLOAT_EQ(near.fB, .25f);
  frame.world[2][0] = 64;
  const auto moved = skia::shader(paint, frame);
  ASSERT_NE(moved, nullptr);
  EXPECT_NE(moved, first);
  EXPECT_EQ(skia::shader(paint, frame), moved);
  EXPECT_NEAR(drawnFloat(skia::paint(moved), 8, 8).getColor4f(2, 3).fR,
              69.f / 256, .00001f);
  frame.rootResolution.x = 512;
  const auto resized = skia::shader(paint, frame);
  ASSERT_NE(resized, nullptr);
  EXPECT_NE(resized, moved);
  EXPECT_EQ(skia::shader(paint, frame), resized);
  EXPECT_FLOAT_EQ(drawnFloat(skia::paint(resized), 8, 8).getColor4f(2, 3).fB,
                  .5f);
  EXPECT_EQ(drawnFloat(skia::paint(first), 8, 8).getColor4f(2, 3), near);
}

TEST(SkiaLit, UnitSamplingUsesUnitResolutionAndRejectsInvalidMappings) {
  const Material material =
      linearGradient({0, 0}, {1, 0},
                     {{0, {.2f, .3f, .4f, 1}}, {1, {.6f, .5f, .2f, 1}}})
          .surface({.roughness = 1.0f});
  const skia::LitSurface prepared(material);
  const Lighting lighting(Light{.intensity = 0, .ambient = 1});
  const FrameData frame{.resolution = {100, 40}};
  const glm::mat3 mapping{80, 0, 0, 0, 24, 0, 12, 8, 1};
  for (const Lighting rig : {lighting, Lighting{}}) {
    const auto mapped = prepared.shader(rig, frame, mapping);
    ASSERT_NE(mapped, nullptr);
    const SkBitmap actual = drawnFloat(skia::paint(mapped), 100, 40);
    for (int x : {20, 52, 84}) {
      const float fraction = (x + .5f - 12) / 80;
      const SkColor4f pixel = actual.getColor4f(x, 20);
      EXPECT_NEAR(pixel.fR, .2f + .4f * fraction, .0001f);
      EXPECT_NEAR(pixel.fG, .3f + .2f * fraction, .0001f);
      EXPECT_NEAR(pixel.fB, .4f - .2f * fraction, .0001f);
      EXPECT_FLOAT_EQ(pixel.fA, 1);
    }
  }
  glm::mat3 invalid = mapping;
  invalid[0][0] = 0;
  EXPECT_EQ(prepared.shader(lighting, frame, invalid), nullptr);
  invalid = mapping;
  invalid[2][0] = std::numeric_limits<float>::infinity();
  EXPECT_EQ(prepared.shader(lighting, frame, invalid), nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    SkiaLit, PreparedPositionedPass,
    testing::Values(LightKind::Point, LightKind::Spot),
    [](const testing::TestParamInfo<LightKind>& parameter) {
      return parameter.param == LightKind::Point ? "Point" : "Spot";
    });

TEST(SkiaLit, APreparedPassReadsOnlyPublishedChildInputsAcrossLiveFrames) {
  struct Parameters {
    std::array<float, 4> uTint{};
    float uGain = 1;
  };
  auto tint = std::make_shared<UniformBlock>(4);
  const std::array initial{.2f, .4f, .6f, 1.f};
  std::copy(initial.begin(), initial.end(), tint->values().begin());
  tint->commit();
  auto gain = sigil::motion::animatable(.5f);
  Material material = shader(
      "half4 main(float2 p) { return half4(float3(uTint[0], uTint[1], "
      "uTint[2]) * uGain + uTime * .125, 1); }",
      Parameters{});
  material.bind("uTint", tint).bind("uGain", gain).surface({.roughness = 1.0f});
  const skia::LitSurface prepared(material);
  const Lighting lighting(Light{.intensity = 0, .ambient = 1});
  const Paint pass = prepared.under(lighting);
  EXPECT_TRUE(pass.isRunning());
  ASSERT_NE(skia::staticShader(pass), nullptr);
  FrameData frame{.resolution = {8, 8}};
  const SkColor4f first = drawnFloat(pass, 8, 8, frame).getColor4f(4, 4);
  EXPECT_NEAR(first.fR, .1f, .0001f);
  EXPECT_NEAR(first.fG, .2f, .0001f);
  EXPECT_NEAR(first.fB, .3f, .0001f);
  const std::array draft{.8f, .6f, .4f, 1.f};
  std::copy(draft.begin(), draft.end(), tint->values().begin());
  gain = 1;
  frame.seconds = 2;
  const auto expectPublished = [&](const Paint& value) {
    const SkColor4f pixel = drawnFloat(value, 8, 8, frame).getColor4f(4, 4);
    EXPECT_NEAR(pixel.fR, .45f, .0001f);
    EXPECT_NEAR(pixel.fG, .65f, .0001f);
    EXPECT_NEAR(pixel.fB, .85f, .0001f);
  };
  expectPublished(pass);
  expectPublished(prepared.under(lighting));
  const glm::mat3 mapping{24, 0, 0, 0, 20, 0, 4, 3, 1};
  const auto mapped = prepared.shader(lighting, frame, mapping);
  ASSERT_TRUE(mapped);
  expectPublished(skia::paint(mapped));
  tint->commit();
  const SkColor4f committed = drawnFloat(pass, 8, 8, frame).getColor4f(4, 4);
  EXPECT_NEAR(committed.fR, 1.05f, .0001f);
  EXPECT_NEAR(committed.fG, .85f, .0001f);
  EXPECT_NEAR(committed.fB, .65f, .0001f);
  const auto remapped = prepared.shader(lighting, frame, mapping);
  ASSERT_TRUE(remapped);
  EXPECT_EQ(drawnFloat(skia::paint(remapped)).getColor4f(4, 4), committed);
  expectPublished(skia::paint(mapped));
}

TEST(SkiaLit, ALiveLightColorUpdatesOnlyThePassAtUnchangedStrength) {
  const std::array colors{SkColorSetRGB(153, 153, 153),
                          SkColorSetRGB(128, 128, 255),
                          SkColorSetRGB(255, 255, 255)};
  std::array<std::shared_ptr<int>, 3> reads;
  std::array<sk_sp<SkImage>, 3> pictures;
  for (size_t i = 0; i < colors.size(); ++i) {
    reads[i] = std::make_shared<int>(0);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(8, 8, true);
    bitmap.eraseColor(colors[i]);
    bitmap.setImmutable();
    pictures[i] = bitmap.asImage();
  }
  const auto input = [&](size_t i) {
    return image(
        sigil::media::PixelSource(CountedPicture{reads[i], pictures[i]}));
  };
  const Material material =
      from(input(0)).surface({.roughness = input(2), .normal = input(1)});
  const skia::LitSurface prepared(material);
  for (size_t i = 0; i < reads.size(); ++i) EXPECT_EQ(*reads[i], 0) << i;
  auto color = sigil::motion::animatable(Color{3, 0, 0, 1});
  const Light light{.elevation = 90, .color = color, .ambient = 0};
  const Paint live = prepared.under(light);
  std::array<int, 3> readOnce;
  for (size_t i = 0; i < reads.size(); ++i) {
    readOnce[i] = *reads[i];
    ASSERT_GT(readOnce[i], 0) << i;
  }
  EXPECT_TRUE(live.isRunning());
  const SkColor4f red = sampled(live);
  EXPECT_GT(red.fR, 1);
  EXPECT_FLOAT_EQ(red.fG, 0);
  EXPECT_FLOAT_EQ(red.fB, 0);
  color = Color{0, 0, 3, 1};
  const SkColor4f blue = sampled(live);
  EXPECT_FLOAT_EQ(blue.fR, 0);
  EXPECT_FLOAT_EQ(blue.fG, 0);
  EXPECT_GT(blue.fB, 1);
  EXPECT_NEAR(blue.fB, red.fR, .00001f);
  EXPECT_FLOAT_EQ(light.intensity.value(), 1);
  const Light constant{
      .elevation = 90, .color = Color{0, 0, 3, 1}, .ambient = 0};
  const Paint fixed = prepared.under(constant);
  EXPECT_FALSE(fixed.isRunning());
  EXPECT_NEAR(sampled(fixed).fB, blue.fB, .00001f);
  for (size_t i = 0; i < reads.size(); ++i)
    EXPECT_EQ(*reads[i], readOnce[i]) << i;
}

TEST(SkiaLit,
     APreparedSurfaceRetainsItsEnvironmentAcrossLightAndOptionChanges) {
  std::array<std::shared_ptr<int>, 2> reads{std::make_shared<int>(0),
                                            std::make_shared<int>(0)};
  std::array<sk_sp<SkImage>, 2> pictures;
  for (size_t i = 0; i < pictures.size(); ++i) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 8, true);
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 16; ++x)
        *bitmap.getAddr32(x, y) =
            SkPreMultiplyARGB(255, i == 0 ? 30 + 10 * x : 180 - 8 * x,
                              20 + 20 * y, i == 0 ? 40 : 160);
    bitmap.setImmutable();
    pictures[i] = bitmap.asImage();
  }
  const Material surface =
      from(Color{.2f, .3f, .4f, 1})
          .surface({.metallic = .7f,
                    .roughness = .4f,
                    .normal = from(Color{.75f, .5f, .75f, 1}),
                    .clearcoat = .5f});
  const skia::LitSurface prepared(surface);
  const skia::LitSurface copied = prepared;
  const auto environmentImage = [&](size_t i) {
    return image(
        sigil::media::PixelSource(CountedPicture{reads[i], pictures[i]}));
  };
  auto rotation = sigil::motion::animatable(0.f);
  Environment around =
      environment(environmentImage(0), {.rotation = rotation, .size = {16, 8}});
  Light light;
  light.kind = LightKind::Point;
  light.position = {4.5f, 4.5f, 20};
  light.range = 100;
  light.intensity = .1f;
  light.ambient = .2f;
  sampled(prepared.under(Lighting(light, around)));
  const int firstRead = *reads[0];
  ASSERT_GT(firstRead, 0);
  const auto expectReference = [&](const Lighting& context, size_t imageIndex) {
    const SkColor4f actual = sampled(copied.under(context));
    Lighting reference = context;
    reference.environment->image =
        std::make_shared<const Material>(image(pictures[imageIndex]));
    const SkColor4f expected = sampled(skia::lit(surface, reference));
    EXPECT_NEAR(actual.fR, expected.fR, .00001f);
    EXPECT_NEAR(actual.fG, expected.fG, .00001f);
    EXPECT_NEAR(actual.fB, expected.fB, .00001f);
    return actual;
  };
  for (int step = 0; step < 12; ++step) {
    rotation = step * 30.f;
    light.position.x = 4.5f + step * 6.f;
    around.options.intensity = .25f + .05f * step;
    around.options.size = step < 6 ? glm::vec2{16, 8} : glm::vec2{24, 12};
    Lighting context(light, around);
    context.frame = step < 6 ? LightingFrame::Surface : LightingFrame::Scene;
    expectReference(context, 0);
    EXPECT_EQ(*reads[0], firstRead) << step;
    EXPECT_EQ(*reads[1], 0);
  }
  // Sample within the picture's bounds when comparing replacement images.
  around.options.size = {16, 8};
  rotation = 0.0f;
  around.image = std::make_shared<const Material>(environmentImage(1));
  const SkColor4f second = expectReference(Lighting(light, around), 1);
  const int secondRead = *reads[1];
  EXPECT_GT(secondRead, 0);
  EXPECT_EQ(*reads[0], firstRead);
  expectReference(Lighting(light, around), 1);
  EXPECT_EQ(*reads[1], secondRead);
  around.image = std::make_shared<const Material>(environmentImage(0));
  const SkColor4f first = expectReference(Lighting(light, around), 0);
  EXPECT_GT(std::abs(first.fR - second.fR) + std::abs(first.fG - second.fG) +
                std::abs(first.fB - second.fB),
            .001f);
  EXPECT_GT(*reads[0], firstRead);
  EXPECT_EQ(*reads[1], secondRead);
}

TEST(SkiaLit, MappedLightingTracksEnvironmentBindingsAndReplacement) {
  const Material material = from(Color{.2f, .3f, .4f, .5f})
                                .surface({.metallic = .7f,
                                          .roughness = .3f,
                                          .normal = Color{.75f, .5f, .9f, 1}});
  const skia::LitSurface prepared(material);
  const skia::LitSurface copied = prepared;
  auto rotation = sigil::motion::animatable(0.f);
  const Material sky = shader(R"(
half4 main(float2 p) { return half4(.2 + .5 * p.x, .3 + .4 * p.y, .8, 1); }
)");
  Environment around = environment(sky, {.rotation = rotation, .size = {1, 1}});
  const FrameData frame{.resolution = {32, 16},
                        .world = glm::mat3{0, 1, 0, -1, 0, 0, 12, 3, 1}};
  SkColor4f firstPixel;
  SkColor4f rotatedPixel;
  for (int step = 0; step < 6; ++step) {
    SCOPED_TRACE(step);
    rotation = step % 2 ? 180.f : 0.f;
    around.options.intensity = step >= 2 ? .4f : 1.f;
    if (step == 3)
      around.image = std::make_shared<const Material>(Color{1, .1f, .2f, 1});
    if (step == 5) around.image = std::make_shared<const Material>(sky);
    Lighting lighting(around);
    if (step >= 2) lighting.frame = LightingFrame::Scene;
    if (step == 4) lighting = {};
    const glm::mat3 mapping{24.f + step * 4.f, 0, 0, 0, 20, 0, 3, 4, 1};
    const auto built = copied.shader(lighting, frame, mapping);
    ASSERT_TRUE(built);
    const SkColor4f actual =
        drawnFloat(skia::paint(built), 32, 16).getColor4f(12, 8);
    const SkColor4f expected =
        drawnFloat(skia::lit(material, lighting), 32, 16, frame)
            .getColor4f(12, 8);
    for (int channel = 0; channel < 4; ++channel)
      EXPECT_NEAR(actual[channel], expected[channel], .00001f);
    if (step == 0) firstPixel = actual;
    if (step == 1) rotatedPixel = actual;
  }
  EXPECT_GT(std::abs(firstPixel.fR - rotatedPixel.fR), .01f);
}
