/** Material inspection desk of Compose material marks. */
// TAGS: Compose/Materials, Materials/Metal, Typography/Material ink,
// Brushes/Painted data

#include <include/core/SkCanvas.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Stroke.h>
#include <sigildraw/brush/Tool.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace draw = sigil::draw;
namespace brush = sigil::draw::brush;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace shapes = sigil::geometry::shapes;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

namespace light_table {

using compose::Element;
constexpr int kWidth = 1440, kHeight = 1080;
constexpr auto kPaper = material::hexColor(0xeae6dc);
constexpr auto kInk = material::hexColor(0x24393f);
constexpr auto kMuted = material::hexColor(0x647b7d);
constexpr auto kRule = material::hexColor(0xb7c2bc);
constexpr auto kPanel = material::hexColor(0xf6f2e8);
constexpr auto kBezel = material::hexColor(0x354c54);
constexpr material::Color kAluminium{.64f, .70f, .72f, 1};
constexpr material::Color kCopper{.70f, .35f, .16f, 1};
constexpr material::Color kGold{.64f, .47f, .24f, 1};

// Normals vary throughout the plate, including regions far from its contours.
constexpr std::string_view kCurved = R"(
half4 main(float2 p) {
  float2 q = clamp(p / extent, float2(0), float2(1)) * 2 - 1;
  float3 n = normalize(float3(q.x * .42, -q.y * .28, 1));
  return half4(n * .5 + .5, 1);
})";
constexpr std::string_view kBrushing = R"(
half4 main(float2 p) {
  float pixel = 1 / max(uContentScale, .125);
  float ridge = (.07 * sin(p.y * 1.43) + .018 * sin(p.y * .27))
                * exp(-pixel * .2);
  float3 n = normalize(float3(.013 * sin(p.x * .17), ridge, 1));
  return half4(n * .5 + .5, 1);
})";
constexpr std::string_view kTilt = R"(
half4 main(float2 p) {
  float3 n = normalize(float3(slope, 1));
  return half4(n * .5 + .5, 1);
})";
constexpr std::string_view kCoverage = R"(
half4 main(float2 p) {
  float coverage = clamp(height.eval(p).r, 0, 1);
  return half4(color.rgb * coverage, coverage);
})";

float bounded(float value, float low, float high, float fallback) {
  return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}
Element label(std::string_view words, float x, float y, float width,
              float size = 11, material::Material ink = kMuted) {
  const int lines = 1 + std::count(words.begin(), words.end(), '\n');
  return compose::kit::at(compose::text(std::string(words)), x, y, width,
                          size * 1.6f * lines)
      .fontSize(size)
      .ink(std::move(ink));
}
Element rule(float x, float y, float width, material::Color color = kRule) {
  return compose::kit::at(x, y, width, 1).fill(color);
}
material::Material normal(glm::vec2 extent) {
  struct Parameters {
    glm::vec2 extent;
  };
  return material::surface::blendNormals(
      material::shader(kCurved, Parameters{extent}),
      material::shader(kBrushing));
}
material::Material tilted(glm::vec2 slope) {
  struct Parameters {
    glm::vec2 slope;
  };
  return material::shader(kTilt, Parameters{slope});
}
material::Material metal(material::Color color, material::Material normals,
                         float roughness = .48f, float normalScale = 1) {
  return material::Material(color).surface({.metallic = 1.f,
                                            .roughness = roughness,
                                            .normal = std::move(normals),
                                            .normalScale = normalScale,
                                            .clearcoat = .04f});
}
material::Material pixels(sk_sp<SkImage> image, glm::vec2 extent) {
  material::Texture texture(std::move(image));
  const glm::vec2 size(texture.size());
  if (size.x > 0 && size.y > 0) {
    glm::mat3 placement(1);
    placement[0][0] = extent.x / size.x;
    placement[1][1] = extent.y / size.y;
    texture.uv(placement);
  }
  return material::image(std::move(texture));
}

}  // namespace light_table

struct LightTable {
  using Element = compose::Element;

  // Prepared material inputs and geometry are built once per density.
  std::unique_ptr<draw::Graphics> brushHeight;
  material::Material plateFinish{light_table::kAluminium};
  material::Material letterFinish{light_table::kGold};
  material::Material copperFinish{light_table::kCopper};
  material::Material brushFinish{material::Color{0, 0, 0, 0}};
  material::Material couponFinish{light_table::kAluminium};
  material::Material couponInk{light_table::kGold};
  material::Material lineFinish{light_table::kCopper};
  Element heroGeometry, couponGeometry, flatGeometry;
  std::unique_ptr<compose::Composer> composer;
  weave::FontContext* fonts = nullptr;
  skgpu::graphite::Recorder* recorder = nullptr;
  GrRecordingContext* recordingContext = nullptr;
  bool painted = false, described = false;
  float preparedDensity = 0;

  void initialize(motion::Engine& engine, weave::FontContext& fontContext) {
    fonts = &fontContext;
    composer = std::make_unique<compose::Composer>(engine, fontContext);
    composer->setSize({light_table::kWidth, light_table::kHeight});
    brushHeight.reset();
    preparedDensity = 0;
    painted = false;
    described = false;
  }

  void prepareInputs(draw::Pen& host) {
    using namespace light_table;
    auto* canvas = host.canvas();
    if (!canvas) return;
    const float density = host.contentScale();
    const bool changedBackend =
        painted && (recorder != canvas->recorder() ||
                    recordingContext != canvas->recordingContext());
    if (brushHeight && density == preparedDensity && !changedBackend) return;
    if (changedBackend) composer->purgeCaches();
    recorder = canvas->recorder();
    recordingContext = canvas->recordingContext();
    painted = true;
    preparedDensity = density;
    brushHeight = std::make_unique<draw::Graphics>(956, 142);
    auto& pen = brushHeight->begin(host);
    pen.push();
    pen.colorMode(draw::RGB);
    pen.blendMode(draw::BLEND);
    pen.background(0);
    pen.randomSeed(0x71ab1eu);
    auto tool = brush::watercolor({1, 1, 1, 1}, 56);
    tool.blend = draw::BLEND;
    tool.opacity = .8f;
    tool.bristles = 40;
    tool.spacing = 1.8f;
    tool.scatter = 1.3f;
    tool.pressure = {.16f, .96f, .20f};
    const std::array<brush::Sample, 5> first{{{{-18, 86}, .16f},
                                              {{192, 32}, .88f},
                                              {{433, 101}, .96f},
                                              {{707, 38}, .86f},
                                              {{976, 69}, .15f}}};
    brush::spline(pen, tool, first, .8f);
    tool.width = 25;
    tool.opacity = .64f;
    tool.bristles = 19;
    const std::array<brush::Sample, 4> crossing{{{{22, 124}, .12f},
                                                 {{280, 67}, .9f},
                                                 {{599, 120}, .82f},
                                                 {{923, 31}, .18f}}};
    brush::spline(pen, tool, crossing, .8f);
    pen.pop();
    brushHeight->end();
    plateFinish = metal(kAluminium, normal({1060, 604}), .46f);
    letterFinish = metal(kGold, material::shader(kBrushing), .52f);
    copperFinish = metal(kCopper, normal({956, 108}), .48f, .35f);
    couponFinish = metal(kAluminium, normal({112, 128}), .48f);
    couponInk = metal(kGold, material::shader(kBrushing), .52f);
    lineFinish = metal(kCopper, tilted({.25f, 0}), .54f);
    const auto height = pixels(brushHeight->image(), {956, 142});
    struct Coverage {
      material::Color color;
    };
    auto coverage =
        material::shader(kCoverage,
                         Coverage{material::Color{.26f, .45f, .50f, 1}},
                         {.textures = {{"height", {}}}})
            .slot("height", height);
    brushFinish = coverage.surface({.metallic = .18f,
                                    .roughness = .52f,
                                    .normal = material::surface::blendNormals(
                                        material::surface::normalFromHeight(
                                            height, {.depth = 6, .step = .8f}),
                                        material::shader(kBrushing)),
                                    .clearcoat = .04f});
    heroGeometry = hero();
    couponGeometry = coupon(true);
    flatGeometry = coupon(false);
    described = false;
  }

  // The main specimens use the ordinary fill, ink and decoration paths.
  Element hero() const {
    using namespace light_table;
    Element face = compose::kit::at(compose::stack(), 10, 10, 1060, 604)
                       .fill(plateFinish)
                       .borderRadius(8)
                       .overflow(compose::Overflow::Clip)
                       .cache(compose::Cache::Picture);
    std::vector<Element> parts{
        label("SPECIMEN 07 / SATIN ALUMINIUM + COPPER + ENAMEL", 26, 20, 900,
              10, material::hexColor(0xc2d0c9)),
        label("FORM / O8", 27, 73, 998, 112, material::Color{0, 0, 0, 0})
            .fontFamily("Georgia, Times New Roman, serif")
            .decorationOutline(compose::Boundary::Glyphs)
            .foreground(
                compose::relief(letterFinish, {.shoulder = 3, .depth = 1.7f})),
        label("MATERIAL INK", 793, 47, 242, 20, letterFinish),
        label("01 / EMBOSSED TYPE", 30, 236, 300, 10,
              material::hexColor(0xb3c6c2)),
        label("02 / PAINTED BRISTLES", 30, 275, 310, 10,
              material::hexColor(0xb3c6c2)),
        compose::kit::at(36, 296, 956, 142).fill(brushFinish),
        label("03 / PRESSURE RIBBON", 30, 454, 360, 10,
              material::hexColor(0xb3c6c2)),
        compose::kit::at(39, 476, 956, 86)
            .shape(shapes::svg("M0 42 C112 0 201 68 327 34 "
                               "C442 1 583 82 680 38 C783 0 876 50 956 22"))
            .stroke(compose::brush::ribbon(path::Profile{{0, .8f},
                                                         {.16f, 28},
                                                         {.34f, 8},
                                                         {.57f, 34},
                                                         {.76f, 14},
                                                         {1, 1}},
                                           copperFinish)),
    };
    constexpr std::array<glm::vec2, 4> slopes{
        {{-.20f, 0}, {.20f, 0}, {0, -.20f}, {0, .20f}}};
    for (size_t i = 0; i < slopes.size(); ++i) {
      const float x = 750 + float(i % 2) * 135;
      const float y = 213 + float(i / 2) * 54;
      parts.push_back(compose::kit::at(x, y, 119, 40)
                          .fill(metal(kAluminium, tilted(slopes[i]), .60f))
                          .borderRadius(3));
    }
    for (int i = 0; i < 14; ++i) {
      parts.push_back(
          compose::kit::at(678 + i * 23.f, 446, 13, 118)
              .shape(shapes::svg("M0 0 L8 118"))
              .stroke(lineFinish,
                      {.width = 1 + (i % 3) * .5f,
                       .position = material::StrokePosition::Center}));
    }
    for (const glm::vec2 center : std::array<glm::vec2, 4>{
             {{17, 17}, {1043, 17}, {17, 587}, {1043, 587}}}) {
      parts.push_back(compose::kit::disc(center, 7)
                          .shape(shapes::circle())
                          .fill(copperFinish)
                          .foreground(compose::relief(
                              copperFinish, {.shoulder = 2, .depth = -.8f})));
    }
    face.children(parts);
    return compose::kit::at(compose::stack(), 0, 0, 1080, 624)
        .fill(kBezel)
        .borderRadius(12)
        .children({face, label("LIGHT TABLE / 07", 21, 606, 800, 9, kPaper)});
  }

  Element coupon(bool raised) const {
    using namespace light_table;
    Element letters =
        label("O8", 9, 29, 100, 48,
              raised ? material::Material(material::Color{0, 0, 0, 0})
                     : couponInk)
            .fontFamily("Georgia, Times New Roman, serif");
    if (raised)
      letters.decorationOutline(compose::Boundary::Glyphs)
          .foreground(
              compose::relief(couponInk, {.shoulder = 2, .depth = 1.2f}));
    return compose::kit::at(compose::stack(), 0, 0, 112, 128)
        .fill(couponFinish)
        .overflow(compose::Overflow::Clip)
        .children(
            {letters,
             compose::kit::at(11, 100, 89, 16)
                 .shape(shapes::svg("M0 8 C25 0 45 16 65 6 L89 10"))
                 .stroke(compose::brush::ribbon(
                     path::Profile{{0, 2}, {.4f, 10}, {1, 2}}, couponInk))});
  }

  Element specimen(float x, float y, const Element& geometry,
                   compose::Cache cache) const {
    return compose::kit::at(compose::box(), x, y, 112, 128)
        .children({compose::stack().cover().children(
            {Element(geometry).key("receiver").cache(cache)})});
  }

  Element describe() const {
    using namespace light_table;
    std::vector<Element> desk{
        Element(heroGeometry).key("hero"),
    };
    const auto card = [](float x, float width, std::string_view title) {
      return compose::kit::at(compose::stack(), x - 324, 678, width, 168)
          .fill(kPanel)
          .borderRadius(6)
          .stroke(compose::stroke(1, compose::Fill::color(kRule)))
          .children({label(title, 8, 5, width - 16, 9, kInk)});
    };
    desk.push_back(card(338, 254, "FLAT INK / RAISED LETTERS"));
    desk.push_back(card(890, 254, "CACHED PICTURE / LIVE"));
    desk.push_back(specimen(22, 702, flatGeometry, compose::Cache::Picture));
    desk.push_back(specimen(148, 702, couponGeometry, compose::Cache::Picture));
    desk.push_back(specimen(574, 702, couponGeometry, compose::Cache::Picture));
    desk.push_back(specimen(700, 702, couponGeometry, compose::Cache::None));
    return compose::stack()
        .width(kWidth)
        .height(kHeight)
        .fill(kPaper)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .children({
            label("LIGHT TABLE", 36, 30, 870, 50, kInk),
            label("A MATERIAL INSPECTION DESK", 39, 94, 780, 13, kMuted),
            label("07 / COMPOSE MATERIALS", 1136, 44, 268, 11, kInk),
            label("Letters, pressure bands and painted bristles\n"
                  "on one surface.",
                  1058, 81, 346, 12, kMuted),
            rule(36, 136, 1368),
            compose::kit::at(compose::box(), 324, 156, 1080, 860)
                .key("primary-desk")
                .children({compose::stack().cover().children(desk)}),
        });
  }

  void drawPrepared(draw::Pen& pen) {
    if (!composer || !fonts || !pen.canvas()) return;
    prepareInputs(pen);
    if (!described) {
      composer->render(describe());
      described = true;
    }
    composer->draw(*pen.canvas());
  }

  // The probe owns an identity canvas; the host pen already owns its transform.
  void drawRaw(SkCanvas& canvas, float density = 1) {
    density = light_table::bounded(density, .125f, 4, 1);
    SkAutoCanvasRestore restore(&canvas, true);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.scale(density, density);
    draw::on(
        canvas, SkSize{light_table::kWidth, light_table::kHeight},
        [this](draw::Pen& pen) { drawPrepared(pen); }, fonts);
  }
  void paint(draw::Pen& pen) { drawPrepared(pen); }
  void setup(sketch::Scene2DContext& ctx) {
    ctx.canvas(light_table::kWidth, light_table::kHeight);
    ctx.background(light_table::kPaper);
    ctx.captureAt(2);
    // The final draw regenerates fixed seeded inputs at its actual density.
    ctx.paintDiscardedFrames(false);
    if (!ctx.fonts) return;
    initialize(ctx.engine, *ctx.fonts);
    ctx.composer.render(compose::pen(
                            "light-table",
                            [this](draw::Pen& pen) { paint(pen); },
                            compose::Cache::None)
                            .width(light_table::kWidth)
                            .height(light_table::kHeight));
  }
};

SIGIL_SKETCH(LightTable, "Study · Materials",
             "An inspection desk of Compose material marks: metal plates, "
             "raised letters, painted bristles and pressure bands.")
