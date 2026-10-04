/** @file
 * A limestone control tablet: porous faces, rounded raised bands, worn
 * recesses and shaped lettering under a moving conservation lamp.
 * The surface treatment follows carved stone; the instrument and its
 * inscriptions are authored, with fixed comparison islands beside it.
 */

// TAGS: Materials/Lighting, Materials/Surfaces, Interfaces/Controls,
// Compose/Layers

#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/core/Factories.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Stone.h"

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace weave = sigil::weave;
namespace material = sigil::material;
namespace compose = sigil::compose;

namespace stone_relief {

using compose::box;
using compose::Element;
using compose::text;
using material::hexColor;

constexpr float kWidth = 1440, kHeight = 1024;
constexpr float kTabletWidth = 1050, kTabletHeight = 780;
constexpr material::Color kStone{0.79f, 0.73f, 0.62f, 1};
constexpr material::Color kDust{0.60f, 0.53f, 0.41f, 1};
constexpr material::Color kIncision{0.38f, 0.32f, 0.23f, 1};
constexpr std::array<std::string_view, 9> kPhases{
    "GRAZING / WEST", "GRAZING / EAST", "FRONTAL LIGHT",
    "NO LIGHT",       "ZERO RELIEF",    "DEEP RELIEF",
    "POLISHED",       "CHALK ROUGH",    "GREEN-DOWN"};

/** Editable scene data. A nonnegative phase pins the corresponding
 *  two-second comparison; -1 follows the scene clock. */
struct Controls {
  int phase = -1;
  bool moveLight = true;
  float lightBearing = 128;
  float lightElevation = 16;
  float relief = 2.6f;
  float roughness = 0.68f;
  float wear = 0.65f;
  float flow = 0.64f;
  float pressure = 0.37f;
  float exposure = 0.78f;
};

struct State {
  Finish finish;
  int phase = 0;
  bool noLight = false;
  bool operator==(const State&) const = default;
};

Element placed(Element node, float x, float y, float width, float height) {
  node.absolute().left(x).top(y).width(width).height(height);
  return node;
}

std::string decimal(float value, int digits = 2) {
  char out[32];
  std::snprintf(out, sizeof(out), "%.*f", digits, (double)value);
  return out;
}

Element stone(float x, float y, float width, float height, const Finish& finish,
              float heightScale = 1, material::Color tone = kStone,
              float radius = 8, bool round = false) {
  StoneField field{.tone = tone,
                   .origin = {x, y},
                   .radius = radius,
                   .shoulder = std::max(0.5f, std::min(5.0f, height * 0.14f)),
                   .relief = finish.relief * heightScale,
                   .circular = round ? 1.0f : 0.0f};
  return placed(box()
                    .borderRadius({round ? width * 0.5f : radius})
                    .fill(limestone(field, finish)),
                x, y, width, height);
}

Element note(std::string_view label, float size, material::Color colour,
             const weave::Face& face, float tracking = 0) {
  return text(label).font(
      {.face = face, .size = size, .color = colour, .track = tracking});
}

Element inscription(std::string_view label, float size, const weave::Face& face,
                    const Finish& finish, float depth = 0.7f,
                    material::Color tone = kIncision) {
  return note(label, size, {0, 0, 0, 0}, face, size > 30 ? 1.4f : 0.7f)
      .decorationOutline(compose::Boundary::Glyphs)
      .foreground(compose::relief(
          material::from(tone).surface({.roughness = finish.roughness}),
          {.shoulder = std::clamp(size * 0.022f, 0.4f, 1.8f),
           .depth = -depth * finish.relief / 2.6f}));
}

Element dial(const Finish& finish, const weave::Face& serif,
             const weave::Face& mono) {
  Element out = placed(box(), 38, 250, 442, 408);
  out.children({stone(11, 2, 402, 402, finish, 0.55f, kDust, 201, true),
                stone(24, 15, 376, 376, finish, -0.7f, kDust, 188, true),
                stone(48, 39, 328, 328, finish, 1.0f, kStone, 164, true),
                stone(70, 61, 284, 284, finish, -0.25f, kStone, 142, true)});
  for (int i = 0; i < 48; ++i) {
    const float angle = (float)i * 7.5f;
    const float radians = angle * 0.01745329252f;
    const bool major = i % 4 == 0;
    const float length = major ? 20.0f : 9.0f;
    Element mark =
        stone(210 + std::sin(radians) * 178,
              202 - std::cos(radians) * 178 - length * 0.5f,
              major ? 2.5f : 1.2f, length, finish, -0.32f, kIncision, 0.4f);
    mark.rotate(angle);
    out.children({std::move(mark)});
  }
  out.children(
      {placed(inscription("37", 100, serif, finish, 1.0f), 142, 107, 190, 120),
       placed(inscription("BAROMETRIC", 15, mono, finish), 139, 238, 200, 24),
       placed(inscription("PRESSURE / kPa", 12, mono, finish), 135, 265, 205,
              22),
       stone(185, 48, 55, 6, finish, -0.5f, kIncision, 2),
       stone(198, 328, 29, 6, finish, -0.5f, kIncision, 2)});
  return out;
}

Element channel(std::string_view title, std::string_view unit, float value,
                float y, const Finish& finish, const weave::Face& mono) {
  const float fraction = std::clamp(value, 0.0f, 1.0f);
  Element row = stone(530, y, 468, 107, finish, -0.3f, kDust, 7);
  row.children(
      {placed(inscription(title, 15, mono, finish), 22, 13, 180, 26),
       placed(inscription(decimal(fraction * 100, 0) + " " + std::string(unit),
                          15, mono, finish),
              330, 13, 116, 26),
       stone(22, 54, 420, 22, finish, -0.7f, kIncision, 11),
       stone(27, 59, 409 * fraction, 12, finish, -0.12f, kDust, 6)});
  for (int i = 0; i <= 20; ++i)
    row.children({stone(27 + i * 20.3f, 82, 1, i % 5 == 0 ? 7 : 3, finish,
                        -0.15f, kIncision, 0)});
  const float knobX = 21 + fraction * 390;
  row.children({stone(knobX - 2, 43, 36, 46, finish, -0.3f, kIncision, 6),
                stone(knobX, 40, 32, 43, finish, 0.95f, kStone, 5),
                stone(knobX + 8, 47, 1.5f, 27, finish, -0.28f, kDust, 0),
                stone(knobX + 15, 47, 1.5f, 27, finish, -0.28f, kDust, 0),
                stone(knobX + 22, 47, 1.5f, 27, finish, -0.28f, kDust, 0)});
  return row;
}

Element tablet(const State& state, const Controls& controls,
               const weave::Face& serif, const weave::Face& mono,
               const material::Lighting& lighting) {
  const Finish& finish = state.finish;
  Element out =
      stone(40, 118, kTabletWidth, kTabletHeight, finish, 1.8f, kStone, 14);
  out.key("tablet").lighting(state.noLight ? material::Lighting{} : lighting);
  out.children(
      {stone(23, 22, 1004, 736, finish, -0.13f, kStone, 8),
       placed(inscription("FIELD STATION  /  IV", 14, mono, finish), 40, 38,
              450, 28),
       placed(inscription("THE STONE", 70, serif, finish, 1.0f), 37, 69, 680,
              87),
       placed(inscription("OBSERVATORY", 47, serif, finish, 0.8f), 39, 145, 690,
              64),
       placed(inscription("MEASURE THE UNSEEN", 12, mono, finish), 715, 173,
              294, 28),
       stone(40, 216, 970, 2, finish, -0.35f, kIncision, 0),
       stone(40, 222, 970, 1, finish, -0.15f, kDust, 0),
       dial(finish, serif, mono),
       channel("I   /   FLOW", "L/M", controls.flow, 263, finish, mono),
       channel("II  /   PRESSURE", "kPa", controls.pressure, 387, finish, mono),
       channel("III /   EXPOSURE", "%", controls.exposure, 511, finish, mono),
       placed(inscription("CALIBRATION / 37.0", 12, mono, finish), 70, 660, 394,
              25),
       placed(inscription("OPEN CHANNELS · SEALED CHAMBER", 12, mono, finish),
              532, 641, 480, 25),
       stone(40, 690, 970, 2, finish, -0.4f, kIncision, 0)});
  constexpr std::array<std::string_view, 5> actions{
      "OBSERVE", "HOLD", "RELEASE", "CALIBRATE", "ARCHIVE"};
  for (std::size_t i = 0; i < actions.size(); ++i) {
    const float x = 40 + (float)i * 198;
    Element key = stone(x, 709, 178, 42, finish, i == 0 ? -0.48f : 0.65f,
                        i == 0 ? kDust : kStone, 5);
    key.children(
        {placed(inscription(actions[i], 13, mono, finish), 18, 11, 148, 26)});
    out.children({std::move(key)});
  }
  // Quiet pigment remaining in the station cartouche, not a luminous display.
  out.children(
      {stone(891, 39, 108, 102, finish, -0.4f, kDust, 3),
       placed(
           inscription("IV", 42, serif, finish, 0.8f, {0.38f, 0.20f, 0.12f, 1}),
           916, 49, 74, 60),
       placed(inscription("SEALED", 10, mono, finish), 916, 111, 78, 19)});
  return out;
}

Element sample(const Finish& finish, float width = 76, float height = 78) {
  Element out = stone(0, 0, width, height, finish, 1.0f, kStone, 6);
  out.children({stone(10, 12, width - 20, 14, finish, -0.8f, kDust, 3),
                stone(14, 37, width - 28, 27, finish, 0.7f, kStone, 6)});
  return out;
}

material::Lighting lamp(float direction, float elevation,
                        float intensity = 1.5f) {
  return material::studio({.direction = direction,
                           .elevation = elevation,
                           .color = material::Color{1.0f, 0.95f, 0.86f, 1},
                           .intensity = intensity,
                           .ambient = 0.42f});
}

Element comparisonRail(const State& state, const weave::Face& mono) {
  const auto label = [&](std::string_view words, float x, float y,
                         float size = 11.0f) {
    return placed(note(words, size, hexColor(0xA49A85), mono, 0.5f), x, y,
                  270 - x, 25);
  };
  Element rail = placed(box(), 1132, 127, 270, 770);
  rail.children({label("LIGHT REVEALS THE CUT", 0, 0, 13),
                 label("Same stone / same relief", 0, 28),
                 label("WEST", 0, 152), label("FRONT", 90, 152),
                 label("NONE", 180, 152)});
  Finish fixed{.relief = 2.6f, .roughness = 0.68f, .wear = 0.65f};
  for (int i = 0; i < 3; ++i) {
    Element tile = placed(sample(fixed), i * 90.0f, 66, 76, 78);
    tile.lighting(i == 2 ? material::Lighting{}
                         : lamp(128, i == 1 ? 90 : 16, i == 1 ? 0.55f : 1.5f));
    rail.children({std::move(tile)});
  }
  rail.children({label("SHOULDER DEPTH", 0, 207, 13)});
  constexpr std::array<float, 3> relief{0, 6, -6};
  constexpr std::array<std::string_view, 3> names{"FLAT", "RAISED", "SUNK"};
  for (int i = 0; i < 3; ++i) {
    Finish varied = fixed;
    varied.relief = relief[i];
    varied.wear = i == 0 ? 0.0f : fixed.wear;
    rail.children(
        {placed(sample(varied).lighting(lamp(128, 16)), i * 90.0f, 243, 76, 78),
         label(names[i], i * 90.0f, 331)});
  }
  rail.children({label("STONE / POLISH", 0, 383, 13)});
  for (int i = 0; i < 2; ++i) {
    Finish varied = fixed;
    varied.roughness = (float)i;
    rail.children(
        {placed(sample(varied, 120, 78).lighting(lamp(122, 55, 0.62f)),
                i * 135.0f, 419, 120, 78),
         label(i == 0 ? "ROUGH 0" : "ROUGH 1", i * 135.0f, 507)});
  }
  rail.children({label("NORMAL CONVENTIONS", 0, 556, 13)});
  for (int i = 0; i < 3; ++i) {
    Finish varied = fixed;
    varied.directX = i == 1;
    varied.mismatchedNormals = i == 2;
    Element tile = sample(varied).lighting(lamp(90, 18));
    rail.children({placed(std::move(tile), i * 90.0f, 592, 76, 78),
                   label(i == 0   ? "UP"
                         : i == 1 ? "DOWN"
                                  : "MISMATCH",
                         i * 90.0f, 681)});
  }
  rail.children({label(kPhases[state.phase], 0, 733, 13)});
  return rail;
}

Element edges(const Finish& finish, const weave::Face& mono) {
  Element out = placed(box().lighting(lamp(128, 16)), 40, 917, 1050, 60);
  out.children(
      {placed(note("EDGE CONDITIONS", 11, hexColor(0xA49A85), mono, 1), 0, 18,
              184, 24),
       stone(184, 22, 0, 0, finish),
       placed(note("0 × 0", 11, hexColor(0xA49A85), mono), 180, 20, 82, 24),
       stone(278, 21, 8, 8, finish, 1.0f, kStone, 2),
       placed(note("8 px", 11, hexColor(0xA49A85), mono), 304, 20, 83, 24),
       placed(box()
                  .overflow(compose::Overflow::Clip)
                  .children(
                      {stone(-8, -10, 68, 68, finish, 1.0f, kStone, 34, true)}),
              415, 10, 40, 34),
       placed(note("CLIPPED", 11, hexColor(0xA49A85), mono), 472, 20, 114, 24),
       stone(595, 20, 116, 1, finish, -0.25f, kDust, 0),
       placed(note("1 px CUT", 11, hexColor(0xA49A85), mono), 736, 20, 142, 24),
       placed(note("POROUS / LIT", 11, hexColor(0xA49A85), mono), 898, 20, 170,
              24)});
  return out;
}

struct StoneRelief {
  Controls controls;
  motion::Animatable<float> bearing = motion::animatable(128.0f);
  motion::Animatable<float> elevation = motion::animatable(16.0f);
  weave::Face serif;
  weave::Face mono;
  std::optional<State> described;

  State at(float seconds) const {
    const int clockPhase =
        (int)std::floor(std::fmod(std::max(seconds, 0.0f), 18.0f) / 2.0f);
    State state{
        .finish = {.relief = std::clamp(controls.relief, 0.0f, 10.0f),
                   .roughness = std::clamp(controls.roughness, 0.0f, 1.0f),
                   .wear = std::clamp(controls.wear, 0.0f, 1.0f)},
        .phase =
            controls.phase < 0 ? clockPhase : std::clamp(controls.phase, 0, 8)};
    state.noLight = state.phase == 3;
    if (state.phase == 4) {
      state.finish.relief = 0;
      state.finish.wear = 0;
    }
    if (state.phase == 5) state.finish.relief = 8;
    if (state.phase == 6) state.finish.roughness = 0;
    if (state.phase == 7) state.finish.roughness = 1;
    state.finish.directX = state.phase == 8;
    return state;
  }

  void describe(const State& state, sketch::SketchContext& ctx) {
    material::Lighting lighting = lamp(128, 16);
    lighting.lights.front().direction = bearing;
    lighting.lights.front().elevation = elevation;
    if (state.phase == 2) lighting.lights.front().intensity = 0.55f;
    Element page =
        box().width(kWidth).height(kHeight).fill(material::linearGradient(
            {0, 0}, {1, 1}, {hexColor(0x282923), hexColor(0x101511)}));
    page.children(
        {placed(note("LAPIS / SURFACE OBSERVATORY", 13, hexColor(0xCBBE9F),
                     mono, 2),
                42, 37, 830, 28),
         placed(note("RELIEF CONTROL TABLET   /   IV", 11, hexColor(0xA49A85),
                     mono, 0.6f),
                1119, 41, 280, 24),
         placed(box()
                    .borderRadius({16})
                    .fill({0, 0, 0, 0.8f})
                    .filter(material::Filter::blur(14)),
                50, 134, kTabletWidth, kTabletHeight),
         tablet(state, controls, serif, mono, lighting),
         comparisonRail(state, mono), edges(state.finish, mono),
         placed(note("LIGHT " + std::string(kPhases[state.phase]) +
                         "   /   RELIEF " + decimal(state.finish.relief, 1) +
                         "   /   ROUGHNESS " + decimal(state.finish.roughness),
                     11, hexColor(0xCBBE9F), mono, 0.6f),
                42, 982, 1310, 24)});
    ctx.composer.render(std::move(page));
    described = state;
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.background(hexColor(0x101511));
    ctx.oversample(1);
    ctx.captureAt(1.0);
    if (ctx.fonts) {
      serif = ctx.fonts->familyTypeface("Baskerville");
      mono = ctx.fonts->familyTypeface("Menlo");
      if (!serif) serif = ctx.fonts->defaultTypeface();
      if (!mono) mono = ctx.fonts->defaultTypeface();
    }
    describe(at(0), ctx);
  }

  void update(float seconds, sketch::SketchContext& ctx) {
    const State state = at(seconds);
    const float sweep = controls.moveLight ? std::sin(seconds * 0.7f) * 12 : 0;
    bearing = (state.phase == 1 ? 52.0f : controls.lightBearing) + sweep;
    elevation = state.phase == 2 ? 90.0f : controls.lightElevation;
    if (!described || *described != state) describe(state, ctx);
  }
};

}  // namespace stone_relief

SIGIL_SKETCH(stone_relief::StoneRelief, "Study · Materials",
             "Limestone relief controls, engraved type and porous normal maps "
             "under a moving raking lamp.")
