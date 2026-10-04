/** @file
 * The paint value's uniforms: the three volatility tiers declared by what
 * an effect reads, constants and bindings sharing one path, typed and
 * packet uploads refused whole when they do not fit, blocks publishing
 * whole arrays, and equality as the prune signature.
 */

#include "SkiaPaintTestSupport.h"

TEST(SkiaPaint, APassBodyIsNotCompiledAsAShaderOfItsOwn) {
  // A pass body is written against declarations the fx() runtime
  // prepends once it knows the track's unit count. Compiled standalone it
  // names four things that do not exist yet and the compiler reports one
  // error per mention — a page of diagnostics about a compile nobody
  // asked for, on a material that then renders correctly through the
  // pass path. So it is not attempted.
  auto pass = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("pass.body")
          .body(Target::SkSL,
                "half4 main(float2 p) {\n"
                "  half4 c = uContent.eval(p);\n"
                "  for (int i = 0; i < kUnitCount; ++i)\n"
                "    c += half4(uUnitRect[i]) * uUnitPhase[i].x;\n"
                "  return c * half4(uColor * uScale);\n"
                "}"));
  std::string said;
  {
    testing::internal::CaptureStderr();
    const Paint paint = Paint::recipe(Material(pass));
    // Nothing to draw on its own — a pass material used as an ordinary
    // fill has no picture to give, and now it says so by drawing nothing.
    EXPECT_EQ(skia::staticShader(paint), nullptr);
    said = testing::internal::GetCapturedStderr();
  }
  EXPECT_EQ(said, "") << said;

  // An ordinary recipe is unaffected: it still compiles at the paint.
  auto plain = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("pass.notone").body(Target::SkSL, kBody));
  EXPECT_NE(skia::staticShader(Paint::recipe(Material(plain))), nullptr);
}

TEST(SkiaPaint, TheThreeTiersAreDeclaredByWhatTheEffectReads) {
  const Paint flat = Paint::solid({1, 0, 0, 1});
  EXPECT_FALSE(flat.isRunning());
  EXPECT_FALSE(flat.geometryDependent());
  EXPECT_TRUE(flat.isSolid());

  Paint constants = skia::sksl(constantEffect(), {{"uK", 1.0f}});
  EXPECT_FALSE(constants.isRunning());
  EXPECT_FALSE(constants.geometryDependent());
  // A constants-only sksl paint has resolved already, so it answers a
  // shader with no frame at all.
  EXPECT_NE(skia::staticShader(constants), nullptr);

  EXPECT_TRUE(skia::sksl(timeEffect()).isRunning());
  const Paint sized = skia::sksl(resolutionEffect());
  EXPECT_FALSE(sized.isRunning());
  EXPECT_TRUE(sized.geometryDependent());
  // Geometry-dependent means the frame decides: the box-less snapshot is
  // not what a consumer paints with, and the framed answer is a different
  // shader.
  EXPECT_NE(skia::shader(sized, FrameData{.resolution = {100, 40}}),
            skia::staticShader(sized));
}

TEST_P(ColorUniform, LiveAndConstantColorsUseTheSameUniformPath) {
  auto color = sigil::motion::animatable(Color{.25f, .5f, .75f, 1});
  Paint paint = describe();
  paint.bind("uColor", color);
  EXPECT_TRUE(paint.isRunning());
  const Paint copied = paint;
  Paint separate = describe();
  separate.bind("uColor", sigil::motion::animatable(color.value()));
  EXPECT_EQ(paint, copied);
  EXPECT_NE(paint, separate);
  const FrameData frame{.resolution = {4, 4}};
  const auto first = skia::shader(paint, frame);
  channels(first, 64, 128, 191);
  EXPECT_EQ(skia::shader(paint, frame), first);
  color = Color{.75f, .25f, .5f, 1};
  const auto changed = skia::shader(paint, frame);
  channels(changed, 191, 64, 128);
  EXPECT_NE(changed, first);
  EXPECT_EQ(skia::shader(paint, frame), changed);
  channels(skia::shader(copied, frame), 191, 64, 128);
  Paint fixed = paint;
  fixed.bind("uColor", Color{.2f, .4f, .6f, 1});
  EXPECT_FALSE(fixed.isRunning());
  EXPECT_TRUE(paint.isRunning());
  channels(skia::shader(fixed, frame), 51, 102, 153);
  Paint same = describe();
  same.bind("uColor", Color{.2f, .4f, .6f, 1});
  EXPECT_EQ(fixed, same);
  same.bind("uColor", Color{.6f, .4f, .2f, 1});
  EXPECT_NE(fixed, same);
}

TEST_P(ColorUniform, ScalarBindingsTakeConstantSnapshotsAndDetachCopies) {
  Paint fresh = describe();
  fresh.bind("uColor", Color{1, 1, 1, 1}).bind("uScale", .25f);
  EXPECT_FALSE(fresh.isRunning());
  channels(skia::shader(fresh), 64, 64, 64);
  auto scale = sigil::motion::animatable(.5f);
  Paint paint = describe();
  paint.bind("uColor", Color{1, 1, 1, 1}).bind("uScale", scale);
  const Paint original = paint;
  const FrameData frame{.resolution = {4, 4}};
  channels(skia::shader(paint, frame), 128, 128, 128);
  paint.bind("uScale", .25f);
  EXPECT_FALSE(paint.isRunning());
  EXPECT_TRUE(original.isRunning());
  channels(skia::shader(paint, frame), 64, 64, 64);
  scale = .75f;
  channels(skia::shader(original, frame), 191, 191, 191);
  channels(skia::shader(paint, frame), 64, 64, 64);
}

TEST_P(ColorUniform, AConstantChangeInvalidatesTheMemoWithUnchangedLiveColor) {
  auto color = sigil::motion::animatable(Color{.25f, .5f, .75f, 1});
  Paint paint = describe();
  paint.bind("uColor", color);
  const FrameData frame{.resolution = {4, 4}};
  const auto first = skia::shader(paint, frame);
  channels(first, 64, 128, 191);
  paint.set("uScale", .5f);
  const auto changed = skia::shader(paint, frame);
  channels(changed, 32, 64, 96);
  EXPECT_NE(changed, first);
  EXPECT_EQ(skia::shader(paint, frame), changed);
}

INSTANTIATE_TEST_SUITE_P(
    SkiaPaint, ColorUniform,
    testing::Values(UniformPaint::DirectPaint, UniformPaint::BackedMaterial),
    [](const testing::TestParamInfo<UniformPaint>& parameter) {
      return parameter.param == UniformPaint::DirectPaint ? "DirectPaint"
                                                          : "BackedMaterial";
    });

TEST(SkiaPaint, ASlotChangeInvalidatesTheMemoWithUnchangedLiveColor) {
  const auto effect = effectFor(
      "uniform shader uSource; uniform float4 uColor; "
      "half4 main(float2 p) { return uSource.eval(p) * half4(uColor); }");
  auto color = sigil::motion::animatable(Color{1, 1, 1, 1});
  Paint paint = skia::sksl(effect);
  paint.slot("uSource", Paint::solid({1, 0, 0, 1})).bind("uColor", color);
  const FrameData frame{.resolution = {4, 4}};
  const auto first = skia::shader(paint, frame);
  ASSERT_TRUE(first);
  EXPECT_EQ(render(first).getColor(1, 1), SK_ColorRED);
  paint.slot("uSource", Paint::solid({0, 0, 1, 1}));
  const auto changed = skia::shader(paint, frame);
  ASSERT_TRUE(changed);
  EXPECT_EQ(render(changed).getColor(1, 1), SK_ColorBLUE);
  EXPECT_NE(changed, first);
  EXPECT_EQ(skia::shader(paint, frame), changed);
}

TEST(SkiaPaint, ABlockReplacesTheOldBindingAndDoesNotDigestItsCell) {
  const auto effect = effectFor(
      "uniform float uScale; uniform float4 uColor; "
      "half4 main(float2 p) { return half4(uColor.rgb * uScale, uColor.a); }");
  const FrameData frame{.resolution = {4, 4}};
  for (bool colorInput : {false, true}) {
    SCOPED_TRACE(colorInput);
    auto scalar = sigil::motion::animatable(.25f);
    auto color = sigil::motion::animatable(Color{1, 0, 0, 1});
    Paint paint = skia::sksl(effect, {{"uScale", 1}});
    paint.set("uColor", Color{1, 1, 1, 1});
    if (colorInput)
      paint.bind("uColor", color);
    else
      paint.bind("uScale", scalar);
    const std::string name = colorInput ? "uColor" : "uScale";
    auto block = std::make_shared<UniformBlock>(colorInput ? 4 : 1);
    if (colorInput) {
      block->values()[2] = 1;
      block->values()[3] = 1;
    } else {
      block->values()[0] = .5f;
    }
    block->commit();
    paint.bind(name, block);
    const auto held = skia::shader(paint, frame);
    ASSERT_TRUE(held);
    scalar = .75f;
    color = Color{0, 1, 0, 1};
    EXPECT_EQ(skia::shader(paint, frame), held);
    if (colorInput) {
      EXPECT_EQ(render(held).getColor(1, 1), SK_ColorBLUE);
      paint.bind(name, Color{0, 1, 0, 1});
    } else {
      EXPECT_NEAR(SkColorGetR(render(held).getColor(1, 1)), 128, 1);
      paint.bind(name, .25f);
    }
    EXPECT_FALSE(paint.isRunning());
    const auto fixed = skia::shader(paint, frame);
    ASSERT_TRUE(fixed);
    if (colorInput)
      EXPECT_EQ(render(fixed).getColor(1, 1), SK_ColorGREEN);
    else
      EXPECT_NEAR(SkColorGetR(render(fixed).getColor(1, 1)), 64, 1);
  }
}

TEST(SkiaPaint, ScalarBindingsRequireASingleFloatAndOwnClockInputs) {
  const auto effect = effectFor(
      "uniform int uInteger; uniform float uArray[1]; "
      "uniform float uTime; uniform float uContentScale; "
      "half4 main(float2 p) { return half4(uTime, uContentScale, "
      "float(uInteger) + uArray[0], 1); }");
  ASSERT_TRUE(effect);
  Paint paint = skia::sksl(effect);
  auto source = sigil::motion::animatable(.75f);
  for (const char* name : {"uInteger", "uArray"}) {
    const Paint before = paint;
    testing::internal::CaptureStderr();
    paint.bind(name, source);
    const std::string said = testing::internal::GetCapturedStderr();
    EXPECT_NE(said.find(name), std::string::npos) << said;
    EXPECT_EQ(paint, before);
  }
  paint.bind("uTime", .25f).bind("uContentScale", .5f);
  EXPECT_FALSE(paint.isRunning());
  FrameData frame{.seconds = 10, .contentScale = 20};
  const auto fixed = skia::shader(paint, frame);
  ASSERT_TRUE(fixed);
  const SkColor pixel = render(fixed).getColor(1, 1);
  EXPECT_NEAR(SkColorGetR(pixel), 64, 1);
  EXPECT_NEAR(SkColorGetG(pixel), 128, 1);
  EXPECT_EQ(SkColorGetB(pixel), 0);
  paint.bind("uTime", source);
  EXPECT_TRUE(paint.isRunning());
  EXPECT_NEAR(SkColorGetR(render(skia::shader(paint, frame)).getColor(1, 1)),
              191, 1);
  paint.bind("uTime", .25f);
  EXPECT_FALSE(paint.isRunning());
  source = 0;
  EXPECT_NEAR(SkColorGetR(render(skia::shader(paint, frame)).getColor(1, 1)),
              64, 1);
}

namespace {

template <class Write>
void expectRejectedUpload(Paint& paint, const char* name,
                          const FrameData& frame, Write write) {
  const Paint before = paint;
  const auto held = skia::shader(paint, frame);
  ASSERT_TRUE(held);
  testing::internal::CaptureStderr();
  write();
  const auto said = testing::internal::GetCapturedStderr();
  EXPECT_NE(said.find(name), std::string::npos) << said;
  EXPECT_EQ(paint, before);
  EXPECT_EQ(skia::shader(paint, frame), held);
  EXPECT_TRUE(identical(render(skia::shader(paint, frame)), render(held)));
}

enum class ConstantShape { Scalar, Float2, Float4, Color };

struct ConstantReplacementCase {
  const char* name;
  ConstantShape shape;
  bool packetLast;
};

class ConstantReplacement
    : public testing::TestWithParam<ConstantReplacementCase> {
 protected:
  size_t count() const {
    switch (GetParam().shape) {
      case ConstantShape::Scalar:
        return 1;
      case ConstantShape::Float2:
        return 2;
      case ConstantShape::Float4:
      case ConstantShape::Color:
        return 4;
    }
    return 0;
  }

  sk_sp<SkRuntimeEffect> effect() const {
    switch (GetParam().shape) {
      case ConstantShape::Scalar:
        return effectFor(
            "uniform float uValue; uniform float2 uResolution; "
            "half4 main(float2 p) { return half4(uValue, "
            "uResolution.x / 32, 0, 1); }");
      case ConstantShape::Float2:
        return effectFor(
            "uniform float2 uValue; uniform float2 uResolution; "
            "half4 main(float2 p) { return half4(uValue, "
            "uResolution.x / 32, 1); }");
      case ConstantShape::Float4:
      case ConstantShape::Color:
        return effectFor(
            "uniform float4 uValue; uniform float2 uResolution; "
            "half4 main(float2 p) { return half4(uValue.rgb + "
            "float3(0, 0, uResolution.x / 32), uValue.a); }");
    }
    return nullptr;
  }

  void write(Paint& paint, bool packet, std::array<float, 4> values) const {
    if (packet) {
      paint.set("uValue",
                std::vector<float>(values.begin(), values.begin() + count()));
      return;
    }
    switch (GetParam().shape) {
      case ConstantShape::Scalar:
        paint.set("uValue", values[0]);
        break;
      case ConstantShape::Float2:
        paint.set("uValue", std::array<float, 2>{values[0], values[1]});
        break;
      case ConstantShape::Float4:
        paint.set("uValue", values);
        break;
      case ConstantShape::Color:
        paint.set("uValue", Color{values[0], values[1], values[2], values[3]});
        break;
    }
  }
};

}  // namespace

TEST_P(ConstantReplacement,
       TheLastAcceptedConstantOwnsItsSnapshotAndFrameWithoutChangingCopies) {
  const auto compiled = effect();
  ASSERT_NE(compiled, nullptr);
  const std::array initial{.25f, .25f, .25f, 1.f};
  const std::array final{.75f, .5f, .25f, 1.f};
  const bool packetLast = GetParam().packetLast;
  Paint paint = skia::sksl(compiled);
  write(paint, !packetLast, initial);
  const Paint original = paint;
  const auto originalSnapshot = skia::staticShader(original);
  ASSERT_NE(originalSnapshot, nullptr);
  FrameData frame{.resolution = {8, 8}};
  const auto originalFrame = skia::shader(original, frame);
  ASSERT_NE(originalFrame, nullptr);
  const SkBitmap originalPixels = render(originalFrame);
  write(paint, packetLast, final);
  Paint finalOnly = skia::sksl(compiled);
  write(finalOnly, packetLast, final);
  EXPECT_EQ(paint, finalOnly);
  EXPECT_NE(paint, original);
  const auto snapshot = skia::staticShader(paint);
  ASSERT_NE(snapshot, nullptr);
  const SkColor snapshotPixel = render(snapshot).getColor(1, 1);
  EXPECT_NEAR(SkColorGetR(snapshotPixel), 191, 1);
  EXPECT_NEAR(SkColorGetG(snapshotPixel), count() == 1 ? 0 : 128, 1);
  EXPECT_NEAR(SkColorGetB(snapshotPixel), count() == 4 ? 64 : 0, 1);
  EXPECT_EQ(SkColorGetA(snapshotPixel), 255);
  EXPECT_TRUE(
      identical(render(snapshot), render(skia::staticShader(finalOnly))));
  const auto resolved = skia::shader(paint, frame);
  ASSERT_NE(resolved, nullptr);
  EXPECT_NE(resolved, snapshot);
  EXPECT_EQ(skia::shader(paint, frame), resolved);
  const SkColor framePixel = render(resolved).getColor(1, 1);
  EXPECT_NEAR(SkColorGetR(framePixel), 191, 1);
  EXPECT_NEAR(SkColorGetG(framePixel), count() == 1 ? 64 : 128, 1);
  EXPECT_NEAR(SkColorGetB(framePixel),
              count() == 4   ? 128
              : count() == 2 ? 64
                             : 0,
              1);
  EXPECT_TRUE(
      identical(render(resolved), render(skia::shader(finalOnly, frame))));
  EXPECT_EQ(skia::staticShader(original), originalSnapshot);
  EXPECT_EQ(skia::shader(original, frame), originalFrame);
  EXPECT_TRUE(identical(render(originalFrame), originalPixels));

  expectRejectedUpload(paint, "uValue", frame, [&] {
    paint.set("uValue", std::vector<float>(count() + 1, 1));
  });
  expectRejectedUpload(paint, "uValue", frame, [&] {
    if (GetParam().shape == ConstantShape::Scalar)
      paint.set("uValue", std::array<float, 2>{1, 1});
    else
      paint.set("uValue", 1.0f);
  });
  EXPECT_EQ(skia::staticShader(paint), snapshot);
  EXPECT_EQ(paint, finalOnly);
}

INSTANTIATE_TEST_SUITE_P(
    SkiaPaint, ConstantReplacement,
    testing::Values(
        ConstantReplacementCase{"ScalarTypedLast", ConstantShape::Scalar,
                                false},
        ConstantReplacementCase{"ScalarPacketLast", ConstantShape::Scalar,
                                true},
        ConstantReplacementCase{"Float2TypedLast", ConstantShape::Float2,
                                false},
        ConstantReplacementCase{"Float2PacketLast", ConstantShape::Float2,
                                true},
        ConstantReplacementCase{"Float4TypedLast", ConstantShape::Float4,
                                false},
        ConstantReplacementCase{"Float4PacketLast", ConstantShape::Float4,
                                true},
        ConstantReplacementCase{"ColorTypedLast", ConstantShape::Color, false},
        ConstantReplacementCase{"ColorPacketLast", ConstantShape::Color, true}),
    [](const testing::TestParamInfo<ConstantReplacementCase>& parameter) {
      return parameter.param.name;
    });

TEST(SkiaPaint, RawTypedConstantsRequireTheDeclaredShape) {
  enum class Input { Scalar, Vec2, Vec4, Color };
  struct Rejection {
    const char* declaration;
    const char* component;
    size_t floats;
    Input input;
  };
  const Rejection cases[] = {
      {"int uValue", "float(uValue) * 1e-8", 0, Input::Scalar},
      {"float uValue[1]", "uValue[0]", 1, Input::Scalar},
      {"int2 uValue", "float(uValue.x) * 1e-8", 0, Input::Vec2},
      {"float uValue[2]", "uValue[0]", 2, Input::Vec2},
      {"int4 uValue", "float(uValue.x) * 1e-8", 0, Input::Vec4},
      {"float2x2 uValue", "uValue[0][0]", 4, Input::Vec4},
      {"float4 uValue[1]", "uValue[0].x", 4, Input::Vec4},
      {"float uValue[4]", "uValue[0]", 4, Input::Vec4},
      {"int4 uValue", "float(uValue.x) * 1e-8", 0, Input::Color},
      {"float2x2 uValue", "uValue[0][0]", 4, Input::Color},
      {"float4 uValue[1]", "uValue[0].x", 4, Input::Color},
      {"float uValue[4]", "uValue[0]", 4, Input::Color},
  };
  const FrameData frame{.resolution = {4, 4}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.declaration);
    SCOPED_TRACE(static_cast<int>(test.input));
    const std::string source = std::string("uniform ") + test.declaration +
                               "; half4 main(float2 p) { return half4(" +
                               test.component + ", 0, 0, 1); }";
    const auto effect = effectFor(source.c_str());
    ASSERT_TRUE(effect);
    Paint paint = skia::sksl(effect);
    if (test.floats) paint.set("uValue", std::vector<float>(test.floats, .25f));
    expectRejectedUpload(paint, "uValue", frame, [&] {
      switch (test.input) {
        case Input::Scalar:
          paint.set("uValue", .75f);
          break;
        case Input::Vec2:
          paint.set("uValue", std::array<float, 2>{.75f, .5f});
          break;
        case Input::Vec4:
          paint.set("uValue", std::array<float, 4>{.75f, .5f, .25f, 1});
          break;
        case Input::Color:
          paint.set("uValue", Color{.75f, .5f, .25f, 1});
          break;
      }
    });
  }
  const auto effect = effectFor(
      "uniform float uScalar; uniform float2 uPair; uniform float4 uVector; "
      "layout(color) uniform float4 uColor; half4 main(float2 p) { return "
      "half4(uScalar, uPair.y, uVector.z, uColor.a); }");
  ASSERT_TRUE(effect);
  Paint paint = skia::sksl(effect);
  paint.set("uScalar", .25f)
      .set("uPair", std::array<float, 2>{0, .5f})
      .set("uVector", std::array<float, 4>{0, 0, .75f, 1})
      .set("uColor", Color{0, 0, 0, 1});
  const SkColor pixel = render(skia::shader(paint, frame)).getColor(1, 1);
  EXPECT_NEAR(SkColorGetR(pixel), 64, 1);
  EXPECT_NEAR(SkColorGetG(pixel), 128, 1);
  EXPECT_NEAR(SkColorGetB(pixel), 191, 1);
}

TEST(SkiaPaint, RawInitialConstantsRejectIntegerArrayAndAbsentNames) {
  const auto effect = effectFor(
      "uniform int uInteger; uniform float uArray[1]; uniform float uValid; "
      "half4 main(float2 p) { return half4(uValid, float(uInteger) * 1e-8, "
      "uArray[0], 1); }");
  ASSERT_TRUE(effect);
  const Paint expected = skia::sksl(effect, {{"uValid", .25f}});
  testing::internal::CaptureStderr();
  const Paint actual = skia::sksl(
      effect,
      {{"uValid", .25f}, {"uInteger", .75f}, {"uArray", .5f}, {"uAbsent", 1}});
  const auto said = testing::internal::GetCapturedStderr();
  for (const char* name : {"uInteger", "uArray", "uAbsent"})
    EXPECT_NE(said.find(name), std::string::npos) << said;
  EXPECT_EQ(actual, expected);
  EXPECT_TRUE(
      identical(render(skia::shader(actual)), render(skia::shader(expected))));
  EXPECT_NEAR(SkColorGetR(render(skia::shader(actual)).getColor(1, 1)), 64, 1);
}

TEST(SkiaPaint, RawFloatPacketsRejectIntegerTypesAndRetainLiveBindings) {
  struct IntegerInput {
    const char* declaration;
    const char* component;
    size_t count;
  };
  const IntegerInput cases[] = {
      {"int uValue", "uValue", 1},       {"int2 uValue", "uValue.x", 2},
      {"int3 uValue", "uValue.x", 3},    {"int4 uValue", "uValue.x", 4},
      {"int uValue[1]", "uValue[0]", 1}, {"int4 uValue[2]", "uValue[0].x", 8},
  };
  const FrameData frame{.resolution = {4, 4}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.declaration);
    const std::string source =
        std::string("uniform ") + test.declaration +
        "; uniform float4 uColor; half4 main(float2 p) { return half4(" +
        "uColor.rgb + float3(float(" + test.component +
        ") * 1e-8, 0, 0), uColor.a); }";
    const auto effect = effectFor(source.c_str());
    ASSERT_TRUE(effect);
    ASSERT_TRUE(effect->findUniform("uValue"));
    auto color = sigil::motion::animatable(Color{1, 0, 0, 1});
    Paint paint = skia::sksl(effect);
    paint.bind("uColor", color);
    expectRejectedUpload(paint, "uValue", frame, [&] {
      paint.set("uValue", std::vector<float>(test.count, .5f));
    });
    auto block = std::make_shared<UniformBlock>(test.count);
    std::fill(block->values().begin(), block->values().end(), .5f);
    block->commit();
    expectRejectedUpload(paint, "uValue", frame,
                         [&] { paint.bind("uValue", block); });
    expectRejectedUpload(paint, "uAbsent", frame,
                         [&] { paint.set("uAbsent", .5f); });
    color = Color{0, 0, 1, 1};
    EXPECT_EQ(render(skia::shader(paint, frame)).getColor(1, 1), SK_ColorBLUE);
  }
  const auto effect = effectFor(
      "uniform float4 uColor; half4 main(float2 p) { return half4(uColor); }");
  ASSERT_TRUE(effect);
  auto color = sigil::motion::animatable(Color{1, 0, 0, 1});
  Paint paint = skia::sksl(effect);
  paint.bind("uColor", color);
  for (int count : {3, 5}) {
    expectRejectedUpload(paint, "uColor", frame, [&] {
      paint.set("uColor", std::vector<float>(count, .5f));
    });
    const auto block = std::make_shared<UniformBlock>(count);
    expectRejectedUpload(paint, "uColor", frame,
                         [&] { paint.bind("uColor", block); });
  }
  color = Color{0, 1, 0, 1};
  EXPECT_EQ(render(skia::shader(paint, frame)).getColor(1, 1), SK_ColorGREEN);
}

TEST(SkiaPaint, RawFloatPacketsFillCompleteVectorsMatricesAndArrays) {
  struct Packet {
    const char* declaration;
    const char* first;
    const char* last;
    size_t count;
  };
  const Packet cases[] = {
      {"float uValue", "uValue", "uValue", 1},
      {"float2 uValue", "uValue.x", "uValue.y", 2},
      {"float3 uValue", "uValue.x", "uValue.z", 3},
      {"float4 uValue", "uValue.x", "uValue.w", 4},
      {"half4 uValue", "uValue.x", "uValue.w", 4},
      {"float2x2 uValue", "uValue[0][0]", "uValue[1][1]", 4},
      {"float3x3 uValue", "uValue[0][0]", "uValue[2][2]", 9},
      {"float4x4 uValue", "uValue[0][0]", "uValue[3][3]", 16},
      {"float uValue[3]", "uValue[0]", "uValue[2]", 3},
      {"float2 uValue[2]", "uValue[0].x", "uValue[1].y", 4},
      {"float4 uValue[2]", "uValue[0].x", "uValue[1].w", 8},
      {"float2x2 uValue[2]", "uValue[0][0][0]", "uValue[1][1][1]", 8},
  };
  const FrameData frame{.resolution = {4, 4}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.declaration);
    const std::string source = std::string("uniform ") + test.declaration +
                               "; half4 main(float2 p) { return half4(" +
                               test.first + ", " + test.last + ", 0, 1); }";
    const auto effect = effectFor(source.c_str());
    ASSERT_TRUE(effect);
    std::vector<float> values(test.count, .25f);
    values.back() = .75f;
    Paint fixed = skia::sksl(effect);
    fixed.set("uValue", values);
    EXPECT_FALSE(fixed.isRunning());
    const SkColor expected = render(skia::shader(fixed, frame)).getColor(1, 1);
    EXPECT_NEAR(SkColorGetR(expected), (test.count == 1 ? .75f : .25f) * 255,
                1);
    EXPECT_NEAR(SkColorGetG(expected), .75f * 255, 1);
    auto block = std::make_shared<UniformBlock>(test.count);
    std::copy(values.begin(), values.end(), block->values().begin());
    block->commit();
    Paint live = skia::sksl(effect);
    live.bind("uValue", block);
    EXPECT_TRUE(live.isRunning());
    const auto held = skia::shader(live, frame);
    EXPECT_EQ(render(held).getColor(1, 1), expected);
    block->values()[0] = .5f;
    EXPECT_EQ(skia::shader(live, frame), held);
    block->commit();
    EXPECT_NEAR(SkColorGetR(render(skia::shader(live, frame)).getColor(1, 1)),
                .5f * 255, 1);
  }
}

TEST(SkiaPaint, RawFrameInputsRequireTheirDeclaredTypes) {
  struct FrameInput {
    const char* declaration;
    const char* component;
    bool clock;
  };
  const FrameInput cases[] = {
      {"int uTime", "float(uTime) * 1e-8", true},
      {"float uTime[1]", "uTime[0]", true},
      {"int uContentScale", "float(uContentScale) * 1e-8", false},
      {"float uContentScale[1]", "uContentScale[0]", false},
      {"int2 uResolution", "float(uResolution.x) * 1e-8", false},
      {"float uResolution[2]", "uResolution[0]", false},
      {"float2 uResolution[1]", "uResolution[0].x", false},
  };
  const FrameData frame{
      .seconds = .75, .resolution = {4, 4}, .contentScale = .5};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.declaration);
    const std::string source =
        std::string("uniform ") + test.declaration +
        "; uniform float uLive; half4 main(float2 p) { return half4(uLive, " +
        test.component + ", 0, 1); }";
    const auto effect = effectFor(source.c_str());
    ASSERT_TRUE(effect);
    Paint paint = skia::sksl(effect);
    EXPECT_FALSE(paint.isRunning());
    EXPECT_FALSE(paint.geometryDependent());
    if (test.clock) {
      const Paint before = paint;
      testing::internal::CaptureStderr();
      paint.quantizeTime(10);
      const auto said = testing::internal::GetCapturedStderr();
      EXPECT_NE(said.find("uniform float uTime"), std::string::npos) << said;
      EXPECT_EQ(paint, before);
    }
    auto value = sigil::motion::animatable(.25f);
    paint.bind("uLive", value);
    const SkColor pixel = render(skia::shader(paint, frame)).getColor(1, 1);
    EXPECT_NEAR(SkColorGetR(pixel), 64, 1);
    EXPECT_EQ(SkColorGetG(pixel), 0);
    value = .5f;
    EXPECT_NEAR(SkColorGetR(render(skia::shader(paint, frame)).getColor(1, 1)),
                128, 1);
  }
}

TEST_P(UniformPublication, DraftArraysStayHiddenWhenOtherInputsChange) {
  struct Parameters {
    std::array<float, 1> uTable{};
    float uSibling = 0;
  };
  constexpr const char* body =
      "half4 main(float2 p) { return half4(uTable[0], uSibling, "
      "uTime * 0.25 + uResolution.x / 256, 1); }";
  auto block = std::make_shared<UniformBlock>(1);
  block->values()[0] = 0.25f;
  block->commit();
  auto sibling = sigil::motion::animatable(0.25f);
  const auto describe = [&] {
    Paint paint;
    if (GetParam() == UniformPaint::DirectPaint) {
      const std::string source =
          std::string(
              "uniform float uTable[1]; uniform float uSibling; "
              "uniform float uTime; uniform float2 uResolution; ") +
          body;
      paint = skia::sksl(effectFor(source.c_str()));
    } else {
      paint = Paint::recipe(sigil::material::shader(body, Parameters{}));
    }
    paint.bind("uTable", block).bind("uSibling", sibling);
    return paint;
  };
  const auto expectChannels = [](const sk_sp<SkShader>& shader, int red,
                                 int green, int blue) {
    ASSERT_NE(shader, nullptr);
    const SkColor pixel = render(shader).getColor(1, 1);
    EXPECT_NEAR(SkColorGetR(pixel), red, 1);
    EXPECT_NEAR(SkColorGetG(pixel), green, 1);
    EXPECT_NEAR(SkColorGetB(pixel), blue, 1);
  };

  const Paint paint = describe();
  FrameData frame{.resolution = {64, 64}};
  const sk_sp<SkShader> first = skia::shader(paint, frame);
  expectChannels(first, 64, 64, 64);
  block->values()[0] = 0.75f;
  EXPECT_EQ(skia::shader(paint, frame), first);
  sibling = 0.75f;
  expectChannels(skia::shader(paint, frame), 64, 191, 64);
  frame.seconds = 1;
  frame.resolution = {32, 32};
  expectChannels(skia::shader(paint, frame), 64, 191, 96);
  expectChannels(skia::shader(describe(), frame), 64, 191, 96);
  expectChannels(skia::shader(describe()), 64, 191, 0);

  block->commit();
  const sk_sp<SkShader> published = skia::shader(paint, frame);
  expectChannels(published, 191, 191, 96);
  EXPECT_EQ(skia::shader(paint, frame), published);
  expectChannels(first, 64, 64, 64);
  block->values()[0] = 0;
  expectChannels(skia::shader(describe(), frame), 191, 191, 96);
}

TEST_P(UniformPublication,
       ClearingABlockRestoresConstantsAndKeepsOtherBindings) {
  struct Parameters {
    std::array<float, 1> uTable{.25f};
    Color uColor{1, 1, 1, 1};
  };
  constexpr const char* body =
      "half4 main(float2 p) { return half4(uColor.rgb * uTable[0], uColor.a); "
      "}";
  Paint paint;
  if (GetParam() == UniformPaint::DirectPaint) {
    const std::string source =
        std::string("uniform float uTable[1]; uniform float4 uColor; ") + body;
    paint = skia::sksl(effectFor(source.c_str()));
    paint.set("uTable", std::vector<float>{.25f});
    paint.set("uColor", Color{1, 1, 1, 1});
  } else {
    paint = Paint::recipe(shader(body, Parameters{}));
  }
  auto block = std::make_shared<UniformBlock>(1);
  block->values()[0] = .75f;
  block->commit();
  paint.bind("uTable", block);
  const Paint original = paint;
  const FrameData frame{.resolution = {4, 4}};
  paint.bind("uTable", std::shared_ptr<const UniformBlock>{});
  EXPECT_FALSE(paint.isRunning());
  EXPECT_TRUE(original.isRunning());
  const auto fixed = skia::shader(paint, frame);
  ASSERT_TRUE(fixed);
  EXPECT_NEAR(SkColorGetR(render(fixed).getColor(1, 1)), 64, 1);
  EXPECT_NEAR(SkColorGetR(render(skia::shader(original, frame)).getColor(1, 1)),
              191, 1);
  block->values()[0] = .5f;
  block->commit();
  EXPECT_EQ(skia::shader(paint, frame), fixed);
  auto color = sigil::motion::animatable(Color{1, 0, 0, 1});
  paint.bind("uColor", color).bind("uTable", block);
  paint.bind("uTable", std::shared_ptr<const UniformBlock>{});
  EXPECT_TRUE(paint.isRunning());
  color = Color{0, 1, 0, 1};
  const auto green = skia::shader(paint, frame);
  ASSERT_TRUE(green);
  const SkColor pixel = render(green).getColor(1, 1);
  EXPECT_EQ(SkColorGetR(pixel), 0);
  EXPECT_NEAR(SkColorGetG(pixel), 64, 1);
  EXPECT_EQ(SkColorGetB(pixel), 0);
}

INSTANTIATE_TEST_SUITE_P(
    SkiaPaint, UniformPublication,
    testing::Values(UniformPaint::DirectPaint, UniformPaint::BackedMaterial),
    [](const testing::TestParamInfo<UniformPaint>& parameter) {
      return parameter.param == UniformPaint::DirectPaint ? "DirectPaint"
                                                          : "BackedMaterial";
    });

TEST(SkiaPaint, SettingOneUniformTwiceReplacesItRatherThanStacking) {
  // Two entries under one name are both assigned by the builder, so the
  // picture is the last one either way — but the lane grows without
  // bound in a live-coding host that re-describes every frame, and the
  // paint stops comparing equal to the same paint described once, which
  // is what a node prunes on.
  Paint twice = skia::sksl(constantEffect());
  twice.set("uK", 0.25f);
  twice.set("uK", 1.0f);
  EXPECT_TRUE(twice == skia::sksl(constantEffect(), {{"uK", 1.0f}}));
  EXPECT_TRUE(identical(render(skia::staticShader(twice)),
                        render(skia::staticShader(
                            skia::sksl(constantEffect(), {{"uK", 1.0f}})))));
}

TEST(SkiaPaint, EqualityIsTheRecipeSoARebuiltPaintPrunes) {
  const std::vector<ColorStop> stops{{0, {1, 0, 0, 1}}, {1, {0, 0, 1, 1}}};
  const GradientOptions pixels{.units = GradientUnits::Pixels};
  EXPECT_TRUE(Paint::solid({1, 0, 0, 1}) == Paint::solid({1, 0, 0, 1}));
  EXPECT_FALSE(Paint::solid({1, 0, 0, 1}) == Paint::solid({1, 0, 0.5f, 1}));
  // Two separately built gradients over the same recipe are equal even
  // though each minted its own SkShader — that is what lets a node prune
  // across describes.
  EXPECT_TRUE(Paint::linearGradient({0, 0}, {10, 0}, stops, pixels) ==
              Paint::linearGradient({0, 0}, {10, 0}, stops, pixels));
  EXPECT_FALSE(Paint::linearGradient({0, 0}, {10, 0}, stops, pixels) ==
               Paint::linearGradient({0, 0}, {20, 0}, stops, pixels));
  // The empty paint is reflexive; a holder that compared unequal to itself
  // would patch forever.
  EXPECT_TRUE(Paint() == Paint{});
  EXPECT_FALSE(Paint{} == Paint::solid({0, 0, 0, 0}));
  // A child is part of the signature: two paints with different second
  // sources must never prune onto each other.
  Paint a =
      skia::sksl(effectFor("uniform shader uSrc;\n"
                           "half4 main(float2 p) { return uSrc.eval(p); }"));
  Paint b = a;
  a.slot("uSrc", Paint::solid({1, 0, 0, 1}));
  b.slot("uSrc", Paint::solid({0, 1, 0, 1}));
  EXPECT_FALSE(a == b);
}

TEST(SkiaPaint, CopyOnWriteKeepsAMutationOffTheValueItWasCopiedFrom) {
  Paint base = skia::sksl(constantEffect(), {{"uK", 1.0f}});
  Paint copy = base;
  copy.set("uK", 0.25f);
  EXPECT_FALSE(base == copy);
  EXPECT_TRUE(base == skia::sksl(constantEffect(), {{"uK", 1.0f}}));
}
