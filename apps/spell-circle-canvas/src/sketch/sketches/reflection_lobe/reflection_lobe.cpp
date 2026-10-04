/** Curved metal specimens and material lettering under a shared studio. */
// TAGS: Compose/Materials, Materials/Metal, Materials/Lighting,
// Typography/Material ink, Brushes/Painted data

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Stroke.h>
#include <sigildraw/brush/Tool.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace compose = sigil::compose;
namespace draw = sigil::draw;
namespace brush = sigil::draw::brush;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;

namespace reflection_lobe {

using compose::Element;
using material::hexColor;
constexpr int kWidth = 1440, kHeight = 1080;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kLoop = 36;
constexpr auto kPaper = hexColor(0xeee9de);
constexpr auto kInk = hexColor(0x293c37);
constexpr auto kMuted = hexColor(0x68786e);
constexpr auto kRule = hexColor(0xbfc8bb);
constexpr auto kWarm = hexColor(0xb6683d);
constexpr auto kCool = hexColor(0x2c7588);
constexpr auto kSteel = hexColor(0xbfcdd0);
constexpr std::array<float, 5> kRoughness{.03f, .2f, .45f, .75f, 1};
constexpr std::array<std::string_view, 9> kNames{
    "01 / STRIP REFLECTION",  "02 / ROUGHNESS ZERO", "03 / ROUGHNESS ONE",
    "04 / SPATIAL ROUGHNESS", "05 / CLEAR COATING",  "06 / SEAM LEFT",
    "07 / SEAM RIGHT",        "08 / POLAR CAPS",     "09 / CONSTANT SKY"};

struct Controls {
  float roughness = .2f;
  float coating = 0;
  float brushDepth = 7;
  float letterDepth = 1.4f;
  float environmentIntensity = .45f;
  float lightSpeed = 1;
};

struct State {
  int phase = 0;
  float roughness = .2f;
  float coating = 0;
  float rotation = 0;
  float sourceMode = 0;
  float polar = 0;
  float roughnessMode = 0;
  bool map = true;
};

// The panorama is a directional source, without finite emitter geometry.
inline constexpr std::string_view kEnvironment = R"(
half4 main(float2 p) {
  float2 uv = p / float2(512, 256);
  float a = (uv.x - .5) * 6.2831853;
  float latitude = clamp(uv.y, 0, 1) * 3.14159265;
  float3 d = float3(sin(latitude) * sin(a), cos(latitude),
                    -sin(latitude) * cos(a));
  if (mode > 1.5) return half4(.3, .3, .3, 1);
  float3 room = float3(.24, .28, .32);
  if (mode > .5) {
    float north = exp(-(1 - d.y) / .014);
    float south = exp(-(1 + d.y) / .014);
    return half4(room + north * float3(1.5, 1.25, .8)
                     + south * float3(.65, 1.2, 1.5), 1);
  }
  float across = d.x / .095;
  float strip = exp(-across * across)
              * (1 - smoothstep(.64, .70, abs(d.y)))
              * step(0, d.z);
  return half4(room + strip * float3(1.45, 1.45, 1.45), 1);
})";

// A normal ramp provides an angular domain on a planar Compose surface.
inline constexpr std::string_view kNormal = R"(
half4 main(float2 p) {
  float coordinate = polar > .5 ? p.y / extent.y : p.x / extent.x;
  float a = (coordinate - .5) * span + polar * .78539816;
  float3 n = polar > .5 ? float3(0, sin(a), cos(a))
                        : float3(sin(a), 0, cos(a));
  return half4(n * .5 + .5, 1);
})";

inline constexpr std::string_view kRoughnessMap = R"(
half4 main(float2 p) {
  float u = clamp(p.x / width, 0, 1);
  float r = mode > .5 ? mix(.03, .75, step(.5, u)) : mix(.03, 1, u);
  return half4(float3(r), 1);
})";

inline constexpr std::string_view kBrushing = R"(
half4 main(float2 p) {
  float slope = .045 * cos(p.y * 2.8) + .012 * cos(p.y * .43);
  float3 n = normalize(float3(0, slope, 1));
  return half4(n * .5 + .5, 1);
})";

inline constexpr std::string_view kBrushCoverage = R"(
half4 main(float2 p) {
  float coverage = clamp(height.eval(p).r, 0, 1);
  return half4(color.rgb * coverage, coverage);
})";

float bounded(float value, float low, float high, float fallback) {
  return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}

Element label(std::string_view words, float x, float y, float width,
              float size = 12, material::Material ink = kMuted) {
  const int lines = 1 + std::count(words.begin(), words.end(), '\n');
  return compose::kit::at(compose::text(std::string(words)), x, y, width,
                          size * 1.55f * lines)
      .fontSize(size)
      .ink(std::move(ink));
}

Element rule(float x, float y, float width, material::Color color = kRule) {
  return compose::kit::at(
      compose::kit::line(
          {.length = width, .fill = compose::Fill::color(color)}),
      x, y, width, 1);
}

material::Material normal(glm::vec2 extent, float span = 2.7f,
                          float polar = 0) {
  struct Parameters {
    glm::vec2 extent;
    float span;
    float polar;
  };
  return material::shader(kNormal, Parameters{extent, span, polar});
}

material::Material roughnessMap(float width, float mode) {
  struct Parameters {
    float width;
    float mode;
  };
  return material::shader(kRoughnessMap, Parameters{width, mode});
}

material::Material metal(material::Channel roughness,
                         material::Material normalMap, float coating = 0,
                         material::Material base = kSteel) {
  return base.surface({.metallic = 1.0f,
                       .roughness = std::move(roughness),
                       .normal = std::move(normalMap),
                       .normalDirectX = false,
                       .clearcoat = coating,
                       .reflectionWeight = 1});
}

Element button(std::string_view words, float x, float y, float width,
               bool active) {
  return compose::kit::at(compose::stack(), x, y, width, 32)
      .borderRadius(3)
      .fill(active ? kInk : kPaper)
      .stroke(compose::stroke(1, compose::Fill::color(kRule)))
      .children({label(words, 10, 8, width - 20, 10, active ? kPaper : kInk)});
}

material::Texture placed(sk_sp<SkImage> image, glm::vec2 extent) {
  material::Texture out(std::move(image));
  const glm::vec2 pixels(out.size());
  if (pixels.x > 0 && pixels.y > 0) {
    glm::mat3 placement(1);
    placement[0][0] = extent.x / pixels.x;
    placement[1][1] = extent.y / pixels.y;
    out.uv(placement);
  }
  return out;
}

}  // namespace reflection_lobe

struct ReflectionLobe {
  using State = reflection_lobe::State;
  using Element = compose::Element;
  reflection_lobe::Controls controls;
  int heldPhase = -1;
  int coatingOverride = -1, mapOverride = -1;
  float roughnessOverride = -1;
  float seconds = 0;
  bool pointerDown = false, painted = false;
  draw::Graphics brushHeight{1024, 128};
  motion::Animatable<float> keyX = motion::animatable(400.f);
  motion::Animatable<float> keyY = motion::animatable(160.f);
  motion::Animatable<float> fillX = motion::animatable(1100.f);
  motion::Animatable<float> fillY = motion::animatable(390.f);
  compose::Shape ribbon = shapes::svg(
      "M0 45 C17 1 31 80 49 33 C66 0 80 77 100 25 "
      "L100 63 C81 100 65 38 49 71 C32 100 17 39 0 79 Z");

  State state() const {
    using namespace reflection_lobe;
    State value;
    value.phase = heldPhase < 0 ? int(std::floor(std::fmod(seconds, kLoop) / 4))
                                : std::clamp(heldPhase, 0, 8);
    value.roughness = bounded(controls.roughness, 0.f, 1.f, .2f);
    value.coating = bounded(controls.coating, 0.f, 1.f, 0.f);
    switch (value.phase) {
      case 1:
        value.roughness = 0;
        value.map = false;
        break;
      case 2:
        value.roughness = 1;
        value.map = false;
        break;
      case 3:
        value.roughnessMode = 1;
        break;
      case 4:
        value.coating = 1;
        break;
      case 5:
        value.rotation = -.35f;
        break;
      case 6:
        value.rotation = .35f;
        break;
      case 7:
        value.sourceMode = 1;
        value.polar = 1;
        break;
      case 8:
        value.sourceMode = 2;
        break;
      default:
        break;
    }
    if (roughnessOverride >= 0) value.roughness = roughnessOverride;
    if (coatingOverride >= 0) value.coating = float(coatingOverride);
    if (mapOverride >= 0) value.map = mapOverride != 0;
    return value;
  }

  material::Material source(const State& value) const {
    struct Parameters {
      float mode;
    };
    return material::shader(reflection_lobe::kEnvironment,
                            Parameters{value.sourceMode});
  }

  material::Environment environment(const State& value) const {
    return material::environment(
        source(value), {.rotation = value.rotation,
                        .intensity = reflection_lobe::bounded(
                            controls.environmentIntensity, 0, 2, .45f),
                        .size = {512, 256}});
  }

  material::Material sourcePreview(const State& value) const {
    struct Parameters {
      glm::vec2 scale;
      float intensity;
    };
    return material::shader(
               "half4 main(float2 p) { float2 uv = p * scale; "
               "uv.x = mod(uv.x + 256, 512); half4 s = source.eval(uv); "
               "return half4(s.rgb * half(intensity), s.a); }",
               Parameters{{512.f / 218, 256.f / 109},
                          reflection_lobe::bounded(
                              controls.environmentIntensity, 0, 2, .45f)},
               {.textures = {{"source", {}}}})
        .slot("source", source(value));
  }

  void paintInput(draw::Pen& pen) {
    using namespace reflection_lobe;
    const auto extent =
        SkISize::Make(std::max(1, int(std::round(1024 * pen.contentScale()))),
                      std::max(1, int(std::round(128 * pen.contentScale()))));
    if (painted && brushHeight.extent() == extent) return;
    auto& p = brushHeight.begin(pen);
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
    brushHeight.end();
    painted = true;
  }

  Element calibration(const State& value) const {
    using namespace reflection_lobe;
    // A zero-ambient source suppresses the environment-only ambient share.
    Element row =
        compose::kit::at(compose::scene(), 320, 612, 1064, 206)
            .environment(environment(value))
            .children({compose::light(
                           material::studio({.intensity = 0, .ambient = 0}))
                           .key("reflection-only")});
    for (int i = 0; i < 5; ++i) {
      std::array<char, 32> caption{};
      std::snprintf(caption.data(), caption.size(), "ROUGHNESS %.2f",
                    kRoughness[i]);
      row.children(
          {compose::kit::at(i * 216.f, 0, 200, 174)
               .fill(metal(kRoughness[i],
                           normal({200, 174}, value.polar > .5f ? 1.3f : 2.7f,
                                  value.polar)))
               .borderRadius(5)
               .overflow(compose::Overflow::Clip)
               .stroke(compose::stroke(1, compose::Fill::color(kRule))),
           label(caption.data(), i * 216.f, 185, 200, 10, kInk)});
    }
    return row;
  }

  Element sidebar(const State& value) const {
    using namespace reflection_lobe;
    const auto paintedHeight =
        material::image(placed(brushHeight.image(), {218, 75}));
    std::array<char, 40> readout{};
    std::snprintf(readout.data(), readout.size(), "BODY ROUGHNESS / %.2f",
                  value.roughness);
    Element out = compose::stack().children(
        {label("STUDIO / FRONT-CENTERED PANORAMA", 56, 181, 218, 10, kInk),
         compose::kit::at(56, 204, 218, 109)
             .fill(sourcePreview(value))
             .borderRadius(3)
             .stroke(compose::stroke(1, compose::Fill::color(kRule))),
         label("One fixed radiance and exposure.", 56, 324, 218, 10),
         label("A / PAINTED BRISTLE HEIGHT", 56, 359, 218, 10, kInk),
         compose::kit::at(56, 383, 218, 75).fill(paintedHeight).borderRadius(3),
         label("B / SPATIAL ROUGHNESS", 56, 487, 218, 10, kInk),
         compose::kit::at(56, 511, 218, 44)
             .fill(roughnessMap(218, value.roughnessMode))
             .borderRadius(3),
         rule(56, 582, 218), label(readout.data(), 56, 603, 218, 10, kInk),
         compose::kit::at(56, 632, 218, 5).fill(kRule),
         compose::kit::at(56, 632, std::max(1.f, value.roughness * 218), 5)
             .fill(kWarm),
         button(value.coating > .5f ? "COAT / ON" : "COAT / OFF", 56, 660, 104,
                value.coating > .5f),
         button(value.map ? "MAP / ON" : "MAP / OFF", 170, 660, 104, value.map),
         button(heldPhase < 0 ? "STATE / AUTO" : "STATE / HELD", 56, 708, 218,
                heldPhase < 0),
         button("PREVIOUS", 56, 752, 104, false),
         button("NEXT", 170, 752, 104, false),
         label("The five specimens isolate\nreflection: no direct highlight,\n"
               "ambient share or coating.",
               56, 816, 218, 12, kInk),
         label("Sharpness should change\nwith roughness. Brightness\n"
               "alone cannot establish spread.",
               56, 911, 218, 12),
         label("RELIEF / BRUSH / LIGHT / RADIANCE", 56, 1020, 218, 9)});
    return out;
  }

  Element hero(const State& value) const {
    using namespace reflection_lobe;
    const auto finishNormal = material::surface::blendNormals(
        normal({1064, 320}, .34f), material::shader(kBrushing));
    const auto finish = metal(value.roughness, finishNormal, value.coating);
    const auto height =
        material::image(placed(brushHeight.image(), {964, 116}));
    const auto brushNormal = material::surface::normalFromHeight(
        height,
        {.depth = bounded(controls.brushDepth, -20, 20, 7), .step = .8f});
    struct Coverage {
      material::Color color;
    };
    const auto coverage = material::shader(kBrushCoverage, Coverage{kSteel},
                                           {.textures = {{"height", {}}}})
                              .slot("height", height);
    const auto brushFinish =
        metal(value.roughness,
              material::surface::blendNormals(brushNormal,
                                              material::shader(kBrushing)),
              value.coating, coverage);
    Element out = compose::kit::at(compose::stack(), 320, 180, 1064, 320)
                      .fill(finish)
                      .borderRadius(7)
                      .overflow(compose::Overflow::Clip)
                      .cache(compose::Cache::Texture)
                      .stroke(compose::stroke(1, compose::Fill::color(kRule)));
    out.children(
        {label("MATERIAL / LETTERS + BRISTLES + RIBBON", 32, 22, 920, 10,
               kRule),
         label("SATIN / 05", 29, 48, 908, 86, material::Color{0, 0, 0, 0})
             .fontFamily("Georgia, Times New Roman, serif")
             .decorationOutline(compose::Boundary::Glyphs)
             .foreground(compose::relief(
                 finish,
                 {.shoulder = 2.4f,
                  .depth = bounded(controls.letterDepth, -4, 4, 1.4f)})),
         label("05", 974, 61, 66, 42, finish),
         compose::kit::at(50, 160, 964, 116).fill(brushFinish),
         compose::kit::at(44, 225, 974, 74)
             .shape(ribbon)
             .background(compose::relief(
                 metal(value.roughness, normal({974, 74}, .24f), value.coating),
                 {.shoulder = 2.5f, .depth = 1.1f})),
         compose::light(
             material::studio({.color = material::Color{1, .63f, .35f, 1},
                               .intensity = .12f,
                               .ambient = .1f,
                               .kind = material::LightKind::Point,
                               .position = {0, 0, 280},
                               .range = 1500}))
             .key("warm-key")
             .translateX(keyX)
             .translateY(keyY),
         compose::kit::ring({0, 0}, 16,
                            compose::stroke(1, compose::Fill::color(kWarm)))
             .translateX(keyX)
             .translateY(keyY)});
    return out;
  }

  Element describe(const State& value) const {
    using namespace reflection_lobe;
    const auto map =
        value.map ? material::Channel(roughnessMap(704, value.roughnessMode))
                  : material::Channel(value.roughness);
    Element lower =
        compose::kit::at(compose::scene(), 320, 858, 1064, 153)
            .environment(environment(value))
            .children(
                {compose::light(
                     material::studio({.intensity = 0, .ambient = 0}))
                     .key("map-reflection-only"),
                 compose::kit::at(0, 0, 704, 118)
                     .fill(metal(
                         map,
                         normal({704, 118}, value.polar > .5f ? 1.3f : 2.7f,
                                value.polar),
                         value.coating))
                     .borderRadius(5)
                     .stroke(compose::stroke(1, compose::Fill::color(kRule))),
                 label(value.map ? "PER-PIXEL ROUGHNESS / SAME NORMAL RAMP"
                                 : "SCALAR ROUGHNESS / SAME NORMAL RAMP",
                       0, 130, 704, 10),
                 compose::kit::at(736, 0, 156, 118)
                     .fill(metal(.75f, normal({156, 118}, 2.7f), 0))
                     .borderRadius(5),
                 compose::kit::at(908, 0, 156, 118)
                     .fill(metal(.75f, normal({156, 118}, 2.7f), 1))
                     .borderRadius(5),
                 label("COATING / 0", 736, 130, 156, 10),
                 label("COATING / 1", 908, 130, 156, 10)});
    Element page =
        compose::stack()
            .width(kWidth)
            .height(kHeight)
            .fill(kPaper)
            .fontFamily("Inter, Helvetica Neue, sans-serif")
            .children(
                {label("REFLECTION / 05", 56, 35, 1040, 58, kInk)
                     .fontFamily("Georgia, Times New Roman, serif"),
                 label("COMPOSE MATERIALS\nPOLISHED / SATIN / ROUGH", 1112, 52,
                       272, 11, kInk),
                 rule(56, 133, 1328, kWarm), sidebar(value), hero(value),
                 label(kNames[value.phase], 320, 526, 840, 12, kInk),
                 label(value.sourceMode > 1.5f  ? "FIVE EQUAL NORMAL RAMPS / "
                                                  "CONSTANT SKY / BASE RESPONSE"
                       : value.sourceMode > .5f ? "FIVE EQUAL NORMAL RAMPS / "
                                                  "POLAR CAPS / BASE RESPONSE"
                                                : "FIVE EQUAL NORMAL RAMPS / "
                                                  "FIXED STRIP / BASE RESPONSE",
                       320, 562, 1064, 11),
                 calibration(value),
                 label("A finish spreads the reflection.", 320, 824, 1044, 23,
                       kInk)
                     .fontFamily("Georgia, Times New Roman, serif"),
                 lower, rule(320, 1035, 1064, kWarm),
                 label("CURVED NORMALS / REAL GLYPH COUNTERS / PAINTED HEIGHT "
                       "/ SCOPED LIGHTS",
                       320, 1049, 1064, 10, kInk),
                 compose::kit::ring(
                     {0, 0}, 16,
                     compose::stroke(1, compose::Fill::color(kCool)))
                     .translateX(fillX)
                     .translateY(fillY)});
    return compose::scene()
        .width(kWidth)
        .height(kHeight)
        .environment(environment(value))
        .children({page, compose::light(
                             material::studio(
                                 {.color = material::Color{.35f, .7f, 1, 1},
                                  .intensity = .08f,
                                  .ambient = .08f,
                                  .kind = material::LightKind::Point,
                                  .position = {0, 0, 320},
                                  .range = 1500}))
                             .key("cool-fill")
                             .translateX(fillX)
                             .translateY(fillY)});
  }

  void handlePointer(const draw::Pen& pen) {
    using namespace reflection_lobe;
    const float x = pen.mouseX, y = pen.mouseY;
    const State value = state();
    if (pen.mouseIsPressed && x >= 56 && x <= 274 && y >= 620 && y <= 644)
      roughnessOverride = std::clamp((x - 56) / 218, 0.f, 1.f);
    if (pen.mouseIsPressed && !pointerDown && x >= 56 && x <= 274) {
      if (y >= 660 && y <= 692) {
        if (x <= 160)
          coatingOverride = value.coating > .5f ? 0 : 1;
        else if (x >= 170)
          mapOverride = value.map ? 0 : 1;
      } else if (y >= 708 && y <= 740) {
        heldPhase = heldPhase < 0 ? value.phase : -1;
        if (heldPhase < 0) {
          coatingOverride = -1;
          mapOverride = -1;
          roughnessOverride = -1;
        }
      } else if (y >= 752 && y <= 784) {
        if (x <= 160)
          heldPhase = (value.phase + 8) % 9;
        else if (x >= 170)
          heldPhase = (value.phase + 1) % 9;
      }
    }
    pointerDown = pen.mouseIsPressed;
  }

  void paint(draw::Pen& pen) {
    handlePointer(pen);
    paintInput(pen);
    pen.element(describe(state()), 0, 0, reflection_lobe::kWidth,
                reflection_lobe::kHeight);
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(reflection_lobe::kWidth, reflection_lobe::kHeight);
    ctx.background(reflection_lobe::kPaper);
    ctx.captureAt(2);
    // The final draw reconstructs the bristle input at its actual density.
    ctx.paintDiscardedFrames(false);
    ctx.composer.render(compose::pen(
                            "reflection-lobe",
                            [this](draw::Pen& pen) { paint(pen); },
                            compose::Cache::None)
                            .width(reflection_lobe::kWidth)
                            .height(reflection_lobe::kHeight));
  }

  void update(double elapsed) {
    using namespace reflection_lobe;
    seconds = std::isfinite(elapsed)
                  ? float(std::fmod(std::max(0., elapsed), double(kLoop)))
                  : 0;
    const float t = seconds * bounded(controls.lightSpeed, -4, 4, 1);
    keyX = 480 + 245 * std::sin(t * 2 * kPi / kLoop);
    keyY = 150 + 25 * std::cos(t * 4 * kPi / kLoop);
    fillX = 1120 - 180 * std::sin(t * 2 * kPi / kLoop + .8f);
    fillY = 390 + 40 * std::cos(t * 4 * kPi / kLoop);
  }
};

SIGIL_SKETCH(ReflectionLobe, "Study · Materials",
             "Curved metal, material letters and painted bristles under a "
             "shared studio.")
