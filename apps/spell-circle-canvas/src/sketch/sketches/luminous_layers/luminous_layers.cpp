/** Material lettering and painted marks retained in a float working image. */
// TAGS: Compose/Materials, Materials/Lighting, Materials/Color,
// Typography/Material ink, Brushes/Painted data, Composition/Layers

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Stroke.h>
#include <sigildraw/brush/Tool.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace draw = sigil::draw;
namespace brush = sigil::draw::brush;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

namespace luminous_layers {

using compose::Element;
constexpr int kWidth = 1440, kHeight = 1080;
constexpr float kLoop = 32, kPi = 3.14159265358979323846f;
constexpr auto kPaper = material::hexColor(0xece8de);
constexpr auto kInk = material::hexColor(0x283b3b);
constexpr auto kMuted = material::hexColor(0x647875);
constexpr auto kRule = material::hexColor(0xbac4bb);
constexpr auto kWarm = material::hexColor(0xc96539);
constexpr auto kCool = material::hexColor(0x367f86);
constexpr material::Color kCopper{.64f, .29f, .13f, 1};
constexpr material::Color kSilver{.32f, .53f, .58f, 1};
constexpr std::array<float, 4> kGains{0, 1, 4, 16};
constexpr std::array<float, 5> kOpacities{0, .01f, .25f, .5f, 1};
constexpr std::array<SkColorType, 3> kTextureTypes{
    kN32_SkColorType, kRGBA_F16_SkColorType, kRGBA_F32_SkColorType};
constexpr std::array<std::string_view, 3> kTextureNames{"N32", "F16", "F32"};
constexpr float kTextureWidth = 188, kTextureHeight = 32;
constexpr float kTextureX = 108, kTextureY = 850, kTextureStride = 44;
constexpr std::array<std::string_view, 8> kPhases{
    "01 / LUMINOUS ALLOY", "02 / EMISSION ZERO",   "03 / LIVE RGB",
    "04 / WHITE SIXTEEN",  "05 / QUARTER OPACITY", "06 / LAYER ORDER",
    "07 / GLASS + CLIP",   "08 / ZERO ALPHA"};

struct Controls {
  float gain = 1.4f;
  float opacity = 1;
  float exposure = 0;
  bool shoulder = true;
  bool bloom = true;
  bool depth = true;
};

struct State {
  int phase = 0;
  bool depth = true;
  bool operator==(const State&) const = default;
};

struct Sample {
  float x = 0, y = 0, gain = 0, opacity = 0;
  std::array<float, 4> premulRGBA{};
  std::array<float, 4> expectedRGBA{};
};

constexpr std::string_view kEmission = R"(
half4 main(float2 p) { return half4(color.rgb * gain, 1); }
)";

constexpr std::string_view kCoverage = R"(
half4 main(float2 p) {
  float a = clamp(field.eval(p).r, 0, 1);
  return half4(color.rgb * a, a);
})";

constexpr std::string_view kBrushed = R"(
half4 main(float2 p) {
  float step = max(.65, 1 / max(uContentScale, .125));
  float slope = (.09 * sin(p.y * 1.7) + .025 * sin(p.y * .31))
              * exp(-step * .18);
  float3 n = normalize(float3(.018 * sin(p.x * .13), slope, 1));
  return half4(n * .5 + .5, 1);
})";

constexpr std::string_view kRoom = R"(
half4 main(float2 p) {
  float2 uv = p / float2(512, 256);
  float a = (uv.x - .5) * 6.2831853;
  float b = uv.y * 3.14159265;
  float3 d = float3(sin(b) * sin(a), cos(b), -sin(b) * cos(a));
  float2 offset = float2((d.x + .25) / .48, (d.y - .18) / .58);
  float box = exp(-dot(offset, offset)) * step(0, d.z);
  return half4(float3(.18, .22, .25) + box * float3(.8, .9, 1), 1);
})";

// Display exposure follows the float image; it never changes its contents.
constexpr std::string_view kView = R"(
half4 main(float2 p) {
  float4 s = raw.eval((p + origin) * density);
  if (s.a <= 0) return half4(0);
  float3 rgb = max(s.rgb / s.a, float3(0)) * exp2(exposure);
  rgb = shoulder > .5 ? rgb / (1 + rgb) : clamp(rgb, 0, 1);
  return half4(rgb * s.a, s.a);
})";

float bounded(float value, float low, float high, float fallback) {
  return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}

Element label(std::string_view words, float x, float y, float width,
              float size = 11, material::Material ink = kMuted) {
  const int lines = 1 + std::count(words.begin(), words.end(), '\n');
  return compose::kit::at(compose::text(std::string(words)), x, y, width,
                          size * 1.6f * lines)
      .fontSize(size)
      .ink(std::move(ink));
}

Element rule(float x, float y, float width, material::Color ink = kRule) {
  return compose::kit::at(x, y, width, 1).fill(ink);
}

Element button(std::string_view words, float x, float y, float width,
               bool active) {
  return compose::kit::at(compose::stack(), x, y, width, 32)
      .fill(active ? kInk : kPaper)
      .borderRadius(3)
      .stroke(compose::stroke(1, compose::Fill::color(kRule)))
      .children({label(words, 10, 8, width - 20, 10, active ? kPaper : kInk)});
}

material::Material pixels(sk_sp<SkImage> image, glm::vec2 extent) {
  material::Texture texture(std::move(image));
  const glm::vec2 size(texture.size());
  if (size.x > 0 && size.y > 0) {
    glm::mat3 placement(1);
    placement[0][0] = extent.x / size.x;
    placement[1][1] = extent.y / size.y;
    texture.uv(placement);
  }
  return material::image(std::move(texture));
}

material::Material emitted(float gain) {
  return material::Material(material::Color{0, 0, 0, 1})
      .surface({.roughness = 1.f,
                .emission = material::Color{1, 1, 1, 1},
                .emissionStrength = gain});
}

}  // namespace luminous_layers

struct LuminousLayers {
  using Element = compose::Element;
  using Sample = luminous_layers::Sample;
  luminous_layers::Controls controls;
  int heldPhase = -1;
  double seconds = 0;
  motion::Animatable<material::Color> warm =
      motion::animatable(material::Color{1, .24f, .07f, 1});
  motion::Animatable<material::Color> cool =
      motion::animatable(material::Color{.06f, .66f, .9f, 1});
  motion::Animatable<float> gain = motion::animatable(1.4f);
  motion::Animatable<float> markOpacity = motion::animatable(1.f);
  motion::Animatable<float> keyX = motion::animatable(480.f);
  motion::Animatable<float> keyY = motion::animatable(130.f);
  motion::Animatable<float> fillX = motion::animatable(1180.f);
  motion::Animatable<float> fillY = motion::animatable(600.f);
  std::unique_ptr<draw::Graphics> coverage =
      std::make_unique<draw::Graphics>(964, 128);
  std::unique_ptr<compose::Composer> rawComposer;
  weave::FontContext* fonts = nullptr;
  sk_sp<SkSurface> working;
  skgpu::graphite::Recorder* recorder = nullptr;
  GrRecordingContext* recordingContext = nullptr;
  skgpu::graphite::Recorder* rawRecorder = nullptr;
  GrRecordingContext* rawRecordingContext = nullptr;
  bool drewRaw = false;
  bool rasterFallback = false, pointerDown = false;
  bool gainOverride = false, opacityOverride = false;
  float currentGain = 1.4f, currentOpacity = 1;
  std::optional<luminous_layers::State> described;
  std::array<std::shared_ptr<compose::TextureScene>, 3> textureWitnesses;
  std::array<uint64_t, 3> textureRevisions{};
  SkImageInfo textureInfo;
  float textureDensity = 0;
  compose::Shape pressure;
  material::Filter displayBloom =
      material::Filter::bloom({.sigma = 3.2f,
                               .strength = .22f,
                               .spread = 3,
                               .tail = .35f,
                               .threshold = .4f,
                               .knee = .12f,
                               .maximumOpacity = .32f});

  luminous_layers::State state() const {
    const int phase = heldPhase < 0
                          ? int(std::floor(std::fmod(seconds, 32.) / 4))
                          : std::clamp(heldPhase, 0, 7);
    return {phase, controls.depth};
  }

  void initialize(motion::Engine& engine, weave::FontContext& fontContext) {
    using namespace luminous_layers;
    fonts = &fontContext;
    rawComposer = std::make_unique<compose::Composer>(engine, fontContext);
    rawComposer->setSize({kWidth, kHeight});
    compose::brush::Ribbon ribbon;
    ribbon.width = path::Profile{{0, .5f},  {.15f, 12}, {.38f, 34},
                                 {.57f, 7}, {.78f, 25}, {1, .5f}};
    ribbon.join = path::Join::Round;
    ribbon.step = 1;
    pressure = compose::heldPath(path::fromSk(ribbon.band(path::toSk(
        path::Outline::svg("M12 56 C100 5 212 2 294 50 C376 103 440 18 526 38 "
                           "C626 70 718 8 838 34")))));
    coverage = std::make_unique<draw::Graphics>(964, 128);
    drewRaw = false;
    described.reset();
    textureWitnesses = {};
    textureDensity = 0;
    applyTime(0);
  }

  void applyTime(double elapsed) {
    using namespace luminous_layers;
    seconds =
        std::isfinite(elapsed) ? std::fmod(std::max(0., elapsed), 32.) : 0;
    const auto value = state();
    currentGain = bounded(controls.gain, 0, 16, 1.4f);
    currentOpacity = bounded(controls.opacity, 0, 1, 1);
    if (!gainOverride) {
      if (value.phase == 1) currentGain = 0;
      if (value.phase == 3) currentGain = 16;
    }
    if (!opacityOverride) {
      if (value.phase == 4) currentOpacity = .25f;
      if (value.phase == 5) currentOpacity = .65f;
      if (value.phase == 7) currentOpacity = 0;
    }
    gain = currentGain;
    markOpacity = currentOpacity;
    const float hue = float(std::fmod(seconds, 4.) / 4.);
    warm = value.phase == 2   ? material::hsv(hue * 360, 1, 1)
           : value.phase == 3 ? material::Color{1, 1, 1, 1}
                              : material::Color{1, .24f, .07f, 1};
    cool = value.phase == 2
               ? material::hsv(std::fmod(hue * 360 + 180, 360.f), 1, 1)
           : value.phase == 3 ? material::Color{1, 1, 1, 1}
                              : material::Color{.06f, .66f, .9f, 1};
    const float t = float(seconds) * 2 * kPi / kLoop;
    keyX = 480 + 235 * std::sin(t);
    keyY = 130 + 65 * std::cos(t * 2);
    fillX = 1150 - 165 * std::sin(t + .7f);
    fillY = 600 + 65 * std::cos(t * 2 + .3f);
    if (textureWitnesses[0]) checkTextureWitnesses();
  }

  void prepareCoverage(draw::Pen& host) {
    const SkISize extent{
        std::max(1, int(std::round(964 * host.contentScale()))),
        std::max(1, int(std::round(128 * host.contentScale())))};
    if (coverage->image() && coverage->extent() == extent) return;
    auto& pen = coverage->begin(host);
    pen.push();
    pen.colorMode(draw::RGB);
    pen.blendMode(draw::BLEND);
    pen.background(0);
    pen.randomSeed(0x1a117u);
    auto tool = brush::watercolor({1, 1, 1, 1}, 51);
    tool.blend = draw::BLEND;
    tool.opacity = .77f;
    tool.bristles = 38;
    tool.spacing = 1.8f;
    tool.scatter = 1.4f;
    tool.pressure = {.13f, .97f, .17f};
    const std::array<brush::Sample, 5> first{{{{-22, 96}, .16f},
                                              {{187, 26}, .86f},
                                              {{444, 92}, 1.f},
                                              {{696, 32}, .87f},
                                              {{988, 78}, .17f}}};
    brush::spline(pen, tool, first, .8f);
    tool.width = 24;
    tool.bristles = 19;
    tool.opacity = .58f;
    const std::array<brush::Sample, 4> second{{{{16, 118}, .15f},
                                               {{247, 71}, .92f},
                                               {{615, 113}, .8f},
                                               {{956, 48}, .12f}}};
    brush::spline(pen, tool, second, .8f);
    pen.pop();
    coverage->end();
    described.reset();
  }

  material::Material emission(motion::Animatable<material::Color> color) const {
    struct Parameters {
      material::Color color;
      float gain;
    };
    return material::shader(luminous_layers::kEmission,
                            Parameters{material::Color{1, 1, 1, 1}, 1})
        .bind("color", std::move(color))
        .bind("gain", gain);
  }

  void checkTextureWitnesses() const {
    using namespace luminous_layers;
    for (size_t i = 0; i < textureWitnesses.size(); ++i) {
      const auto& scene = textureWitnesses[i];
      if (!scene || !scene->image())
        throw std::runtime_error(
            "Luminous layers requires painted image specimens");
      if (scene->image()->colorType() != kTextureTypes[i])
        throw std::runtime_error(
            "Luminous layers requires the requested image formats");
      if (scene->isRunning())
        throw std::runtime_error(
            "Luminous layers requires settled image specimens");
      if (scene->revision() != textureRevisions[i])
        throw std::runtime_error(
            "Luminous layers requires held image revisions");
    }
  }

  void prepareTextureWitnesses(const SkImageInfo& destination, float density) {
    using namespace luminous_layers;
    const auto info = SkImageInfo::Make(
        std::max(1, int(std::round(kTextureWidth * density))),
        std::max(1, int(std::round(kTextureHeight * density))),
        kRGBA_F16_SkColorType, kPremul_SkAlphaType,
        destination.refColorSpace());
    if (textureWitnesses[0] && textureInfo == info &&
        textureDensity == density) {
      checkTextureWitnesses();
      return;
    }
    auto mask = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(std::max(1, int(std::round(118 * density))),
                                   std::max(1, int(std::round(15 * density)))));
    if (!mask)
      throw std::runtime_error(
          "Luminous layers requires a painted image specimen");
    mask->getCanvas()->scale(density, density);
    draw::on(
        *mask->getCanvas(), SkSize{118, 15},
        [](draw::Pen& pen) {
          pen.colorMode(draw::RGB);
          pen.background(0);
          pen.randomSeed(0x1a117u);
          auto tool = brush::watercolor({1, 1, 1, 1}, 8);
          tool.blend = draw::BLEND;
          tool.opacity = .77f;
          tool.bristles = 12;
          tool.spacing = .7f;
          tool.scatter = .3f;
          const std::array<brush::Sample, 4> points{{{{-4, 10}, .2f},
                                                     {{34, 4}, .9f},
                                                     {{80, 10}, 1},
                                                     {{122, 5}, .2f}}};
          brush::spline(pen, tool, points, .6f);
        },
        fonts);
    const auto field = pixels(mask->makeImageSnapshot(), {118, 15});
    auto ink = material::Material(kCopper).surface(
        {.metallic = .8f,
         .roughness = .3f,
         .emission = material::Color{1, .24f, .07f, 1},
         .emissionStrength = 4});
    struct Parameters {
      material::Color color;
    };
    auto painted = material::shader(kCoverage, Parameters{kCopper},
                                    {.textures = {{"field", {}}}})
                       .slot("field", field)
                       .surface({.metallic = .8f,
                                 .roughness = .3f,
                                 .normal = material::surface::normalFromHeight(
                                     field, {.depth = .5f, .step = .5f}),
                                 .emission = material::Color{1, .24f, .07f, 1},
                                 .emissionStrength = 4});
    const auto specimen =
        compose::scene()
            .width(kTextureWidth)
            .height(kTextureHeight)
            .children(
                {label("O8", 0, -1, 66, 32, material::Color{0, 0, 0, 0})
                     .fontFamily("Georgia, Times New Roman, serif")
                     .decorationOutline(compose::Boundary::Glyphs)
                     .foreground(
                         compose::relief(ink, {.shoulder = .7f, .depth = .3f}))
                     .opacity(.5f),
                 compose::kit::at(70, 1, 850, 94)
                     .shape(pressure)
                     .fill(ink)
                     .scale(.12f)
                     .transformOrigin(compose::Dimension(0),
                                      compose::Dimension(0))
                     .opacity(.5f),
                 compose::kit::at(70, 17, 118, 15).fill(painted).opacity(.5f),
                 compose::light(
                     material::studio({.intensity = .12f, .ambient = 0})),
                 compose::kit::at(compose::scene(), 180, 0, 8, 8)
                     .children({compose::light(material::studio(
                                    {.intensity = 0, .ambient = 0})),
                                compose::kit::at(0, 0, 8, 8)
                                    .fill(emitted(4))
                                    .opacity(.5f)})});
    std::array<std::shared_ptr<compose::TextureScene>, 3> prepared;
    for (size_t i = 0; i < prepared.size(); ++i) {
      prepared[i] = compose::TextureScene::make(
          info.makeColorType(kTextureTypes[i]), *fonts);
      if (!prepared[i])
        throw std::runtime_error("Luminous layers requires its image formats");
      prepared[i]->setAutoTexturePromotion(compose::PromotionPolicy::Off);
      Element scaled = specimen;
      scaled.scale(density).transformOrigin(compose::Dimension(0),
                                            compose::Dimension(0));
      const auto root = compose::stack()
                            .width(info.width())
                            .height(info.height())
                            .children({scaled});
      for (int visit = 0; visit < 8 && (visit == 0 || prepared[i]->isRunning());
           ++visit)
        prepared[i]->render(root);
    }
    textureWitnesses = std::move(prepared);
    textureInfo = info;
    textureDensity = density;
    for (size_t i = 0; i < textureWitnesses.size(); ++i)
      textureRevisions[i] = textureWitnesses[i]->revision();
    checkTextureWitnesses();
  }

  std::vector<Sample> readTextureWitnesses() const {
    using namespace luminous_layers;
    if (!textureWitnesses[0]) return {};
    checkTextureWitnesses();
    std::vector<Sample> samples;
    const auto info =
        SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType,
                          textureInfo.refColorSpace());
    for (size_t i = 0; i < textureWitnesses.size(); ++i) {
      Sample sample;
      sample.x = kTextureX + 184;
      sample.y = kTextureY + i * kTextureStride + 4;
      sample.gain = 4;
      sample.opacity = .5f;
      sample.expectedRGBA = {2, 2, 2, .5f};
      if (!textureWitnesses[i]->image()->readPixels(
              nullptr, info, sample.premulRGBA.data(), sizeof(float) * 4,
              int(std::floor(184 * textureDensity)),
              int(std::floor(4 * textureDensity))))
        return {};
      samples.push_back(sample);
    }
    return samples;
  }

  material::Material finish(bool copper, bool painted = false) const {
    using namespace luminous_layers;
    material::Material base(copper ? kCopper : kSilver);
    material::Material normal = material::shader(kBrushed);
    if (painted) {
      const auto field = pixels(coverage->image(), {964, 128});
      struct Parameters {
        material::Color color;
      };
      base = material::shader(kCoverage, Parameters{kCopper},
                              {.textures = {{"field", {}}}})
                 .slot("field", field);
      normal = material::surface::blendNormals(
          material::surface::normalFromHeight(
              field, {.depth = controls.depth ? 5.f : 0.f, .step = .8f}),
          normal);
    } else {
      base.layer(material::noise(.12f, {.seed = 19, .grain = true}),
                 {.blend = material::BlendMode::SoftLight, .opacity = .12f});
    }
    return base.surface({.metallic = .8f,
                         .roughness = .3f,
                         .normal = normal,
                         .normalDirectX = false,
                         .emission = material::Color{1, 1, 1, 1},
                         .emissionStrength = 1,
                         .emissionMap = emission(copper ? warm : cool),
                         .reflectionWeight = .65f});
  }

  Element calibration() const {
    using namespace luminous_layers;
    Element out = compose::scene().width(kWidth).height(kHeight).children(
        {compose::light(material::studio({.intensity = 0, .ambient = 0}))
             .key("emission-only")});
    for (int column = 0; column < 4; ++column)
      for (int row = 0; row < 5; ++row)
        out.children(
            {compose::kit::at(398 + column * 182.f, 856 + row * 24.f, 158, 16)
                 .fill(emitted(kGains[column]))
                 .opacity(kOpacities[row])});
    for (int i = 0; i < 3; ++i)
      out.children({compose::kit::at(1150, 856 + i * 23.f, 154,
                                     std::array{.5f, 1.f, 2.f}[i])
                        .fill(emitted(4))
                        .rotate(-8)});
    out.children({label("O8", 1150, 924, 152, 39, material::Color{0, 0, 0, 0})
                      .fontFamily("Georgia, Times New Roman, serif")
                      .decorationOutline(compose::Boundary::Glyphs)
                      .foreground(compose::relief(emitted(4),
                                                  {.shoulder = 0, .depth = 0}))
                      .opacity(.25f)});
    return out;
  }

  Element hero(const luminous_layers::State& value) const {
    using namespace luminous_layers;
    auto ground = material::Material(material::Color{.018f, .028f, .031f, 1});
    ground.layer(material::noise(.18f, {.seed = 23, .grain = true}),
                 {.blend = material::BlendMode::SoftLight, .opacity = .1f});
    ground.surface({.metallic = .15f, .roughness = .84f});
    Element letters =
        label("LUMEN / 08", 40, 54, 940, 112, material::Color{0, 0, 0, 0})
            .fontFamily("Georgia, Times New Roman, serif")
            .decorationOutline(compose::Boundary::Glyphs)
            .foreground(compose::relief(
                finish(true),
                {.shoulder = 2.2f, .depth = value.depth ? 1.2f : 0}))
            .opacity(markOpacity);
    Element ribbon = compose::kit::at(40, 270, 850, 94)
                         .shape(pressure)
                         .background(compose::relief(
                             finish(false), {.shoulder = 2.5f,
                                             .depth = value.depth ? 1.f : 0}))
                         .opacity(markOpacity);
    Element deposit = compose::kit::at(40, 391, 964, 128)
                          .fill(finish(true, true))
                          .opacity(markOpacity);
    std::vector<Element> parts;
    if (value.phase == 5) {
      deposit.translateY(-63);
      ribbon.translateY(40);
      parts = {deposit, ribbon, letters};
    } else {
      parts = {letters, ribbon, deposit};
    }
    // The same marks faded separately and after their overlap.
    const auto quarter = emitted(4);
    parts.push_back(
        compose::kit::at(compose::stack(), 42, 552, 210, 22)
            .children(
                {compose::kit::at(0, 0, 150, 18).fill(quarter).opacity(.25f),
                 compose::kit::at(60, 4, 150, 18)
                     .fill(quarter)
                     .opacity(.25f)}));
    parts.push_back(
        compose::kit::at(compose::stack(), 287, 552, 210, 22)
            .opacity(.25f)
            .children({compose::kit::at(0, 0, 150, 18).fill(quarter),
                       compose::kit::at(60, 4, 150, 18).fill(quarter)}));
    if (value.phase == 6) {
      parts.push_back(
          compose::kit::at(726, 284, 244, 246)
              .borderRadius(18)
              .overflow(compose::Overflow::Clip)
              .backdropFilter(material::Filter::glass(
                  {.ior = 1.5f,
                   .thickness = 12,
                   .sampleRadius = 24,
                   .normal = material::shader(kBrushed)}))
              .fill(material::Color{.16f, .24f, .25f, .18f})
              .stroke(compose::stroke(
                  1.5f, compose::Fill::color({.5f, .75f, .8f, .6f}))));
    }
    parts.push_back(
        compose::light(material::studio({.color = warm,
                                         .intensity = .16f,
                                         .ambient = .09f,
                                         .kind = material::LightKind::Point,
                                         .position = {0, 0, 360},
                                         .range = 1500}))
            .key("warm-key")
            .translateX(keyX)
            .translateY(keyY));
    return compose::kit::at(compose::stack(), 352, 184, 1032, 604)
        .key("material-panel")
        .fill(ground)
        .borderRadius(9)
        .overflow(compose::Overflow::Clip)
        .cache(compose::Cache::Texture)
        .children(parts);
  }

  Element describe(const luminous_layers::State& value) const {
    using namespace luminous_layers;
    return compose::scene()
        .width(kWidth)
        .height(kHeight)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .environment(material::environment(
            material::shader(kRoom), {.intensity = .24f, .size = {512, 256}}))
        .children({hero(value), calibration(),
                   compose::light(
                       material::studio({.color = cool,
                                         .intensity = .09f,
                                         .ambient = .06f,
                                         .kind = material::LightKind::Point,
                                         .position = {0, 0, 420},
                                         .range = 1500}))
                       .key("cool-fill")
                       .translateX(fillX)
                       .translateY(fillY)});
  }

  void drawRaw(SkCanvas& canvas, float density = 1) {
    using namespace luminous_layers;
    if (!rawComposer || !fonts) return;
    density = bounded(density, .125f, 4, 1);
    if (drewRaw && (rawRecorder != canvas.recorder() ||
                    rawRecordingContext != canvas.recordingContext())) {
      rawComposer->purgeCaches();
      coverage = std::make_unique<draw::Graphics>(964, 128);
      described.reset();
    }
    rawRecorder = canvas.recorder();
    rawRecordingContext = canvas.recordingContext();
    drewRaw = true;
    SkAutoCanvasRestore restore(&canvas, true);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.scale(density, density);
    draw::on(
        canvas, SkSize{float(kWidth), float(kHeight)},
        [this](draw::Pen& pen) { prepareCoverage(pen); }, fonts);
    const auto value = state();
    if (!described || *described != value) {
      rawComposer->render(describe(value));
      described = value;
    }
    rawComposer->draw(canvas);
    prepareTextureWitnesses(canvas.imageInfo(), density);
  }

  std::vector<Sample> readCalibration(SkSurface& surface,
                                      float density = 1) const {
    using namespace luminous_layers;
    std::vector<Sample> samples;
    if (surface.recorder()) return samples;
    density = bounded(density, .125f, 4, 1);
    const auto info =
        SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
    for (int column = 0; column < 4; ++column)
      for (int row = 0; row < 5; ++row) {
        Sample sample;
        sample.x = 477 + column * 182.f;
        sample.y = 864 + row * 24.f;
        sample.gain = kGains[column];
        sample.opacity = kOpacities[row];
        const float expected = sample.gain * sample.opacity;
        sample.expectedRGBA = {expected, expected, expected, sample.opacity};
        if (!surface.readPixels(info, sample.premulRGBA.data(),
                                sizeof(float) * 4,
                                int(std::floor(sample.x * density)),
                                int(std::floor(sample.y * density))))
          return {};
        samples.push_back(sample);
      }
    return samples;
  }

  material::Material view(sk_sp<SkImage> image, glm::vec2 origin,
                          float density) const {
    return view(material::Texture(std::move(image)), origin, density);
  }

  material::Material view(material::Texture texture, glm::vec2 origin,
                          float density) const {
    struct Parameters {
      glm::vec2 origin;
      float density;
      float exposure;
      float shoulder;
    };
    return material::shader(
               luminous_layers::kView,
               Parameters{origin, density,
                          luminous_layers::bounded(controls.exposure, -3, 3, 0),
                          controls.shoulder ? 1.f : 0.f},
               {.textures = {{"raw", {}}}})
        .slot("raw", material::image(std::move(texture)));
  }

  Element interface() const {
    using namespace luminous_layers;
    const auto value = state();
    std::array<char, 80> values{};
    std::snprintf(values.data(), values.size(), "GAIN %.2f / ALPHA %.2f",
                  currentGain, currentOpacity);
    std::vector<Element> parts{
        label("Light on the mark.", 56, 41, 990, 54, kInk)
            .fontFamily("Georgia, Times New Roman, serif"),
        label("COMPOSE / MATERIAL LETTERING\nBRUSHES / LAYERS / FLOAT IMAGE",
              1112, 56, 272, 11, kInk),
        rule(56, 137, 1328, kWarm),
        label("A / WHITE BRUSH DATA", 56, 185, 240, 10, kInk),
        compose::kit::at(56, 214, 240, 94)
            .fill(pixels(coverage->image(), {240, 94}))
            .borderRadius(3),
        label("Coverage and height share\none bounded painted field.", 56, 322,
              240, 12),
        label("LIVE EMISSION / GAIN", 56, 380, 240, 10, kInk),
        label("MARK OPACITY", 56, 434, 240, 10, kInk),
        label("DISPLAY EXPOSURE / STOPS", 56, 488, 240, 10, kInk),
        button("SHOULDER", 56, 554, 114, controls.shoulder),
        button("CLIP", 182, 554, 114, !controls.shoulder),
        button("DISPLAY BLOOM", 56, 600, 114, controls.bloom),
        button("DEPTH", 182, 600, 114, controls.depth),
        button(heldPhase < 0 ? "STATE / AUTO" : "STATE / HELD", 56, 648, 240,
               heldPhase < 0),
        button("PREVIOUS", 56, 692, 114, false),
        button("NEXT", 182, 692, 114, false),
        label(values.data(), 56, 749, 240, 11, kInk),
        label(rasterFallback ? "F16 / RASTER FALLBACK" : "F16 / HOST BACKEND",
              56, 781, 240, 10, kInk),
        label("RETAINED IMAGE / FIXED GAIN × 4", 56, 824, 240, 9, kInk),
        label("Raster images / same display view", 56, 980, 240, 9),
        label("EMISSIVE ALLOY / LUMINOUS TYPE", 392, 207, 810, 10, kRule),
        label("PRESSURE / 0.5 → 34 → 0.5 PX", 392, 433, 860, 10, kRule),
        label("PAINT / BRISTLES + SHARED HEIGHT", 392, 554, 860, 10, kRule),
        label("PER MARK / .25", 394, 726, 210, 10, kRule),
        label("AFTER OVERLAP / .25", 639, 726, 270, 10, kRule),
        label(kPhases[value.phase], 352, 796, 1010, 11, kInk),
        label("B / ISOLATED WHITE EMISSION / GAIN × COVERAGE", 352, 828, 940,
              10, kInk),
        label("AA / O8 COUNTERS", 1146, 828, 178, 10, kInk),
        rule(352, 1006, 1032, kWarm),
        label("FLOAT WORKING RGB / RAW PROBES PRECEDE EXPOSURE, SHOULDER "
              "AND DISPLAY BLOOM",
              352, 1022, 1032, 10, kInk),
        label("PLANAR NORMALS / AUTHORED LIGHTS / NO EMISSIVE LIGHT TRANSPORT",
              352, 1043, 1032, 9)};
    const std::array<float, 3> levels{
        currentGain / 16, currentOpacity,
        (bounded(controls.exposure, -3, 3, 0) + 3) / 6};
    for (int i = 0; i < 3; ++i) {
      const float y = 406 + i * 54.f;
      parts.push_back(compose::kit::at(56, y, 240, 4).fill(kRule));
      parts.push_back(compose::kit::at(56, y, std::max(1.f, levels[i] * 240), 4)
                          .fill(i == 1 ? kCool : kWarm));
      parts.push_back(compose::kit::dot({56 + levels[i] * 240, y + 2}, 5,
                                        compose::Fill::color(kInk)));
    }
    for (int column = 0; column < 4; ++column) {
      std::array<char, 24> caption{};
      std::snprintf(caption.data(), caption.size(), "× %.0f", kGains[column]);
      parts.push_back(
          label(caption.data(), 398 + column * 182.f, 982, 158, 10, kInk));
    }
    for (int row = 0; row < 5; ++row) {
      std::array<char, 24> caption{};
      std::snprintf(caption.data(), caption.size(), "%.2f", kOpacities[row]);
      parts.push_back(label(caption.data(), 352, 856 + row * 24.f, 40, 9));
    }
    if (value.phase == 6)
      parts.push_back(label("GLASS / 2D REFRACTION", 1094, 494, 218, 9, kRule));
    for (size_t i = 0; i < kTextureNames.size(); ++i)
      parts.push_back(label(kTextureNames[i], 56,
                            kTextureY + i * kTextureStride + 10, 44, 10, kInk));
    parts.push_back(
        compose::kit::at(compose::stack(), 352, 184, 1032, 604)
            .children({compose::kit::ring(
                           {0, 0}, 13,
                           compose::stroke(1, compose::Fill::color(kWarm)))
                           .translateX(keyX)
                           .translateY(keyY)}));
    parts.push_back(
        compose::kit::ring({0, 0}, 13,
                           compose::stroke(1, compose::Fill::color(kCool)))
            .translateX(fillX)
            .translateY(fillY));
    return compose::stack()
        .width(kWidth)
        .height(kHeight)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .children(parts);
  }

  void handlePointer(const draw::Pen& pen) {
    using namespace luminous_layers;
    const float x = pen.mouseX, y = pen.mouseY;
    if (pen.mouseIsPressed && x >= 56 && x <= 296) {
      const float u = std::clamp((x - 56) / 240, 0.f, 1.f);
      if (y >= 394 && y <= 420) {
        controls.gain = u * 16;
        gainOverride = true;
      } else if (y >= 448 && y <= 474) {
        controls.opacity = u;
        opacityOverride = true;
      } else if (y >= 502 && y <= 528) {
        controls.exposure = u * 6 - 3;
      }
      if (!pointerDown) {
        if (y >= 554 && y <= 586) controls.shoulder = x < 176;
        if (y >= 600 && y <= 632) {
          if (x < 176)
            controls.bloom = !controls.bloom;
          else
            controls.depth = !controls.depth;
        }
        if (y >= 648 && y <= 680) {
          heldPhase = heldPhase < 0 ? state().phase : -1;
          if (heldPhase < 0) {
            gainOverride = false;
            opacityOverride = false;
            controls.gain = 1.4f;
            controls.opacity = 1;
          }
        }
        if (y >= 692 && y <= 724)
          heldPhase = (state().phase + (x < 176 ? 7 : 1)) % 8;
      }
    }
    pointerDown = pen.mouseIsPressed;
    applyTime(seconds);
  }

  void paint(draw::Pen& pen) {
    using namespace luminous_layers;
    handlePointer(pen);
    auto* canvas = pen.canvas();
    if (!canvas || !rawComposer) return;
    const float density = bounded(pen.contentScale(), .125f, 4, 1);
    const auto info = SkImageInfo::Make(
        std::max(1, int(std::round(kWidth * density))),
        std::max(1, int(std::round(kHeight * density))), kRGBA_F16_SkColorType,
        kPremul_SkAlphaType, canvas->imageInfo().refColorSpace());
    if (!working || working->imageInfo() != info ||
        recorder != canvas->recorder() ||
        recordingContext != canvas->recordingContext()) {
      recorder = canvas->recorder();
      recordingContext = canvas->recordingContext();
      working = canvas->makeSurface(info);
      rasterFallback = !working;
      if (!working) working = SkSurfaces::Raster(info);
      rawComposer->purgeCaches();
    }
    if (!working) return;
    drawRaw(*working->getCanvas(), density);
    const auto image = working->makeImageSnapshot();
    auto heroView = compose::kit::at(352, 184, 1032, 604)
                        .fill(view(image, {352, 184}, density));
    if (controls.bloom) heroView.filter(displayBloom);
    if (state().phase == 6)
      heroView.borderRadius(9).overflow(compose::Overflow::Clip);
    auto page =
        compose::stack().width(kWidth).height(kHeight).fill(kPaper).children(
            {heroView,
             compose::kit::at(352, 816, 1032, 190)
                 .fill(view(image, {352, 816}, density)),
             interface()});
    for (size_t i = 0; i < textureWitnesses.size(); ++i)
      page.children(
          {compose::kit::at(kTextureX, kTextureY + i * kTextureStride,
                            kTextureWidth, kTextureHeight)
               .fill(view(textureWitnesses[i]->texture(), {0, 0}, density))});
    pen.element(page, 0, 0, kWidth, kHeight);
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(luminous_layers::kWidth, luminous_layers::kHeight);
    ctx.background(luminous_layers::kPaper);
    ctx.captureAt(2);
    ctx.paintDiscardedFrames(false);
    if (!ctx.fonts) return;
    initialize(ctx.engine, *ctx.fonts);
    ctx.composer.render(compose::pen(
                            "luminous-layers",
                            [this](draw::Pen& pen) { paint(pen); },
                            compose::Cache::None)
                            .width(luminous_layers::kWidth)
                            .height(luminous_layers::kHeight));
  }

  void update(double elapsed) { applyTime(elapsed); }
};

SIGIL_SKETCH(LuminousLayers, "Study · Materials",
             "Luminous material type, pressure ribbons and painted deposits "
             "with float working pixels and an authored display view.")
