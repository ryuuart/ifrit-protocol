/** @file
 * embossed_foil — a foil-clad orbital thermal-control console.
 *
 * EDIT THESE FIRST
 *   Controls — crinkle strength, roughness, relief and lighting balance.
 *   heldPhase — -1 cycles; 0–8 holds one authored material state.
 *   blanket — the fitted foil silhouette, seams and folded corner.
 */

// TAGS: Materials/Metal, Materials/Foil, Materials/Lighting,
// Typography/Material ink, Composition/Instrumentation

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Foil.h"

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace shapes = sigil::geometry::shapes;
using namespace sigil::compose;

namespace {

constexpr material::Color kInk{0.82f, 0.87f, 0.87f, 1};
constexpr material::Color kMuted{0.42f, 0.51f, 0.56f, 1};
constexpr material::Color kRule{0.18f, 0.27f, 0.32f, 0.8f};
constexpr material::Color kBlue{0.21f, 0.63f, 0.79f, 1};
constexpr material::Color kGold{0.91f, 0.61f, 0.21f, 1};
constexpr material::Color kDark{0.012f, 0.026f, 0.038f, 1};
constexpr float kPhaseSeconds = 4;

struct Controls {
  float crinkle = 0.58f;
  float roughness = 0.24f;
  float metallic = 1;
  float reliefDepth = 1.05f;
  float shoulder = 1.8f;
  float keyElevation = 36;
  float keyIntensity = 0.38f;
  float environmentIntensity = 0.62f;
  float amberLayer = 0.76f;
  bool maskStress = false;
};

struct Phase {
  std::string_view name;
  std::string_view detail;
};
constexpr std::array kPhases{
    Phase{"ORBIT / NOMINAL", "Creased foil, raised controls and material ink"},
    Phase{"NORMAL / ZERO", "Flat foil normal; contour shoulders remain"},
    Phase{"CRINKLE / STRONG", "Stronger folds under the same moving lights"},
    Phase{"ROUGHNESS / ZERO", "Sharp reflected studio field"},
    Phase{"ROUGHNESS / ONE", "Diffuse foil endpoint"},
    Phase{"KEY / FRONTAL", "Face-on light with reduced key exposure"},
    Phase{"KEY / GRAZING", "Low light reveals the shoulder and folds"},
    Phase{"DIE / RECESSED", "Signed depth reverses the contour relief"},
    Phase{"LAYERS / MASK", "Perforated overprint, shaped holes and clipping"},
};

Element label(std::string_view words, float x, float y, float width,
              float size = 12, material::Color color = kMuted,
              float weight = 500) {
  return kit::at(text(std::string(words)), x, y, width, size * 1.55f)
      .fontSize(size)
      .fontWeight(weight)
      .ink(color);
}

Element rule(float x, float y, float width, float height = 1,
             material::Color color = kRule) {
  return kit::at(x, y, width, height).fill(color);
}

std::string number(float value, int precision = 2) {
  std::array<char, 32> out{};
  std::snprintf(out.data(), out.size(), "%.*f", precision,
                static_cast<double>(value));
  return out.data();
}

material::Material foil(const Controls& c, bool silver = false,
                        float seed = 11) {
  const embossed_foil::Parameters parameters{
      .scale = 1,
      .strength = std::clamp(c.crinkle, 0.0f, 1.5f),
      .roughness = std::clamp(c.roughness, 0.0f, 1.0f),
      .variation = c.roughness > 0 && c.roughness < 1 ? 0.12f : 0,
      .seed = seed};
  auto result = material::linearGradient({0, 0}, {0.18f, 1},
                                         {{0, {0.76f, 0.81f, 0.81f, 1}},
                                          {0.48f, {0.52f, 0.59f, 0.60f, 1}},
                                          {0.69f, {0.84f, 0.85f, 0.79f, 1}},
                                          {1, {0.55f, 0.63f, 0.67f, 1}}});
  result.layer(
      material::noise(0.09f,
                      {.octaves = 2, .seed = 16, .grain = true, .stretch = 8}),
      {.blend = material::BlendMode::SoftLight, .opacity = 0.08f});
  if (!silver) {
    result.layer(material::Color{1, 0.53f, 0.025f, 1},
                 {.blend = material::BlendMode::Multiply,
                  .opacity = c.amberLayer,
                  .mask = material::Mask{
                      .source = material::shader(embossed_foil::kFilmMask),
                      .channel = material::MaskChannel::Luminance,
                      .low = 0.0f,
                      .high = 1.0f}});
  }
  if (c.maskStress) {
    result.layer(material::Color{0.012f, 0.067f, 0.094f, 1},
                 {.opacity = 0.87f,
                  .mask = material::Mask{
                      .source = material::shader(embossed_foil::kOverprintMask),
                      .channel = material::MaskChannel::Alpha}});
  }
  return result.surface(
      {.metallic = std::clamp(c.metallic, 0.0f, 1.0f),
       .roughness = material::shader(embossed_foil::kRoughness, parameters),
       .normal = material::shader(embossed_foil::kNormal, parameters),
       .reflectionWeight = 0.88f});
}

Element embossed(std::string_view words, float x, float y, float width,
                 float size, material::Material finish, const Controls& c) {
  return label(words, x, y, width, size, kInk, 650)
      .ink(material::Color{0, 0, 0, 0})
      .decorationOutline(Boundary::Glyphs)
      .foreground(relief(std::move(finish),
                         {.shoulder = c.shoulder, .depth = c.reliefDepth}));
}

Element pressed(float x, float y, float width, float height, float radius,
                material::Material finish, const Controls& c) {
  return kit::at(x, y, width, height)
      .borderRadius({radius})
      .background(relief(std::move(finish),
                         {.shoulder = c.shoulder, .depth = c.reliefDepth}));
}

Element screw(float x, float y, const Controls& c) {
  Controls bolt = c;
  bolt.reliefDepth = 0.7f;
  bolt.shoulder = 1.1f;
  bolt.crinkle = 0;
  return pressed(x, y, 11, 11, 5.5f, foil(bolt, true), bolt)
      .children({rule(2.5f, 5, 6, 0.8f, {0.08f, 0.14f, 0.16f, 0.8f})});
}

Element seam(float x, float y, float width, bool vertical = false) {
  Element out = kit::at(x, y, vertical ? 7.0f : width, vertical ? width : 7.0f);
  out.children(
      {rule(0, 0, vertical ? 1.0f : width, vertical ? width : 1.0f,
            {0.04f, 0.03f, 0.02f, 0.8f}),
       rule(vertical ? 6.0f : 0, vertical ? 0 : 6.0f, vertical ? 1.0f : width,
            vertical ? width : 1.0f, {0.91f, 0.71f, 0.37f, 0.4f})});
  for (int i = 0; i < static_cast<int>(width / 11); ++i) {
    out.children({rule(vertical ? 2.0f : i * 11.0f, vertical ? i * 11.0f : 2.0f,
                       vertical ? 3.0f : 5.0f, vertical ? 5.0f : 3.0f,
                       {0.91f, 0.78f, 0.53f, 0.6f})});
  }
  return out;
}

Element thermalPlot() {
  return kit::at(0, 0, 524, 145)
      .borderRadius({9})
      .overflow(Overflow::Clip)
      .fill(material::shader(embossed_foil::kGrid))
      .stroke(stroke(0.8f, Fill::color(kRule)))
      .children({label("SKIN / ORBITAL CYCLE", 15, 10, 348, 10, kBlue, 600),
                 label("+42", 466, 12, 43, 9), label("−18", 466, 108, 43, 9),
                 kit::at(14, 32, 438, 97)
                     .fill(material::shader(embossed_foil::kTelemetry)),
                 rule(14, 92, 438, 0.7f, {0.77f, 0.44f, 0.12f, 0.45f}),
                 label("SUNLIT", 15, 120, 130, 9),
                 label("ECLIPSE", 321, 120, 130, 9, kBlue)});
}

Element button(std::string_view title, std::string_view detail, float x,
               bool active, const Controls& c) {
  return pressed(x, 0, 166, 63, 10, foil(c, !active, 21), c)
      .children(
          {label(title, 15, 10, 143, 15, {0.032f, 0.052f, 0.065f, 1}, 700),
           label(detail, 15, 38, 143, 9, {0.04f, 0.072f, 0.086f, 1}, 600)});
}

Element thermalCore(const Controls& c) {
  Controls channel = c;
  channel.reliefDepth = -0.7f;
  channel.shoulder = 1.3f;
  const auto ringFinish = foil(c, true, 27);
  std::vector<Element> parts{
      pressed(0, 0, 298, 298, 18, foil(channel, true, 27), channel),
      label("LOOP / A", 20, 17, 180, 10, {0.045f, 0.065f, 0.075f, 1}, 600),
      kit::at(36, 56, 226, 226)
          .shape(shapes::annulus(0.74f))
          .background(
              relief(ringFinish, {.shoulder = 2.2f, .depth = c.reliefDepth})),
      kit::at(55, 75, 188, 188)
          .shape(shapes::annulus(0.87f))
          .fill(foil(c, false, 7)),
      kit::at(80, 100, 138, 138).shape(shapes::circle()).fill(kDark),
      label("ΔT", 115, 117, 95, 20, kMuted),
      label("08.4", 88, 148, 136, 41, kInk, 500).ink(foil(c, true)),
      label("K / NOMINAL", 104, 208, 126, 9, kBlue, 600),
      rule(272, 85, 6, 154, {0.02f, 0.04f, 0.05f, 0.7f})};
  for (int i = 0; i < 18; ++i) {
    const float angle = (-140.0f + i * 16.0f) * 0.0174532925f;
    const float x = 149 + std::cos(angle) * 100;
    const float y = 169 + std::sin(angle) * 100;
    parts.push_back(kit::at(x - 3, y - 1, 6, 2)
                        .fill(i < 11 ? kBlue : kGold)
                        .rotate(i * 16.0f - 140));
  }
  return box().width(298).height(298).children(std::move(parts));
}

Element blanket(const Controls& c, const Shape& outline, const Shape& flap) {
  std::vector<Element> parts{
      kit::at(7, 13, 994, 595)
          .shape(outline)
          .fill({0, 0, 0, 0.7f})
          .filter(material::Filter::blur(17)),
      kit::at(0, 0, 1000, 600)
          .shape(outline)
          .fill(foil(c))
          .stroke(stroke(0.8f, Fill::color({0.59f, 0.39f, 0.12f, 0.7f}))),
      seam(25, 24, 948),
      seam(25, 568, 948),
      seam(23, 31, 531, true),
      seam(969, 31, 531, true),
      kit::at(56, 47, 888, 494)
          .borderRadius({18})
          .fill(kDark)
          .stroke(stroke(1, Fill::color({0.30f, 0.37f, 0.38f, 1}))),
      pressed(76, 64, 848, 83, 11, foil(c, true, 5), c),
      embossed("ORBITAL / 04", 99, 68, 676, 43, foil(c, false, 5), c),
      label("THERMAL", 789, 86, 117, 12, {0.06f, 0.09f, 0.11f, 1}, 700),
      label("CONTROL", 789, 105, 117, 10, {0.06f, 0.09f, 0.11f, 1}, 600),
      kit::at(thermalCore(c), 77, 165, 298, 298),
      label("RADIATOR / A", 395, 163, 232, 11, kBlue, 600),
      label("23.6", 391, 180, 265, 57, kInk, 500).ink(foil(c, true, 18)),
      label("°C", 559, 211, 85, 20),
      label("HEATER / DUTY", 692, 169, 214, 10, kMuted, 600),
      label("34%", 686, 190, 201, 44, kInk).ink(foil(c, false, 18)),
      rule(395, 252, 520),
      kit::at(thermalPlot(), 394, 272, 524, 145),
      label("COMMAND / HEAT REJECTION", 395, 431, 459, 10, kMuted, 600),
      kit::at(box().width(524).height(63).children(
                  {button("AUTO", "CLOSED LOOP", 0, true, c),
                   button("BYPASS", "LOOP ISOLATE", 179, false, c),
                   button("HOLD", "SET POINT", 358, false, c)}),
              394, 457, 524, 63),
      label("MLI / 24", 85, 484, 203, 17, kGold, 600).ink(foil(c, false)),
      label("INTERFACE / FICTIONAL", 84, 514, 286, 8),
      kit::at(880, 2, 120, 130)
          .shape(flap)
          .fill(foil(c, true, 38))
          .stroke(stroke(0.8f, Fill::color({0.78f, 0.83f, 0.83f, 0.8f}))),
      label("FOLD / 02", 885, 18, 95, 9, {0.06f, 0.09f, 0.11f, 1}, 600)};
  for (glm::vec2 p : {glm::vec2{44, 34}, {941, 34}, {44, 552}, {941, 552}})
    parts.push_back(screw(p.x, p.y, c));
  for (int i = 0; i < 12; ++i) {
    parts.push_back(rule(657 + i * 7.5f, 246, 4, 3, i < 7 ? kGold : kRule));
  }
  return box().width(1000).height(600).children(std::move(parts));
}

Element meter(std::string_view heading, std::string value,
              std::string_view unit, float y, float fraction) {
  return kit::at(0, y, 316, 67)
      .children(
          {label(heading, 0, 0, 316, 10, kMuted, 600),
           label(value, 0, 18, 187, 27, kInk), label(unit, 189, 34, 127, 10),
           rule(0, 62, 316),
           rule(0, 61, 316 * std::clamp(fraction, 0.0f, 1.0f), 2, kGold)});
}

struct EmbossedFoil {
  Controls controls;
  int heldPhase = -1;
  int phaseIndex = 0;
  motion::Animatable<float> bearing = motion::animatable(125.0f);
  motion::Animatable<float> environmentTurn = motion::animatable(18.0f);
  Shape blanketOutline = shapes::svg(
      "M1 4 L11 0 L35 3 L63 0 L96 4 L100 12 L98 30 L100 58 "
      "L98 86 L100 96 L87 100 L62 98 L33 100 L12 98 L0 94 "
      "L2 72 L0 45 L2 24 Z");
  Shape foldedCorner = shapes::svg("M0 0 L100 0 L100 100 L46 79 L13 44 Z");

  Controls stateFor(int index) const {
    Controls c = controls;
    switch (index) {
      case 1:
        c.crinkle = 0;
        break;
      case 2:
        c.crinkle = 1.45f;
        break;
      case 3:
        c.roughness = 0;
        break;
      case 4:
        c.roughness = 1;
        break;
      case 5:
        c.keyElevation = 90;
        c.keyIntensity = 0.17f;
        break;
      case 6:
        c.keyElevation = 5;
        break;
      case 7:
        c.reliefDepth = -1.05f;
        break;
      case 8:
        c.maskStress = true;
        c.amberLayer = 1;
        break;
      default:
        break;
    }
    return c;
  }

  material::Lighting light(const Controls& c, bool moving = true) const {
    return material::Lighting{
        material::studio(
            {.direction = moving ? bearing : motion::Animatable<float>(125),
             .elevation = c.keyElevation,
             .color = material::Color{0.95f, 0.94f, 0.88f, 1},
             .intensity = c.keyIntensity,
             .ambient = 0.34f}),
        material::environment(
            material::shader(embossed_foil::kEnvironment),
            {.rotation =
                 moving ? environmentTurn : motion::Animatable<float>(18),
             .intensity = c.environmentIntensity,
             .size = {512, 256}})};
  }

  Element controlsPanel(int index, const Controls& c) const {
    std::vector<Element> parts{
        label("FOIL / MATERIAL RESPONSE", 0, 0, 316, 10, kGold, 600),
        label(kPhases[index].name, 0, 25, 316, 23, kInk),
        label(kPhases[index].detail, 0, 65, 316, 10),
        meter("CRINKLE NORMAL", number(c.crinkle), "field strength", 114,
              c.crinkle / 1.5f),
        meter("SURFACE ROUGHNESS", number(c.roughness), "0 sharp / 1 diffuse",
              193, c.roughness),
        meter("SIGNED RELIEF", number(c.reliefDepth), "raised / recessed", 272,
              std::abs(c.reliefDepth) / 1.5f),
        meter("KEY ELEVATION", number(c.keyElevation, 0), "degrees", 351,
              c.keyElevation / 90),
        meter("ENVIRONMENT", number(c.environmentIntensity), "shared surround",
              430, c.environmentIntensity),
        label("DIRECTIONAL + ENVIRONMENT", 0, 525, 316, 10, kBlue, 600),
        label("Material fills / material ink / outline relief", 0, 548, 316,
              10),
        rule(0, 578, 316)};
    for (int i = 0; i < 9; ++i)
      parts.push_back(rule(i * 36.0f, 592, 27, 5, i == index ? kGold : kRule));
    return box().width(316).height(600).children(std::move(parts));
  }

  Element specimen(int index) const {
    constexpr std::array names{"NORMAL 0", "NORMAL +",   "ROUGH 0",
                               "ROUGH 1",  "FRONT",      "GRAZE",
                               "DIE ±",    "MASK / HOLE"};
    constexpr std::array details{
        "Flat cap", "Strong folds", "Sharp field",   "Diffuse field",
        "Key 90°",  "Key 5°",       "Raised / sunk", "Layers / counters"};
    Controls c = controls;
    c.crinkle = 0.8f;
    if (index == 0) c.crinkle = 0;
    if (index == 1) c.crinkle = 1.45f;
    if (index == 2) c.roughness = 0;
    if (index == 3) c.roughness = 1;
    if (index == 4) {
      c.keyElevation = 90;
      c.keyIntensity = 0.17f;
    }
    if (index == 5) c.keyElevation = 5;
    if (index == 7) c.maskStress = true;
    Element sample = kit::at(0, 25, 153, 91)
                         .borderRadius({10})
                         .overflow(Overflow::Clip)
                         .fill(foil(c, index % 2 != 0, 19))
                         .lighting(light(c, false));
    if (index == 6) {
      c.reliefDepth = 1.05f;
      sample.children({embossed("O", 7, 14, 67, 46, foil(c, true), c)});
      c.reliefDepth = -1.05f;
      sample.children({embossed("8", 78, 14, 68, 46, foil(c, true), c)});
    } else if (index == 7) {
      sample.children(
          {kit::at(8, 15, 60, 60)
               .shape(shapes::annulus(0.56f))
               .background(relief(foil(c, true), {.shoulder = 2, .depth = 1})),
           embossed("O8", 68, 21, 82, 34, foil(c, true), c),
           kit::at(-8, 79, 172, 10).fill(foil(c, false))});
    } else {
      sample.children(
          {label("O8", 13, 16, 136, 43, kInk, 650).ink(foil(c, true, 19))});
    }
    return kit::at(index * 171.0f, 0, 153, 164)
        .children({label(names[index], 0, 0, 153, 10, kInk, 600),
                   std::move(sample), label(details[index], 0, 128, 158, 9)});
  }

  Element describe(int index) const {
    const Controls c = stateFor(index);
    std::vector<Element> samples;
    for (int i = 0; i < 8; ++i) samples.push_back(specimen(i));
    return box()
        .width(1440)
        .height(1040)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .fill(material::linearGradient(
            {0, 0}, {0.3f, 1}, {{0, {0.035f, 0.062f, 0.081f, 1}}, {1, kDark}}))
        .lighting(light(c))
        .children(
            {label("FIELD / 09  •  METALLIZED SURFACES", 40, 25, 807, 11, kGold,
                   600),
             label("THERMAL / O08", 35, 47, 1002, 61, kInk, 650)
                 .ink(foil(c, false, 32)),
             label("AN ORBITAL CONTROL CONSOLE  /  EMBOSSED LETTERING · "
                   "CRINKLED FOIL · SCOPED LIGHT",
                   40, 126, 1024, 10),
             label("ORBITAL MATERIALS LAB", 1085, 33, 315, 11, kInk, 600),
             label("GOLD / SILVER  •  BLANKET 04", 1085, 58, 315, 10),
             label("Generated field / authored interface", 1085, 83, 315, 10),
             rule(40, 160, 1360),
             kit::at(blanket(c, blanketOutline, foldedCorner), 40, 184, 1000,
                     600),
             kit::at(controlsPanel(index, c), 1084, 184, 316, 600),
             label("AMBER FILM / REFLECTIVE ALUMINUM CUE", 40, 807, 524, 10,
                   kGold, 600),
             label("Fictional readings / no thermal simulation", 620, 807, 463,
                   10),
             label("36 S / NINE STATES", 1248, 807, 152, 10),
             rule(40, 835, 1360),
             label("FIXED MATERIAL AND OUTLINE SPECIMENS", 40, 855, 755, 10,
                   kMuted, 600),
             label("Foil fill + actual text ink / counter-hole relief", 990,
                   855, 410, 10),
             kit::at(box().width(1360).height(164).children(std::move(samples)),
                     40, 879, 1360, 164)});
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 1040);
    ctx.oversample(1);
    ctx.background(kDark);
    ctx.captureAt(2);
    phaseIndex = heldPhase < 0 ? 0 : std::clamp(heldPhase, 0, 8);
    ctx.composer.render(describe(phaseIndex));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double seconds =
        std::isfinite(elapsed) ? std::max(0.0, elapsed) : 0.0;
    bearing = 125.0f + 37.0f * static_cast<float>(std::sin(seconds * 0.24));
    environmentTurn =
        18.0f + 51.0f * static_cast<float>(std::sin(seconds * 0.13));
    const int next =
        heldPhase < 0
            ? static_cast<int>(std::fmod(seconds / kPhaseSeconds,
                                         static_cast<double>(kPhases.size())))
            : std::clamp(heldPhase, 0, 8);
    if (next != phaseIndex) {
      phaseIndex = next;
      ctx.composer.render(describe(phaseIndex));
    }
  }
};

}  // namespace

SIGIL_SKETCH(EmbossedFoil, "Study · Materials",
             "crinkled foil fills, material ink and embossed orbital controls")
