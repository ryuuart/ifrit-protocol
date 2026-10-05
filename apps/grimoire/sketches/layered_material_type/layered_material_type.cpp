/** Material type atelier: text, ribbons and rules shape one shared surface. */
// TAGS: Materials/Porcelain, Materials/Metal, Typography/Material ink,
// Compose/Texture, Composition/Layers, Brushes/Ribbon

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/PaintBox.h>
#include <sigilcompose/core/SpanStyle.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilweave/query/Selector.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <glm/mat3x3.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace compose = sigil::compose;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using compose::Element;
using material::hexColor;

namespace {
constexpr int kWidth = 1440, kHeight = 1100, kBakeWidth = 900,
              kBakeHeight = 350;
constexpr int kInkWidth = 384, kInkHeight = 96;
constexpr material::Color kPaper = hexColor(0xf2ecdf);
constexpr material::Color kInk = hexColor(0x302b26);
constexpr material::Color kMuted = hexColor(0x756b5d);
constexpr material::Color kCopper = hexColor(0xb8794e);
constexpr material::Color kWhite{1, 1, 1, 1}, kBlack{0, 0, 0, 1};

// Edit heldPhase (-1 cycles) and the six material states to compare endpoints.
constexpr int heldPhase = -1;
struct MetalInkControls {
  float roughness = .44f;
  float normalStrength = .6f;
};
constexpr MetalInkControls kMetalInk;
struct Controls {
  float depth, roughness, normalStrength, coating, overprint, blur;
};
constexpr std::array kStates{
    Controls{8, .27f, 1, .8f, .18f, 1.4f},   Controls{0, 0, 0, 1, 0, 1.4f},
    Controls{12, 1, 1, 0, 1, 2.4f},          Controls{18, .13f, 1, 1, .4f, 3},
    Controls{-12, .32f, 1, .4f, .58f, 1.8f}, Controls{6, .2f, 1, .7f, 0, .6f},
};
constexpr std::array<std::string_view, 6> kNames{
    "01 / GLAZED IMPRESSION", "02 / FLAT · MIRROR",
    "03 / RAW · MATTE",       "04 / RAISED",
    "05 / COPPER · INTAGLIO", "06 / COUNTERS · CUT"};

Controls state(int phase) {
  Controls c = kStates[phase];
  const auto bound = [](float value, float lo, float hi, float fallback) {
    return std::isfinite(value) ? std::clamp(value, lo, hi) : fallback;
  };
  c.depth = bound(c.depth, -64, 64, 0);
  c.roughness = bound(c.roughness, 0, 1, .3f);
  c.normalStrength = bound(c.normalStrength, 0, 4, 1);
  c.coating = bound(c.coating, 0, 1, 0);
  c.overprint = bound(c.overprint, 0, 1, 0);
  c.blur = bound(c.blur, 0, 12, 1.4f);
  return c;
}

Element label(std::string_view words, float x, float y, float w,
              float size = 12, material::Color ink = kMuted) {
  const auto lines = 1 + std::count(words.begin(), words.end(), '\n');
  return compose::kit::at(compose::text(std::string(words)), x, y, w,
                          size * 1.65f * lines)
      .fontSize(size)
      .ink(ink);
}
Element rule(float x, float y, float w, float h = 1,
             material::Material paint = kCopper) {
  return compose::kit::at(x, y, w, h).fill(std::move(paint));
}
Element letters(float scale = 1) {
  return compose::kit::at(compose::text("OB8"), 44 * scale, 4 * scale,
                          840 * scale, 330 * scale)
      .fontFamily("Georgia, Times New Roman, serif")
      .fontSize(292 * scale)
      .fontWeight(700)
      .letterSpacing(36 * scale);
}
material::Material gold() {
  return material::linearGradient({0, 0}, {.6f, 1},
                                  {{0, hexColor(0xa7783d)},
                                   {.32f, hexColor(0xe9d09a)},
                                   {.61f, hexColor(0xc59855)},
                                   {1, hexColor(0xf0deb0)}});
}
material::Material copper(float roughness = .26f) {
  return material::linearGradient({0, 0}, {.25f, 1},
                                  {{0, hexColor(0x985132)},
                                   {.48f, hexColor(0xd29e78)},
                                   {1, hexColor(0x9d6648)}})
      .surface({.metallic = 1.f, .roughness = roughness});
}
material::Material sizedTexture(material::Texture texture, float w, float h) {
  glm::mat3 placement(1);
  placement[0][0] = w / kBakeWidth;
  placement[1][1] = h / kBakeHeight;
  return material::image(texture.uv(placement));
}
}  // namespace

struct LayeredMaterialType {
  std::shared_ptr<compose::TextureScene> heightScene;
  std::shared_ptr<compose::TextureScene> inkScene;
  std::shared_ptr<compose::TextureScene> normalScene;
  material::Material passageInk{kBlack}, unitInk{kBlack};
  material::Material metalInk{kBlack};
  uint64_t inkRevision = 0;
  uint64_t normalRevision = 0;
  std::array<path::Outline, 3> bands;
  int phase = -1;

  void bakeInk() {
    const auto normal = material::surface::normalFromHeight(
        material::noise(.03f, {.octaves = 2, .seed = 17.f}),
        {.depth = 1.4f, .step = 1.f});
    // Pixel derivatives are taken on the full-sized normal field. Mapped
    // glyphs sample its encoded slopes rather than differentiating a unit box.
    normalScene->render(
        compose::box().width(kInkWidth).height(kInkHeight).fill(normal));
    normalRevision = normalScene->revision();
    glm::mat3 unitPlacement(1.f);
    unitPlacement[0][0] = 1.f / kInkWidth;
    unitPlacement[1][1] = 1.f / kInkHeight;
    const auto encodedNormal =
        material::image(normalScene->texture()
                            .uv(unitPlacement)
                            .sampling(material::Sampling::Linear)
                            .tile(material::Repeat::Pad));
    metalInk = gold().surface({.metallic = .6f,
                               .roughness = kMetalInk.roughness,
                               .normal = encodedNormal,
                               .normalScale = kMetalInk.normalStrength});
    const auto finish =
        material::linearGradient({0, 0}, {1, 0},
                                 {{0, hexColor(0x985132)},
                                  {.24f, hexColor(0xf0deb0)},
                                  {.49f, hexColor(0xb8794e)},
                                  {.73f, hexColor(0xe9d09a)},
                                  {1, hexColor(0x985132)}})
            .surface({.metallic = .85f, .roughness = .38f, .normal = normal});
    inkScene->render(
        compose::box().width(kInkWidth).height(kInkHeight).fill(finish));
    inkRevision = inkScene->revision();

    const auto sampledInk = [&](float width, float height) {
      glm::mat3 placement(1.f);
      placement[0][0] = width / kInkWidth;
      placement[1][1] = height / kInkHeight;
      auto texture = inkScene->texture();
      texture.uv(placement)
          .sampling(material::Sampling::Linear)
          .tile(material::Repeat::Pad);
      return material::image(std::move(texture));
    };
    passageInk = sampledInk(284.f, 46.f);
    unitInk = sampledInk(1.f, 1.f);
    checkInk();
  }

  void checkInk() const {
    if (!inkScene || !inkScene->image() || inkScene->isRunning() ||
        inkScene->revision() != inkRevision || passageInk.isRunning() ||
        unitInk.isRunning() || !normalScene || !normalScene->image() ||
        normalScene->isRunning() || normalScene->revision() != normalRevision ||
        metalInk.isRunning())
      throw std::runtime_error(
          "Material lettering requires a settled ink texture");
  }

  Element inkSpecimen(std::string_view caption, std::string key, float x,
                      compose::PaintBox domain) const {
    const auto& ink =
        domain == compose::PaintBox::Element ? passageInk : unitInk;
    auto phrase = compose::kit::at(compose::text("cast / FOIL"), 0, 17, 284, 46)
                      .key(std::move(key))
                      .fontFamily("Georgia, Times New Roman, serif")
                      .fontSize(28)
                      .fontWeight(700)
                      .ink(kInk)
                      .span(weave::selectors::text(u8"cast /"),
                            compose::SpanStyle()
                                .fontFamily("Inter, Helvetica Neue, sans-serif")
                                .fontSize(13)
                                .fontWeight(400))
                      .span(weave::selectors::text(u8"I"),
                            compose::SpanStyle().fontSize(34))
                      .span(weave::selectors::text(u8"FOIL"),
                            compose::SpanStyle().ink(ink, domain));
    return compose::kit::at(compose::stack(), x, 824, 284, 64)
        .children({label(caption, 0, 1, 284, 9), phrase});
  }

  Element metalSpecimen(std::string_view caption, std::string key, float x,
                        compose::PaintBox domain) const {
    auto phrase = compose::kit::at(compose::text("cast / FOIL"), 0, 35, 284, 64)
                      .key(std::move(key))
                      .cache(compose::Cache::Picture)
                      .fontFamily("Georgia, Times New Roman, serif")
                      .fontSize(38)
                      .fontWeight(700)
                      .ink(kInk)
                      .span(weave::selectors::text(u8"cast /"),
                            compose::SpanStyle()
                                .fontFamily("Inter, Helvetica Neue, sans-serif")
                                .fontSize(13)
                                .fontWeight(400))
                      .span(weave::selectors::text(u8"I"),
                            compose::SpanStyle().fontSize(44))
                      .span(weave::selectors::text(u8"FOIL"),
                            compose::SpanStyle().ink(metalInk, domain));
    return compose::kit::at(compose::box(), x, 912, 284, 120)
        .cache(compose::Cache::Picture)
        .children({label(caption, 0, 0, 284, 9, kInk), phrase});
  }

  Element heightTree(const Controls& c) const {
    Element root = compose::stack()
                       .width(kBakeWidth)
                       .height(kBakeHeight)
                       .fill(kBlack)
                       .fontFamily("Georgia, Times New Roman, serif");
    for (int i = 0; i < 3; ++i)
      root.children({compose::box()
                         .width(kBakeWidth)
                         .height(kBakeHeight)
                         .shape(compose::heldPath(bands[i]))
                         .fill(kWhite)
                         .opacity(i == 1 ? c.overprint : .65f)
                         .filter(material::Filter::blur(c.blur))});
    root.children(
        {letters().ink(kWhite).filter(material::Filter::blur(c.blur))});
    root.children(
        {rule(38, 329, 824, 1.5f, kWhite).filter(material::Filter::blur(.7f))});
    for (int i = 0; i < 17; ++i)
      root.children({rule(38 + i * 51.5f, 319, 1, i % 4 == 0 ? 8 : 4, kWhite)});
    return root;
  }

  material::Material sharedSurface(const Controls& c, float w = kBakeWidth,
                                   float h = kBakeHeight) const {
    const auto height = sizedTexture(heightScene->texture(), w, h);
    const auto normal = material::surface::normalFromHeight(
        height, {.depth = c.depth, .step = 1, .directX = true});
    auto roughness = material::from(
        material::Color{c.roughness, c.roughness, c.roughness, 1});
    roughness.layer(
        material::Color{c.roughness * .7f, c.roughness * .7f, c.roughness * .7f,
                        1},
        {.mask = material::Mask{height, material::MaskChannel::Luminance}});
    return material::from(hexColor(0xe9e1ce))
        .layer(
            c.depth < 0 ? copper(c.roughness) : gold(),
            {.mask = material::Mask{height, material::MaskChannel::Luminance}})
        .layer(
            hexColor(0xac644b),
            {.opacity = c.overprint * .28f,
             .mask = material::Mask{height, material::MaskChannel::Luminance}})
        .surface({.metallic = height,
                  .roughness = roughness,
                  .normal = normal,
                  .normalScale = c.normalStrength,
                  .normalDirectX = true,
                  .clearcoat = c.coating});
  }

  Element comparison(const Controls& c, bool shared, float x) const {
    Element panel = compose::stack().width(432).height(168).fill(
        shared
            ? sharedSurface(c, 432, 168)
            : material::from(hexColor(0xe9e1ce)).surface({.roughness = .27f}));
    if (!shared) {
      auto ink = gold().surface({.metallic = 1.f, .roughness = c.roughness});
      panel.children({letters(.48f)
                          .ink(material::Color{0, 0, 0, 0})
                          .decorationOutline(compose::Boundary::Glyphs)
                          .foreground(compose::relief(
                              ink, {.shoulder = 1.4f, .depth = c.depth / 8}))
                          .opacity(.85f)});
      panel.children({letters(.48f).ink(copper()).opacity(c.overprint * .32f)});
    }
    return compose::kit::at(panel, x, 648, 432, 168);
  }

  Element sidebar(const Controls& c) const {
    char values[180];
    std::snprintf(values, sizeof(values),
                  "DEPTH %+.0f px\nROUGHNESS %.2f\nNORMAL %.1f\nCOATING "
                  "%.1f\nOVERPRINT %.2f",
                  c.depth, c.roughness, c.normalStrength, c.coating,
                  c.overprint);
    const auto height = sizedTexture(heightScene->texture(), 144, 56);
    const auto normal = material::surface::normalFromHeight(
        height, {.depth = c.depth, .step = 1, .directX = true});
    Element counters = label("B / O / 8", 0, 531, 300, 31, kInk).ink(copper());
    if (phase == 5)
      counters =
          compose::kit::at(compose::stack(), 0, 526, 332, 58)
              .overflow(compose::Overflow::Clip)
              .children({label("OB8", -8, -25, 400, 112)
                             .fontFamily("Georgia, Times New Roman, serif")
                             .ink(material::Color{0, 0, 0, 0})
                             .decorationOutline(compose::Boundary::Glyphs)
                             .foreground(compose::relief(
                                 copper(), {.shoulder = 2, .depth = 1}))});
    Element side = compose::stack().width(332).height(650);
    side.children(
        {label("FINISH REGISTER", 0, 0, 300, 11, kInk), rule(0, 32, 332),
         label("a.", 0, 55, 38, 36, kCopper),
         label("Porcelain / precious metal", 50, 64, 272, 15, kInk),
         label("One continuous surface.\nThe letters change its "
               "normal,\ncolour and roughness channels.",
               50, 102, 282, 15),
         label(values, 0, 194, 320, 13, kInk), rule(0, 341, 332),
         label("COMPOSE HEIGHT", 0, 362, 150, 10),
         label("DERIVED NORMAL", 176, 362, 150, 10),
         compose::kit::at(0, 385, 144, 56).fill(height),
         compose::kit::at(176, 385, 144, 56).fill(normal),
         label(
             "Opaque grayscale · real glyphs\nBlurred shoulders · signed depth",
             0, 464, 330, 13),
         counters,
         label("Counters remain unprinted.\nThe bake is held per state.", 0,
               593, 324, 13)});
    return compose::kit::at(side, 1044, 225, 332, 650);
  }

  Element describe(const Controls& c) const {
    Element page =
        compose::stack().width(kWidth).height(kHeight).fill(kPaper).fontFamily(
            "Inter, Helvetica Neue, sans-serif");
    auto title = label("TYPE / MATTER", 62, 47, 775, 58, kInk)
                     .fontFamily("Georgia, Times New Roman, serif")
                     .ink(copper(.38f));
    page.children(
        {title,
         label("ATELIER 08\nGLAZE · FOIL · IMPRESSION", 1044, 62, 332, 13,
               kInk),
         rule(64, 146, 1312, 1.2f, copper(.36f)),
         label(kNames[phase], 64, 170, 580, 11, kInk),
         label("COMPOSE ELEMENTS → HEIGHT → SHARED MATERIAL", 632, 170, 600,
               11),
         compose::kit::at(64, 225, 900, 350)
             .fill(sharedSurface(c))
             .borderRadius(4),
         label("01", 66, 592, 70, 12, kInk),
         label("A relief in the material, rather than a second picture laid "
               "upon it.",
               120, 587, 844, 16, kInk),
         label("CONTOUR INK / OVERPRINT", 64, 625, 432, 10),
         label("THE SAME COMPOSE BAKE / SHARED SURFACE", 534, 625, 440, 10),
         comparison(c, false, 64),
         comparison(c, true, 534),
         sidebar(c),
         inkSpecimen("Across the phrase", "ink-passage", 64,
                     compose::PaintBox::Element),
         inkSpecimen("Each letter", "ink-glyph", 372, compose::PaintBox::Glyph),
         inkSpecimen("Across the word", "ink-word", 680,
                     compose::PaintBox::Word),
         rule(64, 890, 1312, 1, copper(.36f)),
         metalSpecimen("GOLD INK · EACH LETTER", "metal-glyph", 64,
                       compose::PaintBox::Glyph),
         metalSpecimen("GOLD INK · ACROSS THE WORD", "metal-word", 372,
                       compose::PaintBox::Word),
         label("STATIC MATERIAL\nColour ramp + held grain normal\n"
               "Roughness .44 · metal .60\nPrefixes keep the page ink.",
               1044, 914, 332, 13, kInk),
         rule(64, 1039, 1312, 1, copper(.36f)),
         label("24 s · 02 GLAZED / 06 FLAT / 10 MATTE / 14 RAISED / 18 "
               "INTAGLIO / 22 COUNTERS",
               64, 1060, 1135, 10),
         label("STUDY / 08", 1267, 1058, 116, 11, kInk)});
    return page;
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.background(kPaper);
    ctx.captureAt(2);
    ctx.paintDiscardedFrames(false);
    heightScene = ctx.textureScene({kBakeWidth, kBakeHeight}, kBlack);
    inkScene = ctx.textureScene({kInkWidth, kInkHeight}, kBlack);
    normalScene = ctx.textureScene({kInkWidth, kInkHeight}, {.5f, .5f, 1, 1});
    if (!heightScene || !inkScene || !normalScene)
      throw std::runtime_error("Material type requires a font context");
    bakeInk();
    for (int i = 0; i < 3; ++i) {
      SkPathBuilder spine;
      const float y = 290 + i * 12.f;
      spine.moveTo(35, y);
      spine.cubicTo(215, y - 46, 660, y + 28, 862, y - 7);
      const auto cooked = spine.detach();
      compose::brush::Ribbon ribbon;
      ribbon.widthStart = 10 + i * 3;
      ribbon.widthEnd = 2;
      ribbon.nibAngleDeg = 28;
      bands[i] = path::fromSk(ribbon.band(cooked));
    }
    update(0, ctx);
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    checkInk();
    const float t = static_cast<float>(
        std::fmod(std::isfinite(elapsed) ? std::max(0., elapsed) : 0., 24.));
    const int next = heldPhase < 0 ? std::min(5, static_cast<int>(t / 4))
                                   : std::clamp(heldPhase, 0, 5);
    if (next == phase) return;
    phase = next;
    const Controls c = state(phase);
    heightScene->render(heightTree(c),
                        std::isfinite(elapsed) ? std::max(0., elapsed) : 0.);
    ctx.composer.render(describe(c));
  }
};

SIGIL_SKETCH(LayeredMaterialType, "Study · Materials",
             "Porcelain and precious-metal type: Compose impressions in a "
             "shared material")
