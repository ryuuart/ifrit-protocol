/** @file
 * optical_liquid — a layered glass and liquid measuring instrument.
 *
 * The optical filter displaces the destination already beneath each pane.
 * Metal relief is shaded by the shared material surface and inherited light.
 * Six held parameter sets expose the endpoints without rebuilding each frame.
 *
 * EDIT THESE FIRST
 *   Controls — glass index, edge thickness, roughness, opacity and liquid fill.
 *   kPhases — the six four-second capture states.
 *   lightDirection — the moving studio light shared by the metal surfaces.
 */

// TAGS: Materials/Glass, Materials/Liquid, Materials/Lighting,
// Composition/Layers

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/vec2.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace motion = sigil::motion;
using namespace sigil::compose;

namespace {

constexpr material::Color kInk{0.085f, 0.15f, 0.17f, 1};
constexpr material::Color kMuted{0.35f, 0.43f, 0.44f, 1};
constexpr material::Color kRule{0.48f, 0.56f, 0.55f, 0.4f};
constexpr material::Color kTeal{0.03f, 0.44f, 0.43f, 1};
constexpr material::Color kPaper{0.91f, 0.93f, 0.90f, 1};
constexpr float kPhaseSeconds = 4;

struct Controls {
  float ior = 1.52f;
  float thickness = 20;
  float roughness = 0.08f;
  float opacity = 0.34f;
  float liquidFill = 0.62f;
  bool nestedClips = false;
};

struct Phase {
  std::string_view name;
  std::string_view detail;
  Controls controls;
};

constexpr std::array kPhases{
    Phase{"CLEAR / WORKING", "A raised meniscus and a clear optical edge", {}},
    Phase{"DRY / NEUTRAL",
          "No liquid, no edge depth, unit refractive index",
          {.ior = 1, .thickness = 0, .roughness = 0, .liquidFill = 0}},
    Phase{"FULL / DENSE",
          "Full reservoir and the strongest safe displacement",
          {.ior = 2.4f, .thickness = 48, .liquidFill = 1}},
    Phase{
        "FROST / DIFFUSE", "Roughness at its upper endpoint", {.roughness = 1}},
    Phase{"OPAQUE / COATED",
          "The coating covers the destination",
          {.opacity = 1}},
    Phase{"NESTED / EXPOSED",
          "Four clip boundaries; the glazing is absent",
          {.thickness = 48, .opacity = 0, .nestedClips = true}},
};

// All optical parameters are finite, and the displacement stays inside the
// filter's declared sample radius even at the largest index and thickness.
struct GlassParameters {
  glm::vec2 boxSize{1, 1};
  float ior = 1.52f;
  float thickness = 20;
  float roughness = 0.08f;
  float opacity = 0.34f;
  float fill = 0.62f;
  float lens = 0;
};

struct LiquidParameters {
  float fill = 0.62f;
};

constexpr std::string_view kCalibration = R"(
float markCoverage(float distance, float halfWidth, float footprint) {
    float low = max(distance - footprint * 0.5, -halfWidth);
    float high = min(distance + footprint * 0.5, halfWidth);
    return clamp((high - low) / footprint, 0.0, 1.0);
}
half4 main(float2 p) {
    float footprint = 1.0 / max(uContentScale, 0.125);
    float hairline = max(0.5, footprint * 0.5);
    float2 cell = abs(mod(p + 16.0, 32.0) - 16.0);
    float gx = markCoverage(cell.x, hairline, footprint);
    float gy = markCoverage(cell.y, hairline, footprint);
    float grid = 1.0 - (1.0 - gx) * (1.0 - gy);
    float checker = mod(floor(p.x / 64.0) + floor(p.y / 64.0), 2.0);
    float3 color = mix(float3(0.88, 0.91, 0.86),
                       float3(0.94, 0.95, 0.90), checker);
    color = mix(color, float3(0.53, 0.65, 0.61), grid);
    float2 q = mod(p + 48.0, 192.0) - 96.0;
    float arm = max(1.0, footprint * 0.5);
    float cross = max(markCoverage(abs(q.x), arm, footprint) *
                      markCoverage(abs(q.y), 13.5, footprint),
                      markCoverage(abs(q.y), arm, footprint) *
                      markCoverage(abs(q.x), 13.5, footprint));
    float ring = markCoverage(abs(length(q) - 22.0),
                              max(0.9, footprint * 0.5), footprint);
    color = mix(color, float3(0.13, 0.37, 0.37), max(cross, ring * 0.55));
    float2 c = p / max(uResolution, float2(1));
    color += float3(0.035, 0.027, 0.005) * c.y;
    return half4(half3(color), 1);
}
)";

constexpr std::string_view kRelief = R"(
half4 main(float2 p) {
    float2 extent = max(uResolution, float2(1));
    float2 n = float2(exp(-p.x / 12.0) - exp(-(extent.x - p.x) / 12.0),
                      exp(-p.y / 12.0) - exp(-(extent.y - p.y) / 12.0));
    float3 normal = normalize(float3(n * 0.85, 1.0));
    return half4(half3(normal * 0.5 + 0.5), 1);
}
)";

constexpr std::string_view kRefraction = R"(
half4 main(float2 p) {
    float2 extent = max(boxSize, float2(1));
    float2 q = clamp((p - extent * 0.5) / max(extent * 0.5, float2(1)),
                     float2(-1), float2(1));
    float2 edge = float2(exp(-max(p.x, 0.0) / 20.0) -
                            exp(-max(extent.x - p.x, 0.0) / 20.0),
                        exp(-max(p.y, 0.0) / 20.0) -
                            exp(-max(extent.y - p.y, 0.0) / 20.0));
    float2 normal = mix(edge, -q * 0.62, lens);
    float waterline = extent.y * (1.0 - fill);
    float wet = fill > 0.001 ? smoothstep(waterline - 2.0,
                                         waterline + 2.0, p.y) : 0.0;
    float2 ripple = float2(sin(p.y * 0.045), cos(p.x * 0.055));
    float2 shift = (normal * thickness * 0.55 + ripple * wet * 2.4) *
                   max(ior - 1.0, 0.0) * opacity;
    float dispersion = thickness * max(ior - 1.0, 0.0) * opacity * 0.012;
    half4 base = content.eval(p + shift);
    half4 red = content.eval(p + shift + float2(dispersion, 0));
    half4 blue = content.eval(p + shift - float2(dispersion, 0));
    half4 refracted = half4(red.r, base.g, blue.b, base.a);
    float spread = roughness * 3.8 * opacity;
    if (spread > 0.01) {
        half4 diffuse = (content.eval(p + shift + float2(spread, spread)) +
                         content.eval(p + shift - float2(spread, spread))) * 0.5;
        refracted = mix(refracted, diffuse, half(roughness * opacity * 0.72));
    }
    return refracted;
}
)";

constexpr std::string_view kGlazing = R"(
half4 main(float2 p) {
    float2 extent = max(uResolution, float2(1));
    float2 uv = p / extent;
    float edge = min(min(p.x, extent.x - p.x), min(p.y, extent.y - p.y));
    edge = mix(edge, min(extent.x, extent.y) * 0.5 -
                    length(p - extent * 0.5), lens);
    float rim = exp(-max(edge, 0.0) / max(1.0, thickness * 0.24));
    float edgeDistance = (edge - max(2.0, thickness * 0.19)) / 1.6;
    float dark = exp(-edgeDistance * edgeDistance);
    float sweep = uv.x * 0.85 + uv.y * 0.38 - 0.42 - sin(uTime * 0.37) * 0.10;
    float stripDistance = sweep / (0.032 + roughness * 0.16);
    float wideDistance = (sweep + 0.10) / 0.22;
    float strip = exp(-stripDistance * stripDistance);
    float wide = exp(-wideDistance * wideDistance);
    float3 tint = mix(float3(0.69, 0.85, 0.83), float3(0.98, 1.0, 0.96), strip);
    tint = mix(tint, float3(0.10, 0.32, 0.33), dark * 0.5);
    float opticalAlpha = clamp(opacity * (0.035 + rim * 0.80 + strip * 0.35 +
                                         wide * 0.08 + roughness * 0.20), 0.0, 1.0);
    float alpha = mix(opticalAlpha, 1.0, pow(opacity, 7.0));
    return half4(half3(tint * alpha), half(alpha));
}
)";

constexpr std::string_view kLiquid = R"(
half4 main(float2 p) {
    if (fill <= 0.001) return half4(0);
    float2 extent = max(uResolution, float2(1));
    float edge = min(p.x, extent.x - p.x);
    float meniscus = 5.0 - 12.0 * exp(-max(edge, 0.0) / 13.0);
    float wave = sin(p.x * 0.031 + uTime * 0.76) * 1.2 +
                 sin(p.x * 0.057 - uTime * 0.45) * 0.6;
    float surface = fill >= 0.999 ? -12.0 :
                    extent.y * (1.0 - fill) + meniscus + wave;
    float wet = smoothstep(surface - 0.8, surface + 0.8, p.y);
    float depth = clamp((p.y - surface) / max(extent.y * fill, 1.0), 0.0, 1.0);
    float a = sin(p.x * 0.083 + sin(p.y * 0.036 + uTime * 0.6) * 1.8);
    float b = sin(p.y * 0.061 + sin(p.x * 0.045 - uTime * 0.42) * 2.2);
    float caustic = pow(clamp(1.0 - abs(a + b) * 0.62, 0.0, 1.0), 12.0);
    caustic *= smoothstep(0.12, 0.9, depth);
    float lipDistance = (p.y - surface - 1.5) / 1.2;
    float lip = exp(-lipDistance * lipDistance);
    float wall = exp(-max(edge, 0.0) / 9.0);
    float3 color = mix(float3(0.10, 0.66, 0.58),
                       float3(0.015, 0.25, 0.29), depth);
    color += float3(0.43, 0.57, 0.29) * caustic * 0.43;
    color = mix(color, float3(0.72, 0.95, 0.86), lip * 0.78);
    float alpha = wet * clamp(0.18 + depth * 0.34 + wall * 0.18 +
                              caustic * 0.18 + lip * 0.40, 0.0, 0.88);
    return half4(half3(color * alpha), half(alpha));
}
)";

constexpr std::string_view kCaustic = R"(
half4 main(float2 p) {
    float2 uv = p / max(uResolution, float2(1));
    float band = sin(uv.x * 32.0 + sin(uv.y * 13.0 - uTime * 0.6) * 2.3) +
                 sin(uv.y * 21.0 + sin(uv.x * 17.0 + uTime * 0.5) * 1.8);
    float focus = pow(clamp(1.0 - abs(band) * 0.63, 0.0, 1.0), 10.0);
    float envelope = pow(max(0.0, sin(uv.x * 3.14159)), 2.0) *
                     pow(max(0.0, sin(uv.y * 3.14159)), 2.0);
    float alpha = focus * envelope * fill * 0.42;
    return half4(half3(float3(0.40, 0.78, 0.63) * alpha), half(alpha));
}
)";

std::string number(float value, int precision = 2) {
  std::array<char, 32> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.*f", precision,
                static_cast<double>(value));
  return buffer.data();
}

Element label(std::string_view value, float x, float y, float w,
              float size = 12, material::Color ink = kMuted,
              float weight = 500) {
  return kit::at(text(std::string(value)), x, y, w, size * 1.55f)
      .fontSize(size)
      .fontWeight(weight)
      .ink(ink);
}

Element rule(float x, float y, float w, float h = 1,
             material::Color color = kRule) {
  return kit::at(x, y, w, h).fill(color);
}

Element ring(float x, float y, float diameter, float width,
             material::Color color) {
  return kit::at(x, y, diameter, diameter)
      .borderRadius({diameter / 2})
      .stroke(stroke(width, Fill::color(color)));
}

material::Material metal(float roughness = 0.28f) {
  return material::from(
             material::linearGradient({0, 0}, {0.2f, 1},
                                      {{0, {0.89f, 0.92f, 0.91f, 1}},
                                       {0.23f, {0.70f, 0.77f, 0.77f, 1}},
                                       {0.51f, {0.94f, 0.96f, 0.91f, 1}},
                                       {0.82f, {0.65f, 0.72f, 0.73f, 1}},
                                       {1, {0.84f, 0.88f, 0.85f, 1}}}))
      .layer(
          material::noise(
              0.04f, {.octaves = 2, .seed = 18, .grain = true, .stretch = 12}),
          {.blend = material::BlendMode::SoftLight, .opacity = 0.11f})
      .surface({.metallic = 0.86f,
                .roughness = roughness,
                .normal = material::shader(kRelief)});
}

Element screw(float x, float y) {
  return kit::at(x, y, 12, 12)
      .borderRadius({6})
      .fill(metal(0.20f))
      .stroke(stroke(0.7f, Fill::color({0.26f, 0.36f, 0.37f, 0.8f})))
      .children({rule(3, 5.5f, 6, 1, kMuted)});
}

GlassParameters glassParameters(const Controls& c, glm::vec2 size,
                                bool lens = false) {
  return {.boxSize = size,
          .ior = std::clamp(c.ior, 1.0f, 2.4f),
          .thickness = std::clamp(c.thickness, 0.0f, 48.0f),
          .roughness = std::clamp(c.roughness, 0.0f, 1.0f),
          .opacity = std::clamp(c.opacity, 0.0f, 1.0f),
          .fill = std::clamp(c.liquidFill, 0.0f, 1.0f),
          .lens = lens ? 1.0f : 0.0f};
}

Element glass(float x, float y, float w, float h, const Controls& c,
              float radius = 24, bool lens = false) {
  const GlassParameters parameters = glassParameters(c, {w, h}, lens);
  const auto optical = material::shader(kRefraction, parameters,
                                        {.textures = {{"content", {}}}});
  return kit::at(x, y, w, h)
      .borderRadius({radius})
      .overflow(Overflow::Clip)
      .backdropFilter(material::Filter::of(optical, 64))
      .fill(material::shader(kGlazing, parameters))
      .cache(Cache::Picture);
}

// Each rounded parent has a visibly different offset. The stripes overshoot
// every boundary, so a missing clip exposes a distinct strip of colour.
Element nestedClips(float x, float y, float w, float h) {
  Element inside = box().width(w).height(h).children(
      {rule(-20, 10, w + 40, 19, {0.87f, 0.53f, 0.22f, 0.8f}),
       rule(-20, 35, w + 40, 19, {0.07f, 0.50f, 0.50f, 0.8f}),
       rule(-20, 60, w + 40, 19, {0.29f, 0.38f, 0.62f, 0.8f}),
       rule(-20, 85, w + 40, 19, {0.76f, 0.31f, 0.29f, 0.8f})});
  for (int depth = 0; depth < 4; ++depth) {
    inside = box()
                 .width(w)
                 .height(h)
                 .borderRadius({18.0f + depth * 7})
                 .overflow(Overflow::Clip)
                 .stroke(stroke(0.7f, Fill::color(kTeal)))
                 .children({kit::at(std::move(inside), -9 + depth * 5.0f,
                                    4 + depth * 3.0f, w, h)});
  }
  return kit::at(std::move(inside), x, y, w, h);
}

Element reservoir(const Controls& c) {
  std::vector<Element> parts{
      kit::at(4, 10, 354, 474)
          .borderRadius({34})
          .fill({0.14f, 0.28f, 0.27f, 0.14f})
          .filter(material::Filter::blur(9)),
      kit::at(0, 0, 352, 464)
          .borderRadius({30})
          .fill(metal())
          .stroke(stroke(1, Fill::color({0.98f, 1, 0.96f, 0.8f}))),
      kit::at(11, 12, 330, 438)
          .borderRadius({24})
          .fill({0.06f, 0.22f, 0.22f, 0.38f}),
      kit::at(17, 18, 318, 426)
          .borderRadius({20})
          .overflow(Overflow::Clip)
          .children(
              {kit::at(0, 0, 318, 426).fill(material::shader(kCalibration)),
               rule(148, 0, 22, 426, {0.95f, 0.97f, 0.93f, 1}),
               rule(157, 0, 4, 426, {0.09f, 0.32f, 0.57f, 0.8f}),
               label("S / 04", 25, 32, 150, 26, kInk, 600),
               label("OPTICAL MEDIUM", 25, 71, 180, 10, kMuted)})};
  parts.push_back(
      kit::at(17, 18, 318, 426)
          .borderRadius({20})
          .overflow(Overflow::Clip)
          .fill(material::shader(kLiquid, LiquidParameters{c.liquidFill}))
          .cache(Cache::Picture));
  parts.push_back(glass(17, 18, 318, 426, c, 20));
  parts.push_back(ring(248, 48, 50, 0.8f, {0.19f, 0.39f, 0.38f, 0.4f}));
  parts.push_back(label("n", 264, 57, 20, 22, kInk));
  for (int tick = 0; tick <= 20; ++tick) {
    const float y = 36 + tick * 19;
    const bool major = tick % 5 == 0;
    parts.push_back(
        rule(26, y, major ? 22.0f : 10.0f, 0.8f, {0.13f, 0.33f, 0.31f, 0.58f}));
    if (major)
      parts.push_back(label(std::to_string(100 - tick * 5), 53, y - 7, 34, 9,
                            {0.12f, 0.31f, 0.31f, 0.75f}));
  }
  // The marker sits at the nominal fill; the liquid shader raises its sides
  // and lowers the centre instead of drawing a flat horizontal boundary.
  if (c.liquidFill > 0 && c.liquidFill < 1) {
    const float y = 18 + 426 * (1 - c.liquidFill);
    parts.push_back(rule(296, y, 37, 0.8f, {0.94f, 1, 0.88f, 0.9f}));
    parts.push_back(label("READ", 276, y + 8, 40, 8, kTeal));
  }
  parts.push_back(rule(22, 21, 2, 389, {0.96f, 1, 0.96f, 0.8f}));
  parts.push_back(rule(329, 49, 1.2f, 338, {0.15f, 0.39f, 0.40f, 0.6f}));
  parts.push_back(screw(4, 4));
  parts.push_back(screw(336, 4));
  parts.push_back(screw(4, 448));
  parts.push_back(screw(336, 448));
  return box().width(352).height(464).children(std::move(parts));
}

Element opticalBench(const Controls& c) {
  std::vector<Element> parts{
      kit::at(0, 0, 952, 610)
          .borderRadius({26})
          .fill(metal(0.42f))
          .filter(material::Filter::dropShadow(
              {0.08f, 0.18f, 0.17f, 0.18f}, {.blur = 14, .offset = {0, 12}})),
      kit::at(14, 14, 924, 582)
          .borderRadius({17})
          .overflow(Overflow::Clip)
          .fill(material::shader(kCalibration)),
      label("01 / LIQUID CELL", 38, 30, 240, 11, kInk, 600),
      label("02 / OPTICAL STACK", 474, 30, 230, 11, kInk, 600),
      rule(452, 30, 1, 543),
      kit::at(42, 508, 366, 68)
          .fill(material::shader(kCaustic, LiquidParameters{c.liquidFill})),
      kit::at(reservoir(c), 59, 73, 352, 464),
      label("VOLUME", 66, 547, 100, 9),
      label(number(c.liquidFill * 100, 0) + "%", 161, 539, 90, 24, kInk, 600),
      label("MENISCUS / EYE LEVEL", 266, 549, 162, 9),
      rule(505, 97, 335, 4, {0.09f, 0.34f, 0.32f, 0.9f}),
      rule(577, 82, 4, 325, {0.87f, 0.48f, 0.19f, 0.85f}),
      label("DISPLACED", 548, 116, 210, 30, kInk, 600),
      label("BACKGROUND", 548, 156, 210, 23, kInk, 400),
      ring(510, 208, 159, 1, {0.16f, 0.39f, 0.38f, 0.6f}),
      ring(544, 242, 91, 0.8f, {0.16f, 0.39f, 0.38f, 0.6f}),
      rule(480, 286, 218, 1, kTeal),
      rule(590, 196, 1, 191, kTeal),
      glass(493, 83, 320, 242, c, 38),
      label("A", 513, 96, 20, 11, kInk, 600),
      glass(662, 247, 208, 208, c, 104, true),
      ring(658, 243, 216, 3, {0.18f, 0.35f, 0.35f, 0.8f}),
      ring(654, 239, 224, 0.8f, {0.94f, 0.97f, 0.92f, 0.9f}),
      label("B / LENS", 715, 466, 132, 10, kInk, 600),
      label("Actual destination", 487, 365, 168, 12, kInk),
      label("Checker / cross / type", 487, 386, 181, 10),
      rule(487, 416, 132, 0.8f),
      label("UNIT INDEX", 487, 430, 130, 9),
      label("n = 1.00", 487, 448, 133, 24, kInk, 500),
      label("Zero depth gives zero bend", 487, 484, 184, 10)};
  if (c.nestedClips) parts.push_back(nestedClips(493, 93, 288, 202));
  Controls smoked = c;
  smoked.opacity = std::min(0.72f, c.opacity + 0.18f);
  parts.push_back(glass(478, 520, 410, 50, smoked, 25));
  parts.push_back(label("C / SMOKED COVER", 496, 538, 163, 9, kInk, 600));
  parts.push_back(label("INDEPENDENT LAYERS", 691, 538, 182, 9, kInk));
  for (glm::vec2 position : {glm::vec2{6, 6}, {934, 6}, {6, 592}, {934, 592}})
    parts.push_back(screw(position.x, position.y));
  return box().width(952).height(610).children(std::move(parts));
}

Element readout(std::string_view title, std::string value,
                std::string_view unit, float y, float proportion) {
  return kit::at(0, y, 302, 64)
      .children(
          {label(title, 0, 0, 150, 10, kMuted, 600),
           label(value, 0, 17, 136, 25, kInk, 500),
           label(unit, 143, 29, 130, 10),
           rule(0, 57, 302, 1, {0.71f, 0.77f, 0.74f, 0.8f}),
           rule(0, 56, 302 * std::clamp(proportion, 0.0f, 1.0f), 2, kTeal)});
}

Element controlsPanel(int phaseIndex) {
  const Phase& phase = kPhases[phaseIndex];
  const Controls& c = phase.controls;
  std::vector<Element> parts{
      label("OPTICAL PARAMETERS", 0, 0, 302, 11, kInk, 600),
      label(phase.name, 0, 29, 302, 25, kInk, 500),
      label(phase.detail, 0, 69, 302, 11),
      readout("REFRACTIVE INDEX", number(c.ior), "authored bend", 118,
              (c.ior - 1) / 1.4f),
      readout("EDGE THICKNESS", number(c.thickness, 0), "local px", 197,
              c.thickness / 48),
      readout("SURFACE ROUGHNESS", number(c.roughness), "clear / diffuse", 276,
              c.roughness),
      readout("COATING OPACITY", number(c.opacity), "absent / covered", 355,
              c.opacity),
      readout("LIQUID FILL", number(c.liquidFill * 100, 0), "% of reservoir",
              434, c.liquidFill),
      label("LIGHT IN MOTION", 0, 526, 160, 10, kInk, 600),
      label("Relief and reflections move continuously", 0, 546, 300, 10),
      rule(0, 578, 302, 1)};
  for (int index = 0; index < static_cast<int>(kPhases.size()); ++index) {
    const float x = index * 51.0f;
    parts.push_back(kit::at(x, 592, 44, 17)
                        .borderRadius({8})
                        .fill(index == phaseIndex ? kTeal : kPaper)
                        .stroke(stroke(0.7f, Fill::color(kRule))));
    parts.push_back(label(std::to_string(index + 1), x + 18, 593, 15, 9,
                          index == phaseIndex ? kPaper : kMuted));
  }
  return box().width(302).height(610).children(std::move(parts));
}

Element specimen(int index, std::string_view title, std::string_view detail,
                 Controls c, bool clip = false) {
  std::vector<Element> parts{
      label("0" + std::to_string(index + 1) + " / " + std::string(title), 0, 0,
            204, 10, kInk, 600),
      kit::at(0, 24, 204, 97)
          .borderRadius({15})
          .overflow(Overflow::Clip)
          .fill(material::shader(kCalibration)),
      label("+  n", 19, 51, 139, 27, kTeal, 500),
      rule(98, 30, 1, 84, {0.12f, 0.37f, 0.35f, 0.8f}),
      kit::at(8, 32, 188, 80)
          .borderRadius({15})
          .overflow(Overflow::Clip)
          .fill(material::shader(kLiquid, LiquidParameters{c.liquidFill})),
      glass(8, 32, 188, 80, c, 15),
      label(detail, 0, 134, 214, 10)};
  if (clip) parts.push_back(nestedClips(107, 41, 80, 62));
  return kit::at(index * 224.0f, 0, 204, 159).children(std::move(parts));
}

struct OpticalLiquid {
  motion::Animatable<float> lightDirection = motion::animatable(118.0f);
  int phaseIndex = 0;

  Element describe(int index) const {
    const Controls& c = kPhases[index].controls;
    return box()
        .width(1440)
        .height(1000)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .lighting(material::Lighting{
            material::studio({.direction = lightDirection,
                              .elevation = 52,
                              .color = material::Color{0.98f, 1, 0.94f, 1},
                              .intensity = 0.54f,
                              .ambient = 0.52f})})
        .children({
            label("FIELD / 07", 50, 30, 160, 11, kTeal, 600),
            label("OPTICAL / LIQUID", 47, 52, 955, 57, kInk, 500),
            label("A STUDY OF DEPTH, LIGHT AND THE BOUNDARY BETWEEN LAYERS", 50,
                  124, 900, 11, kMuted, 500),
            label("SPECIMEN LABORATORY", 1090, 38, 300, 11, kInk, 600),
            label("S / 04   •   SIX OPTICAL STATES", 1090, 61, 300, 10),
            label("Generated surfaces / no image trace", 1090, 83, 300, 10),
            rule(50, 158, 1340),
            kit::at(opticalBench(c), 50, 186, 952, 610),
            kit::at(controlsPanel(index), 1040, 186, 302, 610),
            label("BOUNDARY SPECIMENS", 50, 825, 290, 11, kInk, 600),
            label("FIXED ENDPOINTS / VISIBLE IN EVERY PHASE", 952, 826, 410,
                  10),
            kit::at(
                box().width(1340).height(159).children(
                    {specimen(0, "NEUTRAL", "n 1 / depth 0 / opacity 0 / dry",
                              {.ior = 1,
                               .thickness = 0,
                               .roughness = 0,
                               .opacity = 0,
                               .liquidFill = 0}),
                     specimen(1, "CLEAR", "n 1.52 / depth 20 / roughness 0",
                              {.roughness = 0, .liquidFill = 0}),
                     specimen(2, "FULL", "fill 1 / no empty headspace",
                              {.ior = 1.33f, .liquidFill = 1}),
                     specimen(3, "THICK", "n 2.4 / depth 48 / finite sampling",
                              {.ior = 2.4f, .thickness = 48}),
                     specimen(4, "FROST", "roughness 1 / diffuse backdrop",
                              {.roughness = 1, .liquidFill = 0}),
                     specimen(5, "COAT / CLIP", "opacity 1 / four nested clips",
                              {.opacity = 1, .liquidFill = 0}, true)}),
                50, 847, 1340, 153),
        });
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 1000);
    ctx.oversample(1);
    ctx.background({0.94f, 0.95f, 0.92f, 1});
    ctx.captureAt(2);
    ctx.composer.render(describe(0));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double seconds =
        std::isfinite(elapsed) ? std::max(0.0, elapsed) : 0.0;
    lightDirection =
        118.0f + 43.0f * std::sin(static_cast<float>(seconds) * 0.37f);
    const int next = static_cast<int>(std::fmod(
        seconds / kPhaseSeconds, static_cast<double>(kPhases.size())));
    if (next != phaseIndex) {
      phaseIndex = next;
      ctx.composer.render(describe(phaseIndex));
    }
  }
};

}  // namespace

SIGIL_SKETCH(
    OpticalLiquid, "Study · Materials",
    "thick glass, liquid meniscus, moving light and nested optical layers")
