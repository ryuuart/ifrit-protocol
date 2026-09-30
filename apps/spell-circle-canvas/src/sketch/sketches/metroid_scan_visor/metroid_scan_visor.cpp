/** A helmet-mounted biological research display. The chamber, anatomy,
 *  instrumentation and visor are retained; the shared clock drives only
 *  optical sweep, specimen drift and the live spectral trace. */
// TAGS: Studies/Games, Drawing/Organic, Drawing/Geometry, Layout/3D
// TAGS: Materials/Shaders, Motion/Bindings, Typography/Interface

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace draw = sigil::draw;
using namespace sigil::compose;

namespace {
const auto black = material::hexColor(0x050C10);
const auto cyan = material::hexColor(0xBBF6F3);
const auto ice = material::hexColor(0x8ACBCF);
const auto blue = material::hexColor(0x3D8196);
const auto muted = material::hexColor(0x54858C);
const auto amber = material::hexColor(0xF7B957);
const auto red = material::hexColor(0xED6655);
const auto green = material::hexColor(0x9AE2AA);

Text words(std::string value, float size, material::Color color = ice,
           bool mono = false, float tracking = 0) {
  static const auto interfaceFace =
      weave::ports::face({"Eurostile", "DIN Alternate", "Helvetica Neue"});
  static const auto terminalFace = weave::ports::face({"Menlo", "Monaco"});
  return text(std::move(value))
      .font({.face = mono ? terminalFace : interfaceFace,
             .size = size,
             .track = tracking})
      .ink(color);
}

Element label(std::string value, float x, float y, float w, float size = 11,
              material::Color color = ice, bool mono = false) {
  return kit::at(words(std::move(value), size, color, mono), x, y, w,
                 size * 1.5f);
}

void polygon(draw::Pen& p, std::initializer_list<glm::vec2> points,
             material::Color fill, material::Color edge, float weight = 1) {
  p.fill(fill);
  p.stroke(edge);
  p.strokeWeight(weight);
  p.beginShape();
  for (const auto& point : points) p.vertex(point.x, point.y);
  p.endShape(draw::CLOSE);
}

void chamfer(draw::Pen& p, float x, float y, float w, float h, float cut,
             material::Color fill, material::Color edge, float weight = 1) {
  p.fill(fill);
  p.stroke(edge);
  p.strokeWeight(weight);
  p.shape(sigil::geometry::shapes::chamfered(cut), x, y, w, h);
}

Element researchPanel(std::string key, float width, float height) {
  return pen(
             key,
             [width, height](draw::Pen& p) {
               chamfer(p, 2, 2, width - 4, height - 4, 15,
                       material::hexColor(0x08191F, 0.93f),
                       material::hexColor(0xB5D4D7, 0.5f), 3);
               chamfer(p, 7, 7, width - 14, height - 14, 12,
                       material::hexColor(0x071015, 0.4f), muted, 1);
               p.noFill();
               p.stroke(material::hexColor(0x6BACB6, 0.10f));
               p.strokeWeight(0.65f);
               for (int row = 18; row < height - 10; row += 4)
                 p.line(12, float(row), width - 12, float(row));
               p.stroke(material::hexColor(0xD7F8F3, 0.68f));
               p.line(19, 3, width - 20, 3);
               p.line(3, 20, 3, height - 20);
               p.stroke(material::hexColor(0x293D48, 0.9f));
               p.line(20, height - 3, width - 20, height - 3);
             },
             Cache::Texture)
      .width(width)
      .height(height);
}

Element chamber() {
  return pen(
             "metroid.chamber",
             [](draw::Pen& p) {
               p.noStroke();
               p.fill(material::linearGradient(
                   {0, 0}, {0, 1},
                   {material::hexColor(0x102D32), material::hexColor(0x0A1C23),
                    black}));
               p.rect(0, 0, 1440, 960);

               // Architecture converges on the containment tank, leaving the
               // instrument panels optically in front of the room.
               for (int side : {-1, 1}) {
                 p.push();
                 p.translate(720, 0);
                 p.scale(float(side), 1);
                 polygon(p, {{205, 0}, {720, 0}, {720, 775}, {276, 631}},
                         material::hexColor(0x101C22),
                         material::hexColor(0x2A4046));
                 polygon(p, {{228, 31}, {634, 74}, {500, 379}, {257, 465}},
                         material::hexColor(0x0A1118),
                         material::hexColor(0x263E47));
                 polygon(p, {{276, 475}, {720, 700}, {720, 803}, {275, 670}},
                         material::hexColor(0x182A31),
                         material::hexColor(0x32515A));
                 polygon(p, {{272, 647}, {720, 773}, {720, 960}, {253, 820}},
                         material::hexColor(0x0B141A),
                         material::hexColor(0x273A42));
                 p.noFill();
                 p.stroke(material::hexColor(0x46727B, 0.24f));
                 p.strokeWeight(2);
                 for (int rib = 0; rib < 9; ++rib) {
                   float q = float(rib) / 8;
                   p.line(300 + q * 388, 84 + q * 17, 265 + q * 425,
                          470 + q * 198);
                 }
                 for (int beam = 0; beam < 5; ++beam) {
                   float q = float(beam) / 5;
                   p.line(287, 665 + q * 139, 720, 817 + q * 131);
                 }
                 p.stroke(material::hexColor(0x547A83, 0.22f));
                 p.strokeWeight(13);
                 p.bezier(344, 0, 420, 210, 299, 449, 399, 730);
                 p.stroke(material::hexColor(0x13202A));
                 p.strokeWeight(8);
                 p.bezier(344, 0, 420, 210, 299, 449, 399, 730);
                 p.stroke(material::hexColor(0xE9B467, 0.2f));
                 p.strokeWeight(3);
                 for (int stripe = 0; stripe < 8; ++stripe)
                   p.line(470 + stripe * 11, 472 + stripe * 7,
                          450 + stripe * 11, 493 + stripe * 7);
                 p.pop();
               }

               p.fill(material::Paint::radialGradient(
                          {0.48f, 0.42f}, 0.58f,
                          {material::hexColor(0x5BA79E, 0.42f),
                           material::hexColor(0x163F48, 0.44f),
                           material::hexColor(0x071016, 0.05f)}),
                      draw::SHAPE);
               p.stroke(material::hexColor(0x5C9AA5, 0.35f));
               p.strokeWeight(2);
               p.beginShape();
               p.vertex(500, 171);
               p.bezierVertex(561, 140, 883, 140, 939, 171);
               p.vertex(944, 643);
               p.bezierVertex(830, 685, 594, 685, 487, 643);
               p.endShape(draw::CLOSE);
               p.noFill();
               for (int rib = 0; rib < 8; ++rib) {
                 float y = 182 + rib * 66;
                 p.stroke(material::hexColor(0x8ED0CA, 0.16f));
                 p.strokeWeight(rib % 3 == 0 ? 2 : 0.8f);
                 p.bezier(502, y, 600, y - 18, 835, y - 18, 939, y);
               }
               for (int rail = 0; rail < 5; ++rail) {
                 float x = 505 + rail * 107;
                 p.stroke(material::hexColor(0x172E34, 0.9f));
                 p.strokeWeight(7);
                 p.line(x, 169, x - 10, 651);
                 p.stroke(material::hexColor(0x72B8B5, 0.24f));
                 p.strokeWeight(1);
                 p.line(x + 3, 169, x - 7, 651);
               }
               polygon(p,
                       {{497, 144},
                        {543, 118},
                        {895, 118},
                        {942, 144},
                        {943, 172},
                        {495, 172}},
                       material::hexColor(0x263F43), muted, 1);
               polygon(p,
                       {{488, 644},
                        {943, 644},
                        {962, 677},
                        {936, 696},
                        {502, 696},
                        {472, 677}},
                       material::hexColor(0x17272F),
                       material::hexColor(0x48727C));
               p.noStroke();
               p.fill(material::hexColor(0x8DEAE3, 0.64f));
               p.rect(596, 151, 246, 5);
               p.rect(532, 672, 369, 4);
               p.fill(material::hexColor(0x254E56));
               p.rect(546, 132, 13, 12);
               p.rect(882, 132, 13, 12);
               for (int mote = 0; mote < 60; ++mote) {
                 float x = 510 + std::fmod(mote * 93.73f, 417);
                 float y = 183 + std::fmod(mote * 117.61f, 458);
                 p.fill(material::hexColor(0xA7DFD0,
                                           mote % 7 == 0 ? 0.27f : 0.09f));
                 p.circle(x, y, mote % 5 == 0 ? 2.8f : 1.1f);
               }
               // Laboratory terminals repeat one screen construction at several
               // physical depths; their glyph-like traces are room geometry.
               for (int index = 0; index < 5; ++index) {
                 float x = 110 + index * 273;
                 float y = 717 + (index % 2) * 38;
                 chamfer(p, x, y, 110, 64, 6, material::hexColor(0x173841),
                         material::hexColor(0x4E8993, 0.6f));
                 p.noFill();
                 p.stroke(material::hexColor(0x7AC0C4, 0.5f));
                 p.strokeWeight(1);
                 p.rect(x + 9, y + 8, 38, 26);
                 p.rect(x + 54, y + 8, 44, 11);
                 p.rect(x + 54, y + 24, 20, 22);
                 for (int row = 0; row < 4; ++row)
                   p.line(x + 10, y + 41 + row * 4, x + 44 + row * 5,
                          y + 41 + row * 4);
                 p.stroke(material::hexColor(0x77B4A6, 0.3f));
                 p.bezier(x + 77, y + 39, x + 77, y + 33, x + 94, y + 51,
                          x + 99, y + 33);
               }
             },
             Cache::Picture)
      .width(1440)
      .height(960);
}

void bellOutline(draw::Pen& p) {
  p.beginShape();
  p.vertex(59, 211);
  p.bezierVertex(25, 171, 37, 101, 83, 61);
  p.bezierVertex(128, 20, 209, 10, 261, 33);
  p.bezierVertex(324, 61, 363, 123, 347, 177);
  p.bezierVertex(340, 219, 299, 234, 272, 217);
  p.bezierVertex(237, 239, 208, 249, 187, 228);
  p.bezierVertex(144, 243, 128, 233, 118, 214);
  p.bezierVertex(97, 235, 68, 232, 59, 211);
  p.endShape(draw::CLOSE);
}

void bioform(draw::Pen& p, bool schematic = false) {
  p.push();
  if (!schematic) {
    p.fill(
        material::Paint::radialGradient({0.37f, 0.20f}, 0.84f,
                                        {material::hexColor(0xC4ED97, 0.72f),
                                         material::hexColor(0x49AF77, 0.71f),
                                         material::hexColor(0x14494C, 0.91f)}),
        draw::SHAPE);
    p.stroke(material::hexColor(0xB8F4B5, 0.7f));
    p.strokeWeight(2);
  } else {
    p.noFill();
    p.stroke(amber);
    p.strokeWeight(1.4f);
  }
  bellOutline(p);
  p.push();
  p.clip([&p] { bellOutline(p); });
  p.noFill();
  p.strokeWeight(0.8f);
  for (int contour = 0; contour < 12; ++contour) {
    float t = float(contour) / 12;
    p.stroke(material::hexColor(schematic ? 0xEDC67A : 0xC9F8BF,
                                schematic ? 0.4f : 0.10f + (1 - t) * 0.1f));
    p.bezier(39 + t * 8, 75 + t * 145, 96, 4 + t * 197, 281, 1 + t * 229, 348,
             97 + t * 137);
  }
  for (int meridian = 0; meridian < 15; ++meridian) {
    float x = 60 + meridian * 20;
    p.stroke(material::hexColor(schematic ? 0xB48540 : 0xC5EDB6, 0.19f));
    p.bezier(197, 20, x, 37, x - 13, 156, x + 2, 244);
  }
  p.strokeWeight(1.2f);
  p.stroke(material::hexColor(schematic ? 0xEAC472 : 0xD1F5CC, 0.32f));
  for (int vein = 0; vein < 24; ++vein) {
    float x = 74 + std::fmod(vein * 51.2f, 242);
    float y = 84 + std::fmod(vein * 33.6f, 99);
    p.bezier(x, y, x + 21, y - 7, x + 26, y + 15, x + 33, y + 24);
    p.line(x + 19, y + 7, x + 31, y + 5);
  }
  for (int cell = 0; cell < 25; ++cell) {
    float x = 68 + std::fmod(cell * 67.9f, 249);
    float y = 66 + std::fmod(cell * 39.7f, 152);
    p.stroke(material::hexColor(schematic ? 0xE9C378 : 0xE1F8C3, 0.16f));
    p.ellipse(x, y, 15 + cell % 8, 9 + cell % 5);
  }
  if (!schematic) {
    p.noStroke();
    p.fill(
        material::Paint::linearGradient({0, 0}, {0, 1},
                                        {material::hexColor(0xFFFBE0, 0.05f),
                                         material::hexColor(0xAACF99, 0.16f),
                                         material::hexColor(0x061C23, 0.65f)}),
        draw::SHAPE);
    p.ellipse(194, 185, 286, 102);
    p.fill(
        material::Paint::radialGradient({0.37f, 0.31f}, 0.65f,
                                        {material::hexColor(0xFFD8BB, 0.96f),
                                         material::hexColor(0xE07360, 0.97f),
                                         material::hexColor(0x722B38, 0.88f)}),
        draw::SHAPE);
  } else {
    p.fill(material::hexColor(0x8D541E, 0.35f));
    p.stroke(amber);
  }
  p.ellipse(174, 139, 71, 89);
  p.ellipse(222, 162, 62, 73);
  p.ellipse(130, 174, 49, 58);
  p.ellipse(267, 151, 38, 53);
  p.noFill();
  p.stroke(material::hexColor(schematic ? 0xEDC46A : 0xF4AC91, 0.6f));
  p.strokeWeight(0.8f);
  for (int rib = 0; rib < 7; ++rib) {
    p.bezier(143, 106 + rib * 10, 169, 113 + rib * 10, 182, 106 + rib * 10, 193,
             113 + rib * 10);
    p.bezier(200, 140 + rib * 8, 220, 128 + rib * 8, 237, 144 + rib * 8, 249,
             142 + rib * 8);
  }
  p.pop();
  // The four teeth are closed Bezier silhouettes, each carrying its
  // own shape-fitted ramp and fine surface contours.
  for (int fang = 0; fang < 4; ++fang) {
    float x = 88 + fang * 63;
    float length = fang == 0 || fang == 3 ? 110 : 142;
    if (!schematic) {
      p.fill(material::Paint::linearGradient(
                 {0, 0}, {1, 0.8f},
                 {material::hexColor(0xE6F4BF), material::hexColor(0x7FAD83),
                  material::hexColor(0x184548)}),
             draw::SHAPE);
      p.stroke(material::hexColor(0xD3F8CF, 0.75f));
    } else {
      p.fill(material::hexColor(0x62482B, 0.15f));
      p.stroke(amber);
    }
    p.strokeWeight(1.4f);
    p.beginShape();
    p.vertex(x - 14, 207);
    p.bezierVertex(x - 13, 241, x - 12, 263, x + 16, 220 + length);
    p.bezierVertex(x + 9, 270, x + 22, 239, x + 25, 209);
    p.endShape(draw::CLOSE);
    p.noFill();
    p.strokeWeight(0.65f);
    p.stroke(material::hexColor(schematic ? 0xDBB35D : 0xCBF7CB, 0.44f));
    p.bezier(x - 2, 224, x - 6, 251, x + 4, 277, x + 16, 220 + length);
    for (int ring = 0; ring < 7; ++ring) {
      float y = 231 + ring * 10;
      p.bezier(x - 8 + ring * 1.4f, y, x, y + 3, x + 4, y - 2,
               x + 12 + ring * 0.2f, y + 2);
    }
  }
  p.noFill();
  p.stroke(material::hexColor(schematic ? 0xEDC472 : 0xF2FFDC, 0.75f));
  p.strokeWeight(1.7f);
  p.bezier(55, 123, 66, 55, 137, 32, 178, 32);
  p.stroke(material::hexColor(schematic ? 0xAB8845 : 0xD4FACC, 0.29f));
  p.strokeWeight(5);
  p.bezier(65, 115, 84, 52, 129, 45, 146, 43);
  p.pop();
}

Element specimen(std::string key, bool schematic = false) {
  return pen(
             key, [schematic](draw::Pen& p) { bioform(p, schematic); },
             Cache::Picture)
      .width(390)
      .height(375);
}

Element reticle() {
  return pen(
             "metroid.reticle",
             [](draw::Pen& p) {
               p.noFill();
               for (int x : {0, 1})
                 for (int y : {0, 1}) {
                   p.push();
                   p.translate(x ? 414 : 0, y ? 434 : 0);
                   p.scale(x ? -1 : 1, y ? -1 : 1);
                   p.stroke(material::hexColor(0x477F8B, 0.9f));
                   p.strokeWeight(11);
                   p.beginShape();
                   p.vertex(0, 31);
                   p.vertex(0, 15);
                   p.vertex(15, 0);
                   p.vertex(36, 0);
                   p.endShape();
                   p.stroke(material::hexColor(0xA9F0E9, 0.75f));
                   p.strokeWeight(3);
                   p.beginShape();
                   p.vertex(0, 31);
                   p.vertex(0, 15);
                   p.vertex(15, 0);
                   p.vertex(36, 0);
                   p.endShape();
                   p.stroke(material::hexColor(0xE7FFF6, 0.8f));
                   p.strokeWeight(1);
                   p.line(5, 26, 5, 17);
                   p.line(17, 5, 30, 5);
                   p.pop();
                 }
               p.stroke(material::hexColor(0x91DBCE, 0.45f));
               p.strokeWeight(1);
               for (int mark = 0; mark < 23; ++mark) {
                 float y = 37 + mark * 16;
                 p.line(0, y, mark % 5 == 0 ? 9 : 4, y);
                 p.line(414, y, mark % 5 == 0 ? 405 : 410, y);
               }
               p.stroke(material::hexColor(0xEAB865, 0.7f));
               p.line(198, 213, 207, 213);
               p.line(217, 213, 226, 213);
               p.line(212, 200, 212, 208);
               p.line(212, 218, 212, 226);
             },
             Cache::Picture)
      .width(414)
      .height(434);
}

Element visor() {
  return pen(
             "metroid.visor-shell",
             [](draw::Pen& p) {
               for (int side : {-1, 1}) {
                 p.push();
                 p.translate(720, 0);
                 p.scale(float(side), 1);
                 polygon(p,
                         {{0, 0},
                          {720, 0},
                          {720, 88},
                          {498, 97},
                          {434, 78},
                          {244, 68},
                          {184, 95},
                          {0, 94}},
                         material::hexColor(0x233138),
                         material::hexColor(0x53676B), 2);
                 polygon(p,
                         {{0, 3},
                          {655, 3},
                          {616, 28},
                          {375, 52},
                          {310, 35},
                          {174, 32},
                          {174, 58},
                          {0, 65}},
                         material::hexColor(0x101B23),
                         material::hexColor(0x35494C));
                 polygon(p,
                         {{0, 76},
                          {174, 64},
                          {190, 76},
                          {374, 69},
                          {553, 46},
                          {606, 52},
                          {579, 76},
                          {351, 104},
                          {192, 106},
                          {179, 96},
                          {0, 104}},
                         material::hexColor(0x29434A), muted);
                 p.noFill();
                 p.stroke(material::hexColor(0xAEDBCF, 0.20f));
                 p.strokeWeight(1);
                 p.bezier(0, 106, 235, 122, 446, 86, 675, 70);
                 p.stroke(material::hexColor(0x14333E));
                 p.strokeWeight(3);
                 for (int rib = 0; rib < 34; ++rib)
                   p.line(257 + rib * 9, 8, 253 + rib * 9, 35 - rib * 0.36f);
                 p.stroke(material::hexColor(0xAFF1EB, 0.54f));
                 p.strokeWeight(2);
                 p.beginShape();
                 p.vertex(221, 126);
                 p.vertex(307, 137);
                 p.vertex(320, 151);
                 p.vertex(327, 167);
                 p.vertex(327, 201);
                 p.vertex(337, 214);
                 p.vertex(347, 214);
                 p.vertex(342, 125);
                 p.vertex(325, 120);
                 p.vertex(220, 111);
                 p.endShape();
                 p.stroke(material::hexColor(0x7DABAB, 0.30f));
                 p.strokeWeight(7);
                 p.line(219, 117, 326, 129);

                 polygon(p,
                         {{720, 960},
                          {720, 876},
                          {614, 897},
                          {510, 917},
                          {430, 900},
                          {333, 902},
                          {291, 944},
                          {0, 936},
                          {0, 960}},
                         material::hexColor(0x26383C),
                         material::hexColor(0x4A666B), 2);
                 polygon(p,
                         {{720, 960},
                          {720, 925},
                          {614, 923},
                          {513, 943},
                          {408, 928},
                          {343, 934},
                          {334, 960}},
                         material::hexColor(0x0C151B),
                         material::hexColor(0x183A42));
                 p.noFill();
                 p.stroke(material::hexColor(0x94CECB, 0.40f));
                 p.strokeWeight(2);
                 p.bezier(340, 924, 430, 889, 588, 943, 703, 908);
                 p.stroke(material::hexColor(0x0A151E));
                 p.strokeWeight(6);
                 for (int rib = 0; rib < 14; ++rib)
                   p.line(374 + rib * 17, 927 + std::sin(rib * 0.34f) * 12,
                          374 + rib * 17, 947 + std::sin(rib * 0.34f) * 12);

                 // The four floating corner pieces are thick etched polyline
                 // paths over the world, not the edges of a full-screen card.
                 for (int lower = 0; lower < 2; ++lower) {
                   p.push();
                   p.translate(0, lower ? 766 : 273);
                   p.scale(1, lower ? -1 : 1);
                   p.noFill();
                   p.stroke(material::hexColor(0x76A7B0, 0.25f));
                   p.strokeWeight(11);
                   p.beginShape();
                   p.vertex(359, 36);
                   p.vertex(359, 17);
                   p.vertex(339, 0);
                   p.vertex(297, 4);
                   p.vertex(285, 10);
                   p.endShape();
                   p.stroke(material::hexColor(0xBAE8E4, 0.70f));
                   p.strokeWeight(2);
                   p.beginShape();
                   p.vertex(364, 35);
                   p.vertex(364, 15);
                   p.vertex(341, -6);
                   p.vertex(297, -2);
                   p.vertex(278, 9);
                   p.endShape();
                   p.stroke(material::hexColor(0xC2DCCB, 0.53f));
                   p.strokeWeight(1);
                   p.line(361, 62, 361, 170);
                   p.line(354, 69, 354, 144);
                   p.bezier(354, 144, 354, 157, 358, 165, 361, 170);
                   p.pop();
                 }
                 p.stroke(material::hexColor(0xBACA96, 0.19f));
                 p.strokeWeight(1);
                 p.bezier(720, 420, 581, 464, 425, 482, 355, 482);
                 p.bezier(720, 509, 581, 481, 425, 482, 355, 482);
                 p.pop();
               }
               p.noStroke();
               p.fill(material::hexColor(0xD1FFFB, 0.88f));
               for (int light = 0; light < 4; ++light)
                 p.rect(618 + light * 55, 76, 39, 5);
               p.fill(material::hexColor(0x8EC7C8, 0.68f));
               p.rect(234, 914, 35, 5);
               p.rect(1171, 914, 35, 5);
             },
             Cache::Picture)
      .width(1440)
      .height(960);
}

Element spectrum() {
  return pen("metroid.live-spectrum",
             [](draw::Pen& p) {
               const float t = float(p.millis() * 0.001);
               p.noFill();
               p.strokeWeight(1);
               for (int lane = 0; lane < 3; ++lane) {
                 p.stroke(lane == 0 ? amber
                                    : material::hexColor(0x67BDBF, 0.33f));
                 p.beginShape();
                 for (int i = 0; i < 238; i += 2) {
                   float x = float(i);
                   float envelope = std::exp(-std::pow((x - 116) / 64, 2));
                   float oscillation =
                       std::sin(x * (0.14f + lane * 0.033f) - t * 2.2f);
                   float beat = 0.45f + 0.55f * std::sin(x * 0.037f + t * 0.7f);
                   p.vertex(
                       x, 36 + oscillation * envelope * beat * (24 - lane * 5));
                 }
                 p.endShape();
               }
             })
      .width(238)
      .height(74);
}
}  // namespace

struct MetroidScanVisor {
  motion::Animatable<float> seconds = motion::animatable(0.0f);

  motion::Animatable<float> drift(float low, float high, float period = 10) {
    return motion::bind(seconds, {.from = {0, period},
                                  .envelope = motion::envelope::cosine(),
                                  .to = {low, high}});
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 960);
    ctx.background(black);
    ctx.captureAt(4.2);
    ctx.engine.timer([this, &engine = ctx.engine] {
      seconds = float(engine.elapsed().count());
    });

    auto fluid = material::shader(ctx.assets.hub(), ctx.local("fluid.sksl"));
    std::vector<Element> scene{
        kit::at(chamber(), 0, 0, 1440, 960)
            .translateX(drift(-2.5f, 2.5f))
            .translateY(drift(-1, 1, 13)),
        kit::at(503, 176, 426, 475).fill(fluid),
        kit::at(specimen("metroid.specimen"), 515, 244, 390, 375)
            .translateX(drift(-4, 4, 11))
            .translateY(drift(-8, 8, 8))
            .rotate(drift(-1.4f, 1.4f, 11))
            .filter(
                material::Filter::glow(material::hexColor(0x95E8A9, 0.15f), 5)),
        kit::at(reticle(), 510, 213, 414, 434)
            .translateX(drift(-0.7f, 0.7f, 11)),
        kit::at(514, 216, 406, 429)
            .overflow(Overflow::Clip)
            .children({kit::at(0, 0, 406, 24)
                           .fill(material::linearGradient(
                               {0, 0}, {0, 1},
                               {material::hexColor(0xB8FCBD, 0),
                                material::hexColor(0xACFFD0, 0.055f),
                                material::hexColor(0xCFFFF0, 0.27f)}))
                           .cache(Cache::Texture)
                           .translateY(motion::bind(
                               seconds, {.from = {0, 6},
                                         .envelope = motion::envelope::shaped(
                                             [](float p) { return p; }),
                                         .to = {-24, 429}}))}),
        label("BIOLOGICAL SIGNATURE", 561, 193, 320, 12, cyan, false),
        label("TARGET LOCK  //  BIOLOGICAL DATA", 558, 650, 346, 10, cyan,
              true),
        label("x  47.203", 927, 302, 100, 9, muted, true),
        label("y  18.991", 927, 321, 100, 9, muted, true),
        label("z  03.628", 927, 340, 100, 9, muted, true),
        label("08.4 m", 931, 580, 93, 11, ice, true),
        kit::at(697, 437, 24, 24)
            .fill(material::hexColor(0xEA9843, 0.14f))
            .foreground(stroke(1.5f, Fill::color(amber)))
            .opacity(drift(0.6f, 1, 2.4f)),
    };

    std::vector<Element> morphology{
        researchPanel("metroid.morphology-panel", 301, 583),
        label("MORPHOLOGY / 01", 24, 24, 257, 12, cyan, false),
        label("BIOFORM DATABASE", 24, 49, 255, 9, muted, true),
        kit::at(specimen("metroid.morphology-specimen", true), 20, 88, 390, 375)
            .scale(0.66f)
            .transformOrigin(pct(0), pct(0)),
        label("A", 30, 129, 35, 10, amber, true),
        label("B", 239, 207, 32, 10, amber, true),
        label("C", 69, 304, 31, 10, amber, true),
        label("01  SEMIPERMEABLE MEMBRANE", 24, 371, 254, 9, amber, true),
        label("02  ENERGY ABSORPTION NUCLEI", 24, 397, 260, 9, ice, true),
        label("03  MANDIBULAR ATTACHMENT", 24, 423, 260, 9, ice, true),
        kit::at(24, 456, 253, 1).fill(muted),
        label("ORIGIN", 24, 473, 96, 10, muted, true),
        label("SR388", 160, 473, 105, 10, amber, true),
        label("LIFE-CYCLE", 24, 501, 114, 10, muted, true),
        label("LARVAL", 160, 501, 110, 10, ice, true),
        label("CODEX  /  078.006", 24, 548, 261, 10, cyan, true),
    };
    morphology.push_back(pen(
                             "metroid.morphology-leaders",
                             [](draw::Pen& p) {
                               p.noFill();
                               p.stroke(material::hexColor(0xECB665, 0.64f));
                               p.strokeWeight(0.8f);
                               p.line(46, 133, 68, 112);
                               p.line(68, 112, 113, 112);
                               p.line(238, 203, 213, 187);
                               p.line(213, 187, 162, 187);
                               p.line(83, 303, 111, 303);
                               p.line(111, 303, 115, 263);
                               for (int tick = 0; tick < 21; ++tick)
                                 p.line(278, 90 + tick * 12,
                                        tick % 5 == 0 ? 269 : 274,
                                        90 + tick * 12);
                               p.stroke(material::hexColor(0x589FAA, 0.22f));
                               p.line(24, 340, 277, 340);
                               p.line(144, 81, 144, 335);
                               p.circle(144, 189, 225);
                             },
                             Cache::Picture)
                             .width(301)
                             .height(583));
    scene.push_back(kit::at(positioned(), 51, 231, 301, 583)
                        .rotateY(drift(6.7f, 7.7f, 12))
                        .rotateX(-1)
                        .children(std::move(morphology)));

    std::vector<Element> analysis{
        researchPanel("metroid.analysis-panel", 294, 497),
        label("SPECTRAL ANALYSIS", 22, 25, 264, 12, cyan),
        label("BETA-RAY RESPONSE", 22, 51, 264, 9, muted, true),
        kit::at(spectrum(), 26, 83, 238, 74),
        label("0       0.25     0.50    0.75", 24, 161, 253, 8, muted, true),
        kit::at(23, 184, 247, 1).fill(muted),
        label("ENERGY TRANSFER", 23, 203, 247, 10, ice, true),
        label("98.4", 21, 224, 160, 40, cyan),
        label("% MATCH", 169, 252, 102, 11, amber, true),
        label("KNOWN SIGNATURE", 23, 283, 248, 9, muted, true),
        label("METROID / LARVAL", 23, 305, 250, 17, amber),
        kit::at(23, 344, 247, 1).fill(muted),
        label("THREAT ASSESSMENT", 23, 364, 252, 10, red, true),
        label("ENERGY DRAIN", 23, 387, 248, 19, cyan),
        label("CONTACT MUST BE AVOIDED", 23, 417, 254, 9, red, true),
        label("ICE BEAM  /  EFFECTIVE", 23, 461, 252, 10, ice, true),
    };
    for (int i = 0; i < 20; ++i) {
      analysis.push_back(
          kit::at(23 + i * 12.3f, 273, 8, 3).fill(i < 19 ? amber : blue));
    }
    analysis.push_back(pen(
                           "metroid.spectrum-grid",
                           [](draw::Pen& p) {
                             p.noFill();
                             p.stroke(material::hexColor(0x508F99, 0.28f));
                             p.strokeWeight(0.6f);
                             for (int i = 0; i < 6; ++i)
                               p.line(23, 87 + i * 12, 269, 87 + i * 12);
                             for (int i = 0; i < 9; ++i)
                               p.line(23 + i * 30.7f, 84, 23 + i * 30.7f, 154);
                           },
                           Cache::Picture)
                           .width(294)
                           .height(497));
    scene.push_back(kit::at(positioned(), 1092, 288, 294, 497)
                        .rotateY(drift(-6.1f, -7.1f, 12))
                        .children(std::move(analysis)));

    std::vector<Element> data{
        researchPanel("metroid.acquired-data", 674, 201),
        label("Morphology: Metroid", 26, 16, 620, 26, cyan),
        label("Research data acquired.", 27, 55, 617, 14, amber),
        kit::at(
            words("A parasitic life-form capable of absorbing the life energy\n"
                  "of other organisms. The transparent membrane protects\n"
                  "several energy nuclei. Extreme cold disrupts the bioform\n"
                  "and leaves the cellular structure vulnerable to impact.",
                  16, material::hexColor(0xD6EAE3)),
            27, 83, 620, 94),
        label("CREATURES  >  TALLON IV  >  LABORATORY", 28, 178, 550, 8, muted,
              true),
        label("[ A ]", 594, 175, 53, 12, cyan, true),
    };
    scene.push_back(kit::at(positioned(), 388, 704, 674, 201)
                        .translateY(drift(-0.6f, 0.6f, 9))
                        .opacity(motion::bind(seconds, {.from = {3.2f, 3.8f},
                                                        .clampFrom = true,
                                                        .to = {0, 1}}))
                        .children(std::move(data)));

    scene.push_back(
        kit::at(visor(), 0, 0, 1440, 960).translateX(drift(1.8f, -1.8f, 10)));
    scene.push_back(label("ENERGY", 572, 124, 135, 10, muted, true));
    scene.push_back(label("92", 557, 145, 93, 47, cyan));
    scene.push_back(
        kit::at(660, 175, 220, 7).fill(material::hexColor(0x315F69, 0.65f)));
    scene.push_back(kit::at(660, 175, 204, 7).fill(cyan));
    for (int tank = 0; tank < 5; ++tank)
      scene.push_back(
          kit::at(664 + tank * 20, 157, 14, 7).fill(tank == 4 ? muted : cyan));
    scene.push_back(label("SCAN VISOR", 84, 147, 237, 15, cyan, false));
    scene.push_back(
        label("PHENDRANA / RESEARCH LAB AETHER", 84, 173, 366, 9, muted, true));
    scene.push_back(
        label("HOLD [ ZL ] TO ANALYZE", 1106, 175, 287, 10, ice, true));
    scene.push_back(label("LOGBOOK", 1110, 812, 261, 12, cyan));
    scene.push_back(label("078 / 120", 1110, 840, 230, 27, cyan));
    scene.push_back(
        label("RESEARCH COMPLETION   65%", 1110, 875, 255, 9, muted, true));
    scene.push_back(
        label("STUDY / METROID PRIME VISOR", 78, 864, 281, 9, muted, true));
    scene.push_back(label("AN ORIGINAL RESEARCH COMPOSITION", 78, 884, 287, 8,
                          muted, true));
    scene.push_back(label("SCANNING / LOCKED", 609, 668, 300, 14, cyan)
                        .opacity(motion::bind(seconds, {.from = {3.1f, 3.3f},
                                                        .clampFrom = true,
                                                        .to = {1, 0}})));
    scene.push_back(label("SCAN COMPLETE", 625, 668, 300, 14, cyan)
                        .opacity(motion::bind(seconds, {.from = {3.3f, 3.6f},
                                                        .clampFrom = true,
                                                        .to = {0, 1}})));
    scene.push_back(
        kit::at(532, 694, 370, 3).fill(material::hexColor(0x4F7D79, 0.6f)));
    scene.push_back(kit::at(532, 694, 370, 3)
                        .fill(cyan)
                        .transformOrigin(pct(0), pct(50))
                        .scaleX(motion::bind(seconds, {.from = {0, 3.3f},
                                                       .clampFrom = true,
                                                       .to = {0, 1}})));
    ctx.composer.render(positioned()
                            .width(1440)
                            .height(960)
                            .perspective(1800)
                            .preserve3d()
                            .children(std::move(scene)));
  }
};

SIGIL_SKETCH(
    MetroidScanVisor, "Study · Game UI",
    "Metroid Prime-inspired helmet optics, translucent creature anatomy, "
    "retained scan overlays and acquired biological research")
