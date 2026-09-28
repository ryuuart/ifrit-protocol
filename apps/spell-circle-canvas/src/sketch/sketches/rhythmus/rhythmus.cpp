/** A phototypeset concert collage. Cut strips, dot screens and circular
 *  lettering move on separate registers over a retained printed sheet. */
// TAGS: Studies/Graphic Design, Typography/Posters, Typography/Paths,
// Materials/Print, Motion/Bindings

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/TextPath.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace geometry = sigil::geometry;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
const auto paper = material::hexColor(0xEAE6D8);
const auto black = material::hexColor(0x17201F);
const auto red = material::hexColor(0xD94B31);

Text type(std::string words, float size, material::Color color = paper,
          bool heavy = true) {
  return text(std::move(words))
      .font({.face = weave::ports::face(
                 {"Helvetica Neue", "Helvetica", "Arial"}, heavy ? 700 : 400),
             .size = size,
             .track = heavy ? -size * 0.045f : 0.1f})
      .ink(color);
}

Element strip(std::string words, float size, float x, float y, float w,
              material::Color ground = paper, material::Color ink = black) {
  return kit::at(type(std::move(words), size, ink), x, y, w, size * 1.17f)
      .fill(ground);
}

Element polygon(const char* path, float x, float y, float w, float h,
                const material::Material& fill) {
  return kit::at(x, y, w, h).shape(geometry::shapes::svg(path)).fill(fill);
}
}  // namespace

struct Rhythmus {
  motion::Animatable<float> orbit = motion::animatable(0.0f);
  motion::Animatable<float> registerX = motion::animatable(0.0f);
  std::array<motion::Animatable<float>, 5> slice;

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1000, 1400);
    ctx.background(black);
    ctx.captureAt(3.2);
    auto dots =
        material::field::halftoneRamp(5.5f, 0.5f, 2.45f, black, 45, 0.0f, 0.8f);
    auto fineDots = material::field::halftoneRamp(3.7f, 0.4f, 1.6f, black, 15);
    std::vector<Element> fixed{
        kit::at(0, 0, 1000, 1400).fill(black),
        polygon("M0 0H850L940 104V760H640L610 1000H60L0 910Z", 153, 170, 847,
                1050, paper),
        kit::at(type("tonhalle\nzürich", 37), 35, 24, 510, 97),
        kit::at(type("NEUE MUSIK\n18—20 OKTOBER\nKONZERTREIHE 04", 14, paper,
                     false),
                731, 33, 230, 66),
        kit::at(type("ZEIT WIRD FORM", 65), 309, 91, 664, 85),
        kit::at(type("FORM", 138, black)
                    .textStroke(1.6f, Fill::color(black))
                    .ink(material::Color{0, 0, 0, 0}),
                559, 206, 425, 172),
        kit::at(153, 246, 623, 641).fill(dots),
        kit::at(555, 164, 445, 90).fill(fineDots),
        polygon("M0 0H260L390 640H0Z", 618, 567, 365, 610,
                material::from(paper).layer(fineDots)),
        strip("DREI TAGE / EIN TAKT", 33, 209, 457, 668),
        strip("percussion. elektronik. kammerorchester.", 19, 239, 495, 690),
        strip("04", 252, 664, 936, 310, red, paper),
        strip("PULS", 73, 30, 1044, 231, black, paper).zIndex(1),
        strip("IMPULS", 71, 30, 1118, 309, black, paper).zIndex(1),
        kit::at(type("EINTRITT / 19.30 UHR", 17, black).rotate(-90), 836, 785,
                294, 31),
    };

    // These concentric bands are the collage's percussion diaphragm.
    // Their hard alternating screens stay registered to the printed sheet.
    fixed.push_back(kit::at(54, 518, 580, 580)
                        .shape(geometry::shapes::circle())
                        .fill(black));
    fixed.push_back(kit::at(59, 523, 570, 570)
                        .shape(geometry::shapes::circle())
                        .fill(Fill::none())
                        .stroke(stroke(2, Fill::color(paper))));
    for (int band = 0; band < 32; ++band) {
      const float radius = 237 - band * 5.4f;
      fixed.push_back(
          kit::at(344 - radius, 808 - radius, radius * 2, radius * 2)
              .shape(geometry::shapes::circle())
              .fill(Fill::none())
              .stroke(stroke(band % 4 == 0 ? 3.0f : 1.0f, Fill::color(paper))));
    }
    fixed.push_back(kit::at(282, 746, 124, 124)
                        .shape(geometry::shapes::circle())
                        .fill(red));
    fixed.push_back(kit::at(319, 783, 50, 50)
                        .shape(geometry::shapes::circle())
                        .fill(black));
    fixed.push_back(kit::at(339, 803, 10, 10)
                        .shape(geometry::shapes::circle())
                        .fill(paper));
    for (int spoke = 0; spoke < 12; ++spoke) {
      const float angle = spoke * 0.5235988f;
      fixed.push_back(kit::at(339.5f + 181 * std::cos(angle),
                              803.5f + 181 * std::sin(angle), 9, 9)
                          .shape(geometry::shapes::circle())
                          .fill(paper));
    }

    // A perspective score crosses the circular image like ruled acetate.
    for (int line = 0; line < 13; ++line) {
      fixed.push_back(
          kit::at(639 - line * 7.3f, 614 + line * 23.5f, 305 + line * 7.3f, 2)
              .fill(black));
    }
    for (int line = 0; line < 8; ++line) {
      fixed.push_back(
          kit::at(665 + line * 43.0f, 612, 1.5f, 313).fill(black).skewX(-15));
    }
    fixed.push_back(strip("RAUM", 64, 665, 652, 278, black, paper));
    fixed.push_back(strip("KLANG", 64, 624, 741, 321, paper, black));
    fixed.push_back(strip("KÖRPER", 64, 596, 828, 351, black, paper));
    fixed.push_back(
        polygon("M0 0H110L440 420H330Z", -120, 903, 457, 465, paper));
    fixed.push_back(polygon("M0 0H70L450 410H370Z", -41, 1053, 345, 347, red));
    fixed.push_back(kit::at(346, 1190, 627, 181).fill(paper));
    const std::array<const char*, 3> titles{"01 / PULS", "02 / RAUM",
                                            "03 / ZEIT"};
    const std::array<const char*, 3> details{
        "18. OKTOBER\nPERCUSSION\nENSEMBLE\n19.30 UHR",
        "19. OKTOBER\nKAMMER-\nORCHESTER\n20.00 UHR",
        "20. OKTOBER\nELEKTRONISCHES\nSTUDIO\n18.00 UHR"};
    for (int column = 0; column < 3; ++column) {
      const float x = 363 + 201 * column;
      fixed.push_back(
          kit::at(type(titles[column], 21, black), x, 1203, 194, 31));
      fixed.push_back(kit::at(x, 1239, 174, 3).fill(red));
      fixed.push_back(
          kit::at(type(details[column], 14, black, false), x, 1256, 194, 99));
    }
    fixed.push_back(kit::at(
        type("RHYTHMUS / EINE TYPOGRAFISCHE PARTITUR", 11, paper, false), 348,
        1377, 617, 19));
    auto printed = positioned()
                       .width(1000)
                       .height(1400)
                       .children(std::move(fixed))
                       .cache(Cache::Texture);

    std::vector<Element> live{
        printed,
        kit::at(
            type("PULS / IMPULS / RAUM / KLANG / ZEIT / KÖRPER / "
                 "PULS / IMPULS / RAUM / KLANG / ZEIT / KÖRPER /",
                 22)
                .textOnPath({.path = geometry::shapes::circle(), .at = orbit}),
            76, 540, 536, 536),
        kit::at(178, 268, 775, 74)
            .fill(material::field::halftoneRamp(6.5f, 1.8f, 2.9f, red, -15))
            .blendMode(material::BlendMode::Multiply)
            .translateX(registerX),
    };
    // Five independently clipped phototype strips share one shaped headline.
    // Their offsets expose the printed registration rather than reflowing text.
    for (int i = 0; i < 5; ++i) {
      slice[i] = motion::animatable(0.0f);
      live.push_back(
          kit::at(positioned()
                      .children({kit::at(type("rhythmus", 201, black), 3,
                                         -i * 37.0f - 37, 960, 243)})
                      .fill(paper)
                      .overflow(Overflow::Clip)
                      .translateX(slice[i]),
                  65, 281 + i * 37.0f, 900, 37));
    }
    // One retained grain layer ties differently screened pieces to one paper.
    live.push_back(kit::at(0, 0, 1000, 1400)
                       .fill(material::noise(0.7f, {.seed = 29, .grain = true}))
                       .cache(Cache::Texture)
                       .blendMode(material::BlendMode::SoftLight)
                       .opacity(0.12f));
    ctx.composer.render(
        positioned().width(1000).height(1400).children(std::move(live)));
  }

  void update(double elapsed) {
    const float t = float(elapsed);
    orbit = t * 0.015f;
    registerX = 6.0f * std::sin(t * 0.62f);
    for (int i = 0; i < 5; ++i)
      slice[i] =
          (i == 0 || i == 4 ? 2.0f : 9.0f) * std::sin(t * 1.1f + i * 0.8f);
  }
};

SIGIL_SKETCH(Rhythmus, "Study · Type",
             "After Weingart's Kunstkredit: cut phototype, layered halftones, "
             "circular lettering and moving print registration")
