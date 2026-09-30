/** An original radio poster: type, a fabricated loudspeaker and a city
 *  montage share two printed inks on fibrous paper. Every image is geometry.
 */
// TAGS: Studies/Cultural, Typography/Lettering, Typography/Effects,
// TAGS: Materials/Shaders, Drawing/Generative

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace compose = sigil::compose;
namespace draw = sigil::draw;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

namespace {

constexpr float kWidth = 900;
constexpr float kHeight = 1260;
const material::Color kPaper = material::hexColor(0xe7d7af);
const material::Color kBlack = material::hexColor(0x24251f);
const material::Color kRed = material::hexColor(0xb42b21);
const material::Color kLightBlack = material::hexColor(0x24251f, 0.31f);

struct Paper {
  material::Color base = kPaper;
};
struct Ink {
  material::Color pigment = kBlack;
  float loss = 0.045f;
};

/** The material grain primitive is spatially homogeneous. Long fibres,
 *  an edge stain and a crease need a field in the page's coordinates, so
 *  the paper carries their joint sampling in one shader. */
constexpr std::string_view kPaperSource = R"(
float hash(float2 p) { return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
half4 main(float2 p) {
  float fine = hash(floor(p * 1.8));
  float soft = hash(floor(p / 19.0));
  float fibre = pow(hash(float2(floor(p.x * 2.3), floor(p.y / 7.0))), 21.0);
  float2 uv = p / uResolution;
  float edge = min(min(uv.x, 1.0-uv.x), min(uv.y, 1.0-uv.y));
  float age = (1.0-smoothstep(0.0, 0.1, edge))*0.055;
  float fold = exp(-abs(p.x-449.0)/2.5)*0.027;
  float3 c = base.rgb + (fine-0.5)*0.034 + (soft-0.5)*0.012;
  c -= age + fibre*0.025 + fold;
  return half4(c, 1);
}
)";

/** The material grain primitive varies luminance. Printed dropout must
 *  remove coverage to expose the stock, with larger regions changing the
 *  pressure of that same pigment, so this field answers premultiplied ink. */
constexpr std::string_view kInkSource = R"(
float hash(float2 p) { return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
half4 main(float2 p) {
  float fine = hash(floor(p * 1.5));
  float mottle = hash(floor(p / 9.0));
  float alpha = smoothstep(loss * 0.45, loss * 1.35, fine);
  alpha *= 0.91 + 0.09*mottle;
  return half4(pigment.rgb * pigment.a * alpha, pigment.a * alpha);
}
)";

material::Material ink(material::Color color, float loss = 0.045f) {
  return material::shader(kInkSource, Ink{color, loss});
}

void polygon(draw::Pen& pen,
             std::initializer_list<std::array<float, 2>> points) {
  pen.beginShape();
  for (const auto& point : points) pen.vertex(point[0], point[1]);
  pen.endShape(draw::CLOSE);
}

/** The city is cut into independently printed silhouettes: oblique roof
 *  planes, pale window holes and aerials form a fabricated montage. */
void city(draw::Pen& pen) {
  pen.push();
  pen.translate(85, 870);
  pen.angleMode(draw::DEGREES);
  pen.rotate(-9);
  pen.noStroke();
  const std::array<float, 13> heights = {82,  145, 122, 205, 111, 169, 231,
                                         138, 176, 117, 220, 151, 99};
  for (int i = 0; i < 13; ++i) {
    float x = i * 54.0f;
    float h = heights[i];
    pen.fill(ink(i % 3 == 0 ? kRed : kBlack));
    polygon(pen, {{x, 50}, {x, 50 - h}, {x + 43, 38 - h}, {x + 43, 50}});
    pen.fill(material::Color{kPaper.r, kPaper.g, kPaper.b, 0.74f});
    for (int row = 0; row < static_cast<int>(h / 18) - 1; ++row)
      for (int col = 0; col < 3; ++col)
        if ((row + col + i) % 7 != 0)
          pen.rect(x + 7 + col * 11, 42 - h + row * 18, 5.5f, 8.5f);
    pen.stroke(kBlack);
    pen.strokeWeight(1.7f);
    pen.line(x + 10, 50 - h, x + 10, 31 - h);
    pen.line(x + 3, 36 - h, x + 34, 27 - h);
    pen.noStroke();
  }
  pen.fill(ink(kBlack));
  polygon(pen, {{0, 50}, {715, 50}, {715, 93}, {0, 93}});
  pen.pop();
}

/** A loudspeaker drawn as a screen of radial metal ribs and a dot field,
 *  with a bright off-centre cone. The dots change size under a synthetic
 *  oblique light, so it reads like a cropped photographic reproduction. */
void speaker(draw::Pen& pen) {
  constexpr float cx = 254, cy = 644, radius = 209;
  pen.noStroke();
  pen.fill(ink(kBlack, 0.035f));
  pen.circle(cx, cy, radius * 2);
  pen.fill(kPaper);
  pen.circle(cx, cy, 387);
  pen.fill(ink(kBlack, 0.08f));
  pen.circle(cx, cy, 373);
  pen.fill(kPaper);
  pen.circle(cx, cy, 342);
  pen.fill(ink(kBlack, 0.1f));
  pen.circle(cx, cy, 332);

  pen.push();
  pen.clip([&] { pen.circle(cx, cy, 332); });
  pen.fill(material::Color{kPaper.r, kPaper.g, kPaper.b, 0.8f});
  for (float y = cy - 166; y <= cy + 166; y += 7.2f) {
    for (float x = cx - 166; x <= cx + 166; x += 7.2f) {
      float dx = (x - cx) / 166.0f, dy = (y - cy) / 166.0f;
      float r = std::sqrt(dx * dx + dy * dy);
      float light = 0.56f + 0.21f * dx - 0.32f * dy;
      float band = 0.5f + 0.5f * std::cos(r * 12);
      float dot =
          1.1f + 3.6f * std::clamp(light * 0.7f + band * 0.25f, 0.0f, 1.0f);
      pen.circle(x, y, dot);
    }
  }
  pen.pop();

  pen.stroke(material::Color{kPaper.r, kPaper.g, kPaper.b, 0.77f});
  pen.strokeWeight(1.1f);
  pen.noFill();
  for (int i = 0; i < 44; ++i) {
    float a = i * 6.2831853f / 44.0f;
    pen.line(cx + std::cos(a) * 60, cy + std::sin(a) * 60,
             cx + std::cos(a) * 159, cy + std::sin(a) * 159);
  }
  pen.strokeWeight(3.0f);
  pen.circle(cx, cy, 179);
  pen.circle(cx, cy, 135);
  pen.noStroke();
  pen.fill(ink(kBlack));
  pen.circle(cx - 9, cy + 7, 124);
  pen.fill(ink(kPaper, 0.08f));
  pen.circle(cx - 13, cy + 2, 85);
  pen.fill(ink(kBlack));
  pen.circle(cx - 17, cy - 1, 50);
  pen.fill(ink(kRed));
  pen.circle(cx - 22, cy - 6, 17);

  for (int i = 0; i < 10; ++i) {
    float a = (i + 0.2f) * 6.2831853f / 10.0f;
    float x = cx + std::cos(a) * 197, y = cy + std::sin(a) * 197;
    pen.fill(kPaper);
    pen.circle(x, y, 7);
    pen.stroke(kBlack);
    pen.strokeWeight(1);
    pen.line(x - 2, y - 2, x + 2, y + 2);
    pen.noStroke();
  }
}

void blackPlate(draw::Pen& pen) {
  pen.angleMode(draw::DEGREES);
  pen.noStroke();
  pen.fill(ink(kBlack));
  polygon(pen, {{45, 285}, {841, 135}, {841, 149}, {45, 299}});
  polygon(pen, {{49, 330}, {406, 254}, {406, 296}, {49, 372}});
  polygon(pen, {{460, 782}, {847, 616}, {847, 654}, {460, 820}});
  for (int i = 0; i < 7; ++i) {
    pen.push();
    pen.translate(704, 672);
    pen.rotate(-23);
    pen.noFill();
    pen.stroke(ink(kLightBlack, 0.08f));
    pen.strokeWeight(2.0f);
    pen.ellipse(0, 0, 150 + i * 21.0f, 106 + i * 18.0f);
    pen.pop();
  }
  city(pen);
}

void redPlate(draw::Pen& pen) {
  pen.noStroke();
  pen.fill(ink(kRed, 0.055f));
  polygon(pen, {{292, 517}, {847, 276}, {847, 586}, {292, 733}});
  polygon(pen, {{48, 1039}, {847, 1039}, {847, 1145}, {48, 1145}});
  polygon(pen, {{49, 406}, {87, 398}, {87, 871}, {49, 879}});
  pen.fill(ink(kBlack));
  polygon(pen, {{504, 651}, {849, 541}, {849, 583}, {504, 693}});
  for (int i = 0; i < 4; ++i)
    polygon(pen, {{514 + i * 32.0f, 734 - i * 13.0f},
                  {532 + i * 32.0f, 727 - i * 13.0f},
                  {553 + i * 32.0f, 772 - i * 13.0f},
                  {535 + i * 32.0f, 779 - i * 13.0f}});
}

/** Registration and paper handling marks stay in the page's space. */
void finishing(draw::Pen& pen) {
  pen.noFill();
  pen.stroke(material::Color{kBlack.r, kBlack.g, kBlack.b, 0.55f});
  pen.strokeWeight(0.7f);
  for (const auto& point : std::array<std::array<float, 2>, 4>{
           {{25, 25}, {875, 25}, {25, 1235}, {875, 1235}}}) {
    pen.circle(point[0], point[1], 12);
    pen.line(point[0] - 10, point[1], point[0] + 10, point[1]);
    pen.line(point[0], point[1] - 10, point[0], point[1] + 10);
  }
  pen.line(48, 1018, 847, 1018);
  pen.line(48, 1164, 847, 1164);
  pen.noStroke();
  pen.randomSeed(7412);
  for (int i = 0; i < 1600; ++i) {
    const float x = pen.random(35, 865), y = pen.random(38, 1224);
    const float a = pen.random(0.018f, 0.072f);
    pen.fill(material::Color{kPaper.r, kPaper.g, kPaper.b, a});
    pen.rect(x, y, pen.random(0.2f, 1.2f), pen.random(0.4f, 3.5f));
  }
  pen.fill(material::Color{kBlack.r, kBlack.g, kBlack.b, 0.12f});
  for (int i = 0; i < 500; ++i)
    pen.circle(pen.random(10, 890), pen.random(10, 1250),
               pen.random(0.4f, 1.1f));
}

struct ConstructivistRadio {
  weave::Face titleFace;
  weave::Face narrowFace;
  weave::Face regularFace;

  compose::Text words(std::u8string_view content, float x, float y, float width,
                      float size, material::Color color = kBlack,
                      float rotation = 0, float track = 0,
                      float condense = 1) const {
    return compose::text(content)
        .font({.face = titleFace,
               .size = size,
               .track = track,
               .condense = condense,
               .language = "ru"})
        .ink(ink(color), compose::PaintBox::Canvas)
        .absolute()
        .left(x)
        .top(y)
        .width(width)
        .height(size * 1.3f)
        .textFirstBaseline(weave::FrameOptions::FirstBaseline::kCapHeight)
        .maxTextLines(1)
        .transformOrigin(compose::pct(0), compose::pct(0))
        .rotate(rotation);
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.background(kPaper);
    ctx.captureAt(0.4);
    titleFace =
        weave::ports::face({"Arial Black", "Arial", "Helvetica Neue"}, 900);
    narrowFace =
        weave::ports::face({"Arial Narrow", "Avenir Next Condensed"}, 700);
    regularFace = weave::ports::face({"Arial", "Helvetica Neue"}, 400);

    std::vector<compose::Element> typography;
    typography.push_back(
        words(u8"РАДИО", 48, 140, 850, 188, kBlack, -10.7f, -6.8f, 0.91f));
    typography.push_back(words(u8"СЛУШАИ\u0306", 448, 475, 460, 85, kPaper,
                               -23.4f, -1.3f, 0.88f));
    typography.push_back(
        words(u8"ГОРОД", 477, 584, 422, 105, kPaper, -23.4f, -3, 0.88f));
    typography.push_back(
        words(u8"ГОВОРИТ", 524, 650, 362, 35, kPaper, -18.1f, 1.1f, 0.9f));
    typography.push_back(words(u8"ЗВУК  /  СВЕТ  /  ДВИЖЕНИЕ", 62, 338, 470, 18,
                               kPaper, -11.7f, 0.65f)
                             .font({.face = narrowFace}));
    typography.push_back(words(u8"THE CITY HAS A VOICE", 132, 958, 718, 48,
                               kBlack, 0, -0.8f, 0.85f));
    typography.push_back(words(u8"НАСТРОЙСЯ НА ВОЛНУ", 70, 1056, 756, 53,
                               kPaper, 0, -0.6f, 0.9f));
    typography.push_back(words(u8"RADIO • CINEMA • POETRY • EVERYDAY LIFE", 74,
                               1115, 750, 14, kPaper, 0, 1.55f)
                             .font({.face = regularFace}));
    typography.push_back(words(u8"02", 735, 171, 120, 71, kRed, 0, -2));
    typography.push_back(words(u8"ВЫПУСК", 750, 238, 135, 14, kBlack, 0, 1.3f)
                             .font({.face = narrowFace}));
    typography.push_back(
        words(u8"ЗВУК ДЛЯ ВСЕХ", 102, 389, 325, 19, kBlack, -11.7f, 2.1f)
            .font({.face = narrowFace}));
    typography.push_back(
        words(u8"VOICE / WAVE / CITY", 865, 917, 650, 16, kBlack, -90, 2.9f)
            .font({.face = narrowFace}));
    typography.push_back(words(u8"ЛИТЕРАТУРА • МУЗЫКА • КИНО", 76, 422, 470, 17,
                               kPaper, 90, 1.3f)
                             .font({.face = narrowFace}));
    typography.push_back(words(u8"A TRANSMISSION IN TWO INKS", 48, 1184, 740,
                               16, kBlack, 0, 1.8f)
                             .font({.face = narrowFace}));
    typography.push_back(
        words(u8"ORIGINAL COMPOSITION  /  CONSTRUCTIVIST RADIO STUDY", 48, 1211,
              820, 10.2f, kBlack, 0, 1.12f)
            .font({.face = regularFace}));
    typography.push_back(
        words(u8"06:00 — 24:00", 506, 919, 280, 27, kBlack, -9, 1)
            .font({.face = narrowFace}));
    typography.push_back(
        words(u8"AIR / EAR / EVERYWHERE", 501, 942, 326, 12, kBlack, -9, 1.5f)
            .font({.face = regularFace}));
    typography.push_back(
        words(u8"ИСКУССТВО В ЭФИРЕ", 111, 1004, 660, 13, kBlack, 0, 3.0f)
            .font({.face = narrowFace}));

    ctx.composer.render(
        compose::box()
            .width(kWidth)
            .height(kHeight)
            .fill(material::shader(kPaperSource, Paper{}))
            .overflow(compose::Overflow::Clip)
            .children(
                {compose::pen("radio.black", blackPlate,
                              compose::Cache::Picture)
                     .inset(0),
                 compose::pen("radio.red", redPlate, compose::Cache::Picture)
                     .inset(0)
                     .translateX(0.75f)
                     .translateY(0.4f)
                     .blendMode(material::BlendMode::Multiply),
                 // The receiver stands over the beam, as a cut-out pasted on
                 // it.
                 compose::pen("radio.receiver", speaker,
                              compose::Cache::Picture)
                     .inset(0),
                 compose::positioned().inset(0).children(std::move(typography)),
                 compose::pen("radio.impressions", finishing,
                              compose::Cache::Picture)
                     .inset(0)}));
  }
};

}  // namespace

SIGIL_SKETCH(ConstructivistRadio, "Study · Typography",
             "An original constructivist radio poster: Cyrillic type, two "
             "printed inks, radial speaker screen and fabricated city montage.")
