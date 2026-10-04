/** @file
 * Metallic linework — a silver and copper routing instrument.
 * EDIT THESE FIRST: Controls, heldPhase, circuit(), and finish().
 */
// TAGS: Materials/Metal, Materials/Lighting, Drawing/Brushes,
// Geometry/Lines, Typography/Material ink, Composition/Layers

#include <include/core/SkPathUtils.h>
#include <sigilcompose/brush/Brush.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Through.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
using namespace sigil::compose;

namespace {

constexpr material::Color kGround{0.025f, 0.030f, 0.033f, 1};
constexpr material::Color kInk{0.78f, 0.83f, 0.83f, 1};
constexpr material::Color kMuted{0.42f, 0.49f, 0.49f, 1};
constexpr material::Color kRule{0.14f, 0.20f, 0.20f, 1};
constexpr material::Color kAmber{0.83f, 0.52f, 0.26f, 1};
constexpr material::Color kTeal{0.32f, 0.66f, 0.61f, 1};
constexpr std::array kPhases{
    "CONDUCTOR / NOMINAL",     "PIGMENT / METALLIC ZERO",
    "POLISH / ROUGHNESS ZERO", "OXIDE / ROUGHNESS ONE",
    "GRAZE / KEY SIX DEGREES", "OVERPRINT / HALF OPACITY",
    "KEY / LIVE RGB SWEEP",    "KEY / HDR WHITE FOUR",
    "KEY / BLACK DIRECT"};

struct Controls {
  float metallic = 1;
  float roughness = 0.25f;
  float normalStrength = 0.24f;
  float depth = 0.85f;
  float shoulder = 1.6f;
  float elevation = 32;
  float overlay = 0.82f;
};

struct Grain {
  float strength = 0.24f;
};
constexpr std::string_view kNormal = R"(
half4 main(float2 p) {
  float step = max(0.65, 1.0 / max(uContentScale, 0.125));
  float phase = p.y * 1.9 + sin(p.x * 0.031) * 0.5;
  float detail = sin(phase) * exp(-step * 0.17);
  float3 n = normalize(float3(0.05 * strength * sin(p.x * 0.23),
                              strength * detail, 1));
  return half4(half3(n * 0.5 + 0.5), 1);
})";
constexpr std::string_view kPlatingMask = R"(
half4 main(float2 p) {
  float cell = fract((p.x + p.y * 0.32) / 77.0);
  float band = smoothstep(0.10, 0.13, cell) *
               (1.0 - smoothstep(0.68, 0.71, cell));
  float scuff = 0.70 + 0.30 * sin(p.x * 0.013 + sin(p.y * 0.042));
  return half4(half3(band * scuff), 1);
})";
constexpr std::string_view kEnvironment = R"(
half4 main(float2 p) {
  float2 uv = p / float2(512, 256);
  float longitude = uv.x * 6.2831853;
  float latitude = (uv.y - 0.5) * 3.14159265;
  float coolX = sin((longitude - 1.2) * 0.5) / 0.36;
  float coolY = (latitude + 0.25) / 0.74;
  float warmX = sin((longitude - 4.4) * 0.5) / 0.32;
  float warmY = (latitude - 0.18) / 0.55;
  float stripeY = (latitude - 0.11) / 0.06;
  float cool = exp(-coolX * coolX - coolY * coolY);
  float warm = exp(-warmX * warmX - warmY * warmY);
  float stripe = exp(-stripeY * stripeY);
  float3 color = float3(0.41, 0.45, 0.48) +
      float3(1.3, 1.5, 1.65) * cool + float3(1.2, 0.94, 0.67) * warm +
      float3(0.05, 0.19, 0.17) * stripe;
  return half4(half3(color), 1);
})";

Element label(std::string_view content, float x, float y, float width,
              float size = 11, material::Color ink = kMuted,
              float weight = 500) {
  return kit::at(text(std::string(content)), x, y, width, size * 1.6f)
      .fontSize(size)
      .fontWeight(weight)
      .ink(ink);
}
Element rule(float x, float y, float w, float h = 1,
             material::Color ink = kRule) {
  return kit::at(x, y, w, h).fill(ink);
}
material::Material finish(const Controls& c, bool copper = false,
                          bool layered = false) {
  const material::Color silver{0.59f, 0.66f, 0.69f, 1};
  const material::Color bronze{0.73f, 0.39f, 0.18f, 1};
  auto out = material::from(copper ? bronze : silver);
  out.layer(material::noise(0.08f, {.seed = 29, .grain = true, .stretch = 10}),
            {.blend = material::BlendMode::SoftLight, .opacity = 0.10f});
  if (layered)
    out.layer(
        copper ? silver : bronze,
        {.opacity = c.overlay,
         .mask = material::Mask{.source = material::shader(kPlatingMask),
                                .channel = material::MaskChannel::Luminance}});
  return out.surface(
      {.metallic = c.metallic,
       .roughness = c.roughness,
       .normal = material::shader(kNormal, Grain{c.normalStrength}),
       .clearcoat = 0.12f,
       .reflectionWeight = 0.8f});
}

Element lettering(std::string_view words, float x, float y, float width,
                  float size, const Controls& c, bool copper = false) {
  return label(words, x, y, width, size, kInk, 650)
      .ink(material::Color{0, 0, 0, 0})
      .decorationOutline(Boundary::Glyphs)
      .foreground(relief(finish(c, copper, true),
                         {.shoulder = c.shoulder, .depth = c.depth}));
}

// Skia supplies exact cap and join coverage. The ordinary material stroke
// preserves surface response but does not expose these geometry decisions.
path::Outline strokeRegion(const path::Outline& spine, float width,
                           path::Cap cap = path::Cap::Round,
                           path::Join join = path::Join::Miter) {
  if (!(width > 0)) return {};
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(width);
  paint.setStrokeCap(path::toSk(cap));
  paint.setStrokeJoin(path::toSk(join));
  return path::fromSk(skpathutils::FillPathWithPaint(path::toSk(spine), paint));
}
path::Outline route(std::initializer_list<glm::vec2> points) {
  return path::through(
      std::span<const glm::vec2>(points.begin(), points.size()));
}
Element conductor(const path::Outline& region, const Controls& c,
                  bool copper = false, bool layered = false) {
  return pathFigure(region, 3)
      .background(shadow(Fill::color({0, 0, 0, 0.8f}), {0, 2}, 2))
      .foreground(relief(finish(c, copper, layered),
                         {.shoulder = c.shoulder, .depth = c.depth}));
}

struct Trace {
  path::Outline region;
  bool copper = false;
  bool layered = false;
};

struct MetalLinework {
  Controls controls;
  int heldPhase = -1;
  int phaseIndex = 0;
  std::vector<Trace> traces;
  std::array<path::Outline, 3> joins;
  std::array<path::Outline, 3> caps;
  std::array<path::Outline, 5> hairlines;
  path::Outline pressure;
  motion::Animatable<float> bearing = motion::animatable(122.0f);
  motion::Animatable<float> environmentTurn = motion::animatable(10.0f);
  motion::Animatable<material::Color> keyColor =
      motion::animatable(material::Color{0.98f, 0.95f, 0.87f, 1});

  Controls state(int phase) const {
    Controls c = controls;
    if (phase == 1) c.metallic = 0;
    if (phase == 2) c.roughness = 0;
    if (phase == 3) c.roughness = 1;
    if (phase == 4) c.elevation = 6;
    if (phase == 5) c.overlay = 0.5f;
    if (phase >= 6) c.elevation = 90;
    return c;
  }
  material::Lighting light(const Controls& c, bool diagnostic) const {
    const auto key = material::studio({.direction = bearing,
                                       .elevation = c.elevation,
                                       .color = keyColor,
                                       .intensity = 0.35f,
                                       .ambient = diagnostic ? 0.f : 0.35f});
    if (diagnostic) return key;
    return {key, material::environment(material::shader(kEnvironment),
                                       {.rotation = environmentTurn,
                                        .intensity = 1.65f,
                                        .size = {512, 256}})};
  }
  void aim(double seconds, int phase) {
    if (phase >= 6) {
      bearing = 122.f;
      if (phase == 6)
        keyColor = material::hsv(
            static_cast<float>(std::fmod(seconds, 4.0) * 90.0), 1, 1);
      else
        keyColor = phase == 7 ? material::Color{4, 4, 4, 1}
                              : material::Color{0, 0, 0, 1};
      return;
    }
    const double cycle = heldPhase < 0 ? std::fmod(seconds, 36.0) : seconds;
    const float angle =
        static_cast<float>(std::fmod(cycle, 24.0) / 24.0 * 6.2831853);
    bearing = 122 + 48 * std::sin(angle);
    environmentTurn = 10 + 58 * std::sin(angle);
    keyColor = material::Color{0.98f, 0.95f, 0.87f, 1};
  }
  void circuit() {
    for (int i = 0; i < 6; ++i) {
      const float y = 115 + i * 55.0f;
      const float end = 142 + i * 19.0f;
      traces.push_back(
          {strokeRegion(
               route({{52, y}, {168, y}, {215 + i * 11.0f, end}, {303, end}}),
               i == 2 ? 9 : 5),
           i % 2 == 0, i == 4});
      traces.push_back({strokeRegion(route({{635, end + 23},
                                            {706 - i * 8.0f, end + 23},
                                            {782, y},
                                            {897, y}}),
                                     4),
                        i % 2 != 0, i == 2});
    }
    for (int i = 0; i < 3; ++i) {
      traces.push_back({strokeRegion(route({{136 + i * 45.0f, 421},
                                            {136 + i * 45.0f, 483},
                                            {285 + i * 45.0f, 483},
                                            {365 + i * 21.0f, 404}}),
                                     5),
                        i == 1, i == 2});
      traces.push_back({strokeRegion(route({{560 + i * 21.0f, 404},
                                            {670 + i * 45.0f, 490},
                                            {859 - i * 45.0f, 490},
                                            {859 - i * 45.0f, 410}}),
                                     4),
                        i != 1});
    }
    const auto corner = route({{14, 57}, {78, 13}, {111, 65}, {158, 23}});
    for (int i = 0; i < 3; ++i) {
      joins[i] =
          strokeRegion(corner, 12, path::Cap::Butt, static_cast<path::Join>(i));
      caps[i] = strokeRegion(route({{22, 28}, {157, 28}}), 16,
                             static_cast<path::Cap>(i));
    }
    constexpr std::array widths{0.0f, 0.5f, 1.0f, 2.0f, 4.0f};
    for (int i = 0; i < 5; ++i)
      hairlines[i] = strokeRegion(
          route({{11, 14 + i * 14.0f}, {190, 14 + i * 14.0f}}), widths[i]);
    brush::Ribbon nib;
    nib.width = path::Profile{{0, 0.5f},  {0.15f, 6},  {0.42f, 18},
                              {0.60f, 4}, {0.78f, 12}, {1, 0.5f}};
    nib.join = path::Join::Round;
    nib.step = 1;
    pressure = path::fromSk(nib.band(path::toSk(path::Outline::svg(
        "M10 56 C32 6 70 12 73 44 C78 93 98 18 126 37 C153 60 166 9 198 19"))));
  }

  Element board(const Controls& c) const {
    const auto backing =
        material::from(kGround)
            .layer(material::noise(0.35f, {.seed = 17, .grain = true}),
                   {.blend = material::BlendMode::SoftLight, .opacity = 0.24f})
            .surface({.roughness = 0.96f});
    std::vector<Element> parts{
        label("A / CONDUCTIVE NETWORK", 26, 19, 480, 11, kTeal),
        label("ETCH / INLAY / OVERPRINT", 685, 19, 261, 10), rule(26, 46, 920)};
    for (const Trace& t : traces)
      parts.push_back(conductor(t.region, c, t.copper, t.layered));
    for (int i = 0; i < 12; ++i) {
      const float x = i < 6 ? 38.0f : 880.0f;
      const float y = 104 + (i % 6) * 55.0f;
      parts.push_back(kit::at(x, y, 28, 28)
                          .shape(shapes::annulus(0.47f))
                          .foreground(relief(finish(c, i % 2 == 0),
                                             {.shoulder = 1.5f, .depth = 1})));
      parts.push_back(label(i < 6 ? "IN" : "OUT", x - 6, y + 34, 54, 8));
    }
    parts.push_back(kit::at(302, 85, 340, 340)
                        .shape(shapes::annulus(0.90f))
                        .foreground(relief(finish(c, false, true),
                                           {.shoulder = 2, .depth = 1})));
    parts.push_back(kit::at(325, 108, 294, 294)
                        .shape(shapes::annulus(0.93f))
                        .fill(finish(c, true)));
    parts.push_back(lettering("O8", 373, 153, 233, 106, c));
    parts.push_back(label("SIGNAL MATRIX", 380, 282, 210, 11, kTeal, 600));
    parts.push_back(label("Cu / Ag  •  ROUTE 08", 377, 307, 246, 9));
    for (int i = 0; i < 30; ++i) {
      const float a = i * 12.0f * 0.0174532925f;
      parts.push_back(
          kit::at(470 + std::cos(a) * 119, 253 + std::sin(a) * 119, 10, 1.5f)
              .fill(finish(c, i % 5 == 0))
              .rotate(i * 12.0f));
    }
    parts.push_back(rule(26, 530, 920));
    parts.push_back(
        label("INSULATING GROUND / ROUGHNESS 0.96", 27, 548, 482, 9));
    parts.push_back(label("OPEN COUNTERS · ANNULAR VIAS · LAYERED TRACES", 566,
                          548, 380, 9));
    return box()
        .width(976)
        .height(592)
        .borderRadius({18})
        .overflow(Overflow::Clip)
        .fill(backing)
        .stroke(material::from(kRule), {.width = 1})
        .children(std::move(parts));
  }
  Element inspector(int phase, const Controls& c) const {
    std::vector<Element> parts{
        label("B / SURFACE RESPONSE", 0, 0, 318, 11, kAmber),
        label(kPhases[phase], 0, 30, 318, 19, kInk, 600), rule(0, 82, 318)};
    const std::array<std::string_view, 4> names{
        "METALLIC", "ROUGHNESS", "KEY ELEVATION", "MASK OPACITY"};
    const std::array<float, 4> values{c.metallic, c.roughness, c.elevation,
                                      c.overlay};
    for (int i = 0; i < 4; ++i) {
      std::array<char, 24> value{};
      std::snprintf(value.data(), value.size(), "%.2f",
                    static_cast<double>(values[i]));
      parts.push_back(label(names[i], 0, 107 + i * 79, 205, 10));
      parts.push_back(label(value.data(), 214, 101 + i * 79, 104, 22, kInk));
      parts.push_back(rule(0, 146 + i * 79, 318));
      parts.push_back(rule(0, 145 + i * 79, 318 * values[i] / (i == 2 ? 90 : 1),
                           2, kAmber));
    }
    parts.push_back(label("THREE VISIBLE LAYERS", 0, 442, 318, 11, kTeal, 600));
    parts.push_back(
        label("01  Matte insulating substrate\n02  Brushed silver / copper "
              "contour\n03  Masked plating and shoulder light",
              0, 473, 318, 11));
    parts.push_back(
        label("36 S / NINE STATES / SOURCE CONTROLS", 0, 565, 318, 9));
    return box().width(318).height(592).children(std::move(parts));
  }
  Element specimens(const Controls& c) const {
    std::vector<Element> parts;
    constexpr std::array headings{
        "JOIN / ROUND · MITER · BEVEL", "CAP / BUTT · ROUND · SQUARE",
        "WIDTH / 0 · 0.5 · 1 · 2 · 4 PX", "PRESSURE / PLATED RIBBON"};
    for (int i = 0; i < 4; ++i) {
      std::vector<Element> samples;
      if (i == 0 || i == 1) {
        for (int j = 0; j < 3; ++j)
          samples.push_back(
              kit::at(conductor(i == 0 ? joins[j] : caps[j], c, j == 1, j == 2),
                      8 + j * 105.0f, 14, 180, 86)
                  .transformOrigin(pct(0), pct(0))
                  .scale(0.55f));
      } else if (i == 2) {
        constexpr std::array widths{"0 / EMPTY", "0.5", "1.0", "2.0", "4.0"};
        for (int j = 0; j < 5; ++j) {
          samples.push_back(conductor(hairlines[j], c, false, true));
          samples.push_back(label(widths[j], 219, 7 + j * 14.0f, 99, 9));
        }
      } else {
        samples.push_back(conductor(pressure, c, true, true));
        samples.push_back(label("0.5 → 18 → 0.5 px", 15, 85, 265, 9));
      }
      parts.push_back(kit::at(i * 345.0f, 0, 325, 158)
                          .children({label(headings[i], 0, 0, 325, 10, kInk),
                                     kit::at(0, 28, 325, 123)
                                         .borderRadius({8})
                                         .overflow(Overflow::Clip)
                                         .fill(kGround)
                                         .children(std::move(samples))}));
    }
    return box().width(1360).height(158).children(std::move(parts));
  }
  Element describe(int phase) const {
    const Controls c = state(phase);
    return box()
        .width(1440)
        .height(1000)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .fill(material::linearGradient({0, 0}, {0.4f, 1},
                                       {{0, {0.052f, 0.065f, 0.066f, 1}},
                                        {1, {0.014f, 0.022f, 0.026f, 1}}}))
        .lighting(light(c, phase >= 6))
        .children(
            {label("MATERIAL WRITING / 03", 40, 26, 590, 11, kAmber, 600),
             lettering("METAL / LINEWORK", 34, 46, 1030, 68, c, true),
             label("INLAID ROUTING INSTRUMENT  /  MATERIAL BRUSHES · LETTERS · "
                   "OPEN CONTOURS",
                   40, 129, 1012, 10),
             label("Cu / Ag", 1123, 48, 270, 35, kInk).ink(finish(c)),
             label("AUTHORED STUDY / NOT HARDWARE", 1099, 116, 301, 9),
             rule(40, 159, 1360), kit::at(board(c), 40, 183, 976, 592),
             kit::at(inspector(phase, c), 1082, 183, 318, 592),
             label("GEOMETRY DETERMINES COVERAGE / MATERIAL DETERMINES "
                   "APPEARANCE",
                   40, 798, 957, 10, kTeal),
             label("SILVER OVER COPPER / MASKED OVERPRINT", 1060, 798, 340, 9),
             rule(40, 826, 1360), kit::at(specimens(c), 40, 849, 1360, 158)});
  }
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 1000);
    ctx.background(kGround);
    ctx.captureAt(2);
    circuit();
    phaseIndex = heldPhase < 0 ? 0 : std::clamp(heldPhase, 0, 8);
    aim(0, phaseIndex);
    ctx.composer.render(describe(phaseIndex));
  }
  void update(double elapsed, sketch::SketchContext& ctx) {
    const double seconds =
        std::isfinite(elapsed) ? std::max(0.0, elapsed) : 0.0;
    const int next = heldPhase < 0
                         ? static_cast<int>(std::fmod(seconds / 4, 9.0))
                         : std::clamp(heldPhase, 0, 8);
    aim(seconds, next);
    if (next != phaseIndex) {
      phaseIndex = next;
      ctx.composer.render(describe(next));
    }
  }
};
}  // namespace

SIGIL_SKETCH(MetalLinework, "Study · Materials",
             "layered silver and copper linework, pressure ribbons and "
             "material lettering")
