// An industrial diagnostic console with monochrome graphics, a cooling-loop
// topology, signal traces and a machine inquiry. Telemetry is authored fiction.
// TAGS: Interfaces/Film, Typography/Monospace, Materials/CRT, Diagrams/Systems,
// TAGS: Animation/Signals

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Feed.h>
#include <sigilcompose/typography/Presets.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace compose = sigil::compose;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using compose::box;
using compose::Element;
using material::hexColor;
using sigil::draw::Pen;

namespace {
constexpr float kWidth = 1600, kHeight = 1000;
constexpr float kScreenW = 1376, kScreenH = 724;
const auto kGreen = hexColor(0xA9D886);
const auto kDim = hexColor(0x5E8A53);
const auto kFaint = hexColor(0x264829);
const auto kAmber = hexColor(0xE6BA69);
const auto kGround = hexColor(0x040B07);

weave::Face terminalFace() {
  return weave::ports::face({"Courier New", "Courier", "Menlo"});
}

Element words(const std::string& text, float x, float y, float size = 17,
              material::Color color = kGreen, float width = 0) {
  auto node = compose::text(text)
                  .font({.face = terminalFace(), .size = size, .color = color})
                  .at({x, y});
  if (width > 0) node.width(width);
  return node;
}

std::string number(float value, int precision = 1) {
  std::array<char, 48> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.*f", precision, value);
  return buffer.data();
}

// The stock filters supply the optical light. This program supplies the
// tube's coordinate distortion and beam modulation; neither a blur nor a
// colour function expresses those spatial operations.
struct Tube {
  glm::vec2 uSize{kScreenW, kScreenH};
  float uSeconds = 0;
  float uCurvature = 0.022f;
  float uRaster = 0.13f;
  float uNoise = 0.004f;
};
material::Filter tubeFilter(const material::Material& program,
                            motion::Animatable<float> seconds) {
  const std::array<std::string_view, 1> leave{"content"};
  const auto built = material::skia::builder(program, {}, {}, leave);
  if (!built)
    throw std::runtime_error("The tube runtime program could not be built");
  // workaround: recipe filters snapshot their program, so live uniforms
  // are supplied by the direct runtime-program filter.
  auto tube = material::skia::program(sk_ref_sp(built->effect()));
  const Tube settings;
  tube.set("uSize", std::array<float, 2>{settings.uSize.x, settings.uSize.y});
  tube.set("uCurvature", settings.uCurvature);
  tube.set("uRaster", settings.uRaster);
  tube.set("uNoise", settings.uNoise);
  tube.bind("uSeconds", seconds);
  static const auto phosphor = material::Filter::bloom({.sigma = 1.3f,
                                                        .strength = 0.16f,
                                                        .spread = 2.8f,
                                                        .tail = 0.08f,
                                                        .threshold = 0.16f,
                                                        .knee = 0.12f,
                                                        .softness = 0.18f,
                                                        .whitening = 0.04f,
                                                        .dilation = 0.12f,
                                                        .deepening = 0.8f});
  return phosphor.then(tube);
}

void screw(Pen& p, float x, float y) {
  p.noStroke();
  p.fill(hexColor(0x131512));
  p.circle(x + 2, y + 2, 14);
  p.fill(hexColor(0x676B5B));
  p.circle(x, y, 11);
  p.fill(hexColor(0x3D4437));
  p.circle(x, y, 8);
  p.stroke(hexColor(0x161A13));
  p.strokeWeight(2);
  p.line(x - 3, y + 2, x + 3, y - 2);
}

Element hardware() {
  auto cabinet = compose::pen(
      "nostromo.cabinet",
      [](Pen& p) {
        p.noStroke();
        p.fill(material::linearGradient({0, 0}, {0, 1},
                                        {{0, hexColor(0x22241C)},
                                         {0.55f, hexColor(0x141B16)},
                                         {1, hexColor(0x0A110D)}}));
        p.rect(0, 0, kWidth, kHeight);
        p.fill(hexColor(0x090D09));
        p.rect(44, 30, 1512, 916, 14);
        p.fill(material::linearGradient({0, 0}, {0, 1},
                                        {{0, hexColor(0x655F45)},
                                         {0.08f, hexColor(0x514E39)},
                                         {0.64f, hexColor(0x353E2D)},
                                         {1, hexColor(0x454936)}}));
        p.rect(53, 32, 1494, 904, 10);
        p.fill(hexColor(0x20271C));
        p.rect(71, 88, 1458, 774, 28);
        p.fill(hexColor(0x0B140E));
        p.rect(99, 106, 1402, 750, 21);
        p.fill(hexColor(0x030804));
        p.rect(110, 115, 1380, 728, 13);
        p.stroke(hexColor(0x898269));
        p.strokeWeight(1);
        p.line(80, 69, 1507, 69);
        p.line(71, 877, 1528, 877);
        p.stroke(hexColor(0x171E15));
        p.line(77, 72, 1507, 72);
        p.noStroke();
        for (int i = 0; i < 10; ++i) {
          p.fill(hexColor(0x1F281A));
          p.rect(70, 190 + i * 58, 20, 28, 3);
          p.fill(hexColor(i == 2 || i == 7 ? 0xBD8640 : 0x607447));
          p.rect(74, 194 + i * 58, 12, 19, 2);
          p.fill(hexColor(0x1F281A));
          p.rect(1510, 190 + i * 58, 20, 28, 3);
          p.fill(hexColor(i == 3 ? 0xCD944C : 0x54603B));
          p.rect(1514, 194 + i * 58, 12, 19, 2);
        }
        for (int i = 0; i < 44; ++i) {
          const float x = 166 + i * 29.0f;
          p.fill(hexColor(0x252C20));
          p.rect(x, 43, 17, 16, 1);
          p.fill(hexColor(i % 11 == 0 ? 0x936F3E : 0x9A8A51));
          p.rect(x + 3, 46, 11, 10, 1);
          p.fill(hexColor(0x182216));
          p.rect(x, 894, 17, 13, 1);
          p.fill(hexColor(0xAE8950));
          p.rect(x + 3, 897, 11, 6);
        }
        for (float x : {69.0f, 1530.0f})
          for (float y : {49.0f, 97.0f, 850.0f, 917.0f}) screw(p, x, y);
        for (float x : {125.0f, 1475.0f})
          for (float y : {95.0f, 859.0f}) screw(p, x, y);
        for (int i = 0; i < 14; ++i) {
          p.fill(hexColor(0x131B13));
          p.rect(100 + i * 103.0f, 955, 94, 28, 2);
          p.fill(hexColor(i == 13 ? 0x8F5636 : 0x4C5240));
          p.rect(102 + i * 103.0f, 956, 90, 22, 2);
          p.fill(hexColor(0x646652));
          p.rect(102 + i * 103.0f, 956, 90, 2);
        }
        // Deterministic scoring follows the enclosure's edges and fasteners.
        p.stroke(hexColor(0x79745A));
        p.strokeWeight(0.7f);
        for (int i = 0; i < 120; ++i) {
          const float x = 93 + std::fmod(i * 131.31f, 1410.0f);
          const float y = i % 2 ? 77 + (i % 9) : 913 + (i % 11);
          p.line(x, y, x + 4 + i % 15, y - (i % 3));
        }
        p.stroke(hexColor(0x141A11));
        for (int i = 0; i < 18; ++i) p.line(12, 95 + i * 43, 38, 95 + i * 43);
        for (int i = 0; i < 18; ++i)
          p.line(1562, 95 + i * 43, 1588, 95 + i * 43);
      },
      compose::Cache::Texture);
  auto result = box().inset(0).children(
      {cabinet, words("N O S T R O M O", 121, 76, 15, hexColor(0xD0C5A0)),
       words("MU/TH/UR 6000", 1218, 76, 15, hexColor(0xBDB496)),
       words("ENGINEERING / COOLANT SERVICE", 115, 871, 13, hexColor(0xB9AF8C)),
       words("COMMERCIAL TOWING VEHICLE  /  LOCAL DISPLAY 04", 946, 871, 13,
             hexColor(0xB9AF8C)),
       words("PRESSURE CONTAINMENT SYSTEM     DO NOT ISOLATE UNDER LOAD", 400,
             915, 13, hexColor(0xA59C7E))});
  const std::array<const char*, 14> keys{
      "SYS",  "NAV",   "ENG",   "COM",   "CREW",   "QUERY", "HOLD",
      "STEP", "TRACE", "PRINT", "LOCAL", "REMOTE", "ACK",   "ABORT"};
  for (int i = 0; i < 14; ++i)
    result.children(
        {words(keys[i], 119 + i * 103.0f, 958, 12, hexColor(0xD2CCB5))});
  return result;
}

void polyline(Pen& p, std::initializer_list<glm::vec2> points) {
  p.beginShape();
  for (auto v : points) p.vertex(v.x, v.y);
  p.endShape();
}

Element screenGround() {
  return compose::pen(
      "nostromo.screen-structure",
      [](Pen& p) {
        p.noStroke();
        p.fill(kGround);
        p.rect(0, 0, kScreenW, kScreenH);
        p.stroke(kFaint);
        p.strokeWeight(1);
        p.noFill();
        p.rect(34, 122, 304, 308);
        p.rect(360, 122, 630, 308);
        p.rect(1012, 122, 330, 308);
        p.rect(34, 454, 642, 218);
        p.rect(698, 454, 644, 218);
        p.line(34, 93, 1342, 93);
        p.line(34, 692, 1342, 692);
        p.line(34, 151, 338, 151);
        p.line(360, 151, 990, 151);
        p.line(1012, 151, 1342, 151);
        p.line(34, 484, 676, 484);
        p.line(698, 484, 1342, 484);
        for (int i = 0; i < 11; ++i)
          p.line(1057 + i * 25, 208, 1057 + i * 25, 265);
        for (int i = 0; i < 11; ++i)
          p.line(1057 + i * 25, 300, 1057 + i * 25, 357);
        for (int i = 0; i < 4; ++i) {
          p.line(1057, 208 + i * 19, 1328, 208 + i * 19);
          p.line(1057, 300 + i * 19, 1328, 300 + i * 19);
        }
        for (int i = 0; i < 18; ++i) {
          p.line(729 + i * 34.0f, 505, 729 + i * 34.0f, 643);
          p.line(716, 505 + i * 8.0f, 1327, 505 + i * 8.0f);
        }
        p.stroke(kDim);
        for (int i = 0; i < 26; ++i) {
          p.line(382 + i * 22, 167, 382 + i * 22, 171);
          p.line(382 + i * 22, 411, 382 + i * 22, 415);
        }
      },
      compose::Cache::Texture);
}

Element systemDiagram() {
  auto layout = compose::pen(
      "nostromo.cooling-diagram",
      [](Pen& p) {
        p.noFill();
        p.strokeWeight(1);
        p.stroke(kFaint);
        polyline(p, {{430, 197},
                     {572, 179},
                     {761, 179},
                     {925, 201},
                     {960, 230},
                     {960, 339},
                     {925, 368},
                     {761, 392},
                     {572, 392},
                     {430, 370},
                     {391, 341},
                     {391, 230},
                     {430, 197}});
        for (int i = 0; i < 4; ++i)
          p.line(441, 204 + i * 52, 939, 204 + i * 52);
        for (int i = 0; i < 7; ++i)
          p.line(450 + i * 78, 192, 450 + i * 78, 379);
        p.stroke(kGreen);
        p.strokeWeight(1.6f);
        // Paired pressure lines make the flow direction legible without colour.
        polyline(p, {{463, 222},
                     {566, 222},
                     {566, 197},
                     {791, 197},
                     {791, 222},
                     {893, 222}});
        polyline(p, {{463, 348},
                     {566, 348},
                     {566, 373},
                     {791, 373},
                     {791, 348},
                     {893, 348}});
        polyline(p, {{450, 239},
                     {450, 267},
                     {527, 267},
                     {527, 304},
                     {450, 304},
                     {450, 331}});
        polyline(p, {{906, 239},
                     {906, 267},
                     {831, 267},
                     {831, 304},
                     {906, 304},
                     {906, 331}});
        p.line(587, 223, 766, 223);
        p.line(587, 346, 766, 346);
        p.line(615, 247, 615, 322);
        p.line(739, 247, 739, 322);
        p.line(657, 247, 657, 322);
        p.line(699, 247, 699, 322);
        p.stroke(kDim);
        p.strokeWeight(1);
        for (int i = 0; i < 16; ++i) {
          const float x = 611 + i * 8.5f;
          p.line(x, 248, x - 10, 266);
          p.line(x, 304, x - 10, 322);
        }
        p.fill(kGround);
        p.stroke(kGreen);
        for (auto v :
             {glm::vec2{450, 222}, {906, 222}, {450, 348}, {906, 348}}) {
          p.circle(v.x, v.y, 34);
          p.line(v.x - 7, v.y, v.x + 7, v.y);
          p.line(v.x, v.y - 7, v.x, v.y + 7);
        }
        p.rect(582, 214, 190, 33);
        p.rect(582, 323, 190, 33);
        p.rect(580, 274, 194, 23);
        p.stroke(kAmber);
        p.circle(677, 286, 18);
        p.line(670, 280, 684, 293);
        p.line(684, 280, 670, 293);
        p.noFill();
        p.stroke(kDim);
        for (int i = 0; i < 7; ++i) {
          const float x = 467 + i * 21;
          p.rect(x, 281, 14, 13);
          p.line(x + 2, 294, x + 12, 281);
        }
        for (int i = 0; i < 7; ++i) {
          const float x = 792 + i * 21;
          p.rect(x, 281, 14, 13);
          p.line(x + 2, 294, x + 12, 281);
        }
        p.stroke(kGreen);
        for (auto v : {glm::vec2{540, 222},
                       {610, 197},
                       {750, 197},
                       {810, 348},
                       {739, 373},
                       {600, 373}}) {
          p.line(v.x - 4, v.y - 4, v.x, v.y);
          p.line(v.x, v.y, v.x - 4, v.y + 4);
        }
        p.stroke(kAmber);
        p.strokeWeight(1);
        p.line(686, 286, 801, 286);
        p.line(801, 286, 801, 391);
        p.line(801, 391, 948, 391);
        p.noStroke();
        p.fill(kAmber);
        p.rect(944, 388, 7, 7);
      },
      compose::Cache::Texture);
  return box().inset(0).children(
      {layout, words("01  PRIMARY CIRCULATION", 373, 128, 16),
       words("P-01", 419, 180, 13, kDim), words("P-02", 878, 180, 13, kDim),
       words("P-03", 419, 374, 13, kDim), words("P-04", 878, 374, 13, kDim),
       words("HEAT EXCHANGER", 603, 221, 14),
       words("RETURN MANIFOLD", 599, 330, 14),
       words("BYPASS", 596, 277, 13, kAmber),
       words("RADIATOR / PORT", 407, 308, 12, kDim),
       words("RADIATOR / STBD", 816, 308, 12, kDim),
       words("V-17 STICKING", 813, 398, 12, kAmber),
       words("FLOW  --->       CLOSED LOOP / 4.8 BAR", 391, 432, 12, kDim)});
}

Element signalTraces() {
  return compose::pen(
      "nostromo.traces",
      [](Pen& p) {
        p.noFill();
        p.strokeWeight(1.4f);
        p.stroke(kGreen);
        p.beginShape();
        for (int i = 0; i <= 270; ++i) {
          const float x = 1057 + i;
          const float pulse = std::exp(
              -std::pow((std::fmod(i + 17.0f, 53.0f) - 13) / 3.0f, 2.0f));
          p.vertex(x, 240 - 22 * pulse + 3 * std::sin(i * 0.20f));
        }
        p.endShape();
        p.stroke(kAmber);
        p.beginShape();
        for (int i = 0; i <= 270; ++i)
          p.vertex(1057 + i,
                   333 + 10 * std::sin(i * 0.084f) + 4 * std::sin(i * 0.23f));
        p.endShape();
        p.stroke(kDim);
        p.strokeWeight(1);
        for (int row = 0; row < 5; ++row) {
          p.beginShape();
          for (int i = 0; i <= 600; i += 3) {
            const float burst =
                std::exp(-std::pow((i - 275.0f - row * 18) / 28, 2.0f));
            const float y = 517 + row * 29 + 3 * std::sin(i * 0.11f + row) +
                            burst * 12 * std::sin(i * 0.63f);
            p.vertex(725 + i, y);
          }
          p.endShape();
        }
        p.stroke(kAmber);
        p.strokeWeight(1.5f);
        p.line(1030, 502, 1030, 643);
        p.line(1125, 502, 1125, 643);
        p.stroke(kDim);
        p.strokeWeight(1);
        for (float y : {516.0f, 545.0f, 574.0f, 603.0f, 632.0f}) {
          p.line(717, y, 724, y);
          p.line(1327, y, 1335, y);
        }
      },
      compose::Cache::Texture);
}

Element staticType() {
  auto result = box().inset(0).children(
      {words("MU/TH/UR", 34, 24, 29),
       words("6000", 216, 26, 25, kDim),
       words("NOSTROMO", 443, 22, 29),
       words("180924609", 445, 59, 17, kDim),
       words("LOCAL 04 / ENG", 1110, 24, 19),
       words("ACCESS: SERVICE", 1099, 58, 15, kDim),
       words("INTERFACE 2037 / ENGINEERING SYSTEMS INQUIRY", 35, 64, 15),
       words("00  SYSTEM STATUS", 47, 128, 16),
       words("02  SIGNAL / SUPPLY", 1025, 128, 16),
       words("INQUIRY CHANNEL A / LOCAL", 47, 460, 16),
       words("03  RETURN-LINE CORRELATION", 711, 460, 16),
       words("CH A  PRESSURE", 1027, 172, 14, kDim),
       words("BAR", 1294, 178, 12, kDim),
       words("CH B  VALVE RESPONSE", 1027, 278, 14, kDim),
       words("MS", 1304, 280, 12, kDim),
       words("SAMPLE  02.40 KHZ", 1027, 380, 14, kDim),
       words("WINDOW  250.0 MS", 1027, 403, 14, kDim),
       words("A", 704, 508, 12, kDim),
       words("B", 704, 537, 12, kDim),
       words("C", 704, 566, 12, kDim),
       words("D", 704, 595, 12, kDim),
       words("E", 704, 624, 12, kDim),
       words("0", 723, 650, 12, kDim),
       words("125", 1008, 650, 12, kDim),
       words("250 MS", 1269, 650, 12, kDim),
       words("CORRELATION DELAY +14.3 MS", 962, 432, 12, kAmber),
       words("F1 SYS   F2 QUERY   F3 TRACE   F4 HOLD", 35, 700, 14, kDim),
       words("TOWING VEHICLE / AUTOMATIC FLIGHT", 973, 700, 14, kDim)});
  const std::array<const char*, 8> names{
      "REACTOR CORE", "COOLANT A", "COOLANT B",  "HYPERSLEEP",
      "FLIGHT DATA",  "ANTENNA",   "CARGO LOCK", "TOW COUPLING"};
  const std::array<const char*, 8> state{"NOMINAL", "NOMINAL", "BYPASS",
                                         "7 / 7",   "SYNCED",  "LOCKED",
                                         "SEALED",  "ENGAGED"};
  for (int i = 0; i < 8; ++i) {
    const float y = 167 + i * 28;
    result.children({words(names[i], 47, y, 15, kDim),
                     words(state[i], 229, y, 15, i == 2 ? kAmber : kGreen)});
  }
  result.children({words("FAULTS  01 / ADVISORY", 47, 405, 14, kAmber)});
  return result;
}

struct NostromoMonitor {
  struct InquiryLine {
    std::string text;
    float arrived = 0;
    bool operator==(const InquiryLine&) const = default;
  };
  motion::Animatable<float> seconds = motion::animatable(0.0f);
  compose::feed::Ring<InquiryLine> log{16};
  int tick = -1;
  int received = -1;

  void receive(int event) {
    static const std::array<const char*, 11> lines{
        "> EXAMINE SECONDARY COOLANT RETURN",
        "  ADDRESS  0041:01A0 / SUBSYSTEM 2",
        "  PRESSURE DIFFERENTIAL.... 0.41 BAR",
        "  ACTUATOR V-17............ DELAYED",
        "  BACKUP LOOP.............. AVAILABLE",
        "  MAIN CIRCUIT............ RETAINED",
        "  BYPASS V-09.............. ENABLED",
        "  RESPONSE WITHIN SERVICE TOLERANCE",
        "  LOCAL INSPECTION REQUEST LOGGED",
        "  TRANSMIT TO ENGINEERING WATCH",
        "  PRIORITY 3 / NO CREW WAKE REQUIRED"};
    while (received < event) {
      ++received;
      log.append({lines[received % lines.size()],
                  std::max(0.0f, (received - 4) * 3.0f)});
    }
  }

  Element movingSignals() const {
    auto result = box().inset(0);
    // The lines and curves are retained. Only signal points and shutter
    // masks move, each derived from the one scene clock.
    for (int i = 0; i < 3; ++i) {
      result.children({box()
                           .rect(584, 195 + i * 88.0f, 7, 4)
                           .fill(i == 1 ? kAmber : kGreen)
                           .translateX(motion::bind(
                               seconds, {.from = {i * 0.7f, 4.8f + i * 0.7f},
                                         .to = {0, 177},
                                         .wrap = 177}))});
    }
    result.children(
        {box()
             .rect(1057, 206, 2, 59)
             .fill(kGreen)
             .opacity(0.45f)
             .translateX(motion::bind(
                 seconds, {.from = {0, 2.7f}, .to = {0, 270}, .wrap = 270})),
         box()
             .rect(1057, 299, 2, 59)
             .fill(kAmber)
             .opacity(0.35f)
             .translateX(motion::bind(
                 seconds, {.from = {0.9f, 4.1f}, .to = {0, 270}, .wrap = 270})),
         box()
             .rect(725, 501, 2, 144)
             .fill(kGreen)
             .opacity(0.25f)
             .translateX(motion::bind(
                 seconds, {.from = {0, 8.1f}, .to = {0, 600}, .wrap = 600})),
         box()
             .rect(319, 176, 6, 6)
             .fill(kGreen)
             .opacity(motion::bind(
                 seconds, {.from = {0, 1.7f},
                           .envelope = motion::envelope::square(0.24f)})),
         box()
             .rect(319, 232, 6, 6)
             .fill(kAmber)
             .opacity(motion::bind(
                 seconds, {.from = {0.6f, 3.0f},
                           .envelope = motion::envelope::square(0.72f)})),
         box()
             .rect(150, 641, 9, 16)
             .fill(kGreen)
             .opacity(motion::bind(
                 seconds, {.from = {0, 0.92f},
                           .envelope = motion::envelope::square(0.57f)}))});
    return result;
  }

  Element readings(int step) const {
    const float time = step * 0.6f;
    const float pressure = 4.78f + 0.03f * std::sin(time * 0.51f);
    const float lag = 14.3f + 0.2f * std::sin(time * 0.29f);
    return box().inset(0).children(
        {words(number(pressure, 2), 1190, 164, 25),
         words(number(lag), 1210, 270, 24, kAmber),
         words("SCAN " + number(440 + step, 0), 1165, 95, 12, kDim),
         words("ENGINEERING CYCLE  " + number(step / 10.0f, 1), 40, 436, 12,
               kDim)});
  }

  Element inquiry() const {
    auto print = compose::textFx::typeOn();
    print.duration = std::chrono::milliseconds(70);
    print.delay = motion::stagger(std::chrono::milliseconds(15));
    auto rows = compose::feed::feed(
                    log, {.visible = 6, .gap = 0},
                    [print, clock = seconds](const InquiryLine& line) {
                      const bool advisory =
                          line.text.find("DELAYED") != std::string::npos ||
                          line.text.find("BYPASS") != std::string::npos;
                      auto progress = motion::bind(
                          clock, {.from = {line.arrived, line.arrived + 0.7f},
                                  .to = {0, 1}});
                      return compose::text(line.text)
                          .font({.face = terminalFace(),
                                 .size = 15,
                                 .color = advisory ? kAmber : kGreen})
                          .height(22)
                          .textFx(compose::textFx::entrance(
                              print, {.progress = progress}));
                    })
                    .rect(48, 499, 615, 132);
    return box().inset(0).children({rows, words("READY", 48, 642, 16)});
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.background(hexColor(0x0B110D));
    ctx.captureAt(8.4);
    receive(4);
    const auto tube = material::shader(ctx.assets.hub(), ctx.local("Tube.sksl"),
                                       Tube{}, {.textures = {{"content", {}}}});
    ctx.composer.render(box().inset(0).children(
        {hardware(),
         box()
             .rect(112, 117, kScreenW, kScreenH)
             .borderRadius({10})
             .overflow(compose::Overflow::Clip)
             .children({screenGround(), systemDiagram(), signalTraces(),
                        staticType(), movingSignals(),
                        compose::slot("nostromo.readings"),
                        compose::slot("nostromo.inquiry")})
             .filter(tubeFilter(tube, seconds))}));
    tick = 0;
    ctx.composer.renderSlot("nostromo.readings", readings(tick));
    ctx.composer.renderSlot("nostromo.inquiry", inquiry());
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    seconds = static_cast<float>(elapsed);
    const int now = static_cast<int>(elapsed / 0.6);
    if (now == tick) return;
    tick = now;
    ctx.composer.renderSlot("nostromo.readings", readings(tick));
    const int event = 4 + tick / 5;
    if (event > received) {
      receive(event);
      ctx.composer.renderSlot("nostromo.inquiry", inquiry());
    }
  }
};
}  // namespace

SIGIL_SKETCH(NostromoMonitor, "Study · Film",
             "Alien — a MU/TH/UR service console with original cooling-loop "
             "telemetry, CRT optics and asynchronous diagnostic signals")
