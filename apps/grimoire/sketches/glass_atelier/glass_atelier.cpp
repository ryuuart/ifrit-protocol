/** @file
 * A fictional optical-material editor built from retained Compose nodes.
 * Shape coverage supplies the normals; glass samples the live destination.
 *
 * EDIT THESE FIRST
 *   kControls — index, depth, sample reach, shoulder and motion amplitude.
 *   kPhases — the eight held states in the thirty-two-second loop.
 *   outlineOf — the six cut contours shared by refraction and surface paint.
 */

// TAGS: Materials/Glass, Materials/Lighting, Typography/Material ink,
// Geometry/Contours, Compose/Layers, Interfaces/Controls

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Operations.h>
#include <sigilgeometry/path/Transform.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Bevel.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "../shapeworks_lab/Environments.h"

namespace compose = sigil::compose;
namespace geometry = sigil::geometry;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;

namespace {

using compose::Element;
using material::hexColor;

constexpr float kWidth = 1440, kHeight = 960;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kLoopSeconds = 32;
constexpr material::Color kInk{0.08f, 0.13f, 0.17f, 1};
constexpr material::Color kMuted{0.37f, 0.43f, 0.46f, 1};
constexpr material::Color kPaper{0.94f, 0.95f, 0.93f, 1};
constexpr material::Color kRule{0.23f, 0.31f, 0.34f, 0.20f};

struct Controls {
  int heldPhase = -1;
  float ior = 1.4714f;
  float thickness = 28;
  float sampleRadius = 32;
  float shoulder = 14;
  float coatingOpacity = 0.045f;
  float roughness = 0.10f;
  float motion = 1;
};
constexpr Controls kControls;

struct Phase {
  std::string_view name;
  std::string_view detail;
};
constexpr std::array kPhases{
    Phase{"CLEAR / WORKING", "Live destination through a cut optical shoulder"},
    Phase{"INDEX / ONE", "Unit index leaves the destination undisplaced"},
    Phase{"DEPTH / ZERO", "No optical path length; the coating remains"},
    Phase{"REACH / ZERO", "Zero sampling reach is a deliberate identity"},
    Phase{"BOUND / SATURATED", "High index and depth, sixteen-pixel reach"},
    Phase{"COAT / OVERLAP", "Translucent coating and nested shaped clipping"},
    Phase{"NORMAL / REVERSE",
          "Back-facing normals leave the destination intact"},
    Phase{"LAYOUT / COMPACT", "A narrower workbench and resized optical panes"},
};

float finite(float value, float low, float high, float fallback) {
  return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

std::string number(float value, int precision = 2) {
  std::array<char, 40> result{};
  std::snprintf(result.data(), result.size(), "%.*f", precision,
                static_cast<double>(value));
  return result.data();
}

Element placed(Element node, float x, float y, float width, float height) {
  return compose::kit::at(std::move(node), x, y, width, height).absolute();
}

Element boxAt(float x, float y, float width, float height) {
  return placed(compose::box(), x, y, width, height);
}

Element label(std::string_view value, float x, float y, float width,
              float size = 12, material::Color ink = kMuted,
              float weight = 500) {
  return placed(compose::text(std::string(value)), x, y, width, size * 1.55f)
      .fontSize(size)
      .fontWeight(weight)
      .ink(ink);
}

Element rule(float x, float y, float width, float height = 1,
             material::Color ink = kRule) {
  return boxAt(x, y, width, height).fill(ink);
}

geometry::path::Outline outlineOf(int kind, glm::vec2 size) {
  if (size.x <= 0 || size.y <= 0) return {};
  switch (kind) {
    case 0:
      return shapes::squircle(4.5f).outline(size);
    case 1:
      return shapes::circle().outline(size);
    case 2:
      return shapes::annulus(0.48f).outline(size);
    case 3:
      return shapes::rounded(shapes::star(5, 0.57f),
                             std::min(size.x, size.y) * 0.055f)
          .outline(size);
    case 4: {
      const auto disc = shapes::circle().outline(size);
      const auto cut = shapes::circle()
                           .outline(size * 0.88f)
                           .transformed(geometry::path::Transform::translate(
                               {size.x * 0.34f, size.y * 0.04f}));
      return geometry::path::operations::subtract(disc, cut);
    }
    default: {
      // Four lobes are joined by the geometry library's path boolean.
      std::array<geometry::path::Outline, 4> lobes;
      for (size_t i = 0; i < lobes.size(); ++i) {
        const float angle = static_cast<float>(i) * kPi * 0.5f;
        lobes[i] = shapes::circle()
                       .outline(size * 0.68f)
                       .transformed(geometry::path::Transform::translate(
                           {size.x * (0.16f + 0.16f * std::cos(angle)),
                            size.y * (0.16f + 0.16f * std::sin(angle))}));
      }
      return geometry::path::operations::unite(lobes);
    }
  }
}

struct Cut {
  geometry::path::Outline outline;
  material::Material normal = material::Color{0.5f, 0.5f, 1, 1};
};

Cut cutOf(int kind, glm::vec2 size, float shoulder) {
  Cut cut;
  cut.outline = outlineOf(kind, size);
  if (!cut.outline.empty()) {
    const SkPath path = geometry::path::toSk(cut.outline);
    cut.normal = material::image(
        material::skia::bevelNormals(path, std::max(0.5f, shoulder), 1.1f));
  }
  return cut;
}

material::Material typeMetal() {
  return material::linearGradient({0, 0}, {0.3f, 1},
                                  {{0, hexColor(0x263d46)},
                                   {0.46f, hexColor(0x547987)},
                                   {0.58f, hexColor(0x1b333e)},
                                   {1, hexColor(0x203f49)}})
      .surface(
          {.metallic = 0.72f, .roughness = 0.32f, .reflectionWeight = 0.12f});
}

struct GlassAtelier {
  int phaseIndex = 0;
  motion::Animatable<float> ior = motion::animatable(kControls.ior);
  motion::Animatable<float> depth = motion::animatable(kControls.thickness);
  motion::Animatable<float> bearing = motion::animatable(120.0f);
  motion::Animatable<float> reflection = motion::animatable(0.0f);
  motion::Animatable<float> paneX = motion::animatable(0.0f);
  motion::Animatable<float> paneY = motion::animatable(0.0f);
  motion::Animatable<float> paneTurn = motion::animatable(-8.0f);
  motion::Animatable<float> stripeX = motion::animatable(0.0f);
  motion::Animatable<float> orbitX = motion::animatable(0.0f);
  motion::Animatable<float> orbitY = motion::animatable(0.0f);
  std::array<Cut, 6> small;
  Cut hero, compactHero, ring, crescent, emptyCut, onePixel;
  material::EnvironmentMap studio;
  material::Material graph = kPaper;
  material::Material alphaGrid = kPaper;
  material::Material narrowGrid = kPaper;
  material::Material stripe = kPaper;

  Controls state(int phase) const {
    Controls out = kControls;
    out.ior = finite(out.ior, 0, 3, 1.4714f);
    out.thickness = finite(out.thickness, 0, 80, 28);
    out.sampleRadius = finite(out.sampleRadius, 0, 64, 32);
    out.shoulder = finite(out.shoulder, 0.5f, 40, 14);
    out.coatingOpacity = finite(out.coatingOpacity, 0, 1, 0.045f);
    out.roughness = finite(out.roughness, 0, 1, 0.1f);
    if (phase == 1) out.ior = 1;
    if (phase == 2) out.thickness = 0;
    if (phase == 3) out.sampleRadius = 0;
    if (phase == 4) {
      out.ior = 2.4f;
      out.thickness = 56;
      out.sampleRadius = 16;
    }
    if (phase == 5) out.coatingOpacity = 0.22f;
    return out;
  }

  material::Material coating(const Cut& cut, const Controls& c) const {
    return material::linearGradient(
               {0, 0}, {0.8f, 1},
               {{0, {0.75f, 0.95f, 1, c.coatingOpacity}},
                {0.46f, {0.32f, 0.57f, 0.61f, c.coatingOpacity * 0.65f}},
                {1, {0.80f, 0.92f, 0.86f, c.coatingOpacity}}})
        .surface({.roughness = c.roughness,
                  .normal = cut.normal,
                  .normalDirectX = true,
                  .clearcoat = 0.8f,
                  .reflectionWeight = 0.45f});
  }

  Element pane(const Cut& cut, float x, float y, float width, float height,
               const Controls& c, bool reverse = false,
               bool driven = true) const {
    material::Filter optical = material::Filter::glass(
        {.ior = c.ior,
         .thickness = c.thickness,
         .sampleRadius = c.sampleRadius,
         .normal = reverse
                       ? material::Material(material::Color{0.5f, 0.5f, 0, 1})
                       : cut.normal,
         .normalDirectX = true});
    if (driven) optical.bind("ior", ior).bind("thickness", depth);
    return boxAt(x, y, width, height)
        .shape(compose::heldPath(cut.outline))
        .backdropFilter(std::move(optical))
        .fill(coating(cut, c))
        .stroke(compose::stroke(
            0.8f, compose::Fill::color({0.73f, 0.89f, 0.91f, 0.72f}),
            compose::PathFormat::Align::Inner));
  }

  Element sidebar() const {
    Element out =
        boxAt(24, 106, 194, 646)
            .borderRadius({18})
            .fill(material::linearGradient(
                {0, 0}, {0, 1}, {hexColor(0xe8ece8), hexColor(0xf5f5ef)}))
            .stroke(compose::stroke(1, compose::Fill::color(kRule)));
    out.children({label("LIBRARY", 18, 19, 150, 11, kMuted, 700),
                  label("Cut contours", 18, 45, 162, 21, kInk, 600),
                  rule(18, 88, 158)});
    constexpr std::array<std::string_view, 6> names{
        "Soft square", "Lens", "Annulus", "Star", "Crescent", "Clover"};
    for (int i = 0; i < 6; ++i) {
      const float y = 108 + i * 68.0f;
      out.children({boxAt(11, y - 7, 172, 59)
                        .borderRadius({10})
                        .fill(i == 0 ? material::Color{0.75f, 0.83f, 0.81f, 1}
                                     : material::Color{0, 0, 0, 0}),
                    boxAt(22, y + 2, 36, 36)
                        .shape(compose::heldPath(outlineOf(i, {36, 36})))
                        .fill(typeMetal()),
                    label(names[i], 70, y + 8, 103, 13, kInk),
                    label("0" + std::to_string(i + 1), 148, y - 2, 24, 8)});
    }
    out.children({rule(18, 533, 158), label("COLLECTION / 06", 18, 550, 156, 9),
                  label("Normals follow every cut.", 18, 575, 163, 11),
                  label("No reference bitmaps.", 18, 594, 163, 11)});
    return out;
  }

  Element workbench(const Controls& c, int phase) const {
    const float width = phase == 7 ? 750.0f : 906.0f;
    const float mainWidth = phase == 7 ? 460.0f : 580.0f;
    const Cut& mainCut = phase == 7 ? compactHero : hero;
    Element out = boxAt(234, 106, width, 646)
                      .borderRadius({22})
                      .fill(hexColor(0xdde7e2))
                      .overflow(compose::Overflow::Clip);
    out.children({boxAt(0, 0, width, 646).fill(graph),
                  label("SURFACE / 07", 24, 20, 218, 12, kInk, 700),
                  label("LIVE OPTICAL WORKBENCH", 24, 46, 570, 29, kInk, 600)
                      .ink(typeMetal()),
                  label("Contour normals / moving content / retained placement",
                        24, 86, width - 46, 11),
                  rule(24, 113, width - 48)});
    // Actual moving Compose content is behind the glass, not a frozen image.
    Element content = boxAt(0, 121, width, 490);
    content.children({boxAt(-150, 66, width + 300, 120)
                          .fill(stripe)
                          .rotate(-12)
                          .translateX(stripeX),
                      boxAt(100, 20, 250, 250)
                          .shape(shapes::annulus(0.82f))
                          .fill(hexColor(0xeb6d42))
                          .translateX(orbitX)
                          .translateY(orbitY),
                      boxAt(width - 390, 157, 300, 300)
                          .shape(shapes::annulus(0.92f))
                          .fill(hexColor(0x1b6467))
                          .translateX(orbitY),
                      label("Aa", 63, 63, 270, 158, hexColor(0x153d47), 650)
                          .ink(typeMetal()),
                      label("0123456789", 111, 326, width - 180, 47,
                            hexColor(0x285c61), 500),
                      rule(24, 261, width - 48, 2, hexColor(0xec824c)),
                      rule(width * 0.52f, 5, 2, 410, hexColor(0x2c7c83))});
    out.children({std::move(content)});
    Element lenses = boxAt(48, 173, mainWidth + 220, 420)
                         .translateX(paneX)
                         .translateY(paneY);
    lenses.children(
        {pane(crescent, 1, 2, 244, 244, c, phase == 6).rotate(-17),
         pane(mainCut, 126, 40, mainWidth, 328, c, phase == 6).rotate(paneTurn),
         pane(ring, mainWidth - 26, 178, 204, 204, c, phase == 6).rotate(9)});
    if (phase == 5)
      lenses = boxAt(48, 173, mainWidth + 220, 380)
                   .borderRadius({82})
                   .overflow(compose::Overflow::Clip)
                   .children({std::move(lenses).left(0).top(0)});
    out.children({std::move(lenses),
                  label("CUT / BOROSILICATE", 24, 608, 360, 11, kInk, 650),
                  label("VIEW / 100%", width - 129, 608, 107, 10, kMuted),
                  label("07", width - 81, 20, 60, 31, kInk, 400)});
    return out;
  }

  Element inspector(const Controls& c, int phase) const {
    const float x = phase == 7 ? 1000.0f : 1156.0f;
    const float width = kWidth - x - 24;
    Element out = boxAt(x, 106, width, 646)
                      .borderRadius({18})
                      .fill(hexColor(0xe8edeb))
                      .stroke(compose::stroke(1, compose::Fill::color(kRule)));
    out.children({label("MATERIAL", 20, 19, width - 40, 11, kMuted, 700),
                  label("Clear glass", 20, 44, width - 40, 24, kInk, 550)
                      .ink(typeMetal()),
                  label("A bounded optical layer", 20, 80, width - 40, 11),
                  rule(20, 110, width - 40)});
    const std::array<std::pair<std::string_view, float>, 4> values{
        {{"REFRACTIVE INDEX", c.ior},
         {"THICKNESS / px", c.thickness},
         {"SAMPLE REACH / px", c.sampleRadius},
         {"COATING / alpha", c.coatingOpacity}}};
    for (size_t i = 0; i < values.size(); ++i) {
      const float y = 129 + static_cast<float>(i) * 85;
      const float fraction = i == 0   ? c.ior / 3
                             : i == 1 ? c.thickness / 80
                             : i == 2 ? c.sampleRadius / 64
                                      : c.coatingOpacity;
      out.children({label(values[i].first, 20, y, width - 42, 9, kMuted, 650),
                    label(number(values[i].second), 20, y + 20, width - 42, 23,
                          kInk, 500)
                        .ink(typeMetal()),
                    boxAt(20, y + 61, width - 40, 4)
                        .borderRadius({2})
                        .fill(hexColor(0xb9cbc5)),
                    boxAt(20, y + 61, (width - 40) * fraction, 4)
                        .borderRadius({2})
                        .fill(hexColor(0x285d65)),
                    boxAt(17 + (width - 40) * fraction, y + 57, 12, 12)
                        .shape(shapes::circle())
                        .fill(typeMetal())});
    }
    out.children(
        {rule(20, 485, width - 40),
         label("COATING + REFRACTION", 20, 505, width - 40, 10, kInk, 700),
         label("Green-down contour normals", 20, 532, width - 40, 10),
         label(phase == 6 ? "Back-facing identity" : "Front-facing shoulder",
               20, 552, width - 40, 10),
         boxAt(20, 586, width - 40, 36)
             .borderRadius({9})
             .fill(typeMetal())
             .children({label("SAVE MATERIAL", 14, 9, width - 65, 11,
                              hexColor(0xf0f4ec), 650)})});
    return out;
  }

  Element shapeRail(const Controls& c) const {
    Element out =
        boxAt(24, 770, 914, 153).borderRadius({18}).fill(hexColor(0xe7ece6));
    out.children(
        {label("CUT LIBRARY / SAME MATERIAL", 18, 12, 560, 10, kInk, 650),
         label("HOLES STAY OPEN", 702, 12, 192, 9)});
    constexpr std::array<std::string_view, 6> names{
        "SQUIRCLE", "DISC", "ANNULUS", "STAR", "CRESCENT", "CLOVER"};
    for (int i = 0; i < 6; ++i) {
      const float x = 19 + i * 148.0f;
      out.children(
          {boxAt(x, 34, 136, 88)
               .fill(alphaGrid)
               .borderRadius({10})
               .overflow(compose::Overflow::Clip)
               .children({pane(small[i], 27, 4, 80, 80, c, false, false)}),
           label(names[i], x, 128, 136, 9, kInk, 650)});
    }
    return out;
  }

  Element extentReferences() const {
    Controls fixed;
    fixed.ior = 1.4714f;
    fixed.thickness = 28;
    fixed.sampleRadius = 32;
    fixed.coatingOpacity = 0;
    Element out =
        boxAt(954, 770, 462, 153).borderRadius({18}).fill(hexColor(0xe7ece6));
    out.children({label("IDENTITY + EXTENT", 18, 12, 400, 10, kInk, 650)});
    constexpr std::array<std::string_view, 4> names{"IOR 1", "RADIUS 0", "0 px",
                                                    "1 px"};
    for (int i = 0; i < 4; ++i) {
      Controls c = fixed;
      if (i == 0) c.ior = 1;
      if (i == 1) c.sampleRadius = 0;
      const float x = 18 + i * 109.0f;
      const float width = i < 2 ? 80.0f : i == 2 ? 0.0f : 1.0f;
      const Cut& cut = i < 2 ? small[0] : i == 2 ? emptyCut : onePixel;
      out.children(
          {boxAt(x, 34, 96, 88)
               .fill(narrowGrid)
               .children({pane(cut, i < 2 ? 8.0f : 47.0f, 8, width,
                               i < 2 ? 80.0f : 72.0f, c, false, false)}),
           rule(x + 35, 61, 1, 35, hexColor(0x266e79)),
           rule(x + 59, 61, 1, 35, hexColor(0x266e79)),
           rule(x + 35, 61, 7, 1, hexColor(0x266e79)),
           rule(x + 53, 95, 7, 1, hexColor(0x266e79)),
           label(names[i], x, 128, 96, 9, kInk, 650)});
    }
    return out;
  }

  Element describe(int phase) {
    const Controls c = state(phase);
    ior = c.ior;
    depth = c.thickness;
    const material::Lighting lighting(
        material::studio({.direction = bearing,
                          .elevation = 38,
                          .intensity = 0.70f,
                          .ambient = 0.26f}),
        material::environment(material::image(studio.texture()),
                              {.rotation = reflection, .intensity = 0.35f}));
    Element page = compose::stack()
                       .width(kWidth)
                       .height(kHeight)
                       .fontFamily("Inter, Helvetica Neue, sans-serif")
                       .lighting(lighting)
                       .fill(hexColor(0xf5f5ef));
    page.children({label("FORM", 27, 21, 120, 28, kInk, 720).ink(typeMetal()),
                   label("GLASS ATELIER", 151, 27, 395, 16, kInk, 500),
                   label("Material library", 550, 28, 148, 12),
                   label("Surface editor", 715, 28, 161, 12, kInk, 650),
                   rule(713, 58, 95, 2, hexColor(0x326e76)),
                   label("EXPORT", 1290, 29, 114, 11, kInk, 650),
                   rule(24, 72, 1392),
                   label(kPhases[phase].name, 25, 83, 490, 10, kInk, 700),
                   label(kPhases[phase].detail, 576, 83, 832, 10), sidebar(),
                   workbench(c, phase), inspector(c, phase), shapeRail(c),
                   extentReferences(),
                   label("32 s / 02 CLEAR · 06 IOR1 · 10 DEPTH0 · 14 REACH0 · "
                         "18 BOUND · 22 OVERLAP · 26 REVERSE · 30 COMPACT",
                         25, 936, 1360, 9)});
    return page;
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.oversample(1);
    ctx.captureAt(2);
    ctx.background(hexColor(0xf5f5ef));
    const float shoulder = finite(kControls.shoulder, 0.5f, 40, 14);
    for (int i = 0; i < 6; ++i) small[i] = cutOf(i, {80, 80}, 8);
    hero = cutOf(0, {580, 328}, shoulder);
    compactHero = cutOf(0, {460, 328}, shoulder);
    ring = cutOf(2, {204, 204}, shoulder * 0.6f);
    crescent = cutOf(4, {244, 244}, shoulder);
    onePixel = cutOf(0, {1, 72}, 2);
    studio = shapeworks_lab::studioEnvironment(256);
    auto paper = material::from(hexColor(0xdfe9e4));
    paper.layer(material::image(
        material::pattern::gridLines(24, 0.8f, {0.13f, 0.32f, 0.31f, 0.24f})
            .texture()));
    graph = std::move(paper);
    alphaGrid = material::image(
        material::pattern::checker(11, hexColor(0xd6dfd9), hexColor(0xf8f9f0))
            .texture());
    narrowGrid = material::image(
        material::pattern::stripes(4, 4, {0.2f, 0.43f, 0.47f, 0.3f}).texture());
    stripe = material::linearGradient(
                 {0, 0}, {0.7f, 1},
                 {hexColor(0xef925e), hexColor(0xeeb770), hexColor(0x83bebb)})
                 .layer(material::image(
                     material::pattern::stripes(2, 18, {1, 1, 0.93f, 0.65f})
                         .texture()));
    phaseIndex =
        kControls.heldPhase < 0 ? 0 : std::clamp(kControls.heldPhase, 0, 7);
    ctx.composer.render(describe(phaseIndex));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double seconds = std::isfinite(elapsed) ? std::max(0.0, elapsed) : 0;
    const float t =
        static_cast<float>(std::fmod(seconds, double(kLoopSeconds)));
    const float angle = t * 2 * kPi / kLoopSeconds;
    const float amplitude = finite(kControls.motion, 0, 2, 1);
    bearing = 120 + 65 * std::sin(angle);
    reflection = 32 * std::sin(angle - 0.5f);
    paneX = 14 * amplitude * std::sin(angle);
    paneY = 8 * amplitude * std::sin(angle * 2);
    paneTurn = -8 + 3 * amplitude * std::sin(angle);
    stripeX = 60 * amplitude * std::sin(angle * 2);
    orbitX = 38 * amplitude * std::cos(angle);
    orbitY = 20 * amplitude * std::sin(angle);
    const int next = kControls.heldPhase < 0
                         ? std::min(7, static_cast<int>(t / 4))
                         : std::clamp(kControls.heldPhase, 0, 7);
    if (next != phaseIndex) {
      phaseIndex = next;
      ctx.composer.render(describe(next));
    }
  }
};

}  // namespace

SIGIL_SKETCH(GlassAtelier, "Study · Materials",
             "A glass material editor with cut contours, live destination "
             "refraction, lit material inks and bounded optical endpoints")
