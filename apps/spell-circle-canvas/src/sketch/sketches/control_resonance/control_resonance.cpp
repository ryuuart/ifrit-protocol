/** An invented Bureau evidence folio. Redactions attach to selected
 *  text, the concrete construction is retained, and the resonance field
 *  and its instrument traces run on the sketch clock. */
// TAGS: Studies/Games, Typography/Attachments, Drawing/Geometry,
// Materials/Shaders

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/query/Selector.h>

#include <cmath>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
const auto paper = material::hexColor(0xDEDCD1);
const auto black = material::hexColor(0x202320);
const auto gray = material::hexColor(0x686C62);
const auto red = material::hexColor(0xB82120);
const auto chalk = material::hexColor(0xF0E7D9);

Text label(std::string value, float size, material::Color color = black,
           bool mono = false) {
  static const auto sans = weave::ports::face({"Helvetica Neue", "Arial"});
  static const auto terminal = weave::ports::face({"IBM Plex Mono", "Menlo"});
  return text(std::move(value))
      .font({.face = mono ? terminal : sans, .size = size})
      .ink(color);
}

Element line(float left, float top, float width) {
  return kit::at(left, top, width, 1).fill(material::hexColor(0x9A9D91));
}

Text restrictedReport() {
  auto passage = label(
      "At 03:17 the eastern service corridor\n"
      "returned a space absent from all plans.\n"
      "Agent Mercer reported a second voice\n"
      "from the sealed observation chamber.\n"
      "The recording contains no human speech.\n"
      "Access remains under Director review.",
      17, black, true);
  for (const auto word : {u8"Mercer", u8"observation", u8"Director"}) {
    passage.textAttach(
        weave::selectors::text(word),
        box().left(0).top(1).width(pct(100)).height(pct(100)).fill(black));
  }
  return passage;
}

Element pyramid() {
  return pen(
             "bureau.concrete",
             [](sigil::draw::Pen& brush) {
               const auto triangle = [&](glm::vec2 first, glm::vec2 second,
                                         glm::vec2 third,
                                         material::Color color) {
                 brush.noStroke();
                 brush.fill(material::from(color).layer(
                     material::noise(0.35f,
                                     {.octaves = 2, .seed = 8, .grain = true}),
                     {.blend = material::BlendMode::Multiply,
                      .opacity = 0.12f}));
                 brush.beginShape();
                 brush.vertex(first.x, first.y);
                 brush.vertex(second.x, second.y);
                 brush.vertex(third.x, third.y);
                 brush.endShape(sigil::draw::CLOSE);
               };
               triangle({70, 58}, {540, 58}, {330, 440},
                        material::hexColor(0x342F2B));
               triangle({70, 58}, {300, 115}, {330, 440},
                        material::hexColor(0x797465));
               triangle({540, 58}, {300, 115}, {330, 440},
                        material::hexColor(0x292C28));
               triangle({70, 58}, {540, 58}, {300, 115},
                        material::hexColor(0xABA18B));
               brush.noFill();
               brush.strokeWeight(1);
               for (int course = 1; course < 28; ++course) {
                 const float amount = course / 28.0f;
                 const glm::vec2 left =
                     glm::mix(glm::vec2{70, 58}, {330, 440}, amount);
                 const glm::vec2 seam =
                     glm::mix(glm::vec2{300, 115}, {330, 440}, amount);
                 const glm::vec2 right =
                     glm::mix(glm::vec2{540, 58}, {330, 440}, amount);
                 brush.stroke(material::hexColor(0xD0C6B1, 0.34f));
                 brush.line(left.x, left.y, seam.x, seam.y);
                 brush.stroke(material::hexColor(0xA4937B, 0.24f));
                 brush.line(seam.x, seam.y, right.x, right.y);
               }
               brush.stroke(material::hexColor(0xE3D7BD, 0.75f));
               brush.line(70, 58, 300, 115);
               brush.line(300, 115, 330, 440);
               brush.line(300, 115, 540, 58);
               brush.stroke(material::hexColor(0xE0A08B, 0.28f));
               for (int index = 0; index < 13; ++index) {
                 brush.line(330, 440, 10 + index * 49, 560);
                 brush.line(10, 466 + index * 7, 610, 466 + index * 7);
               }
               brush.noStroke();
               brush.fill(material::hexColor(0x171D1A));
               brush.rect(293, 514, 68, 8);
               brush.rect(304, 505, 46, 9);
               brush.rect(316, 496, 22, 9);
             },
             Cache::Texture)
      .width(620)
      .height(560);
}

Element resonanceTrace() {
  return pen("bureau.trace",
             [](sigil::draw::Pen& brush) {
               const float time = float(brush.millis() * 0.001);
               brush.noFill();
               brush.strokeWeight(1.25f);
               for (int lane = 0; lane < 3; ++lane) {
                 brush.stroke(lane == 0 ? red
                                        : material::hexColor(0x666A60, 0.5f));
                 brush.beginShape();
                 for (int sample = 0; sample < 540; sample += 2) {
                   const float position = float(sample);
                   const float burst =
                       std::exp(-std::pow((position - 280) / 140, 2));
                   const float height =
                       58 +
                       std::sin(position * (0.09f + lane * 0.014f) - time * 2) *
                           std::sin(position * 0.012f + time * 0.4f) * burst *
                           (46 - lane * 10);
                   brush.vertex(position, height);
                 }
                 brush.endShape();
               }
             })
      .width(540)
      .height(116);
}
}  // namespace

struct ControlResonance {
  motion::Animatable<float> phase = motion::animatable(0.0f);

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 960);
    ctx.background(paper);
    ctx.captureAt(3.4);
    const auto field =
        material::shader(ctx.assets.hub(), ctx.local("resonance.sksl"));
    std::vector<Element> folio{
        kit::at(0, 0, 1440, 960)
            .fill(material::from(paper).layer(
                material::noise(0.55f, {.seed = 12, .grain = true}),
                {.blend = material::BlendMode::Multiply, .opacity = 0.035f})),
        kit::at(42, 29, 40, 40).fill(red),
        kit::at(label("FBC", 14, paper, true), 48, 38, 38, 21),
        kit::at(label("FEDERAL BUREAU\nOF CONTROL", 12, black, true), 99, 27,
                300, 40),
        kit::at(label("RESEARCH & RECORDS\nINTERNAL CIRCULATION ONLY", 11, gray,
                      true),
                1040, 29, 355, 36),
        line(42, 90, 1354),
        kit::at(label("RESONANCE", 74).fontWeight(700), 38, 101, 980, 94),
        kit::at(label("CASE 04—19\nCLASSIFIED / A", 13, red, true), 1140, 126,
                258, 40),
        line(42, 213, 1354),
        kit::at(label("ALTERED SPACE / FIELD REPORT", 12, red, true), 44, 243,
                580, 22),
        kit::at(label("The architecture\nanswers back.", 42), 41, 278, 600,
                110),
        kit::at(label("LOCATION", 10, gray, true), 44, 424, 160, 20),
        kit::at(label("RESEARCH SECTOR / SUBLEVEL 06", 12, black, true), 212,
                421, 410, 22),
        line(44, 452, 554),
        kit::at(label("CLEARANCE", 10, gray, true), 44, 469, 160, 20),
        kit::at(label("BLACK ROCK PROTOCOL", 12, black, true), 212, 466, 410,
                22),
        line(44, 497, 554),
        kit::at(restrictedReport(), 44, 525, 565, 175),
        kit::at(label("01 / CARRIER INTERFERENCE", 10, red, true), 44, 727, 520,
                20),
        kit::at(resonanceTrace(), 44, 750, 540, 116),
        kit::at(label("0 Hz                  300                 600", 10, gray,
                      true),
                44, 872, 560, 18),
        kit::at(650, 238, 746, 661).fill(field),
        kit::at(label("FIG. 01 / INVERTED STRUCTURE", 11, chalk, true), 675,
                257, 650, 24),
        kit::at(pyramid(), 704, 283, 620, 560)
            .translateY(motion::bind(
                phase,
                {.envelope = motion::envelope::cosine(), .to = {-5, 5}})),
        kit::at(label("MASS: INDETERMINATE", 10, chalk, true), 674, 843, 350,
                20),
        kit::at(label("ANCHOR  /  0.00", 10, chalk, true), 1178, 843, 205, 20),
        kit::at(label("DO NOT ENTER", 21, red, true), 968, 568, 300, 32)
            .rotate(-90)
            .opacity(motion::bind(
                phase,
                {.envelope = motion::envelope::cosine(), .to = {0.5f, 1}})),
        line(42, 921, 1354),
        kit::at(label("CONTROL / AN INTERPRETIVE STUDY", 10, gray, true), 44,
                935, 700, 18),
        kit::at(label("RECORDS COPY   //   019—A", 10, gray, true), 1100, 935,
                295, 18)};
    for (int mark = 0; mark < 19; ++mark) {
      folio.push_back(kit::at(1369, 321 + mark * 24.0f, mark % 3 ? 7 : 15, 1)
                          .fill(material::hexColor(0xEECBBC, 0.55f)));
    }
    ctx.composer.render(
        positioned().width(1440).height(960).children(std::move(folio)));
  }

  void update(double elapsed) {
    phase = motion::phase(motion::Duration(elapsed), motion::Duration(8));
  }
};

SIGIL_SKETCH(ControlResonance, "Study · Game UI",
             "Control-inspired classified folio with live resonance, "
             "selected-text redactions and suspended concrete")
