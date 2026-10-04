/** Real Compose letters and layered brush marks shape stone and chased copper.
 */
// TAGS: Compose/Materials, Materials/Stone, Materials/Metal,
// Typography/Material ink, Brushes/Layered, Materials/Lighting

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilweave/query/Selector.h>

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

#include "Fields.h"

namespace compose = sigil::compose;
namespace draw = sigil::draw;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

namespace carved_marks {

using compose::Element;
using material::hexColor;
constexpr int kWidth = 1440, kHeight = 1120;
constexpr int kFieldWidth = 620, kFieldHeight = 292;
constexpr float kLoop = 24, kPi = 3.14159265358979323846f;
constexpr auto kPaper = hexColor(0xeee9dc);
constexpr auto kInk = hexColor(0x28372f);
constexpr auto kMuted = hexColor(0x697468);
constexpr auto kRule = hexColor(0xc0c5b6);
constexpr auto kCopper = hexColor(0xb16d46);
constexpr auto kGold = hexColor(0xe6bb70);
constexpr material::Color kWhite{1, 1, 1, 1}, kBlack{0, 0, 0, 1};
constexpr std::array<std::string_view, 6> kNames{
    "01 / FRONT · CUT & CHASE",  "02 / RAKE FROM LEFT",
    "03 / RAKE FROM RIGHT",      "04 / ZERO MACRO · GRAIN REMAINS",
    "05 / HALF GILT · INK WIPE", "06 / UNITS · SPLIT & MIRROR"};

struct Controls {
  float depth = -2.8f;
  float detail = .35f;
  float roughness = .52f;
  float gilding = .72f;
  float bearing = 132;
  float elevation = 84;
  bool normals = true;
};

struct Finish {
  float depth = -2.8f, detail = .35f, roughness = .52f, gilding = .72f;
  bool normals = true;
  bool operator==(const Finish&) const = default;
};

struct State {
  int phase = 0;
  Finish finish;
  float bearing = 132, elevation = 84;
};

struct Slider {
  std::string_view name;
  float low, high;
};
constexpr std::array<Slider, 6> kSliders{{{"CUT DEPTH / PX", -5, 5},
                                          {"GRAIN / PX", 0, 1},
                                          {"COPPER ROUGHNESS", 0, 1},
                                          {"GILT / OPACITY", 0, 1},
                                          {"LIGHT BEARING / DEG", 0, 360},
                                          {"LIGHT ELEVATION / DEG", 0, 90}}};

float bounded(float value, float low, float high, float fallback) {
  return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}

Element label(std::string_view content, float x, float y, float width,
              float size = 11, material::Color color = kMuted) {
  const auto lines = 1 + std::count(content.begin(), content.end(), '\n');
  return compose::kit::at(compose::text(std::string(content)), x, y, width,
                          size * 1.5f * lines)
      .fontSize(size)
      .ink(color);
}

Element rule(float x, float y, float width, material::Color color = kRule,
             float thickness = 1) {
  return compose::kit::at(x, y, width, thickness).fill(color);
}

Element button(std::string_view content, float x, float width, bool active) {
  return compose::kit::at(compose::stack(), x, 582, width, 32)
      .fill(active ? kInk : kPaper)
      .borderRadius(3)
      .stroke(compose::stroke(1, compose::Fill::color(kRule)))
      .children(
          {label(content, 12, 8, width - 24, 10, active ? kPaper : kInk)});
}

material::Material sampled(material::Texture texture, glm::vec2 extent) {
  const glm::vec2 pixels(texture.size());
  glm::mat3 placement(1);
  placement[0][0] = extent.x / std::max(pixels.x, 1.f);
  placement[1][1] = extent.y / std::max(pixels.y, 1.f);
  return material::image(texture.uv(placement)
                             .tile(material::Repeat::Pad)
                             .sampling(material::Sampling::Linear));
}

material::Mask marked(const material::Material& height) {
  return {height, material::MaskChannel::Luminance};
}

material::Material unitColor(glm::vec2 extent) {
  struct Parameters {
    glm::vec2 extent;
  };
  return material::shader(R"(
half4 main(float2 p) {
  float2 uv = p / max(extent, float2(.0001));
  float stripe = .5 + .5 * sin(uv.x * 6.2831853 + uv.y * 2.1);
  float3 color = mix(float3(.42, .21, .10), float3(.94, .73, .37), stripe);
  return half4(color, 1);
})",
                          Parameters{extent});
}

}  // namespace carved_marks

struct CarvedMarks {
  using Element = compose::Element;
  using State = carved_marks::State;
  using Finish = carved_marks::Finish;

  carved_marks::Controls controls;
  int heldPhase = -1;
  std::optional<State> manual;
  float seconds = 0, fieldDensity = 0;
  bool pointerDown = false, treeDirty = true;
  int describedPhase = -1;
  weave::FontContext* fonts = nullptr;
  std::shared_ptr<compose::TextureScene> heightScene, normalScene;
  uint64_t heightRevision = 0, normalRevision = 0;
  material::EnvironmentMap room;
  material::Environment surroundings;
  material::Material height{carved_marks::kBlack};
  material::Material stoneFace{carved_marks::kPaper};
  material::Material copperFace{carved_marks::kCopper};
  std::array<material::Material, 3> unitInks{
      carved_marks::kCopper, carved_marks::kCopper, carved_marks::kCopper};
  std::optional<Finish> preparedFinish;
  Element fibreArt = compose::stack();
  Element heightTree = compose::stack();
  Element tree = compose::stack();
  path::Outline pressure, fibre, crossing;
  motion::Animatable<float> bearing = motion::animatable(132.f);
  motion::Animatable<float> elevation = motion::animatable(84.f);
  motion::Animatable<float> reflection = motion::animatable(0.f);
  motion::Animatable<float> sourceX = motion::animatable(220.f);
  motion::Animatable<float> sourceY = motion::animatable(198.f);
  motion::Animatable<float> sourceZ = motion::animatable(260.f);

  State state() const {
    using namespace carved_marks;
    State value;
    value.phase = heldPhase < 0 ? std::min(5, int(seconds / 4))
                                : std::clamp(heldPhase, 0, 5);
    value.finish = {controls.depth, controls.detail, controls.roughness,
                    controls.gilding, controls.normals};
    value.bearing = controls.bearing;
    value.elevation = controls.elevation;
    if (value.phase == 1) {
      value.bearing = 150;
      value.elevation = 18;
    } else if (value.phase == 2) {
      value.bearing = 30;
      value.elevation = 18;
    } else if (value.phase == 3) {
      value.finish.depth = 0;
    } else if (value.phase == 4) {
      value.finish.gilding = .5f;
    }
    if (manual) value = *manual;
    value.finish.depth = bounded(value.finish.depth, -5, 5, -2.8f);
    value.finish.detail = bounded(value.finish.detail, 0, 1, .35f);
    value.finish.roughness = bounded(value.finish.roughness, 0, 1, .52f);
    value.finish.gilding = bounded(value.finish.gilding, 0, 1, .72f);
    value.bearing = bounded(value.bearing, 0, 360, 132);
    value.elevation = bounded(value.elevation, 0, 90, 84);
    return value;
  }

  void prepareArt() {
    using namespace carved_marks;
    pressure = path::Outline::svg(
        "M24 252 C136 204 225 275 315 239 C420 189 501 259 600 218");
    fibre = path::Outline::svg(
        "M18 224 C151 184 220 270 325 221 C442 170 509 245 608 193");
    crossing = path::Outline::svg(
        "M104 284 C168 221 332 199 491 256 C532 269 562 275 602 268");
    fibreArt = compose::stack().width(420).height(48);
    for (int i = 0; i < 18; ++i) {
      char spine[160];
      const float y = 2.5f + i * 2.4f;
      std::snprintf(spine, sizeof(spine), "M0 %.2f C88 %.2f 276 %.2f 420 %.2f",
                    y, y - 3 + std::sin(float(i)) * 2, y + 3, y - 1);
      compose::brush::Ribbon strand;
      strand.width = path::Profile{
          {0, .1f}, {.12f, .8f}, {.43f, 1.3f}, {.76f, .7f}, {1, .1f}};
      strand.step = 1;
      const auto band =
          path::fromSk(strand.band(path::toSk(path::Outline::svg(spine))));
      fibreArt.children({compose::kit::at(0, 0, 420, 48)
                             .shape(compose::heldPath(band))
                             .fill(material::Color{.68f, .68f, .68f, 1})
                             .opacity(.40f + .035f * (i % 6))});
    }
    compose::brush::Ribbon loaded;
    loaded.width = path::Profile{{0, 1},    {.12f, 12}, {.35f, 27},
                                 {.61f, 8}, {.84f, 20}, {1, 1}};
    loaded.step = 1;
    loaded.join = path::Join::Round;
    const auto band = path::fromSk(loaded.band(path::toSk(pressure)));
    heightTree =
        compose::stack()
            .width(kFieldWidth)
            .height(kFieldHeight)
            .fill(kBlack)
            .children(
                {compose::kit::at(compose::text("OB8"), 43, 10, 560, 222)
                     .fontFamily("Georgia, Times New Roman, serif")
                     .fontSize(196)
                     .fontWeight(700)
                     .letterSpacing(10)
                     .ink(kWhite)
                     .filter(material::Filter::blur(.9f)),
                 compose::kit::at(0, 0, kFieldWidth, kFieldHeight)
                     .shape(compose::heldPath(band))
                     .fill(material::Color{.86f, .86f, .86f, 1})
                     .opacity(.67f)
                     .filter(material::Filter::blur(.55f)),
                 compose::kit::at(0, 0, kFieldWidth, kFieldHeight)
                     .shape(compose::heldPath(fibre))
                     .foreground(compose::brush::artAlong(fibreArt, 44, 3)),
                 compose::kit::at(0, 0, kFieldWidth, kFieldHeight)
                     .shape(compose::heldPath(crossing))
                     .foreground(compose::brush::artAlong(fibreArt, 22, 3))
                     .opacity(.65f),
                 rule(24, 278, 572, kWhite, .8f),
                 compose::kit::ring(
                     {569, 35}, 14,
                     compose::stroke(.8f, compose::Fill::color(kWhite)))});
    for (int i = 0; i < 23; ++i)
      heightTree.children(
          {rule(24 + i * 26.f, 280, .8f, kWhite, i % 5 == 0 ? 7 : 3)});
  }

  void prepareFields(float density) {
    using namespace carved_marks;
    const float d = bounded(density, .125f, 4, 1);
    const SkISize pixels = SkISize::Make(int(std::round(kFieldWidth * d)),
                                         int(std::round(kFieldHeight * d)));
    if (heightScene && heightScene->size() == pixels) return;
    if (!fonts)
      throw std::runtime_error("Carved marks requires a font context");
    heightScene = compose::TextureScene::make(pixels, *fonts, kBlack);
    normalScene = compose::TextureScene::make(pixels, *fonts, {.5f, .5f, 1, 1});
    heightScene->setAutoTexturePromotion(compose::PromotionPolicy::Off);
    normalScene->setAutoTexturePromotion(compose::PromotionPolicy::Off);
    Element scaledHeight = heightTree;
    scaledHeight.scale(d).transformOrigin(compose::Dimension(0),
                                          compose::Dimension(0));
    heightScene->render(compose::stack()
                            .width(pixels.width())
                            .height(pixels.height())
                            .children({scaledHeight}));
    height = sampled(heightScene->texture(), {kFieldWidth, kFieldHeight});
    const auto reference = material::surface::normalFromHeight(
        height, {.depth = -2.8f, .step = .75f, .directX = true});
    normalScene->render(
        compose::stack()
            .width(pixels.width())
            .height(pixels.height())
            .children({compose::kit::at(0, 0, kFieldWidth, kFieldHeight)
                           .fill(reference)
                           .scale(d)
                           .transformOrigin(compose::Dimension(0),
                                            compose::Dimension(0))}));
    heightRevision = heightScene->revision();
    normalRevision = normalScene->revision();
    fieldDensity = d;
    preparedFinish.reset();
    treeDirty = true;
    checkFields();
  }

  void checkFields() const {
    if (!heightScene || !normalScene || !heightScene->image() ||
        !normalScene->image() || heightScene->isRunning() ||
        normalScene->isRunning() || heightScene->revision() != heightRevision ||
        normalScene->revision() != normalRevision)
      throw std::runtime_error(
          "Carved marks requires settled height and normal fields");
  }

  void prepareMaterials(const Finish& finish) {
    using namespace carved_marks;
    if (preparedFinish && *preparedFinish == finish) return;
    const auto gilt = giltMask(height);
    const auto gold = material::from(kGold);
    auto stone = stoneColor();
    stone.layer(hexColor(0x404b38), {.opacity = .62f, .mask = marked(height)});
    stone.layer(gold, {.opacity = finish.gilding * .65f, .mask = marked(gilt)});
    const auto stoneNormal = material::surface::blendNormals(
        material::surface::normalFromHeight(
            height, {.depth = finish.depth, .step = .75f, .directX = true}),
        material::surface::normalFromHeight(
            stoneHeight(), {.depth = finish.detail, .directX = true}),
        {.baseDirectX = true, .detailDirectX = true, .outputDirectX = true});
    auto stoneMetal = material::from(kBlack);
    const float goldMetal = finish.gilding * .75f;
    stoneMetal.layer(material::Color{goldMetal, goldMetal, goldMetal, 1},
                     {.mask = marked(gilt)});
    stoneFace = stone.surface(
        {.metallic = stoneMetal,
         .roughness = roughness(height, std::min(1.f, finish.roughness + .28f)),
         .normal = stoneNormal,
         .normalScale = finish.normals ? 1.f : 0.f,
         .normalDirectX = true});
    auto copper = copperColor();
    copper.layer(hexColor(0x3b3027), {.opacity = .40f, .mask = marked(height)});
    copper.layer(gold, {.opacity = finish.gilding, .mask = marked(gilt)});
    const auto copperNormal = material::surface::blendNormals(
        material::surface::normalFromHeight(
            height,
            {.depth = -finish.depth * .58f, .step = .75f, .directX = true}),
        material::surface::normalFromHeight(
            copperHeight(), {.depth = finish.detail, .directX = true}),
        {.baseDirectX = true, .detailDirectX = true, .outputDirectX = true});
    auto copperMetal = material::from(material::Color{.78f, .78f, .78f, 1});
    copperMetal.layer(material::Color{.30f, .30f, .30f, 1},
                      {.mask = marked(height)});
    copperMetal.layer(kWhite,
                      {.opacity = finish.gilding, .mask = marked(gilt)});
    copperFace =
        copper.surface({.metallic = copperMetal,
                        .roughness = roughness(height, finish.roughness),
                        .normal = copperNormal,
                        .normalScale = finish.normals ? 1.f : 0.f,
                        .normalDirectX = true,
                        .clearcoat = .08f});
    for (int i = 0; i < 3; ++i) {
      const glm::vec2 extent = i == 0 ? glm::vec2{396, 82} : glm::vec2{1, 1};
      unitInks[i] = unitColor(extent).surface(
          {.metallic = .68f,
           .roughness = std::max(.40f, finish.roughness),
           .normal = material::surface::normalFromHeight(
               unitHeight(extent),
               {.depth = finish.depth, .step = .75f, .directX = true}),
           .normalScale = finish.normals ? 1.f : 0.f,
           .normalDirectX = true});
    }
    preparedFinish = finish;
  }

  Element plate(const State& value, bool copper) const {
    using namespace carved_marks;
    const float x = copper ? 732 : 40;
    const auto rim = material::surface::normalFromHeight(
        shoulder({668, 420}), {.depth = 3, .directX = true});
    auto stock = (copper ? copperColor() : stoneColor())
                     .surface({.metallic = copper ? .68f : 0.f,
                               .roughness = copper ? .56f : .8f,
                               .normal = rim,
                               .normalDirectX = true});
    auto panel = compose::kit::at(compose::stack(), x, 132, 668, 420)
                     .cache(compose::Cache::Picture)
                     .fill(stock)
                     .borderRadius(10)
                     .stroke(compose::stroke(1, compose::Fill::color(kRule)));
    char finish[100];
    std::snprintf(finish, sizeof(finish),
                  "%+.2f PX / ROUGH %.2f\nLAYER MASK · SHARED HEIGHT",
                  copper ? -value.finish.depth * .58f : value.finish.depth,
                  copper ? value.finish.roughness
                         : std::min(1.f, value.finish.roughness + .28f));
    panel.children(
        {label(copper ? "02 / CHASED COPPER" : "01 / CUT LIMESTONE", 24, 18,
               360, 12, copper ? kPaper : kInk),
         label(copper ? "BRUSHED / PARCEL GILT" : "PORES / INKED INCISIONS",
               408, 19, 236, 10, copper ? kPaper : kInk),
         compose::kit::at(24, 56, kFieldWidth, kFieldHeight)
             .fill(copper ? copperFace : stoneFace)
             .borderRadius(3),
         label(copper ? "ALBEDO / BEFORE LIGHT" : "H / COMPOSE HEIGHT", 24, 353,
               150, 8, copper ? kPaper : kInk),
         label(copper ? "SHARED MARKS" : "N / FIXED −2.8 PX", 168, 353, 150, 8,
               copper ? kPaper : kInk),
         compose::kit::at(24, 364, 120, 40)
             .fill(copper ? copperColor()
                          : sampled(heightScene->texture(), {120, 40})),
         compose::kit::at(168, 364, 120, 40)
             .fill(copper ? sampled(heightScene->texture(), {120, 40})
                          : sampled(normalScene->texture(), {120, 40})),
         label(finish, 320, 368, 310, 11, copper ? kPaper : kInk)});
    for (float edge : {12.f, 656.f})
      panel.children(
          {compose::kit::disc({edge, 12}, 2.5f)
               .fill(copper ? hexColor(0x392b20) : hexColor(0x716c5d))});
    return panel;
  }

  Element unitSpecimen(const State& value, int index) const {
    using namespace carved_marks;
    constexpr std::array domains{compose::PaintBox::Element,
                                 compose::PaintBox::Glyph,
                                 compose::PaintBox::Word};
    constexpr std::array captions{"ELEMENT / PIXEL FIELD",
                                  "GLYPH / PIXEL DEPTH", "WORD / FONT SPLIT"};
    const float x = 40 + index * 462.f;
    const auto kind = index == 0   ? material::LightKind::Directional
                      : index == 1 ? material::LightKind::Point
                                   : material::LightKind::Spot;
    material::Light source{.direction = bearing,
                           .elevation = index == 0 ? 84.f : 90.f,
                           .color = material::Color{1, .96f, .87f, 1},
                           .intensity = .25f,
                           .ambient = .35f,
                           .kind = kind,
                           .range = 780,
                           .innerAngle = 18,
                           .outerAngle = 42};
    auto lamp =
        compose::light(source).key("unit-source-" + std::to_string(index));
    auto marker =
        compose::kit::at(190, 179, 22, 1.5f).fill(kCopper).rotate(bearing);
    if (index != 0) {
      lamp.translateX(sourceX).translateY(sourceY).translateZ(sourceZ);
      marker = compose::kit::ring(
                   {0, 0}, 5, compose::stroke(1, compose::Fill::color(kCopper)))
                   .translateX(sourceX)
                   .translateY(sourceY);
    }
    auto words =
        compose::kit::at(compose::text("cut / WIDE OB8"), 18, 54, 396, 82)
            .key("unit-type-" + std::to_string(index))
            .cache(compose::Cache::Picture)
            .fontFamily("Georgia, Times New Roman, serif")
            .fontSize(47)
            .fontWeight(700)
            .ink(kInk)
            .span(weave::selectors::text(u8"cut /"),
                  compose::SpanStyle()
                      .fontFamily("Inter, Helvetica Neue, sans-serif")
                      .fontSize(13)
                      .fontWeight(400)
                      .ink(kInk));
    if (index == 0)
      words.ink(unitInks[index], domains[index]);
    else
      words.span(weave::selectors::text(u8"WIDE OB8"),
                 compose::SpanStyle().ink(unitInks[index], domains[index]));
    if (index == 2)
      words
          .span(weave::selectors::text(u8"I"),
                compose::SpanStyle().fontSize(32))
          .span(weave::selectors::text(u8"B"),
                compose::SpanStyle().fontSize(57));
    if (value.phase == 5 && index == 1)
      words.scaleX(-.82f).scaleY(1.12f).transformOrigin(compose::pct(50),
                                                        compose::pct(50));
    char channel[100];
    std::snprintf(
        channel, sizeof(channel),
        "HEIGHT %+.2f PX · STEP .75 PX\n%s / STATIC INPUTS · LIVE LIGHT",
        value.finish.depth,
        index == 0   ? "FRONT 84°"
        : index == 1 ? "POINT / Z 260±24"
                     : "SPOT / 18°–42°");
    return compose::kit::at(compose::scene(), x, 724, 436, 220)
        .environment(surroundings)
        .fill(hexColor(0xf7f0e0))
        .borderRadius(5)
        .stroke(compose::stroke(1, compose::Fill::color(kRule)))
        .children({lamp, label(captions[index], 18, 17, 400, 11, kInk), words,
                   rule(18, 149, 400), label(channel, 18, 158, 390, 9),
                   marker});
  }

  Element coupons(const State& value) const {
    using namespace carved_marks;
    auto rail = compose::kit::at(compose::stack(), 40, 988, 1360, 96)
                    .fill(kInk)
                    .borderRadius(4);
    constexpr std::array captions{"SIGNED CUT",    "ZERO DEPTH",
                                  ".5 / 1 / 2 PX", "ROUGHNESS 0",
                                  "ROUGHNESS 1",   "GREEN AXIS PAIR"};
    for (int i = 0; i < 6; ++i) {
      const float x = 16 + i * 225.f;
      material::Material input = unitHeight({198, 42});
      if (i == 2)
        input = material::shader(R"(
half4 main(float2 p) {
  float a = 1 - smoothstep(.25, .75, abs(p.y - 10));
  float b = 1 - smoothstep(.50, 1.0, abs(p.y - 21));
  float c = 1 - smoothstep(1.0, 1.5, abs(p.y - 33));
  return half4(float3(max(a, max(b, c))), 1);
})");
      const float depth = i == 1 ? 0.f : value.finish.depth;
      const auto normal = material::surface::normalFromHeight(
          input, {.depth = depth, .step = .75f, .directX = true});
      auto ink = material::from(kCopper).surface({.metallic = .65f,
                                                  .roughness = i == 3   ? 0.f
                                                               : i == 4 ? 1.f
                                                                        : .55f,
                                                  .normal = normal,
                                                  .normalDirectX = true});
      rail.children(
          {label(captions[i], x, 10, 206, 9, kPaper),
           compose::kit::at(x, 30, 198, 42).fill(ink).borderRadius(2)});
      if (i == 5) {
        const float slope = .22f;
        const float z = 1.f / std::sqrt(1 + slope * slope);
        for (int j = 0; j < 2; ++j) {
          const auto encoded = material::Color{
              .5f, .5f + (j ? slope : -slope) * z * .5f, .5f + z * .5f, 1};
          auto pair = material::from(kGold).surface(
              {.metallic = .6f,
               .roughness = .55f,
               .normal = material::Material(encoded),
               .normalDirectX = j != 0});
          rail.children(
              {compose::kit::at(x + j * 103.f, 30, 95, 42).fill(pair)});
        }
      }
    }
    return rail;
  }

  Element interface(const State& value) const {
    using namespace carved_marks;
    const auto& f = value.finish;
    auto ui = compose::stack().width(kWidth).height(kHeight);
    ui.children({button("HOLD", 40, 100, heldPhase >= 0),
                 button("AUTO", 152, 100, heldPhase < 0),
                 button("PREVIOUS", 264, 100, false),
                 button("NEXT", 376, 100, false),
                 button("NORMALS", 488, 112, f.normals),
                 label(kNames[value.phase], 652, 584, 748, 15, kInk),
                 label("Light moves while the finish is held. Drag a rail to "
                       "edit; AUTO restores presets.",
                       40, 620, 1240, 10)});
    const std::array<float, 6> values{f.depth,       f.detail,
                                      f.roughness,   f.gilding,
                                      value.bearing, value.elevation};
    for (int i = 0; i < 6; ++i) {
      const float x = 40 + i * 230.f;
      char reading[80];
      std::snprintf(reading, sizeof(reading), "%s  %s%.2f",
                    kSliders[i].name.data(),
                    i == 0 && values[i] >= 0 ? "+" : "", values[i]);
      const float amount =
          (values[i] - kSliders[i].low) / (kSliders[i].high - kSliders[i].low);
      ui.children(
          {label(reading, x, 643, 210, 9, kInk),
           compose::kit::at(x, 665, 210, 5).fill(kRule).borderRadius(2),
           compose::kit::at(x, 665, std::max(1.f, amount * 210), 5)
               .fill(i == 3 ? kCopper : kInk)
               .borderRadius(2),
           compose::kit::disc({x + amount * 210, 667.5f}, 5)
               .fill(kPaper)
               .stroke(compose::stroke(1.5f, compose::Fill::color(kInk)))});
    }
    return ui;
  }

  Element describe(const State& value) const {
    using namespace carved_marks;
    char source[140];
    std::snprintf(
        source, sizeof(source),
        "%s / KEY BASE %.0f° · ELEV %.0f°\nHELD HEIGHT %.2f× / N32 DATA",
        manual          ? "MANUAL"
        : heldPhase < 0 ? "AUTO"
                        : "HELD",
        value.bearing, value.elevation, fieldDensity);
    auto page = compose::scene()
                    .width(kWidth)
                    .height(kHeight)
                    .fill(kPaper)
                    .fontFamily("Inter, Helvetica Neue, sans-serif")
                    .environment(surroundings);
    page.children(
        {compose::light(
             material::studio({.direction = bearing,
                               .elevation = elevation,
                               .color = material::Color{1, .96f, .88f, 1},
                               .intensity = .36f,
                               .ambient = .72f}))
             .key("atelier-key"),
         label("CARVED / MARKS", 40, 26, 1050, 47, kInk)
             .fontFamily("Georgia, Times New Roman, serif"),
         label("A MATERIAL ATELIER / LETTERS, PRESSURE & FIBRE BECOME THE "
               "SURFACE",
               44, 87, 1030, 11),
         label(source, 1100, 37, 300, 11, kInk), rule(40, 113, 1360, kCopper),
         compose::kit::at(44, 139, 668, 420)
             .fill(hexColor(0xd5d0c2))
             .borderRadius(10),
         compose::kit::at(736, 139, 668, 420)
             .fill(hexColor(0xd5d0c2))
             .borderRadius(10),
         plate(value, false), plate(value, true),
         label("ONE HELD COMPOSE DIE / REAL COUNTERS · RIBBON PRESSURE · TWO "
               "FIBRE PASSES",
               40, 561, 1360, 10, kInk),
         interface(value),
         label("DIRECT HEIGHT → MATERIAL INK / SAME PIXEL DEPTH, DIFFERENT "
               "TEXT DOMAINS",
               40, 701, 1360, 11, kInk),
         unitSpecimen(value, 0), unitSpecimen(value, 1), unitSpecimen(value, 2),
         label("Element ink covers the leaf; Glyph and Word ink cover the "
               "selected "
               "span. No lit colour is baked here.",
               40, 956, 1260, 11),
         coupons(value),
         label("24 S / FRONT · LEFT RAKE · RIGHT RAKE · ZERO MACRO · HALF GILT "
               "· UNIT MAPPING",
               40, 1096, 1180, 9),
         label("STUDY / 11", 1294, 1094, 106, 11, kInk)});
    return page;
  }

  void selectPhase(int phase) {
    heldPhase = (phase + 6) % 6;
    manual.reset();
    treeDirty = true;
  }

  void pointer(float x, float y, bool pressed) {
    using namespace carved_marks;
    if (!std::isfinite(x) || !std::isfinite(y)) {
      pointerDown = pressed;
      return;
    }
    if (pressed && y >= 650 && y <= 687) {
      for (int i = 0; i < 6; ++i) {
        const float left = 40 + i * 230.f;
        if (x < left || x > left + 210) continue;
        State edit = state();
        const float value =
            kSliders[i].low + std::clamp((x - left) / 210, 0.f, 1.f) *
                                  (kSliders[i].high - kSliders[i].low);
        if (i == 0)
          edit.finish.depth = value;
        else if (i == 1)
          edit.finish.detail = value;
        else if (i == 2)
          edit.finish.roughness = value;
        else if (i == 3)
          edit.finish.gilding = value;
        else if (i == 4)
          edit.bearing = value;
        else
          edit.elevation = value;
        heldPhase = edit.phase;
        manual = edit;
        treeDirty = true;
      }
    }
    if (pressed && !pointerDown && y >= 582 && y <= 614) {
      const State before = state();
      if (x >= 40 && x <= 140) {
        heldPhase = before.phase;
      } else if (x >= 152 && x <= 252) {
        heldPhase = -1;
        manual.reset();
      } else if (x >= 264 && x <= 364) {
        selectPhase(before.phase + 5);
      } else if (x >= 376 && x <= 476) {
        selectPhase(before.phase + 1);
      } else if (x >= 488 && x <= 600) {
        State edit = before;
        edit.finish.normals = !edit.finish.normals;
        manual = edit;
        heldPhase = edit.phase;
      }
      treeDirty = true;
    }
    pointerDown = pressed;
  }

  void update(double elapsed) {
    using namespace carved_marks;
    seconds = std::isfinite(elapsed)
                  ? float(std::fmod(std::max(0., elapsed), double(kLoop)))
                  : 0.f;
    const State value = state();
    const float turn = seconds * 2 * kPi / kLoop;
    bearing = value.bearing + 18 * std::sin(turn);
    elevation = value.elevation;
    reflection = 12 * std::sin(turn + .4f);
    sourceX = 220 + 115 * std::sin(turn);
    sourceY = 198 + 4 * std::cos(turn * 2);
    sourceZ = 260 + 24 * std::cos(turn);
    if (describedPhase != value.phase) treeDirty = true;
    if (heightScene) checkFields();
  }

  void paint(draw::Pen& pen) {
    pointer(pen.mouseX, pen.mouseY, pen.mouseIsPressed);
    update(seconds);
    prepareFields(pen.contentScale());
    const State value = state();
    prepareMaterials(value.finish);
    if (treeDirty) {
      tree = describe(value);
      describedPhase = value.phase;
      treeDirty = false;
    }
    checkFields();
    pen.element(tree, 0, 0, carved_marks::kWidth, carved_marks::kHeight);
  }

  void setup(sketch::SketchContext& ctx) {
    using namespace carved_marks;
    ctx.canvas(kWidth, kHeight);
    ctx.background(kPaper);
    ctx.captureAt(2);
    ctx.paintDiscardedFrames(false);
    fonts = ctx.fonts;
    prepareArt();
    room = material::EnvironmentMap::baked(256, [](float u, float v) {
      const float wx = std::sin((u - .025f) * kPi) / .43f;
      const float wy = (v - .37f) / .28f;
      const float sx = std::sin((u - .84f) * kPi) / .085f;
      const float sy = (v - .44f) / .30f;
      const float window = std::exp(-wx * wx - wy * wy);
      const float strip = std::exp(-sx * sx - sy * sy);
      return glm::vec3(.65f, .70f, .74f) + window * glm::vec3(1.2f, 1.1f, .9f) +
             strip * glm::vec3(.3f, .6f, .8f);
    });
    surroundings = material::environment(
        material::image(room.texture()),
        {.rotation = reflection, .intensity = .55f, .size = {256, 128}});
    update(0);
    ctx.composer.render(compose::pen(
                            "carved-marks",
                            [this](draw::Pen& pen) { paint(pen); },
                            compose::Cache::None)
                            .width(kWidth)
                            .height(kHeight));
  }
};

SIGIL_SKETCH(
    CarvedMarks, "Study · Materials",
    "Carved stone and chased copper: material letters and layered brush marks")
