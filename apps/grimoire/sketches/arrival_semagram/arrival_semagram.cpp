/** An invented contact-language workstation: the ink is a retained drawing,
 *  its suspension is a live material mask, and annotations share the
 *  contour's polar frame. The fragments describe visual hypotheses rather
 *  than assigning translations to the film's language. */
// TAGS: Studies/Film, Drawing/Generative, Materials/Masks, Typography/Lettering

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cmath>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace arrange = sigil::geometry::arrange;
using namespace sigil::compose;

namespace {
const auto paper = material::hexColor(0xE7E7DC);
const auto ink = material::hexColor(0x242C2A);
const auto quiet = material::hexColor(0x65716B);
const auto moss = material::hexColor(0x5B7260);
const auto ruleInk = material::hexColor(0xACB4A8);

weave::Face face(bool terminal = false) {
  static const auto sans = weave::ports::face({"Helvetica Neue", "Arial"});
  static const auto mono = weave::ports::face({"IBM Plex Mono", "Menlo"});
  return terminal ? mono : sans;
}

Text words(std::string value, float size, material::Color color = ink,
           bool terminal = false) {
  return text(std::move(value))
      .font({.face = face(terminal), .size = size})
      .ink(color);
}

Element rule(float left, float top, float width) {
  return kit::at(left, top, width, 1).fill(ruleInk);
}

/** Ink deposited around a closed sentence, with local branches and dry
 *  edges. The pen supplies seeded noise and randomness; the geometry
 *  library supplies every polar placement. */
Element semagram(float size, uint32_t seed) {
  return pen(
             "semagram." + std::to_string(seed) + "." + std::to_string(size),
             [size, seed](sigil::draw::Pen& brush) {
               brush.randomSeed(seed);
               brush.noiseSeed(seed);
               const glm::vec2 center{size * 0.5f, size * 0.5f};
               const float radius = size * 0.315f;
               const auto point = [&](float angle, float offset) {
                 return arrange::onEllipse(
                     center, {radius + offset, radius + offset}, angle);
               };
               brush.noFill();
               for (int strand = 0; strand < 32; ++strand) {
                 brush.stroke(material::Color{0.075f, 0.095f, 0.09f,
                                              strand < 8 ? 0.74f : 0.19f});
                 brush.strokeWeight(size * (strand < 8 ? 0.013f : 0.0012f));
                 brush.beginShape();
                 for (int step = 0; step <= 360; ++step) {
                   const float angle = float(step) * 6.2831853f / 360.0f;
                   const float edge =
                       (brush.noise(std::cos(angle) * 1.7f + 4,
                                    std::sin(angle) * 1.7f + 4, strand * 0.1f) -
                        0.5f);
                   const float swelling =
                       0.7f + 0.3f * std::sin(angle * 5 + 0.4f);
                   const glm::vec2 at =
                       point(angle, edge * size * 0.15f * swelling +
                                        (strand - 16) * size * 0.0007f);
                   brush.vertex(at.x, at.y);
                 }
                 brush.endShape();
               }
               brush.noStroke();
               for (int deposit = 0; deposit < 780; ++deposit) {
                 const float angle = brush.random(6.2831853f);
                 const float edge =
                     brush.noise(std::cos(angle) * 1.7f + 4,
                                 std::sin(angle) * 1.7f + 4, 0.4f) -
                     0.5f;
                 const float swelling =
                     0.7f + 0.3f * std::sin(angle * 5 + 0.4f);
                 const float spread = brush.random(-0.018f, 0.018f) * size;
                 const auto at =
                     point(angle, edge * size * 0.15f * swelling + spread);
                 brush.fill(material::Color{0.07f, 0.09f, 0.085f,
                                            brush.random(0.10f, 0.7f)});
                 brush.circle(at.x, at.y,
                              brush.random(0.4f, 3.0f) * size / 500);
               }
               brush.noFill();
               for (int branch = 0; branch < 240; ++branch) {
                 const float angle = brush.random(6.2831853f);
                 const float bunch =
                     std::pow(std::max(0.0f, std::sin(angle * 7 + seed)), 3);
                 const float length =
                     size * (0.012f + 0.11f * bunch) * brush.random(0.3f, 1);
                 const float side = branch % 4 == 0 ? -1.0f : 1.0f;
                 const auto start =
                     point(angle, brush.random(-size * 0.015f, size * 0.015f));
                 const auto end =
                     point(angle + brush.random(-0.09f, 0.09f), length * side);
                 const auto control =
                     point(angle + side * 0.045f, length * side * 0.65f);
                 brush.stroke(material::Color{0.10f, 0.12f, 0.11f,
                                              brush.random(0.15f, 0.7f)});
                 brush.strokeWeight(brush.random(0.35f, 2.2f) * size / 500);
                 brush.bezier(start.x, start.y, control.x, control.y, end.x - 4,
                              end.y + 4, end.x, end.y);
                 if (branch % 3 == 0) {
                   brush.noStroke();
                   brush.fill(material::Color{0.08f, 0.10f, 0.09f, 0.45f});
                   brush.circle(end.x, end.y,
                                brush.random(0.8f, 3.5f) * size / 500);
                   brush.noFill();
                 }
               }
             },
             Cache::Texture)
      .width(size)
      .height(size);
}

Element spectralTrace() {
  return pen(
             "semagram.spectrum",
             [](sigil::draw::Pen& brush) {
               brush.noFill();
               for (int lane = 0; lane < 3; ++lane) {
                 brush.stroke(
                     material::Color{0.28f, 0.37f, 0.30f, 0.8f - lane * 0.2f});
                 brush.strokeWeight(1.2f);
                 brush.beginShape();
                 for (int sample = 0; sample < 420; ++sample) {
                   const float phase = sample * 0.021f;
                   const float envelope =
                       std::pow(std::sin(sample * 0.00748f), 2);
                   const float displacement = std::sin(phase * (9 + lane * 3)) *
                                              std::sin(phase * 1.3f + lane) *
                                              envelope;
                   brush.vertex(float(sample),
                                40 + displacement * (29 - lane * 5));
                 }
                 brush.endShape();
               }
             },
             Cache::Picture)
      .width(420)
      .height(80);
}
}  // namespace

struct ArrivalSemagram {
  motion::Animatable<float> sweep = motion::animatable(0.0f);

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 960);
    ctx.background(paper);
    ctx.captureAt(4.8);
    const auto suspended =
        material::shader(ctx.assets.hub(), ctx.local("suspension.sksl"),
                         {.textures = {{"uNoise", {}}}})
            .slot("uNoise",
                  material::noise(0.013f, {.octaves = 1, .seed = 17}));
    std::vector<Element> elements{
        kit::at(0, 0, 1440, 960)
            .fill(material::from(paper).layer(
                material::noise(0.72f, {.seed = 4, .grain = true}),
                {.blend = material::BlendMode::Multiply, .opacity = 0.06f})),
        kit::at(words("XENOLINGUISTICS / FIELD STATION 12", 12, moss, true), 44,
                30, 700, 20),
        kit::at(words("SEMAGRAM", 68), 40, 57, 800, 85),
        kit::at(words("CONTACT  /  012\nOBSERVATION WINDOW\nTEMPORAL ORDER: "
                      "UNRESOLVED",
                      12, quiet, true),
                1030, 48, 360, 76),
        rule(44, 150, 1352),
        kit::at(44, 178, 826, 554)
            .fill(material::linearGradient(
                {0, 0}, {1, 1},
                {material::hexColor(0xD6DDD3), material::hexColor(0xF3F3E9)})),
        kit::at(positioned(), 160, 153, 590, 590)
            .mask(by::alpha(suspended))
            .children({semagram(590, 47)}),
        kit::at(words("A / SUSPENDED INK", 11, quiet, true), 62, 194, 280, 20),
        kit::at(
            words("OPTICAL DEPTH  0.83\nPARTICLE FIELD  HELD", 10, quiet, true),
            62, 666, 240, 40),
        kit::at(words("THE SENTENCE\nHAS NO FIRST WORD.", 30), 920, 181, 470,
                84),
        kit::at(words("01  /  CONTINUITY", 12, moss, true), 920, 296, 450, 20),
        kit::at(words("A closed contour carries the entire utterance.\nThe "
                      "boundary is read as a relationship,\nnot as a line of "
                      "separate symbols.",
                      19, quiet),
                920, 325, 455, 92),
        rule(920, 436, 476),
        kit::at(words("02  /  PRESSURE & INTENT", 12, moss, true), 920, 460,
                450, 20),
        kit::at(words("Weight, branching and empty space remain\nindependent "
                      "observations. A repeated form\nis evidence, not a "
                      "translation.",
                      19, quiet),
                920, 488, 455, 90),
        kit::at(spectralTrace(), 920, 610, 420, 80),
        kit::at(words("ACOUSTIC CORRELATION     0.37 / 1.00", 10, quiet, true),
                920, 704, 475, 20),
        rule(44, 759, 1352),
        kit::at(words("MORPHOLOGICAL NEIGHBOURS", 11, moss, true), 44, 779, 820,
                20),
        kit::at(words("Meaning may precede\nthe order of reading.", 29), 920,
                800, 470, 82),
        rule(44, 919, 1352),
        kit::at(words("ARRIVAL / AN INTERPRETIVE STUDY", 10, quiet, true), 44,
                933, 800, 18),
        kit::at(words("INK / MEMORY / NONLINEAR TIME", 10, quiet, true), 1090,
                933, 306, 18)};
    for (int index = 0; index < 4; ++index) {
      const float left = 44 + index * 210.0f;
      elements.push_back(
          kit::at(semagram(118, 21 + index * 13), left - 8, 795, 118, 118));
      elements.push_back(
          kit::at(words("0" + std::to_string(index + 1), 24, moss, true),
                  left + 113, 815, 80, 32));
      const char* names[] = {"OPEN\nBRANCH", "DOUBLE\nRETURN", "DENSE\nINTENT",
                             "QUIET\nEDGE"};
      elements.push_back(kit::at(words(names[index], 10, quiet, true),
                                 left + 114, 854, 88, 36));
    }
    const arrange::Ring ring{{455, 448}, {239, 239}};
    for (int index = 0; index < 60; ++index) {
      const auto at = arrange::onRing(index, 60, ring);
      elements.push_back(kit::at(at.x, at.y, 1, index % 5 == 0 ? 8 : 3)
                             .fill(ruleInk)
                             .rotate(index * 6.0f));
    }
    elements.push_back(kit::at(78, 246, 755, 1)
                           .fill(material::hexColor(0x668474, 0.30f))
                           .translateY(motion::bind(sweep, {.to = {0, 380}})));
    elements.push_back(kit::at(words("03 / RETURN VECTOR", 10, moss, true), 626,
                               609, 220, 20));
    elements.push_back(
        kit::at(words("01 / ENTRY", 10, moss, true), 82, 297, 200, 20));
    ctx.composer.render(
        positioned().width(1440).height(960).children(std::move(elements)));
  }

  void update(double elapsed) {
    sweep = motion::phase(motion::Duration(elapsed), motion::Duration(9));
  }
};

SIGIL_SKETCH(ArrivalSemagram, "Study · Film",
             "Arrival-inspired ink suspension, polar annotations and a "
             "nonlinear language workstation")
