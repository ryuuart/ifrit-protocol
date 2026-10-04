/** @file
 * wet_glass_console — water beads over a live environmental instrument.
 *
 * EDIT THESE FIRST
 *   Controls — wetness, bead size, bounded bend, cap relief and coating.
 *   heldPhase — hold one of the nine states; -1 advances every four seconds.
 *   kDrops — authored bead placement over the live Compose display.
 */

// TAGS: Materials/Glass, Materials/Water, Materials/Lighting,
// Composition/Layers, Composition/Instrumentation

#include <sigilcompose/brush/Decorations.h>
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

#include "Water.h"

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace shapes = sigil::geometry::shapes;
using namespace sigil::compose;

namespace {

constexpr material::Color kInk{0.78f, 0.90f, 0.91f, 1};
constexpr material::Color kMuted{0.35f, 0.52f, 0.57f, 1};
constexpr material::Color kDim{0.20f, 0.34f, 0.39f, 1};
constexpr material::Color kTeal{0.18f, 0.83f, 0.75f, 1};
constexpr material::Color kAmber{0.94f, 0.64f, 0.25f, 1};
constexpr material::Color kRule{0.19f, 0.32f, 0.37f, 0.7f};
constexpr float kPhaseSeconds = 4;

struct Controls {
  float wetness = 0.72f;
  float beadScale = 1;
  float bendPixels = 12;
  float capHeight = 0.72f;
  float roughness = 0.035f;
  float coatingOpacity = 0.42f;
  float filmAmount = 0.10f;
  float lightElevation = 38;
  float driftPixels = 7;
  bool overlap = false;
};

struct Phase {
  std::string_view name;
  std::string_view detail;
};

constexpr std::array kPhases{
    Phase{"BEADING / ACTIVE", "Separate caps over a live instrument"},
    Phase{"DRY / BASELINE", "Uncovered display; the trace keeps moving"},
    Phase{"WET / FILM", "Spread water, streaks and shallow rivulets"},
    Phase{"MICRO / DENSITY", "Small caps and the same bounded optics"},
    Phase{"MACRO / LENS", "Large beads magnify type and the live trace"},
    Phase{"GRAZING / RELIEF", "Low light reveals the analytic cap normals"},
    Phase{"OVERLAP / CLIP", "Stacked beads cross a rounded display edge"},
    Phase{"COATING / ZERO", "Zero coating and zero optical displacement"},
    Phase{"COATING / ONE", "Opaque coating endpoint; not clear water"},
};

struct Drop {
  float x;
  float y;
  float width;
  float height;
  bool pear = false;
};

constexpr std::array kDrops{
    Drop{62, 70, 54, 60},    Drop{211, 111, 97, 108, true},
    Drop{382, 54, 35, 39},   Drop{528, 132, 75, 83},
    Drop{754, 73, 118, 128}, Drop{845, 234, 41, 49, true},
    Drop{660, 277, 65, 77},  Drop{431, 254, 114, 133, true},
    Drop{217, 298, 45, 50},  Drop{64, 394, 86, 96},
    Drop{352, 444, 31, 35},  Drop{706, 431, 92, 106, true},
    Drop{153, 54, 16, 18},   Drop{325, 203, 23, 24},
    Drop{608, 60, 12, 14},   Drop{870, 424, 22, 26},
    Drop{583, 397, 26, 30},  Drop{174, 459, 15, 18},
    Drop{482, 69, 17, 19},   Drop{782, 331, 18, 22},
    Drop{317, 365, 13, 15},  Drop{101, 262, 22, 25},
    Drop{624, 192, 20, 22},  Drop{823, 498, 14, 16},
};

std::string number(float value, int precision = 2) {
  std::array<char, 32> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.*f", precision,
                static_cast<double>(value));
  return buffer.data();
}

Element label(std::string_view words, float x, float y, float width,
              float size = 12, material::Color ink = kMuted,
              float weight = 500) {
  return kit::at(text(std::string(words)), x, y, width, size * 1.55f)
      .fontSize(size)
      .fontWeight(weight)
      .ink(ink);
}

Element rule(float x, float y, float width, float height = 1,
             material::Color color = kRule) {
  return kit::at(x, y, width, height).fill(color);
}

Element rounded(float x, float y, float width, float height, float radius,
                material::Color color) {
  return kit::at(x, y, width, height).borderRadius({radius}).fill(color);
}

material::Material chassis(float roughness = 0.38f) {
  return material::from(
             material::linearGradient({0, 0}, {0.2f, 1},
                                      {{0, {0.23f, 0.32f, 0.36f, 1}},
                                       {0.06f, {0.09f, 0.15f, 0.19f, 1}},
                                       {0.48f, {0.15f, 0.22f, 0.26f, 1}},
                                       {0.94f, {0.08f, 0.13f, 0.17f, 1}},
                                       {1, {0.25f, 0.35f, 0.39f, 1}}}))
      .layer(
          material::noise(
              0.05f, {.octaves = 2, .seed = 27, .grain = true, .stretch = 18}),
          {.blend = material::BlendMode::SoftLight, .opacity = 0.15f})
      .surface({.metallic = 0.68f,
                .roughness = roughness,
                .normal = material::shader(wet_glass::kPanelNormal)});
}

material::Material lettering() {
  return material::linearGradient({0, 0}, {0.12f, 1},
                                  {{0, {0.83f, 0.95f, 0.96f, 1}},
                                   {0.42f, {0.52f, 0.75f, 0.80f, 1}},
                                   {0.62f, {0.89f, 0.98f, 0.97f, 1}},
                                   {1, {0.46f, 0.66f, 0.73f, 1}}})
      .surface({.metallic = 0.32f,
                .roughness = 0.30f,
                .normal = material::shader(wet_glass::kPanelNormal)});
}

Element screw(float x, float y) {
  return kit::at(x, y, 10, 10)
      .shape(shapes::circle())
      .fill(chassis(0.26f))
      .stroke(stroke(0.6f, Fill::color(kDim)))
      .children({rule(2, 4.6f, 6, 0.8f, kMuted)});
}

Element water(float x, float y, float width, float height, const Controls& c,
              const Shape& silhouette, float film = 0) {
  const auto parameters =
      wet_glass::parameters({width, height}, c.bendPixels, c.capHeight,
                            c.roughness, c.coatingOpacity, film);
  const auto optical = material::shader(wet_glass::kRefraction, parameters,
                                        {.textures = {{"content", {}}}});
  return kit::at(x, y, width, height)
      .shape(silhouette)
      .overflow(Overflow::Clip)
      .backdropFilter(material::Filter::of(optical, 40))
      .fill(material::shader(wet_glass::kGlazing, parameters)
                .surface({.roughness = c.roughness,
                          .normal = material::shader(wet_glass::kCapNormal,
                                                     parameters),
                          .normalScale = 0.70f}))
      .cache(Cache::Picture);
}

Element numericCell(std::string_view heading, std::string_view value,
                    std::string_view unit, float x, float y, float width,
                    material::Color color = kTeal) {
  return kit::at(x, y, width, 111)
      .borderRadius({9})
      .fill({0.018f, 0.043f, 0.060f, 0.9f})
      .stroke(stroke(0.8f, Fill::color(kRule)))
      .children({label(heading, 15, 11, width - 28, 10, kMuted, 600),
                 label(value, 13, 27, width - 53, 44, color, 500),
                 label(unit, width - 55, 58, 50, 12, kMuted),
                 rule(15, 94, width - 30, 1),
                 rule(15, 94, (width - 30) * 0.62f, 1.5f, color)});
}

Element chart(float x, float y, float width, float height, float channel) {
  const material::Color color = channel == 0 ? kTeal : kAmber;
  return kit::at(x, y, width, height)
      .borderRadius({8})
      .overflow(Overflow::Clip)
      .fill(material::shader(wet_glass::kDisplay))
      .stroke(stroke(0.8f, Fill::color(kRule)))
      .children(
          {label(channel == 0 ? "01 / HUMIDITY" : "02 / PRESSURE", 14, 11,
                 width - 25, 10, color, 600),
           label("LIVE / 32 S", width - 104, 11, 94, 9, kMuted),
           kit::at(12, 37, width - 24, height - 66)
               .fill(material::shader(wet_glass::kTrace,
                                      wet_glass::TraceParameters{
                                          {width - 24, height - 66}, channel})),
           label("−16", 14, height - 22, 45, 9),
           label("−08", width * 0.47f, height - 22, 45, 9),
           label("NOW", width - 50, height - 22, 42, 9, color)});
}

Element channelCell(int index, float x, float y) {
  constexpr std::array names{"AIR", "GLASS", "WIND", "FILM"};
  constexpr std::array values{"23.8", "21.4", "07.6", "0.34"};
  constexpr std::array units{"°C", "°C", "m/s", "mm"};
  return kit::at(x, y, 208, 79)
      .borderRadius({7})
      .fill({0.023f, 0.055f, 0.071f, 1})
      .stroke(stroke(0.7f, Fill::color(kRule)))
      .children({label("C" + std::to_string(index + 1), 11, 9, 28, 9, kTeal),
                 label(names[index], 42, 8, 150, 10, kMuted, 600),
                 label(values[index], 11, 25, 137, 28, kInk),
                 label(units[index], 147, 44, 50, 10), rule(11, 68, 186, 1),
                 rule(11, 68, 41.0f + index * 34, 1.5f,
                      index == 3 ? kAmber : kTeal)});
}

Element display() {
  std::vector<Element> parts{
      label("HYGRO / 08", 24, 17, 228, 15, kInk, 600),
      label("STATION 04  /  COASTAL ARRAY", 254, 21, 370, 10),
      rounded(779, 19, 6, 6, 3, kTeal),
      label("LINK / ACTIVE", 795, 17, 120, 10, kTeal),
      rule(24, 47, 880),
      numericCell("RELATIVE HUMIDITY", "84.2", "% RH", 24, 67, 273),
      numericCell("DEW POINT", "18.6", "°C", 24, 188, 273, kAmber),
      chart(315, 67, 589, 136, 0),
      chart(315, 214, 589, 136, 1),
      label("SENSOR ARRAY / LIVE TELEMETRY", 24, 325, 276, 10, kMuted, 600),
      rule(24, 349, 273),
      rule(24, 483, 880),
      label("AUTO / ACQUISITION", 24, 506, 238, 10, kTeal, 600),
      label("ENV 04.118   •   WINDOW HEATER STANDBY", 274, 506, 417, 10),
      label("SYNC  /  60 HZ", 794, 506, 110, 10, kMuted),
  };
  for (int i = 0; i < 4; ++i)
    parts.push_back(channelCell(i, 24 + i * 224.0f, 378));
  for (int i = 0; i < 12; ++i) {
    const float x = 24 + i * 22.7f;
    parts.push_back(
        rounded(x, 356, 16, 5, 2,
                i < 8 ? kTeal : material::Color{0.04f, 0.14f, 0.16f, 1}));
  }
  return box()
      .width(928)
      .height(540)
      .fill(material::shader(wet_glass::kDisplay))
      .children(std::move(parts));
}

Element meter(std::string_view title, std::string value, std::string_view unit,
              float y, float fraction, material::Color color = kTeal) {
  return kit::at(0, y, 332, 67)
      .children(
          {label(title, 0, 0, 328, 10, kMuted, 600),
           label(value, 0, 16, 193, 26, kInk), label(unit, 197, 32, 135, 10),
           rule(0, 62, 332, 1),
           rule(0, 61, 332 * std::clamp(fraction, 0.0f, 1.0f), 2, color)});
}

struct WetGlassConsole {
  Controls controls;
  int heldPhase = -1;
  int phaseIndex = 0;
  motion::Animatable<float> lightDirection = motion::animatable(132.0f);
  std::array<motion::Animatable<float>, kDrops.size()> dropX;
  std::array<motion::Animatable<float>, kDrops.size()> dropY;
  Shape circle = shapes::circle();
  Shape pear = shapes::svg(
      "M50 0 C72 10 98 42 99 65 C102 86 81 100 51 100 "
      "C21 100 0 86 1 64 C2 39 24 11 50 0 Z");
  Shape rivulet = shapes::svg(
      "M43 0 C58 0 71 8 68 20 C65 34 46 41 48 53 "
      "C50 65 81 68 82 81 C83 96 65 100 49 100 "
      "C28 100 16 92 21 78 C26 63 37 61 32 48 "
      "C27 35 24 28 26 18 C28 7 32 0 43 0 Z");

  Controls stateFor(int index) const {
    Controls state = controls;
    switch (index) {
      case 1:
        state.wetness = 0;
        state.filmAmount = 0;
        break;
      case 2:
        state.wetness = 1;
        state.filmAmount = 0.60f;
        state.roughness = 0.13f;
        break;
      case 3:
        state.wetness = 1;
        state.beadScale = 0.32f;
        state.bendPixels = 6;
        state.capHeight = 1.1f;
        break;
      case 4:
        state.beadScale = 1.55f;
        state.bendPixels = 18;
        break;
      case 5:
        state.lightElevation = 6;
        break;
      case 6:
        state.wetness = 1;
        state.overlap = true;
        state.bendPixels = 18;
        break;
      case 7:
        state.coatingOpacity = 0;
        state.wetness = 1;
        break;
      case 8:
        state.coatingOpacity = 1;
        state.wetness = 1;
        break;
      default:
        break;
    }
    return state;
  }

  Element droplets(const Controls& c) const {
    std::vector<Element> parts;
    const int count =
        static_cast<int>(kDrops.size() * std::clamp(c.wetness, 0.0f, 1.0f));
    if (c.filmAmount > 0 && count > 0) {
      Controls film = c;
      film.bendPixels = 3;
      film.capHeight = 0.12f;
      film.coatingOpacity = std::min(c.coatingOpacity, 0.55f) * c.filmAmount;
      parts.push_back(water(304, 20, 335, 474, film, rivulet, 1));
      parts.push_back(water(821, -17, 49, 610, film, rivulet, 1));
      parts.push_back(water(141, 214, 146, 239, film, pear, 1).rotate(-21));
    }
    for (int i = 0; i < count; ++i) {
      const Drop& drop = kDrops[i];
      const float width = std::max(drop.width * c.beadScale, 1.0f);
      const float height = std::max(drop.height * c.beadScale, 1.0f);
      parts.push_back(
          water(drop.x, drop.y, width, height, c, drop.pear ? pear : circle)
              .translateX(dropX[i])
              .translateY(dropY[i]));
    }
    if (c.overlap) {
      parts.push_back(water(460, 100, 180, 199, c, circle));
      parts.push_back(water(532, 176, 138, 155, c, pear));
      parts.push_back(water(879, -43, 131, 156, c, circle));
      parts.push_back(water(-57, 466, 159, 153, c, circle));
    }
    return box().width(928).height(540).children(std::move(parts));
  }

  Element console(const Controls& c) const {
    Element screen =
        kit::at(stack(), 26, 26, 928, 540)
            .borderRadius({15})
            .overflow(Overflow::Clip)
            .children({display(),
                       kit::at(0, 0, 928, 540)
                           .borderRadius({15})
                           .fill(material::shader(wet_glass::kWindow)
                                     .surface({.roughness = 0.10f,
                                               .normal = material::shader(
                                                   wet_glass::kPanelNormal)}))
                           .cache(Cache::Picture),
                       droplets(c)});
    return box().width(980).height(592).children(
        {kit::at(3, 8, 977, 587)
             .borderRadius({25})
             .fill({0, 0, 0, 0.7f})
             .filter(material::Filter::blur(14)),
         kit::at(0, 0, 980, 592)
             .borderRadius({23})
             .fill(chassis())
             .stroke(stroke(0.8f, Fill::color(kDim))),
         rounded(16, 16, 948, 560, 20, {0.006f, 0.016f, 0.021f, 1}),
         std::move(screen),
         rule(37, 24, 900, 0.7f, {0.41f, 0.62f, 0.65f, 0.5f}),
         label("IP / 68", 36, 574, 100, 8, kMuted, 600),
         label("HERMETIC WINDOW  /  LAMINATE 02", 655, 574, 285, 8),
         screw(7, 7), screw(963, 7), screw(7, 575), screw(963, 575)});
  }

  Element controlsPanel(int index, const Controls& c) const {
    std::vector<Element> parts{
        label("SURFACE / OBSERVATION", 0, 0, 332, 10, kTeal, 600),
        label(kPhases[index].name, 0, 25, 332, 24, kInk),
        label(kPhases[index].detail, 0, 66, 332, 10),
        meter("WET COVERAGE", number(c.wetness * 100, 0), "% authored", 111,
              c.wetness),
        meter("OPTICAL DISPLACEMENT", number(c.bendPixels, 0), "local px / ≤18",
              190, c.bendPixels / 18),
        meter("CAP RELIEF", number(c.capHeight), "normal amplitude", 269,
              c.capHeight / 1.3f),
        meter("COATING OPACITY", number(c.coatingOpacity),
              "0 absent / 1 covered", 348, c.coatingOpacity, kAmber),
        meter("LIGHT ELEVATION", number(c.lightElevation, 0), "degrees", 427,
              c.lightElevation / 90),
        label("LIVE TRACE  /  FIXED LOCAL OPTICS", 0, 518, 332, 10, kTeal, 600),
        label("Drops drift; the display remains a Compose tree", 0, 539, 332,
              10),
        rule(0, 565, 332)};
    for (int i = 0; i < static_cast<int>(kPhases.size()); ++i) {
      parts.push_back(
          rounded(i * 37.0f, 579, 29, 7, 3, i == index ? kTeal : kDim));
    }
    return box().width(332).height(592).children(std::move(parts));
  }

  Element coupon(int index, std::string_view title, std::string_view detail,
                 int kind) const {
    Controls c = controls;
    c.filmAmount = 0;
    c.coatingOpacity = 0.50f;
    std::vector<Element> parts{
        label("0" + std::to_string(index + 1) + " / " + std::string(title), 0,
              0, 208, 10, kInk, 600),
        kit::at(0, 24, 208, 113)
            .borderRadius({12})
            .overflow(Overflow::Clip)
            .fill(material::shader(wet_glass::kCalibration))
            .children({label("08 / +", 16, 51, 186, 29, kInk, 600),
                       rule(121, 29, 2, 99, kAmber)}),
        label(detail, 0, 149, 214, 10),
    };
    Element sample =
        kit::at(0, 24, 208, 113).borderRadius({12}).overflow(Overflow::Clip);
    if (kind == 1) sample.children({water(35, 10, 92, 100, c, circle)});
    if (kind == 2) {
      for (int i = 0; i < 11; ++i) {
        const float size = 3.0f + (i % 4) * 3.0f;
        sample.children({water(12 + i * 17.0f, 15 + (i % 3) * 31.0f, size, size,
                               c, circle)});
      }
    }
    if (kind == 3) sample.children({water(13, -20, 174, 170, c, circle)});
    if (kind == 4) {
      c.roughness = 1;
      sample.children({water(14, 10, 114, 104, c, circle),
                       water(98, -25, 118, 150, c, pear)});
    }
    if (kind == 5) {
      c.coatingOpacity = 0;
      sample.children({water(13, 12, 85, 88, c, circle)});
      c.coatingOpacity = 1;
      sample.children({water(113, 19, 66, 76, c, pear),
                       water(5, 5, 0, 0, c, circle),
                       water(196, 101, 1, 1, c, circle)});
    }
    parts.push_back(std::move(sample));
    return kit::at(index * 226.0f, 0, 208, 176).children(std::move(parts));
  }

  Element describe(int index) const {
    const Controls c = stateFor(index);
    return box()
        .width(1440)
        .height(1040)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .fill(material::linearGradient({0, 0}, {0.25f, 1},
                                       {{0, {0.035f, 0.071f, 0.093f, 1}},
                                        {1, {0.011f, 0.026f, 0.038f, 1}}}))
        .lighting(material::Lighting{
            material::studio({.direction = lightDirection,
                              .elevation = c.lightElevation,
                              .color = material::Color{0.77f, 0.91f, 1, 1},
                              .intensity = 0.58f,
                              .ambient = 0.44f})})
        .children(
            {label("FIELD / 08   •   SURFACE OPTICS", 40, 26, 706, 11, kTeal,
                   600),
             label("WET GLASS / CONSOLE", 37, 48, 1006, 53, kInk, 500)
                 .ink(lettering()),
             label("WATER BEADING OVER AN ACTIVE INSTRUMENT  /  CAPS · FILM · "
                   "CLIPPED LAYERS",
                   40, 120, 1009, 11),
             label("COASTAL OBSERVATORY", 1070, 33, 330, 11, kInk, 600),
             label("CALIBRATION / W–08", 1070, 56, 330, 10),
             label("Analytic optics / generated surfaces", 1070, 79, 330, 10),
             rule(40, 158, 1360), kit::at(console(c), 40, 183, 980, 592),
             kit::at(controlsPanel(index, c), 1068, 183, 332, 592),
             label("BENEATH / LIVE TELEMETRY", 40, 799, 290, 10, kTeal, 600),
             label("ABOVE / STATIC LOCAL LENSES IN MOTION", 385, 799, 543, 10),
             label("36 S LOOP  /  NINE HELD STATES", 1075, 799, 330, 10),
             rule(40, 829, 1360),
             label("FIXED CALIBRATION STRIP", 40, 849, 430, 10, kMuted, 600),
             label("Same chart / visible wet–dry and sampling endpoints", 941,
                   849, 459, 10),
             kit::at(
                 box().width(1360).height(176).children(
                     {coupon(0, "DRY", "No overlay / undisplaced type", 0),
                      coupon(1, "BEAD", "Cap normal / local magnification", 1),
                      coupon(2, "MICRO", "3–12 px / density-aware chart", 2),
                      coupon(3, "MACRO", "Large cap / clipped to glass", 3),
                      coupon(4, "STACK", "Two layers / roughness 1", 4),
                      coupon(5, "ENDPOINTS", "Opacity 0 / 1 · size 0 / 1 px",
                             5)}),
                 40, 868, 1360, 176)});
  }

  void setup(sketch::SketchContext& ctx) {
    for (std::size_t i = 0; i < kDrops.size(); ++i) {
      dropX[i] = motion::animatable(0.0f);
      dropY[i] = motion::animatable(0.0f);
    }
    ctx.canvas(1440, 1040);
    ctx.oversample(1);
    ctx.background({0.011f, 0.026f, 0.038f, 1});
    ctx.captureAt(2);
    phaseIndex = heldPhase < 0 ? 0 : std::clamp(heldPhase, 0, 8);
    ctx.composer.render(describe(phaseIndex));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double seconds =
        std::isfinite(elapsed) ? std::max(0.0, elapsed) : 0.0;
    lightDirection =
        132.0f + 34.0f * static_cast<float>(std::sin(seconds * 0.29));
    for (std::size_t i = 0; i < kDrops.size(); ++i) {
      const float seed = static_cast<float>(i) * 1.73f;
      dropX[i] = controls.driftPixels * 0.22f *
                 static_cast<float>(std::sin(seconds * 0.19 + seed));
      dropY[i] =
          controls.driftPixels *
          static_cast<float>(0.55 * std::sin(seconds * 0.13 + seed) +
                             0.30 * std::sin(seconds * 0.31 + seed * 0.53));
    }
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

SIGIL_SKETCH(
    WetGlassConsole, "Study · Materials",
    "water beads, rivulets and layered glass over a live Compose instrument")
