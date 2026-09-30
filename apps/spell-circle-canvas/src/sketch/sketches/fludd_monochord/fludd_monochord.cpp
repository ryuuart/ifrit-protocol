/** A cosmic monochord drawn as a copperplate: nested interval arcs,
 *  a celestial belt, a figured soundboard, an engraved tuning hand and
 *  elemental names. The string's small vibration is an authored motion. */
// TAGS: Studies/Manuscripts, Studies/Science, Geometry/Diagrams,
// TAGS: Typography/Curved, Drawing/Engraving, Materials/Paper, Motion/Waves

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/TextPath.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace geometry = sigil::geometry;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
constexpr float kWidth = 1340, kHeight = 1720;
constexpr float kPi = 3.14159265358979f;
const auto kInk = material::hexColor(0x342B21);
const auto kQuiet = material::hexColor(0x77644B);
const auto kRed = material::hexColor(0x98452F);
const auto kPaper = material::hexColor(0xEEE4CA);

Text type(std::string words, float size, bool italic = false,
          material::Color ink = kInk) {
  return text(std::move(words))
      .font({.face = weave::ports::face(
                 {"Baskerville", "Hoefler Text", "Georgia"}, 400,
                 italic ? weave::FaceSlant::Italic : weave::FaceSlant::Upright),
             .size = size})
      .ink(ink);
}

Element center(std::string words, float x, float y, float width, float size,
               bool italic = false, material::Color ink = kInk) {
  return kit::at(type(std::move(words), size, italic, ink)
                     .textAlign(weave::TextAlignment::kCenter),
                 x - width * 0.5f, y, width, size * 1.7f);
}

Element line(float x, float y, float width, float height,
             material::Color ink = kInk) {
  return kit::at(x, y, width, height).fill(ink);
}

Element ellipse(float x, float y, float width, float height, float weight = 1,
                material::Color ink = kInk) {
  return kit::at(x, y, width, height)
      .shape(geometry::shapes::circle())
      .fill(Fill::none())
      .stroke(stroke(weight, Fill::color(ink)));
}

Element arc(float x, float y, float width, float height, float start,
            float sweep, float weight = 1, material::Color ink = kInk) {
  return kit::at(x, y, width, height)
      .shape(geometry::shapes::arc(start, sweep))
      .fill(Fill::none())
      .stroke(stroke(weight, Fill::color(ink)));
}

Element curved(std::string words, float x, float y, float width, float height,
               float start, float sweep, float size, float at = 0.5f) {
  return kit::at(type(std::move(words), size, true)
                     .textOnPath({.path = geometry::shapes::arc(start, sweep),
                                  .at = at,
                                  .align = TextPath::Align::Center,
                                  .offset = 5,
                                  .exactTangent = true}),
                 x, y, width, height);
}

/** Fine line work stays as vector commands. Seeded marks describe copper
 *  cuts rather than frame-dependent noise, so the engraving is retained. */
Element engraving() {
  return pen(
             "fludd.copperplate",
             [](sigil::draw::Pen& p) {
               p.noFill();
               p.stroke(kInk);
               p.strokeWeight(0.75f);
               p.randomSeed(1617);

               // The sphere's meridians converge on the ends of the monochord.
               for (int i = 1; i <= 5; ++i) {
                 const float bend = 62.0f * i;
                 p.bezier(670, 345, 670 - bend, 560, 670 - bend, 1210, 670,
                          1450);
                 p.bezier(670, 345, 670 + bend, 560, 670 + bend, 1210, 670,
                          1450);
               }
               for (int row = 0; row < 5; ++row) {
                 const float y = 604 + row * 133;
                 p.bezier(216, y + 50, 423, y - 104, 905, y - 90, 1125, y + 50);
               }

               // The stellar belt follows its bowed parallel rather than a
               // circle.
               for (int i = 0; i < 30; ++i) {
                 const float x = 221 + i * 30.8f;
                 const float q = (x - 670) / 455;
                 const float y = 797 + 158 * q * q;
                 const float r = i % 3 == 0 ? 12.2f : 8.2f;
                 p.push();
                 p.translate(x, y);
                 p.rotate(std::atan(q * 0.72f));
                 p.fill(kPaper);
                 p.strokeWeight(0.85f);
                 p.beginShape();
                 for (int n = 0; n < 16; ++n) {
                   const float theta = n * kPi / 8 - kPi / 2;
                   const float reach = n % 2 == 0 ? r : r * 0.23f;
                   p.vertex(std::cos(theta) * reach, std::sin(theta) * reach);
                 }
                 p.endShape(sigil::draw::CLOSE);
                 p.line(-r * 0.71f, r * 0.71f, r * 0.71f, -r * 0.71f);
                 p.pop();
               }

               // Cuts across the underside distinguish the two pyramids of
               // light.
               p.stroke(material::withAlpha(kInk, 0.48f));
               for (int i = 0; i < 99; ++i) {
                 const float x = 232 + i * 8.8f;
                 const float q = (x - 670) / 443;
                 if (std::abs(q) > 1) continue;
                 const float y = 934 + 196 * q * q;
                 p.line(x, y, x + 7, y - 17 - 6 * (1 - std::abs(q)));
               }
               p.stroke(kInk);

               // Crosshatching on the narrow side of the carved soundboard.
               for (int y = 365; y < 1494; y += 6) {
                 p.line(708, y, 716, y - 7);
                 p.line(708, y + 4, 716, y + 11);
               }
               for (int y = 394; y < 1492; y += 9) p.line(604, y, 609, y - 4);

               // The bridge and two sound holes are small engraved surfaces.
               p.strokeWeight(1.4f);
               p.bezier(617, 1422, 627, 1402, 644, 1403, 654, 1422);
               p.bezier(617, 1426, 627, 1409, 644, 1410, 654, 1426);
               for (int i = 0; i < 9; ++i)
                 p.line(620 + i * 3.6f, 1424, 620 + i * 3.6f,
                        1419 - 10 * std::sin(i * kPi / 8));
               p.bezier(630, 1380, 609, 1350, 610, 1404, 632, 1385);
               p.bezier(632, 1385, 650, 1407, 636, 1425, 622, 1420);
               p.circle(629, 1379, 8);
               p.circle(622, 1420, 8);

               // Radiating cuts behind the tuning hand.
               p.stroke(material::withAlpha(kInk, 0.33f));
               for (int i = 0; i < 44; ++i) {
                 const float a = (28 + i * 2.7f) * kPi / 180;
                 const float shortR = 81 + p.random(5, 26);
                 const float longR = 137 + p.random(0, 25);
                 p.line(975 + std::cos(a) * shortR, 224 + std::sin(a) * shortR,
                        975 + std::cos(a) * longR, 224 + std::sin(a) * longR);
               }
               p.stroke(kInk);

               // The profile, fingernails and knuckles make the hand a figured
               // mark.
               p.strokeWeight(1.25f);
               p.bezier(912, 247, 845, 248, 787, 263, 753, 283);
               p.bezier(753, 283, 734, 292, 720, 310, 736, 315);
               p.bezier(736, 315, 749, 321, 788, 293, 822, 290);
               p.bezier(822, 290, 788, 309, 774, 324, 791, 328);
               p.bezier(791, 328, 812, 332, 835, 312, 850, 310);
               p.bezier(850, 310, 828, 333, 830, 344, 845, 342);
               p.bezier(845, 342, 869, 335, 897, 310, 923, 307);
               p.bezier(923, 307, 943, 296, 944, 261, 912, 247);
               p.bezier(742, 298, 750, 290, 758, 291, 760, 300);
               p.bezier(800, 304, 799, 285, 810, 277, 824, 271);
               p.bezier(855, 277, 865, 286, 867, 297, 862, 307);
               p.bezier(890, 274, 904, 281, 905, 289, 899, 302);
               p.bezier(777, 291, 793, 280, 810, 273, 831, 271);
               p.bezier(835, 320, 850, 309, 865, 301, 882, 298);
               for (int i = 0; i < 16; ++i) {
                 const float x = 806 + i * 6;
                 p.strokeWeight(i % 4 == 0 ? 0.9f : 0.6f);
                 p.line(x, 259 + i * 0.05f, x + 7, 269 + i * 0.3f);
                 if (i > 5) p.line(x, 322 - i, x + 9, 313 - i);
               }

               // A cloud is shaped by lobes and by curved engraving, not white
               // discs.
               p.fill(kPaper);
               p.strokeWeight(1.2f);
               p.beginShape();
               p.vertex(918, 245);
               p.bezierVertex(892, 222, 899, 196, 929, 195);
               p.bezierVertex(917, 169, 945, 146, 975, 162);
               p.bezierVertex(985, 127, 1030, 128, 1044, 159);
               p.bezierVertex(1086, 139, 1113, 166, 1105, 189);
               p.bezierVertex(1147, 176, 1179, 207, 1161, 234);
               p.bezierVertex(1196, 250, 1178, 291, 1146, 291);
               p.bezierVertex(1126, 327, 1087, 317, 1073, 297);
               p.bezierVertex(1040, 323, 1009, 298, 1004, 280);
               p.bezierVertex(977, 302, 945, 279, 939, 261);
               p.bezierVertex(934, 253, 925, 249, 918, 245);
               p.endShape(sigil::draw::CLOSE);
               p.noFill();
               for (int i = 0; i < 10; ++i) {
                 const float d = i * 2.5f;
                 p.bezier(1009 + d, 275 - d, 1040 + d, 308 - d, 1072 + d,
                          278 - d, 1069 + d, 262 - d);
                 p.bezier(1090 + d, 286 - d, 1138 + d, 307 - d, 1159 + d,
                          274 - d, 1152 + d, 257 - d);
               }
               p.bezier(927, 198, 947, 221, 970, 192, 963, 181);
               p.bezier(981, 162, 1008, 185, 1027, 169, 1030, 157);
               p.bezier(936, 251, 955, 254, 970, 245, 972, 227);

               // The central solar face follows the convention of figured
               // planets.
               p.push();
               p.translate(628, 908);
               p.strokeWeight(0.9f);
               for (int n = 0; n < 24; ++n) {
                 const float a = n * kPi / 12;
                 const float r = n % 2 ? 21 : 27;
                 p.line(std::cos(a) * 16, std::sin(a) * 16,
                        std::cos(a + 0.11f) * r, std::sin(a + 0.11f) * r);
               }
               p.fill(kPaper);
               p.circle(0, 0, 31);
               p.noFill();
               p.bezier(-10, -4, -6, -8, -3, -8, -1, -4);
               p.bezier(2, -4, 5, -8, 9, -7, 11, -3);
               p.line(-1, -3, -2, 4);
               p.line(-2, 4, 2, 5);
               p.bezier(-5, 9, 0, 12, 5, 12, 8, 7);
               p.pop();

               // A marginal compass cut gives the plate its hand-made finish.
               p.stroke(kRed);
               p.strokeWeight(0.75f);
               for (int n = 0; n < 16; ++n) {
                 const float a = n * kPi / 8;
                 const float r = n % 2 == 0 ? 32 : 22;
                 p.line(670, 1581, 670 + r * std::cos(a),
                        1581 + r * std::sin(a));
               }
               p.circle(670, 1581, 17);
             },
             Cache::Picture)
      .inset(0);
}

Element instrument() {
  std::vector<Element> nodes;
  nodes.push_back(
      kit::at(603, 244, 115, 1276)
          .shape(geometry::shapes::svg(
              "M68 0C98 -4 112 18 107 43L94 96L94 1190C94 1250 69 1276 35 1269"
              "C7 1263 -1 1240 0 1207L10 110L34 76L44 33C43 13 50 4 68 0Z"))
          .fill(material::linearGradient({0, 0}, {1, 0},
                                         {material::hexColor(0xDDD0AE), kPaper,
                                          material::hexColor(0xC8B995)}))
          .stroke(stroke(1.7f, Fill::color(kInk))));
  nodes.push_back(ellipse(650, 255, 36, 40, 1.5f));
  nodes.push_back(ellipse(660, 265, 13, 17, 1.2f));
  nodes.push_back(line(685, 296, 44, 9));
  nodes.push_back(ellipse(716, 282, 23, 34, 1.2f));
  nodes.push_back(line(647, 348, 1.2f, 1066));
  nodes.push_back(line(688, 348, 1.0f, 1144));
  nodes.push_back(line(601, 348, 106, 1.2f));
  nodes.push_back(line(601, 368, 106, 0.8f));
  constexpr std::array<float, 15> positions{389,  459,  640,  746,  810,
                                            849,  888,  928,  967,  1018,
                                            1083, 1162, 1248, 1388, 1459};
  constexpr std::array<const char*, 15> notes{"gg", "f", "e", "d", "c",
                                              "b",  "a", "G", "F", "E",
                                              "D",  "C", "B", "A", "Γ"};
  for (std::size_t i = 0; i < positions.size(); ++i) {
    nodes.push_back(line(606, positions[i] + 18, 88, 0.72f, kQuiet));
    nodes.push_back(
        center(notes[i], 669, positions[i] - 5, 34, i > 6 ? 26 : 25, i < 7));
  }
  nodes.push_back(center("Empyræum", 623, 418, 118, 15, true).rotate(-90));
  nodes.push_back(center("Regio ætherea", 622, 594, 142, 15, true).rotate(-90));
  nodes.push_back(center("Igniſ", 624, 1210, 52, 21, true));
  nodes.push_back(center("Aer", 625, 1320, 52, 21, true));
  nodes.push_back(center("Aqua", 624, 1436, 60, 19, true));
  nodes.push_back(center("Terra", 625, 1466, 66, 19, true));
  const auto symbols = weave::ports::face({"Apple Symbols", "Times New Roman"});
  constexpr std::array<const char*, 6> planets{"♄", "♃", "♂", "♀", "☿", "☾"};
  constexpr std::array<float, 6> heights{768, 812, 850, 966, 1018, 1083};
  for (std::size_t i = 0; i < planets.size(); ++i)
    nodes.push_back(kit::at(text(planets[i])
                                .font({.face = symbols, .size = 25})
                                .ink(kInk)
                                .textAlign(weave::TextAlignment::kCenter),
                            606, heights[i], 38, 34));
  return positioned().width(kWidth).height(kHeight).children(std::move(nodes));
}

struct FluddMonochord {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.background(kPaper);
    ctx.captureAt(4.8);

    auto paper = material::shader(ctx.assets.hub(), ctx.local("paper.sksl"));
    std::vector<Element> plate;
    plate.push_back(center("TRACTATVS I.     LIBER III.", 670, 38, 1040, 18));
    plate.push_back(kit::at(type("90", 20), 1213, 36, 52, 40));
    plate.push_back(center("MONOCHORDVM MVNDANVM", 670, 79, 1160, 42));
    plate.push_back(center("De muſica mundana, & harmonia totius creaturæ.",
                           670, 138, 980, 25, true));
    plate.push_back(line(101, 192, 1138, 1.1f, kQuiet));
    plate.push_back(
        center("Hic autem mundi chordam cum ſuis proportionibus exhibemus.",
               580, 211, 939, 17, true));

    plate.push_back(ellipse(192, 337, 956, 1130, 1.25f));
    plate.push_back(ellipse(205, 343, 930, 1120, 0.4f, kQuiet));
    for (int i = 0; i < 3; ++i) {
      const float x = 239 + i * 86;
      const float y = 501 + i * 134;
      plate.push_back(
          arc(x, y, 2 * (670 - x), 2 * (1468 - y), 190, 155, 0.65f));
    }

    // Each bracket's text shares the bracket's baseline and its bounds.
    plate.push_back(curved("Proportio quadrupla", 203, 347, 928, 1110, 102, 155,
                           26, 0.55f));
    plate.push_back(
        curved("Proportio dupla", 295, 392, 730, 791, 118, 146, 23, 0.66f));
    plate.push_back(curved("Proportio ſeſquitertia", 386, 359, 555, 585, 97,
                           157, 23, 0.49f));
    plate.push_back(curved("Diatessaron formalis", 427, 360, 486, 560, -85, 145,
                           24, 0.42f));
    plate.push_back(
        curved("Diapason formalis", 332, 375, 677, 930, -81, 143, 25, 0.34f));
    plate.push_back(
        curved("Disdiapason", 211, 347, 920, 1110, -51, 110, 28, 0.65f));
    plate.push_back(
        curved("Diapente materialis", 378, 819, 590, 640, -77, 148, 25, 0.49f));
    plate.push_back(curved("Diatessaron materialis", 469, 1030, 394, 443, -75,
                           146, 22, 0.5f));
    plate.push_back(
        curved("Proportio dupla", 292, 814, 760, 649, 105, 125, 23, 0.48f));
    plate.push_back(curved("Proportio ſesquialtera", 404, 909, 529, 553, 106,
                           119, 22, 0.48f));
    plate.push_back(
        curved("Pyramis lucis", 296, 940, 751, 322, 205, 130, 20, 0.16f));

    plate.push_back(arc(217, 788, 906, 367, 190, 160, 1.4f));
    plate.push_back(arc(217, 808, 906, 367, 190, 160, 0.65f));
    plate.push_back(arc(242, 920, 856, 325, 187, 166, 0.7f));
    plate.push_back(arc(240, 944, 860, 325, 187, 166, 0.6f));
    plate.push_back(instrument());
    plate.push_back(engraving());

    plate.push_back(
        kit::at(type("Diſdiapaſon\n4 : 1", 21, true, kRed), 62, 788, 155, 76)
            .rotate(-90));
    plate.push_back(
        kit::at(type("Diapaſon\n2 : 1", 21, true, kRed), 1118, 802, 165, 76)
            .rotate(90));
    plate.push_back(
        kit::at(type("ſpiritus", 19, true, kRed), 123, 343, 150, 35));
    plate.push_back(
        kit::at(type("materia", 19, true, kRed), 1025, 1402, 150, 35));
    plate.push_back(center("Vna chorda : duo diapaſones : vniverſa harmonia.",
                           670, 1507, 1110, 29, true));
    plate.push_back(kit::at(type("Terra, aqua, aer & ignis.\nThe material "
                                 "octave aſcends\nthrough the planetary order.",
                                 19, true),
                            111, 1575, 447, 91));
    plate.push_back(
        kit::at(type("The ſame ſtring joins the\ncelestial & elemental "
                     "worlds.\nA diagram of correſpondences.",
                     19, true),
                823, 1575, 425, 91));
    plate.push_back(center(
        "ROBERT FLVDD    ·    DE MVSICA MVNDANA    ·    AN ENGRAVED STUDY", 670,
        1678, 1160, 12));

    // Only a narrow string surface is live. The plate's commands are fixed.
    auto string =
        kit::at(646, 374, 17, 1068)
            .fill(material::shader(ctx.assets.hub(), ctx.local("string.sksl")));
    ctx.composer.render(
        positioned().width(kWidth).height(kHeight).fill(paper).children(
            {positioned()
                 .width(kWidth)
                 .height(kHeight)
                 .children(std::move(plate))
                 .cache(Cache::Picture),
             string}));
  }
};
}  // namespace

SIGIL_SKETCH(
    FluddMonochord, "Study · Manuscript",
    "A cosmic monochord after Fludd: engraved harmonies, planetary signs, "
    "figured tuning hand and a vibrating string")
