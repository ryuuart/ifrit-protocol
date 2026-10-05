/** Painted height and wetness shared by stone, steel and bronze Compose panels.
 */
// TAGS: Compose/Texture, Materials/Stone, Materials/Metal, Brushes/Painted
// data, Typography/Material ink

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigildraw/Graphics.h>
#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include "Fields.h"
#include "Surfaces.h"

namespace compose = sigil::compose;
namespace draw = sigil::draw;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
using compose::Element;
using material::hexColor;
using painted_fields::Controls;
using painted_fields::Finish;

namespace {
constexpr int kWidth = 1440, kHeight = 1040;
constexpr float kPi = 3.14159265358979323846f;
constexpr material::Color kGround = hexColor(0xeee9de);
constexpr material::Color kInk = hexColor(0x293c37);
constexpr material::Color kMuted = hexColor(0x6d7972);
constexpr material::Color kAccent = hexColor(0xb6683d);
constexpr int heldPhase = -1;
constexpr std::array<Controls, 8> kStates{{
    {11, .8f, .82f, .13f},
    {0, .8f, .82f, .13f},
    {-14, .8f, .82f, .13f},
    {11, 0, .82f, .13f},
    {11, 1, .82f, 0},
    {11, 1, 1, 1},
    {17, .9f, .82f, .13f},
    {11, 0, .82f, .13f},
}};
constexpr std::array<std::string_view, 8> kNames{
    "01 / PAINTED FIELDS", "02 / ZERO IMPRESSION", "03 / RECESSED TYPE",
    "04 / DRY SURFACE",    "05 / GLOSS ENDPOINT",  "06 / MATTE ENDPOINT",
    "07 / ERASE & CLIP",   "08 / CLEAR THE CANVAS"};

Element label(std::string_view value, float x, float y, float width,
              float size = 12, material::Material ink = kMuted) {
  const int lines = 1 + std::count(value.begin(), value.end(), '\n');
  return compose::kit::at(compose::text(std::string(value)), x, y, width,
                          size * 1.55f * lines)
      .fontSize(size)
      .ink(std::move(ink));
}
Element rule(float x, float y, float width, float height = 1,
             material::Material ink = hexColor(0xc6cec4)) {
  return compose::kit::at(x, y, width, height).fill(std::move(ink));
}
Element tickScale(float x, float y, float width) {
  Element out = compose::kit::at(x, y, width, 15);
  for (int i = 0; i <= 32; ++i)
    out.children({rule(i * width / 32, 0, 1, i % 8 == 0 ? 13 : 5)});
  return out;
}
}  // namespace

struct PaintedFields {
  draw::Graphics height{painted_fields::kFieldWidth,
                        painted_fields::kFieldHeight};
  draw::Graphics water{painted_fields::kFieldWidth,
                       painted_fields::kFieldHeight};
  std::shared_ptr<material::UniformBlock> wetness =
      std::make_shared<material::UniformBlock>(1);
  Controls controls;
  int phase = 0;
  int paintedPhase = -1;
  float pulse = .8f;

  material::Material finish(Finish value, float width,
                            float panelHeight) const {
    return painted_fields::surface(
        value, controls,
        painted_fields::placed(height.image(), width, panelHeight),
        painted_fields::placed(water.image(), width, panelHeight), wetness);
  }

  Element map(std::string_view name, material::Material value, float y) const {
    return compose::kit::at(compose::stack(), 56, y, 218, 112)
        .children({label(name, 0, 0, 218, 10), compose::kit::at(0, 23, 218, 85)
                                                   .fill(std::move(value))
                                                   .borderRadius(2)});
  }

  Element sidebar() const {
    auto heightMap =
        material::image(painted_fields::placed(height.image(), 218, 85));
    auto waterMap =
        material::image(painted_fields::placed(water.image(), 218, 85));
    auto normal = material::surface::normalFromHeight(
        material::image(painted_fields::placed(height.image(), 1064, 432)),
        {.depth = controls.depth, .step = .8f, .directX = true});
    struct Preview {
      glm::vec2 scale;
    };
    normal =
        material::shader(
            "half4 main(float2 p) { return field.eval(p * scale); }",
            Preview{{1064.f / 218, 432.f / 85}}, {.textures = {{"field", {}}}})
            .slot("field", normal);
    std::array<char, 120> values{};
    const auto hero = painted_fields::parameters(Finish::Steel, controls);
    std::snprintf(
        values.data(), values.size(),
        "IMPRESSION  %+.0f px\nDRY ROUGHNESS  %.2f\nWET ROUGHNESS  %.2f",
        controls.depth, hero.dryRoughness, hero.wetRoughness);
    Element out = compose::stack().children(
        {label("INPUT CANVASES", 56, 182, 218, 11, kInk),
         map("A / PAINTED HEIGHT", heightMap, 218),
         map("B / WATER COVERAGE", waterMap, 349),
         map("C / HEIGHT NORMAL", normal, 480), rule(56, 621, 218),
         label(values.data(), 56, 642, 226, 11, kInk),
         label("WETNESS UNIFORM", 56, 716, 218, 10),
         rule(56, 743, 218, 5, hexColor(0xcdd3c9)),
         rule(56, 743, std::max(1.f, 218 * pulse), 5, kAccent),
         rule(56, 912, 218), label("32 SEC / 8 STATES", 56, 934, 218, 10),
         label("A brush writes coverage.\nThe surface interprets it.", 56, 774,
               218, 14, kInk),
         label("Same pixels in every finish.\nWet stone darkens; metal does "
               "not.",
               56, 836, 218, 11)});
    return out;
  }

  Element coupon(Finish value, std::string_view name, std::string_view detail,
                 float x) const {
    return compose::kit::at(compose::stack(), x, 752, 338, 174)
        .children({label(name, 0, 0, 338, 11, kInk),
                   compose::kit::at(0, 27, 338, 108)
                       .fill(finish(value, 338, 108))
                       .borderRadius(3),
                   label(detail, 0, 147, 338, 10)});
  }

  Element describe() const {
    Element panel = compose::kit::at(compose::stack(), 320, 225, 1064, 432)
                        .fill(finish(Finish::Steel, 1064, 432))
                        .borderRadius(7)
                        .overflow(compose::Overflow::Clip);
    panel.children({rule(24, 22, 60, 1.4f, hexColor(0xb7beb0)),
                    label("BRUSHED STEEL / SHARED PAINT", 104, 15, 760, 10,
                          hexColor(0xc7cfbe)),
                    label("08", 988, 16, 58, 12, hexColor(0xd0d8c8))});
    for (const float x : {18.f, 1046.f})
      for (const float y : {18.f, 414.f})
        panel.children({compose::kit::at(x - 3, y - 3, 6, 6)
                            .borderRadius(3)
                            .fill(finish(Finish::Steel, 6, 6))});
    Element page =
        compose::stack().width(kWidth).height(kHeight).fill(kGround).fontFamily(
            "Inter, Helvetica Neue, sans-serif");
    page.children({label("FIELDWORK", 56, 38, 900, 58, kInk)
                       .fontFamily("Georgia, Times New Roman, serif"),
                   label("MATERIAL CANVASES\nPAINT / IMPRESSION / WATER", 1112,
                         52, 272, 11, kInk),
                   rule(56, 130, 1328, 1.3f, kAccent),
                   label(kNames[phase], 320, 174, 760, 12, kInk), sidebar(),
                   panel, tickScale(320, 676, 1064),
                   label("Paint becomes the surface.", 320, 700, 740, 24, kInk)
                       .fontFamily("Georgia, Times New Roman, serif"),
                   label("HEIGHT + COVERAGE → SURFACE", 1112, 711, 272, 10),
                   coupon(Finish::Stone, "01 / ABSORBENT STONE",
                          "Darkened pores where wet", 320),
                   coupon(Finish::Steel, "02 / BRUSHED STEEL",
                          "No absorption darkening · directional grooves", 684),
                   coupon(Finish::Bronze, "03 / BRONZE ALLOY",
                          "Same maps · warm conductor response", 1048),
                   rule(320, 958, 1064, 1, kAccent),
                   label("PAINTED HEIGHT / LIVE WETNESS / CLIP & ERASE / CLEAR",
                         320, 984, 1050, 10, kInk)});
    return page;
  }

  void paint(draw::Pen& pen) {
    const bool form =
        height.extent() !=
        SkISize::Make(std::max(1, static_cast<int>(std::round(
                                      height.width() * pen.contentScale()))),
                      std::max(1, static_cast<int>(std::round(
                                      height.height() * pen.contentScale()))));
    const int fieldPhase = phase < 6 ? 0 : phase;
    if (paintedPhase != fieldPhase || form) {
      auto& heightPen = height.begin(pen);
      painted_fields::paintHeight(heightPen, phase);
      height.end();
      auto& waterPen = water.begin(pen);
      painted_fields::paintWetness(waterPen, phase);
      water.end();
      paintedPhase = fieldPhase;
    }
    pen.element(describe(), 0, 0, kWidth, kHeight);
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.background(kGround);
    ctx.captureAt(2);
    update(0, ctx);
    ctx.composer.render(compose::custom("painted-fields",
                                        [this](draw::Pen& pen) { paint(pen); })
                            .width(kWidth)
                            .height(kHeight)
                            .cache(compose::Cache::None));
  }

  void update(double elapsed, sketch::SketchContext&) {
    const double seconds = std::isfinite(elapsed) ? std::max(0., elapsed) : 0.;
    const float t = static_cast<float>(std::fmod(seconds, 32.));
    phase = heldPhase < 0 ? std::min(7, static_cast<int>(t / 4))
                          : std::clamp(heldPhase, 0, 7);
    controls = kStates[phase];
    pulse = controls.wetness;
    if (phase < 3 || phase == 6)
      pulse *= .82f + .18f * std::sin(t * 2 * kPi / 8);
    if (wetness->values()[0] != pulse) {
      wetness->values()[0] = pulse;
      wetness->commit();
    }
  }
};

SIGIL_SKETCH(
    PaintedFields, "Study · Materials",
    "Painted height and wetness drive shared stone and metal Compose surfaces")
