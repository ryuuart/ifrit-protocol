/** Material inspection desk with live, scoped light controls. */
// TAGS: Compose/Materials, Materials/Lighting, Materials/Metal,
// Typography/Material ink, Brushes/Painted data, Composition/Scopes

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
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>
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
constexpr double kLoop = 32;
constexpr float kPi = 3.14159265358979323846f;
constexpr auto kPaper = material::hexColor(0xeae6dc);
constexpr auto kInk = material::hexColor(0x24393f);
constexpr auto kMuted = material::hexColor(0x647b7d);
constexpr auto kRule = material::hexColor(0xb7c2bc);
constexpr auto kPanel = material::hexColor(0xf6f2e8);
constexpr auto kBezel = material::hexColor(0x354c54);
constexpr auto kWarm = material::hexColor(0xb96539);
constexpr auto kCool = material::hexColor(0x367f98);
constexpr material::Color kWhite{1, 1, 1, 1};
constexpr material::Color kAluminium{.64f, .70f, .72f, 1};
constexpr material::Color kCopper{.70f, .35f, .16f, 1};
constexpr material::Color kGold{.64f, .47f, .24f, 1};
constexpr std::array<std::string_view, 8> kPhases{
    "01 / FRONT", "02 / RAKE",  "03 / BEARING", "04 / LIVE RGB",
    "05 / BLACK", "06 / POINT", "07 / SPOT",    "08 / MIXED"};

struct Crop {
  std::string_view name;
  int x, y, width, height;
};
constexpr std::array<Crop, 7> kCrops{{
    {"Flat", 346, 858, 112, 128},
    {"Relief", 472, 858, 112, 128},
    {"Source first", 622, 858, 112, 128},
    {"Source last", 748, 858, 112, 128},
    {"Cached", 898, 858, 112, 128},
    {"Live", 1024, 858, 112, 128},
    {"Nested cool", 1174, 856, 204, 132},
}};

enum class Control {
  Brightness,
  Bearing,
  Elevation,
  X,
  Y,
  Height,
  Range,
  InnerCone,
  OuterCone
};
struct Slider {
  Control control;
  std::string_view name, units;
  float y, low, high;
};
constexpr std::array<Slider, 9> kSliders{{
    {Control::Brightness, "Brightness", "", 314, 0, .45f},
    {Control::Bearing, "Bearing", "deg", 439, 0, 360},
    {Control::Elevation, "Elevation", "deg", 505, 0, 90},
    {Control::X, "X", "%", 596, 0, 1},
    {Control::Y, "Y", "%", 654, 0, 1},
    {Control::Height, "Height", "px", 712, 80, 480},
    {Control::Range, "Range", "px", 770, 200, 1400},
    {Control::InnerCone, "Inner cone", "deg", 854, 0, 60},
    {Control::OuterCone, "Outer cone", "deg", 912, 0, 75},
}};
constexpr std::array<material::Color, 5> kSwatches{{
    kWhite,
    {1, .16f, .06f, 1},
    {.10f, 1, .25f, 1},
    {.12f, .38f, 1, 1},
    {0, 0, 0, 1},
}};

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
double loopTime(double elapsed) {
  return std::isfinite(elapsed) ? std::fmod(std::max(0., elapsed), kLoop) : 0;
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
Element button(std::string_view words, float x, float y, float width,
               bool active) {
  return compose::kit::at(compose::stack(), x, y, width, 30)
      .fill(active ? kInk : kPanel)
      .borderRadius(3)
      .stroke(compose::stroke(1, compose::Fill::color(kRule)))
      .children({label(words, 9, 7, width - 18, 10, active ? kPanel : kInk)});
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
  using Control = light_table::Control;
  using Element = compose::Element;
  enum class Mode { Auto, Hold, Manual };

  // Authored controls share live cells with the retained sources and meters.
  Mode mode = Mode::Auto;
  int phase = 0;
  double seconds = 0;
  material::LightKind kind = material::LightKind::Directional;
  float range = 1100, innerCone = 14, outerCone = 40;
  float keyShare = 1, fillShare = 0;
  motion::Animatable<float> brightness = motion::animatable(.32f);
  motion::Animatable<float> bearing = motion::animatable(120.f);
  motion::Animatable<float> elevation = motion::animatable(90.f);
  motion::Animatable<material::Color> color =
      motion::animatable(light_table::kWhite);
  motion::Animatable<float> sourceX = motion::animatable(.5f);
  motion::Animatable<float> sourceY = motion::animatable(.5f);
  motion::Animatable<float> sourceHeight = motion::animatable(240.f);
  motion::Animatable<float> rangeMeter = motion::animatable(1100.f);
  motion::Animatable<float> innerMeter = motion::animatable(14.f);
  motion::Animatable<float> outerMeter = motion::animatable(40.f);

  // Prepared material inputs and geometry survive light changes.
  std::unique_ptr<draw::Graphics> brushHeight;
  material::Material plateFinish{light_table::kAluminium};
  material::Material letterFinish{light_table::kGold};
  material::Material copperFinish{light_table::kCopper};
  material::Material brushFinish{material::Color{0, 0, 0, 0}};
  material::Material couponFinish{light_table::kAluminium};
  material::Material couponInk{light_table::kGold};
  material::Material coolFinish{light_table::kAluminium};
  material::Material lineFinish{light_table::kCopper};
  Element heroGeometry, couponGeometry, flatGeometry, coolGeometry;
  std::unique_ptr<compose::Composer> composer;
  weave::FontContext* fonts = nullptr;
  skgpu::graphite::Recorder* recorder = nullptr;
  GrRecordingContext* recordingContext = nullptr;
  bool painted = false, pointerDown = false;
  float preparedDensity = 0;

  struct Description {
    material::LightKind kind;
    float range, inner, outer, keyShare, fillShare;
    Mode mode;
    bool operator==(const Description&) const = default;
  };
  std::optional<Description> described;
  std::string readout;

  void initialize(motion::Engine& engine, weave::FontContext& fontContext) {
    fonts = &fontContext;
    composer = std::make_unique<compose::Composer>(engine, fontContext);
    composer->setSize({light_table::kWidth, light_table::kHeight});
    brushHeight.reset();
    preparedDensity = 0;
    painted = false;
    described.reset();
    readout.clear();
    mode = Mode::Auto;
    applyTime(0);
  }

  void preset(int index, double time) {
    using namespace light_table;
    phase = std::clamp(index, 0, 7);
    const float u = float(std::fmod(time, 4.) / 4.);
    kind = material::LightKind::Directional;
    brightness = .32f;
    bearing = 120.f;
    elevation = 90.f;
    color = kWhite;
    sourceX = .5f;
    sourceY = .5f;
    sourceHeight = 240.f;
    keyShare = 1;
    fillShare = 0;
    range = 1100;
    innerCone = 14;
    outerCone = 40;
    if (phase == 1) elevation = 6.f;
    if (phase == 2) {
      bearing = 360 * u;
      elevation = 28.f;
    }
    if (phase == 3) color = material::hsv(240 * u, 1, 1);
    if (phase == 4) brightness = 0.f;
    if (phase == 5) {
      kind = material::LightKind::Point;
      sourceX = .5f + .32f * std::cos(2 * kPi * u);
      sourceY = .5f + .24f * std::sin(2 * kPi * u);
    }
    if (phase == 6) {
      kind = material::LightKind::Spot;
      bearing = 0.f;
      elevation = 90.f;
      sourceHeight = 320.f;
      if (u >= .5f) innerCone = outerCone = 24;
    }
    if (phase == 7) {
      color = material::Color{1, .65f, .35f, 1};
      brightness = .36f;
      bearing = 140.f;
      elevation = 36.f;
      keyShare = 2.f / 3;
      fillShare = 1.f / 3;
    }
    rangeMeter = range;
    innerMeter = innerCone;
    outerMeter = outerCone;
  }

  // Capture state is a pure function of scene time; drawing accumulates
  // nothing.
  void applyTime(double elapsed) {
    seconds = light_table::loopTime(elapsed);
    if (mode == Mode::Auto) preset(int(seconds / 4), seconds);
  }
  void hold() { mode = Mode::Hold; }
  void resume() {
    mode = Mode::Auto;
    applyTime(seconds);
  }
  void selectPhase(int index) {
    mode = Mode::Hold;
    preset((index % 8 + 8) % 8, ((index % 8 + 8) % 8) * 4. + 2);
  }
  void setKind(material::LightKind value) {
    mode = Mode::Manual;
    kind = value;
  }
  void setColor(material::Color value) {
    mode = Mode::Manual;
    color = material::Color{light_table::bounded(value.r, 0, 1, 1),
                            light_table::bounded(value.g, 0, 1, 1),
                            light_table::bounded(value.b, 0, 1, 1), 1};
  }
  float value(Control control) const {
    switch (control) {
      case Control::Brightness:
        return brightness.value();
      case Control::Bearing:
        return bearing.value();
      case Control::Elevation:
        return elevation.value();
      case Control::X:
        return sourceX.value();
      case Control::Y:
        return sourceY.value();
      case Control::Height:
        return sourceHeight.value();
      case Control::Range:
        return range;
      case Control::InnerCone:
        return innerCone;
      case Control::OuterCone:
        return outerCone;
    }
    return 0;
  }
  bool enabled(Control control) const {
    if (control == Control::Bearing || control == Control::Elevation)
      return kind != material::LightKind::Point;
    if (control == Control::X || control == Control::Y ||
        control == Control::Height || control == Control::Range)
      return kind != material::LightKind::Directional;
    if (control == Control::InnerCone || control == Control::OuterCone)
      return kind == material::LightKind::Spot;
    return true;
  }
  void setControl(Control control, float number) {
    using namespace light_table;
    const auto slider = std::find_if(
        kSliders.begin(), kSliders.end(),
        [control](const Slider& s) { return s.control == control; });
    if (slider == kSliders.end()) return;
    const float next =
        bounded(number, slider->low, slider->high, value(control));
    mode = Mode::Manual;
    switch (control) {
      case Control::Brightness:
        brightness = next;
        break;
      case Control::Bearing:
        bearing = next;
        break;
      case Control::Elevation:
        elevation = next;
        break;
      case Control::X:
        sourceX = next;
        break;
      case Control::Y:
        sourceY = next;
        break;
      case Control::Height:
        sourceHeight = next;
        break;
      case Control::Range:
        range = next;
        rangeMeter = next;
        break;
      case Control::InnerCone:
        innerCone = std::min(next, outerCone);
        innerMeter = innerCone;
        break;
      case Control::OuterCone:
        outerCone = next;
        outerMeter = next;
        innerCone = std::min(innerCone, next);
        innerMeter = innerCone;
        break;
    }
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
    coolFinish = metal(kAluminium, normal({204, 132}), .48f);
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
    coolGeometry = cool();
    described.reset();
    readout.clear();
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
  Element cool() const {
    using namespace light_table;
    return compose::kit::at(compose::stack(), 0, 0, 204, 132)
        .fill(coolFinish)
        .overflow(compose::Overflow::Clip)
        .children({label("O8", 19, 14, 182, 67, material::Color{0, 0, 0, 0})
                       .fontFamily("Georgia, Times New Roman, serif")
                       .decorationOutline(compose::Boundary::Glyphs)
                       .foreground(compose::relief(
                           coolFinish, {.shoulder = 2.5f, .depth = 1.6f})),
                   compose::kit::at(14, 102, 178, 12).fill(coolFinish)});
  }

  // A translated scope carries both its receiver and its positioned emitters.
  Element keyLight(glm::vec2 extent, float scale) const {
    material::Light source;
    source.direction = bearing;
    source.elevation = elevation;
    source.color = color;
    source.intensity = motion::bind(brightness, {.to = {0, keyShare}});
    source.ambient = 0;
    source.kind = kind;
    source.range = range * scale;
    source.innerAngle = innerCone;
    source.outerAngle = outerCone;
    return compose::light(source)
        .key("key")
        .translateX(motion::bind(sourceX, {.to = {0, extent.x}}))
        .translateY(motion::bind(sourceY, {.to = {0, extent.y}}))
        .translateZ(motion::bind(sourceHeight, {.to = {0, scale}}));
  }
  Element fillLight(glm::vec2 extent, float scale) const {
    material::Light source;
    source.color = material::Color{.24f, .64f, 1, 1};
    source.intensity = motion::bind(brightness, {.to = {0, fillShare}});
    source.ambient = 0;
    source.kind = material::LightKind::Point;
    source.position = {extent.x * .83f, extent.y * .72f, 200 * scale};
    source.range = 1100 * scale;
    return compose::light(source).key("fill");
  }
  Element specimen(float x, float y, const Element& geometry,
                   compose::Cache cache, bool sourceFirst = true) const {
    std::vector<Element> children;
    const auto key = keyLight({112, 128}, 128.f / 624);
    const auto fill = fillLight({112, 128}, 128.f / 624);
    const auto receiver = Element(geometry).key("receiver").cache(cache);
    if (sourceFirst)
      children = {key, fill, receiver};
    else
      children = {receiver, fill, key};
    return compose::kit::at(compose::scene(), x, y, 112, 128)
        .children({compose::stack().cover().children(children)});
  }

  motion::Animatable<float> meter(Control control) const {
    switch (control) {
      case Control::Brightness:
        return brightness;
      case Control::Bearing:
        return bearing;
      case Control::Elevation:
        return elevation;
      case Control::X:
        return sourceX;
      case Control::Y:
        return sourceY;
      case Control::Height:
        return sourceHeight;
      case Control::Range:
        return rangeMeter;
      case Control::InnerCone:
        return innerMeter;
      case Control::OuterCone:
        return outerMeter;
    }
    return 0.f;
  }

  Element controlPanel() const {
    using namespace light_table;
    std::vector<Element> items{
        label("SOURCE / CONTROLS", 52, 173, 220, 11, kInk),
        button("AUTO", 52, 201, 105, mode == Mode::Auto),
        button("HOLD", 171, 201, 105, mode == Mode::Hold),
        button("KEY", 52, 246, 66, kind == material::LightKind::Directional),
        button("POINT", 131, 246, 66, kind == material::LightKind::Point),
        button("SPOT", 210, 246, 66, kind == material::LightKind::Spot),
        label("KEY COLOR", 52, 348, 180, 10, kInk),
        label("PLACEMENT / POINT + SPOT", 52, 549, 224, 10, kInk),
        label("CONE / SPOT", 52, 809, 224, 10, kInk),
        button("PREVIOUS", 52, 966, 105, false),
        button("NEXT", 171, 966, 105, false),
    };
    for (size_t i = 0; i < kSwatches.size(); ++i) {
      items.push_back(
          compose::kit::at(52 + i * 46.f, 370, 40, 24)
              .fill(kSwatches[i])
              .borderRadius(3)
              .stroke(compose::stroke(1, compose::Fill::color(kRule))));
    }
    for (const auto& slider : kSliders) {
      const auto ink = enabled(slider.control) ? kInk : kRule;
      items.push_back(label(slider.name, 52, slider.y - 22, 150, 11, ink));
      items.push_back(
          compose::kit::at(52, slider.y, 224, 20).fill(kPaper).borderRadius(3));
      items.push_back(compose::kit::at(55, slider.y + 8, 218, 4)
                          .fill(kRule)
                          .borderRadius(2));
      items.push_back(
          compose::kit::at(55, slider.y + 8, 218, 4)
              .transformOrigin(compose::pct(0), compose::pct(50))
              .scaleX(motion::bind(meter(slider.control),
                                   {.from = {slider.low, slider.high},
                                    .clampFrom = true,
                                    .to = {0, 1}}))
              .fill(enabled(slider.control) ? kWarm : kRule)
              .borderRadius(2));
      items.push_back(
          compose::kit::at(50, slider.y + 3, 9, 14)
              .translateX(motion::bind(meter(slider.control),
                                       {.from = {slider.low, slider.high},
                                        .clampFrom = true,
                                        .to = {0, 218}}))
              .fill(ink)
              .borderRadius(2));
    }
    return compose::stack().cover().children(
        {compose::kit::at(36, 156, 264, 860)
             .fill(kPanel)
             .borderRadius(9)
             .stroke(compose::stroke(1, compose::Fill::color(kRule))),
         compose::stack().cover().children(items)});
  }

  Element describe() const {
    using namespace light_table;
    std::vector<Element> desk{
        Element(heroGeometry).key("hero"),
        keyLight({1080, 624}, 1),
        fillLight({1080, 624}, 1),
        label("READ THE SAME SURFACE / CHANGE ONLY THE LIGHT", 0, 640, 1050, 11,
              kInk),
    };
    const auto card = [](float x, float width, std::string_view title) {
      return compose::kit::at(compose::stack(), x - 324, 678, width, 168)
          .fill(kPanel)
          .borderRadius(6)
          .stroke(compose::stroke(1, compose::Fill::color(kRule)))
          .children({label(title, 8, 5, width - 16, 9, kInk)});
    };
    desk.push_back(card(338, 254, "FLAT INK / RAISED LETTERS"));
    desk.push_back(card(614, 254, "SOURCE FIRST / SOURCE LAST"));
    desk.push_back(card(890, 254, "CACHED PICTURE / LIVE"));
    desk.push_back(card(1166, 220, "NESTED SCENE / FIXED COOL KEY"));
    desk.push_back(specimen(22, 702, flatGeometry, compose::Cache::Picture));
    desk.push_back(specimen(148, 702, couponGeometry, compose::Cache::Picture));
    desk.push_back(specimen(298, 702, couponGeometry, compose::Cache::Picture));
    desk.push_back(
        specimen(424, 702, couponGeometry, compose::Cache::Picture, false));
    desk.push_back(specimen(574, 702, couponGeometry, compose::Cache::Picture));
    desk.push_back(specimen(700, 702, couponGeometry, compose::Cache::None));
    material::Light fixed;
    fixed.direction = 225.f;
    fixed.elevation = 55.f;
    fixed.color = material::Color{.24f, .72f, 1, 1};
    fixed.intensity = .32f;
    fixed.ambient = 0;
    desk.push_back(compose::kit::at(compose::scene(), 850, 700, 204, 132)
                       .children({compose::stack().cover().children(
                           {Element(coolGeometry).key("cool-receiver"),
                            compose::light(fixed)})}));
    if (kind != material::LightKind::Directional) {
      desk.push_back(
          compose::kit::ring(
              {0, 0}, 12, compose::stroke(1.5f, compose::Fill::color(kPaper)))
              .translateX(motion::bind(sourceX, {.to = {0, 1080}}))
              .translateY(motion::bind(sourceY, {.to = {0, 624}})));
      desk.push_back(
          compose::kit::ring({0, 0}, 4,
                             compose::stroke(1, compose::Fill::color(kWarm)))
              .translateX(motion::bind(sourceX, {.to = {0, 1080}}))
              .translateY(motion::bind(sourceY, {.to = {0, 624}})));
    }
    return compose::stack()
        .width(kWidth)
        .height(kHeight)
        .fill(kPaper)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .children({
            label("LIGHT TABLE", 36, 30, 870, 50, kInk),
            label("A MATERIAL INSPECTION DESK", 39, 94, 780, 13, kMuted),
            label("07 / COMPOSE MATERIALS", 1136, 44, 268, 11, kInk),
            label("Letters, pressure bands and painted bristles.\n"
                  "One surface; eight ways to look at it.",
                  1058, 81, 346, 12, kMuted),
            rule(36, 136, 1368),
            controlPanel(),
            compose::kit::at(compose::scene(), 324, 156, 1080, 860)
                .key("primary-scene")
                .children({compose::stack().cover().children(desk)}),
            compose::slot("light-table.readouts").cover(),
            label("DRAG A CONTROL TO ENTER MANUAL / AUTO RESTORES THE STUDY",
                  36, 1036, 1000, 10, kMuted),
            label("NO AMBIENT / NO ENVIRONMENT", 1111, 1036, 293, 10, kMuted),
        });
  }

  std::string readoutKey() const {
    char text[512];
    const auto rgb = color.value();
    std::snprintf(text, sizeof(text),
                  "%d %d %.3f %.0f %.0f %.0f %.0f %.0f "
                  "%.0f %.0f %.0f %.2f %.2f %.2f",
                  phase, int(mode), brightness.value(), bearing.value(),
                  elevation.value(), sourceX.value() * 100,
                  sourceY.value() * 100, sourceHeight.value(), range, innerCone,
                  outerCone, rgb.r, rgb.g, rgb.b);
    return text;
  }
  Element readouts() const {
    using namespace light_table;
    std::vector<Element> items{
        label(kPhases[phase], 1031, 117, 373, 11, kInk),
        label(mode == Mode::Auto   ? "AUTO"
              : mode == Mode::Hold ? "HOLD"
                                   : "MANUAL",
              220, 173, 70, 10, mode == Mode::Manual ? kWarm : kMuted),
    };
    for (const auto& slider : kSliders) {
      char text[48];
      const float number =
          value(slider.control) *
          (slider.control == Control::X || slider.control == Control::Y ? 100
                                                                        : 1);
      if (slider.control == Control::Brightness)
        std::snprintf(text, sizeof(text), "%.3f", number);
      else
        std::snprintf(text, sizeof(text), "%.0f %.*s", number,
                      int(slider.units.size()), slider.units.data());
      items.push_back(label(text, 205, slider.y - 22, 79, 10,
                            enabled(slider.control) ? kInk : kRule)
                          .fontFamily("Menlo, monospace"));
    }
    const auto rgb = color.value();
    char text[80];
    std::snprintf(text, sizeof(text), "%.2f  %.2f  %.2f", rgb.r, rgb.g, rgb.b);
    items.push_back(
        label(text, 128, 348, 155, 9, kMuted).fontFamily("Menlo, monospace"));
    return compose::stack().cover().children(items);
  }

  void drawPrepared(draw::Pen& pen) {
    if (!composer || !fonts || !pen.canvas()) return;
    prepareInputs(pen);
    const Description next{kind,     range,     innerCone, outerCone,
                           keyShare, fillShare, mode};
    if (!described || *described != next) {
      composer->render(describe());
      described = next;
      readout.clear();
    }
    const auto text = readoutKey();
    if (text != readout) {
      composer->renderSlot("light-table.readouts", readouts());
      readout = text;
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
  void handlePointer(const draw::Pen& pen) {
    using namespace light_table;
    const float x = pen.mouseX, y = pen.mouseY;
    if (pen.mouseIsPressed) {
      for (const auto& slider : kSliders) {
        if (enabled(slider.control) && x >= 52 && x <= 276 && y >= slider.y &&
            y <= slider.y + 20)
          setControl(slider.control,
                     slider.low + (slider.high - slider.low) *
                                      std::clamp((x - 55) / 218, 0.f, 1.f));
      }
      if (kind != material::LightKind::Directional && x >= 334 && x <= 1394 &&
          y >= 166 && y <= 760) {
        setControl(Control::X, (x - 324) / 1080);
        setControl(Control::Y, (y - 156) / 624);
      }
      if (!pointerDown) {
        if (y >= 201 && y <= 231) {
          if (x >= 52 && x <= 157) resume();
          if (x >= 171 && x <= 276) hold();
        }
        if (y >= 246 && y <= 276) {
          if (x >= 52 && x <= 118) setKind(material::LightKind::Directional);
          if (x >= 131 && x <= 197) setKind(material::LightKind::Point);
          if (x >= 210 && x <= 276) setKind(material::LightKind::Spot);
        }
        if (y >= 370 && y <= 394)
          for (size_t i = 0; i < kSwatches.size(); ++i)
            if (x >= 52 + i * 46 && x <= 92 + i * 46) setColor(kSwatches[i]);
        if (y >= 966 && y <= 996) {
          if (x >= 52 && x <= 157) selectPhase(phase - 1);
          if (x >= 171 && x <= 276) selectPhase(phase + 1);
        }
      }
    }
    pointerDown = pen.mouseIsPressed;
  }
  void paint(draw::Pen& pen) {
    handlePointer(pen);
    drawPrepared(pen);
  }
  void setup(sketch::SketchContext& ctx) {
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
  void update(double elapsed) { applyTime(elapsed); }
};

SIGIL_SKETCH(LightTable, "Study · Materials",
             "An inspection desk for live light color, strength, direction, "
             "point placement and spot cones over Compose material marks.")
