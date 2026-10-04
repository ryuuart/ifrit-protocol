/** @file
 * The paint's placement: the injected node-to-root matrix, a caller's
 * own matrix, root anchoring of height steps, sampling metrics, and a
 * parent tracking its slots' dependency on placement.
 */

#include "SkiaPaintTestSupport.h"

TEST(SkiaPaint, ClearingAMatrixBlockRestoresWorldInjection) {
  Paint paint = skia::sksl(worldEffect());
  auto block = std::make_shared<UniformBlock>(16);
  SkM44().getColMajor(block->values().data());
  block->commit();
  paint.bind("uWorld", block);
  EXPECT_FALSE(paint.geometryDependent());
  FrameData frame{.resolution = {4, 4}};
  frame.world[2][0] = 32;
  const SkColor explicitWorld =
      render(skia::shader(paint, frame)).getColor(1, 1);
  paint.bind("uWorld", std::shared_ptr<const UniformBlock>{});
  EXPECT_FALSE(paint.isRunning());
  EXPECT_TRUE(paint.geometryDependent());
  EXPECT_TRUE(paint.usesWorldSpace());
  const SkColor injectedWorld =
      render(skia::shader(paint, frame)).getColor(1, 1);
  EXPECT_NEAR(SkColorGetR(explicitWorld), 1.5f / 128 * 255, 1);
  EXPECT_NEAR(SkColorGetR(injectedWorld), 33.5f / 128 * 255, 1);
}

TEST_P(WorldPlacement, InjectedMatrixUsesNodeLocalToRootCoordinates) {
  const Paint paint = skia::sksl(worldEffect());
  ASSERT_NE(worldEffect(), nullptr);
  EXPECT_TRUE(paint.geometryDependent());
  EXPECT_TRUE(paint.usesWorldSpace());
  EXPECT_FALSE(std::as_const(paint).worldSpace());
  EXPECT_FALSE(paint.isRunning());
  FrameData frame{.resolution = {8, 8}, .world = GetParam().world};
  const sk_sp<SkShader> first = skia::shader(paint, frame);
  ASSERT_NE(first, nullptr);
  const SkColor pixel = render(first, 8, 8).getColor(2, 3);
  const glm::vec3 root = frame.world * glm::vec3(2.5f, 3.5f, 1);
  EXPECT_NEAR(SkColorGetR(pixel), root.x / root.z / 128 * 255, 1);
  EXPECT_NEAR(SkColorGetG(pixel), root.y / root.z / 128 * 255, 1);
  EXPECT_EQ(skia::shader(paint, frame), first);
  frame.seconds = 9;
  frame.contentScale = 2;
  frame.resolution = {40, 20};
  EXPECT_EQ(skia::shader(paint, frame), first);

  // Keep the affine coefficients fixed: changing only the perspective row
  // must change both the uploaded matrix and its resolve key.
  frame.world[0][2] += .02f;
  const sk_sp<SkShader> moved = skia::shader(paint, frame);
  ASSERT_NE(moved, nullptr);
  EXPECT_NE(moved, first);
  const SkColor movedPixel = render(moved, 8, 8).getColor(2, 3);
  const glm::vec3 movedRoot = frame.world * glm::vec3(2.5f, 3.5f, 1);
  EXPECT_NEAR(SkColorGetR(movedPixel), movedRoot.x / movedRoot.z / 128 * 255,
              1);
  EXPECT_NEAR(SkColorGetG(movedPixel), movedRoot.y / movedRoot.z / 128 * 255,
              1);
  EXPECT_EQ(skia::shader(paint, frame), moved);
}

INSTANTIATE_TEST_SUITE_P(
    SkiaPaint, WorldPlacement,
    testing::Values(
        WorldPlacementCase{"Identity", glm::mat3(1)},
        WorldPlacementCase{"Translated",
                           glm::mat3{1, 0, 0, 0, 1, 0, 32, 16, 1}},
        WorldPlacementCase{"ScaledAndSkewed",
                           glm::mat3{2, .25f, 0, .5f, 3, 0, 8, 12, 1}},
        WorldPlacementCase{"Perspective",
                           glm::mat3{1, .25f, .02f, .5f, 1, .01f, 32, 16, 1}}),
    [](const testing::TestParamInfo<WorldPlacementCase>& parameter) {
      return parameter.param.name;
    });

TEST_P(ExplicitWorldMatrix, CallerMatrixOwnsItsUniformAndIgnoresPlacement) {
  const Paint automatic = skia::sksl(worldEffect());
  Paint explicitMatrix = automatic;
  std::vector<float> matrix(16);
  SkM44::Translate(32, 16).getColMajor(matrix.data());
  auto block = std::make_shared<UniformBlock>(16);
  if (GetParam() == MatrixOwnership::Bound) {
    std::copy(matrix.begin(), matrix.end(), block->values().begin());
    block->commit();
    explicitMatrix.bind("uWorld", block);
  } else {
    explicitMatrix.set("uWorld", matrix);
  }
  EXPECT_FALSE(explicitMatrix.geometryDependent());
  EXPECT_FALSE(explicitMatrix.usesWorldSpace());
  EXPECT_TRUE(automatic.geometryDependent());
  EXPECT_TRUE(automatic.usesWorldSpace());
  FrameData frame{.resolution = {8, 8}};
  const sk_sp<SkShader> first = skia::shader(explicitMatrix, frame);
  ASSERT_NE(first, nullptr);
  const SkColor pixel = render(first, 8, 8).getColor(2, 3);
  EXPECT_NEAR(SkColorGetR(pixel), 34.5f / 128 * 255, 1);
  EXPECT_NEAR(SkColorGetG(pixel), 19.5f / 128 * 255, 1);
  frame.world[2][0] = 64;
  frame.world[2][1] = 48;
  EXPECT_EQ(skia::shader(explicitMatrix, frame), first);
  if (GetParam() == MatrixOwnership::Bound) {
    SkM44::Translate(16, 8).getColMajor(block->values().data());
    EXPECT_EQ(skia::shader(explicitMatrix, frame), first);
    block->commit();
    const sk_sp<SkShader> committed = skia::shader(explicitMatrix, frame);
    ASSERT_NE(committed, nullptr);
    EXPECT_NE(committed, first);
    const SkColor committedPixel = render(committed, 8, 8).getColor(2, 3);
    EXPECT_NEAR(SkColorGetR(committedPixel), 18.5f / 128 * 255, 1);
    EXPECT_NEAR(SkColorGetG(committedPixel), 11.5f / 128 * 255, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    SkiaPaint, ExplicitWorldMatrix,
    testing::Values(MatrixOwnership::Constant, MatrixOwnership::Bound),
    [](const testing::TestParamInfo<MatrixOwnership>& parameter) {
      return parameter.param == MatrixOwnership::Constant ? "Constant"
                                                          : "Bound";
    });

TEST_P(UnsupportedWorldUniform, OtherDeclarationsAreNotInjected) {
  const auto effect = effectFor(GetParam().source);
  ASSERT_NE(effect, nullptr);
  const Paint paint = skia::sksl(effect);
  EXPECT_FALSE(paint.geometryDependent());
  EXPECT_FALSE(paint.usesWorldSpace());
  const FrameData frame{.world = glm::mat3{1, 0, 0, 0, 1, 0, 32, 16, 1}};
  const auto shader = skia::shader(paint, frame);
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(SkColorGetR(render(shader).getColor(1, 1)), 0);
}

INSTANTIATE_TEST_SUITE_P(
    SkiaPaint, UnsupportedWorldUniform,
    testing::Values(
        WorldUniformCase{"Scalar",
                         "uniform float uWorld; half4 main(float2 p) {"
                         " return half4(uWorld, 0, 0, 1); }"},
        WorldUniformCase{"Matrix3",
                         "uniform float3x3 uWorld; half4 main(float2 p) {"
                         " return half4(uWorld[0][0], 0, 0, 1); }"},
        WorldUniformCase{"Array16",
                         "uniform float uWorld[16]; half4 main(float2 p) {"
                         " return half4(uWorld[0], 0, 0, 1); }"}),
    [](const testing::TestParamInfo<WorldUniformCase>& parameter) {
      return parameter.param.name;
    });

TEST(SkiaPaint, RecipeBackedWorldInputUsesTheTranslatedFrame) {
  const Material material = sigil::material::shader(
      "half4 main(float2 p) { float3 root = uWorld * float3(p, 1);"
      " return half4(root.xy / root.z / 128, 0, 1); }");
  const Paint paint = Paint::recipe(material);
  EXPECT_TRUE(paint.geometryDependent());
  EXPECT_TRUE(paint.usesWorldSpace());
  FrameData frame{.resolution = {8, 8},
                  .world = glm::mat3{1, 0, 0, 0, 1, 0, 32, 16, 1}};
  const sk_sp<SkShader> first = skia::shader(paint, frame);
  ASSERT_NE(first, nullptr);
  const SkColor pixel = render(first, 8, 8).getColor(2, 3);
  EXPECT_NEAR(SkColorGetR(pixel), 34.5f / 128 * 255, 1);
  EXPECT_NEAR(SkColorGetG(pixel), 19.5f / 128 * 255, 1);
  EXPECT_EQ(skia::shader(paint, frame), first);
  frame.world[2][0] = 16;
  const sk_sp<SkShader> moved = skia::shader(paint, frame);
  ASSERT_NE(moved, nullptr);
  EXPECT_NE(moved, first);
  EXPECT_NEAR(SkColorGetR(render(moved, 8, 8).getColor(2, 3)),
              18.5f / 128 * 255, 1);
  EXPECT_EQ(skia::shader(paint, frame), moved);
}

TEST(SkiaPaint, RootAnchoringTransportsHeightPixelStepsExactlyOnce) {
  const Material height = shader(R"(
half4 main(float2 p) {
  float h = .25 + dot(p, float2(.01, .02));
  return half4(half3(h), 1);
})");
  const Material normal = surface::normalFromHeight(height, {.depth = 4});
  Material anchored = normal;
  anchored.worldSpace();
  Paint raw = rawHeightNormal(normal, height);
  raw.worldSpace();
  Paint blended = Paint::blend({{skia::paint(normal), BlendMode::Normal}});
  blended.worldSpace();
  const std::array paints{skia::paint(anchored), raw, blended};
  const std::array names{"Backed", "Raw", "Blend"};
  FrameData frame{.resolution = {8, 8},
                  .world = glm::mat3{2, .25f, 0, .5f, 1.5f, 0, 8, 4, 1},
                  .localToSample = glm::mat3{.5f, 0, 0, 0, .25f, 0, 0, 0, 1}};
  const auto expect = [&](const sk_sp<SkShader>& shader) {
    const glm::mat3 metric = frame.world * frame.localToSample;
    expectSamplingNormal(
        shader, -4 * glm::dot(glm::vec2{.01f, .02f}, glm::vec2(metric[0])),
        4 * glm::dot(glm::vec2{.01f, .02f}, glm::vec2(metric[1])));
  };
  expect(skia::shader(anchored, frame));
  std::array<sk_sp<SkShader>, 3> initial;
  for (size_t i = 0; i < paints.size(); ++i) {
    SCOPED_TRACE(names[i]);
    EXPECT_TRUE(paints[i].geometryDependent());
    EXPECT_FALSE(paints[i].isRunning());
    initial[i] = skia::shader(paints[i], frame);
    expect(initial[i]);
    EXPECT_EQ(skia::shader(paints[i], frame), initial[i]);
    expectSamplingNormal(skia::shader(paints[i]), -.04f, .08f);
  }
  frame.localToSample[0][0] = .25f;
  expect(skia::shader(anchored, frame));
  for (size_t i = 0; i < paints.size(); ++i) {
    SCOPED_TRACE(names[i]);
    const auto changed = skia::shader(paints[i], frame);
    EXPECT_NE(changed, initial[i]);
    expect(changed);
    EXPECT_EQ(skia::shader(paints[i], frame), changed);
    expectSamplingNormal(skia::shader(paints[i]), -.04f, .08f);
  }
}

TEST(SkiaPaint, RootAnchoredHeightChildrenTrackPlacementInsideHeldParents) {
  const Material height = shader(R"(
half4 main(float2 p) {
  float h = .2 + .0005 * p.x * p.x + .004 * p.y;
  return half4(half3(h), 1);
})");
  const Material normal = surface::normalFromHeight(height, {.depth = 4});
  Material anchored = normal;
  anchored.worldSpace();
  Material parent = shader("half4 main(float2 p) { return uSource.eval(p); }",
                           ShaderOptions{.textures = {{"uSource", {}}}});
  parent.slot("uSource", anchored);
  Paint rawChild = rawHeightNormal(normal, height);
  rawChild.worldSpace();
  Paint rawParent =
      skia::sksl(effectFor("uniform shader uSource; half4 main(float2 p) {"
                           " return uSource.eval(p); }"));
  rawParent.slot("uSource", rawChild);
  const std::array paints{
      skia::paint(parent), rawParent,
      Paint::blend({{skia::paint(anchored), BlendMode::Normal}})};
  FrameData frame{.resolution = {8, 8},
                  .world = glm::mat3{2, 0, 0, 0, 1.5f, 0, 8, 4, 1}};
  const auto expect = [&](const sk_sp<SkShader>& shader) {
    const float rootX = 2 * 2.5f + frame.world[2][0];
    // A quadratic's central difference is its analytic derivative.
    expectSamplingNormal(shader, -4 * .001f * rootX * 2, 4 * .004f * 1.5f);
  };
  expect(skia::shader(parent, frame));
  std::array<sk_sp<SkShader>, 3> first;
  for (size_t i = 0; i < paints.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(paints[i].geometryDependent());
    EXPECT_TRUE(paints[i].usesWorldSpace());
    EXPECT_FALSE(paints[i].isRunning());
    first[i] = skia::shader(paints[i], frame);
    expect(first[i]);
  }
  frame.world[2][0] = 16;
  expect(skia::shader(parent, frame));
  for (size_t i = 0; i < paints.size(); ++i) {
    SCOPED_TRACE(i);
    const auto moved = skia::shader(paints[i], frame);
    EXPECT_NE(moved, first[i]);
    expect(moved);
    expect(skia::shader(paints[i], frame));
    EXPECT_FALSE(identical(render(first[i], 8, 8), render(moved, 8, 8)));
  }
}

TEST(SkiaPaint, UnsupportedRootDifferentialsFlattenHeightRelief) {
  const Material height = shader(
      "half4 main(float2 p) { return half4(half3(.25 + .01 * p.x), 1); }");
  const Material normal = surface::normalFromHeight(height, {.depth = 4});
  Material anchored = normal;
  anchored.worldSpace();
  Paint raw = rawHeightNormal(normal, height);
  raw.worldSpace();
  Paint blended = Paint::blend({{skia::paint(normal), BlendMode::Normal}});
  blended.worldSpace();
  const std::array paints{skia::paint(anchored), raw, blended};
  for (const glm::mat3& world : {glm::mat3{1, 0, .01f, 0, 1, 0, 8, 4, 1},
                                 glm::mat3{0, 0, 0, 0, 1, 0, 8, 4, 1}}) {
    const FrameData frame{.resolution = {8, 8}, .world = world};
    expectSamplingNormal(skia::shader(anchored, frame), 0, 0);
    for (const Paint& paint : paints)
      expectSamplingNormal(skia::shader(paint, frame), 0, 0);
  }
}

TEST(SkiaPaint, AuthoredSamplingUniformsOverrideInjectedMetrics) {
  constexpr const char* body =
      "half4 main(float2 p) {"
      " float2 d = (uLocalToSample * float3(1, 0, 0)).xy;"
      " return half4(d * .25 + .5, .5, 1); }";
  const auto effect = effectFor(
      (std::string("uniform float3x3 uLocalToSample;") + body).c_str());
  ASSERT_TRUE(effect);
  const std::vector<float> values{.5f, .25f, 0, 0, 1, 0, 0, 0, 1};
  Paint raw = skia::sksl(effect);
  raw.set("uLocalToSample", values);
  EXPECT_FALSE(raw.geometryDependent());
  const Schema parameters =
      packedSchema({{.name = "uLocalToSample", .kind = ParameterType::Mat3}});
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of("paint.authored-sampling", parameters)
          .body(Target::SkSL, body));
  Material material(recipe);
  material.set("uLocalToSample", std::span<const float>(values));
  EXPECT_FALSE(material.geometryDependent());
  material.worldSpace();
  raw.worldSpace();
  const std::array paints{raw, skia::paint(material)};
  FrameData frame{.resolution = {8, 8},
                  .world = glm::mat3{2, 0, 0, 0, 3, 0, 8, 4, 1},
                  .localToSample = glm::mat3{4}};
  const auto expect = [](const sk_sp<SkShader>& shader) {
    ASSERT_TRUE(shader);
    const SkColor pixel = render(shader, 8, 8).getColor(2, 3);
    EXPECT_NEAR(SkColorGetR(pixel), .625f * 255, 1);
    EXPECT_NEAR(SkColorGetG(pixel), .5625f * 255, 1);
    EXPECT_NEAR(SkColorGetB(pixel), .5f * 255, 1);
    EXPECT_EQ(SkColorGetA(pixel), 255u);
  };
  expect(skia::shader(material, frame));
  for (const Paint& paint : paints) {
    expect(skia::shader(paint, frame));
    expect(skia::shader(paint));
  }
  Paint blockPaint = skia::sksl(effect);
  auto block = std::make_shared<UniformBlock>(9);
  std::copy(values.begin(), values.end(), block->values().begin());
  block->commit();
  blockPaint.bind("uLocalToSample", block);
  EXPECT_FALSE(blockPaint.geometryDependent());
  expect(skia::shader(blockPaint, frame));
  blockPaint.bind("uLocalToSample", std::shared_ptr<const UniformBlock>{});
  EXPECT_TRUE(blockPaint.geometryDependent());
  frame.localToSample = glm::mat3{.25f, 0, 0, 0, 1, 0, 0, 0, 1};
  const auto injected = skia::shader(blockPaint, frame);
  ASSERT_TRUE(injected);
  EXPECT_NEAR(SkColorGetR(render(injected, 8, 8).getColor(2, 3)), .5625f * 255,
              1);
}

TEST_P(WorldChildPlacement, ParentTracksItsSlotsRootTransformDependency) {
  Paint paint;
  if (GetParam() == UniformPaint::DirectPaint) {
    paint = skia::sksl(
        effectFor("uniform shader uField;"
                  " half4 main(float2 p) { return uField.eval(p); }"));
    paint.slot("uField", skia::sksl(worldEffect()));
  } else {
    Material parent = sigil::material::shader(
        "half4 main(float2 p) { return uField.eval(p); }",
        ShaderOptions{.textures = {{"uField", {}}}});
    parent.slot("uField",
                sigil::material::shader(
                    "half4 main(float2 p) {"
                    " float3 root = uWorld * float3(p, 1);"
                    " return half4(root.xy / root.z / 128, 0, 1); }"));
    paint = Paint::recipe(std::move(parent));
  }
  EXPECT_TRUE(paint.geometryDependent());
  EXPECT_TRUE(paint.usesWorldSpace());
  EXPECT_FALSE(paint.isRunning());
  FrameData frame{.resolution = {8, 8},
                  .world = glm::mat3{1, 0, 0, 0, 1, 0, 32, 16, 1}};
  const sk_sp<SkShader> first = skia::shader(paint, frame);
  ASSERT_NE(first, nullptr);
  EXPECT_NEAR(SkColorGetR(render(first, 8, 8).getColor(2, 3)),
              34.5f / 128 * 255, 1);
  frame.world[2][0] = 48;
  const sk_sp<SkShader> moved = skia::shader(paint, frame);
  ASSERT_NE(moved, nullptr);
  EXPECT_NEAR(SkColorGetR(render(moved, 8, 8).getColor(2, 3)),
              50.5f / 128 * 255, 1);
}

INSTANTIATE_TEST_SUITE_P(
    SkiaPaint, WorldChildPlacement,
    testing::Values(UniformPaint::DirectPaint, UniformPaint::BackedMaterial),
    [](const testing::TestParamInfo<UniformPaint>& parameter) {
      return parameter.param == UniformPaint::DirectPaint ? "DirectPaint"
                                                          : "BackedMaterial";
    });

TEST(SkiaPaint, AWorldSpacePaintDegradesToBoxLocalWithoutAMatrix) {
  Paint anchored = skia::sksl(resolutionEffect());
  anchored.worldSpace();
  // The reader is the CONST overload; on a mutable value the same
  // spelling is the setter.
  EXPECT_TRUE(std::as_const(anchored).worldSpace());
  EXPECT_TRUE(anchored.usesWorldSpace());
  // An identity toRoot is the honest answer outside a composite: the
  // paint resolves, and it resolves box-locally.
  EXPECT_NE(skia::shader(anchored, FrameData{.resolution = {64, 64}}), nullptr);
  // The flag is part of the recipe, so it cannot prune onto the unflagged
  // paint it was copied from.
  EXPECT_FALSE(anchored == skia::sksl(resolutionEffect()));
}

TEST(SkiaPaint, SharedRawPaintMemoSeparatesLocalAndAnchoredSampling) {
  const auto effect = effectFor(
      "uniform float4x4 uWorld; uniform float2 uResolution;"
      " half4 main(float2 p) {"
      " float4 root = uWorld * float4(p, 0, 1);"
      " return half4(root.xy / root.w / uResolution / 16, 0, 1); }");
  ASSERT_TRUE(effect);
  const FrameData frame{.resolution = {8, 8},
                        .rootResolution = {8, 8},
                        .world = glm::mat3{1, 0, 0, 0, 1, 0, 32, 16, 1}};
  const Paint localReference = skia::sksl(effect);
  Paint anchoredReference = skia::sksl(effect);
  anchoredReference.worldSpace();
  const SkBitmap localPixels = render(skia::shader(localReference, frame));
  const SkBitmap anchoredPixels =
      render(skia::shader(anchoredReference, frame));
  ASSERT_FALSE(identical(localPixels, anchoredPixels));

  for (bool anchoredFirst : {false, true}) {
    SCOPED_TRACE(anchoredFirst);
    const Paint local = skia::sksl(effect);
    Paint anchored = local;
    anchored.worldSpace();
    EXPECT_NE(local, anchored);
    const std::array<const Paint*, 2> order{anchoredFirst ? &anchored : &local,
                                            anchoredFirst ? &local : &anchored};
    for (int round = 0; round < 3; ++round) {
      SCOPED_TRACE(round);
      for (const Paint* paint : order) {
        const auto resolved = skia::shader(*paint, frame);
        ASSERT_TRUE(resolved);
        EXPECT_TRUE(identical(render(resolved),
                              paint == &local ? localPixels : anchoredPixels));
        EXPECT_EQ(skia::shader(*paint, frame), resolved);
      }
    }
  }
}
