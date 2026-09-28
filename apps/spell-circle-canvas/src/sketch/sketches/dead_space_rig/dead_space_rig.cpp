/** An invented engineering station for a mining suit. Projected planes
 *  hold the holographic interface; the body drawing is retained while
 *  health, scan position and live traces share the session clock. */
// TAGS: Studies/Games, Layout/3D, Materials/Shaders, Motion/Bindings,
// Drawing/Geometry

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildraw/Pen.h>
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
using namespace sigil::compose;

namespace {
const auto cyan = material::hexColor(0x9EEBE4);
const auto muted = material::hexColor(0x619998);
const auto edge = material::hexColor(0x366569);
const auto white = material::hexColor(0xD5E3DC);
const auto amber = material::hexColor(0xE8AD64);
const auto ground = material::hexColor(0x101919);

Text readout(std::string value, float size, material::Color color = cyan,
             bool mono = true) {
  static const auto terminal = weave::ports::face({"IBM Plex Mono", "Menlo"});
  static const auto sans =
      weave::ports::face({"DIN Alternate", "Helvetica Neue", "Arial"});
  return text(std::move(value))
      .font({.face = mono ? terminal : sans, .size = size})
      .ink(color);
}

Element divider(float left, float top, float width) {
  return kit::at(left, top, width, 1).fill(edge);
}

Element suit() {
  return pen(
             "rig.engineering-outline",
             [](sigil::draw::Pen& brush) {
               const auto plate = [&](std::initializer_list<glm::vec2> points,
                                      material::Color fill) {
                 brush.fill(fill);
                 brush.stroke(edge);
                 brush.strokeWeight(1.5f);
                 brush.beginShape();
                 for (const auto point : points) brush.vertex(point.x, point.y);
                 brush.endShape(sigil::draw::CLOSE);
               };
               const auto armor = material::hexColor(0x2C4544);
               const auto dark = material::hexColor(0x142A2D);
               plate({{137, 28},
                      {175, 12},
                      {213, 28},
                      {211, 88},
                      {195, 106},
                      {155, 106},
                      {139, 88}},
                     armor);
               plate({{113, 110},
                      {149, 99},
                      {201, 99},
                      {238, 110},
                      {256, 154},
                      {221, 194},
                      {211, 273},
                      {139, 273},
                      {129, 194},
                      {94, 154}},
                     dark);
               plate({{99, 108}, {133, 125}, {119, 174}, {75, 159}, {68, 133}},
                     armor);
               plate(
                   {{251, 108}, {217, 125}, {231, 174}, {275, 159}, {282, 133}},
                   armor);
               plate({{77, 170}, {112, 182}, {102, 233}, {89, 253}, {61, 239}},
                     armor);
               plate(
                   {{273, 170}, {238, 182}, {248, 233}, {261, 253}, {289, 239}},
                   armor);
               plate({{61, 247}, {87, 260}, {81, 315}, {65, 332}, {46, 318}},
                     dark);
               plate(
                   {{289, 247}, {263, 260}, {269, 315}, {285, 332}, {304, 318}},
                   dark);
               plate({{139, 282},
                      {211, 282},
                      {223, 327},
                      {192, 346},
                      {175, 329},
                      {158, 346},
                      {127, 327}},
                     armor);
               plate(
                   {{131, 340}, {162, 351}, {163, 414}, {132, 423}, {122, 391}},
                   dark);
               plate(
                   {{219, 340}, {188, 351}, {187, 414}, {218, 423}, {228, 391}},
                   dark);
               plate(
                   {{130, 430}, {162, 423}, {163, 485}, {152, 512}, {118, 512}},
                   armor);
               plate(
                   {{220, 430}, {188, 423}, {187, 485}, {198, 512}, {232, 512}},
                   armor);
               plate({{117, 516}, {153, 516}, {153, 538}, {99, 538}, {99, 527}},
                     dark);
               plate(
                   {{233, 516}, {197, 516}, {197, 538}, {251, 538}, {251, 527}},
                   dark);
               brush.noFill();
               brush.stroke(muted);
               brush.strokeWeight(1);
               for (int rib = 0; rib < 7; ++rib) {
                 const float top = 146 + rib * 17;
                 brush.line(135 + rib * 0.6f, top, 163, top + 10);
                 brush.line(215 - rib * 0.6f, top, 187, top + 10);
               }
               for (int slat = 0; slat < 4; ++slat) {
                 brush.line(148, 42 + slat * 11.0f, 202, 42 + slat * 11.0f);
               }
               for (int rib = 0; rib < 4; ++rib) {
                 brush.line(130, 451 + rib * 12.0f, 152, 445 + rib * 12.0f);
                 brush.line(220, 451 + rib * 12.0f, 198, 445 + rib * 12.0f);
               }
               brush.stroke(cyan);
               brush.line(161, 34, 189, 34);
               brush.line(157, 93, 193, 93);
               brush.circle(232, 280, 29);
               brush.circle(232, 280, 19);
               brush.stroke(muted);
               brush.rect(157, 122, 36, 151);
               brush.rect(154, 284, 42, 29);
               brush.circle(80, 246, 17);
               brush.circle(270, 246, 17);
               brush.circle(147, 419, 15);
               brush.circle(203, 419, 15);
               brush.bezier(150, 268, 96, 261, 118, 188, 86, 171);
               brush.bezier(200, 268, 254, 261, 232, 188, 264, 171);
               for (int seam = 0; seam < 3; ++seam) {
                 brush.line(126 + seam * 7, 355, 136 + seam * 7, 397);
                 brush.line(224 - seam * 7, 355, 214 - seam * 7, 397);
                 brush.line(80, 184 + seam * 11, 101, 191 + seam * 11);
                 brush.line(270, 184 + seam * 11, 249, 191 + seam * 11);
               }
               brush.stroke(amber);
               brush.line(80, 129, 96, 119);
               brush.line(83, 136, 102, 123);
               brush.line(86, 143, 108, 128);
               brush.stroke(edge);
               brush.line(77, 139, 35, 112);
               brush.line(35, 112, 0, 112);
               brush.line(206, 97, 274, 63);
               brush.line(274, 63, 337, 63);
               brush.line(240, 280, 300, 301);
               brush.line(300, 301, 345, 301);
               brush.stroke(material::hexColor(0x74AAA6, 0.35f));
               brush.line(175, 0, 175, 552);
               brush.line(25, 277, 326, 277);
               for (int mark = 0; mark < 23; ++mark) {
                 brush.line(20, 15 + mark * 24.0f, mark % 4 == 0 ? 34 : 27,
                            15 + mark * 24.0f);
               }
             },
             Cache::Picture)
      .width(350)
      .height(552);
}

Element telemetry() {
  return pen("rig.vitals",
             [](sigil::draw::Pen& brush) {
               const float time = float(brush.millis() * 0.001);
               brush.noFill();
               brush.strokeWeight(1.4f);
               for (int channel = 0; channel < 2; ++channel) {
                 brush.stroke(channel == 0 ? cyan : muted);
                 brush.beginShape();
                 for (int position = 0; position < 254; position += 2) {
                   const float phase =
                       std::fmod(position / 92.0f + time * 0.65f, 1.0f);
                   const float beat =
                       std::exp(-std::pow((phase - 0.5f) * 31, 2)) * 28 -
                       std::exp(-std::pow((phase - 0.59f) * 25, 2)) * 16;
                   const float wave =
                       channel == 0 ? beat
                                    : std::sin(position * 0.035f + time) * 8;
                   brush.vertex(float(position), 33 + channel * 44 - wave);
                 }
                 brush.endShape();
               }
             })
      .width(254)
      .height(106);
}

Element cutter() {
  return pen(
             "rig.cutter",
             [](sigil::draw::Pen& brush) {
               brush.noFill();
               brush.stroke(muted);
               brush.strokeWeight(1.5f);
               brush.rect(48, 18, 114, 32);
               brush.rect(155, 7, 30, 54);
               brush.rect(27, 26, 23, 17);
               brush.line(55, 50, 65, 82);
               brush.line(85, 50, 83, 82);
               brush.line(65, 82, 83, 82);
               brush.stroke(cyan);
               for (int emitter = 0; emitter < 3; ++emitter) {
                 brush.line(185, 13 + emitter * 21, 231, 13 + emitter * 21);
               }
               brush.line(62, 30, 143, 30);
               brush.line(65, 39, 120, 39);
             },
             Cache::Picture)
      .width(260)
      .height(92);
}
}  // namespace

struct DeadSpaceRig {
  motion::Animatable<float> phase = motion::animatable(0.0f);

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 960);
    ctx.background(ground);
    ctx.captureAt(4.2);
    const auto holo =
        material::shader(ctx.assets.hub(), ctx.local("hologram.sksl"));
    std::vector<Element> instruments{
        kit::at(0, 0, 950, 701).fill(holo).cache(Cache::Texture),
        kit::at(0, 0, 950, 28)
            .fill(
                material::linearGradient({0, 0}, {0, 1},
                                         {material::hexColor(0xA5F4EA, 0),
                                          material::hexColor(0xA5F4EA, 0.018f),
                                          material::hexColor(0xA5F4EA, 0)}))
            .cache(Cache::Texture)
            .translateY(motion::bind(phase, {.to = {0, 673}})),
        kit::at(readout("RESOURCE INTEGRATION GEAR", 12), 26, 21, 650, 22),
        kit::at(readout("SUIT STATUS / LEVEL 03", 24, white, false), 24, 49,
                750, 38),
        divider(26, 102, 884),
        kit::at(suit(), 283, 112, 350, 552),
        kit::at(readout("REAR ELEVATION", 10, muted), 374, 665, 300, 18),
        kit::at(readout("01", 9, amber), 287, 203, 40, 18),
        kit::at(readout("02", 9, muted), 594, 156, 40, 18),
        kit::at(readout("03", 9, muted), 594, 394, 40, 18),
        kit::at(readout("VITALS", 13), 26, 131, 230, 23),
        kit::at(readout("82", 55, white, false), 24, 163, 120, 72),
        kit::at(readout("BPM\nSTABLE", 11, muted), 112, 186, 130, 42),
        kit::at(telemetry(), 26, 252, 254, 106),
        divider(26, 373, 229),
        kit::at(readout("OXYGEN RESERVE", 11, muted), 26, 397, 255, 22),
        kit::at(readout("148", 45, white, false), 24, 424, 130, 61),
        kit::at(readout("SECONDS", 10, muted), 133, 456, 118, 18),
        kit::at(readout("PRESSURE       101 kPa\nSEAL INTEGRITY   "
                        "98.4%\nTHERMAL LOAD      0.62",
                        10, muted),
                26, 527, 261, 66),
        kit::at(readout("ARMOR SYSTEMS", 13), 678, 131, 225, 23),
        kit::at(readout("RIG SPINE\nNEURAL LINK ACTIVE", 11), 678, 174, 235,
                45),
        divider(678, 235, 229),
        kit::at(readout("STASIS", 13), 678, 259, 235, 24),
        kit::at(readout("72%", 44, white, false), 676, 291, 232, 60),
        kit::at(readout("MODULE SYNC  /  READY", 10, muted), 678, 365, 240, 23),
        divider(678, 401, 229),
        kit::at(readout("KINESIS", 13), 678, 422, 235, 23),
        kit::at(readout("CALIBRATED", 23, white, false), 676, 459, 246, 36),
        kit::at(
            readout("WORKING RANGE   18 m\nMAGNETIC LOCK   0.91", 10, muted),
            678, 515, 239, 46),
        kit::at(readout("SCAN / MUSCULOSKELETAL", 10), 26, 641, 300, 20)};
    for (int segment = 0; segment < 8; ++segment) {
      instruments.push_back(
          kit::at(450, 237 + segment * 17.0f, 17, 12)
              .fill(segment < 6 ? cyan : edge)
              .opacity(motion::bind(
                  phase,
                  {.envelope = motion::envelope::cosine(), .to = {0.66f, 1}})));
    }
    for (int segment = 0; segment < 12; ++segment) {
      instruments.push_back(kit::at(678 + segment * 19.0f, 345, 13, 5)
                                .fill(segment < 9 ? cyan : edge));
      instruments.push_back(kit::at(26 + segment * 19.0f, 497, 13, 5)
                                .fill(segment < 11 ? cyan : edge));
    }
    instruments.push_back(
        kit::at(322, 162, 274, 1)
            .fill(material::hexColor(0xBEFFF5, 0.6f))
            .translateY(motion::bind(phase, {.to = {0, 460}})));
    auto main = kit::at(positioned(), 63, 185, 950, 701)
                    .foreground(stroke(
                        1, Fill::color(material::hexColor(0x6DB9B9, 0.6f))))
                    .rotateY(motion::bind(
                        phase, {.envelope = motion::envelope::cosine(),
                                .to = {-6.5f, -4.0f}}))
                    .rotateX(1.6f)
                    .translateZ(12)
                    .children(std::move(instruments));

    std::vector<Element> scene{
        kit::at(0, 0, 1440, 960)
            .fill(material::linearGradient(
                {0, 0}, {1, 1},
                {material::hexColor(0x27332F), material::hexColor(0x090F12)}))
            .cache(Cache::Texture)
            .translateZ(-80),
        kit::at(readout("CEC / ENGINEERING SYSTEMS", 12, muted), 50, 34, 820,
                24),
        kit::at(readout("RIG", 86, white, false), 43, 65, 230, 108),
        kit::at(
            readout("PERSONNEL DIAGNOSTICS\nUSG ISHIMURA / DECK 04", 14, muted),
            250, 94, 580, 51),
        kit::at(readout("LOCAL CONNECTION\nSIGNAL / SECURE", 12, cyan), 1140,
                49, 260, 47),
        std::move(main),
        kit::at(positioned(), 1080, 214, 307, 640)
            .fill(material::hexColor(0x12292B, 0.93f))
            .foreground(stroke(1, Fill::color(edge)))
            .cache(Cache::Texture)
            .rotateY(-3)
            .translateZ(22)
            .children(
                {kit::at(readout("EQUIPMENT / 01", 12), 20, 20, 273, 26),
                 divider(20, 58, 265),
                 kit::at(readout("PLASMA CUTTER", 25, white, false), 19, 79,
                         270, 40),
                 kit::at(cutter(), 20, 135, 260, 92),
                 kit::at(readout("ENERGY", 10, muted), 20, 244, 125, 20),
                 kit::at(readout("010 / 025", 25), 20, 272, 265, 37),
                 divider(20, 329, 265),
                 kit::at(readout("POWER ROUTING", 12), 20, 353, 266, 22),
                 kit::at(readout("RIG CORE         ONLINE\nSTASIS CELL      "
                                 "ONLINE\nBEACON           SILENT",
                                 10, muted),
                         20, 396, 276, 70),
                 divider(20, 481, 265),
                 kit::at(readout("SERVICE ADVISORY", 12, amber), 20, 503, 270,
                         24),
                 kit::at(
                     readout("LEFT PAULDRON / ABRASION\nINSPECT AT NEXT BENCH",
                             10, amber),
                     20, 542, 275, 43)}),
        divider(48, 917, 1340),
        kit::at(readout("DEAD SPACE / AN INTERPRETIVE STUDY", 10, muted), 48,
                934, 700, 18),
        kit::at(readout("MAINTENANCE MODE   /   04.12", 10, muted), 1110, 934,
                290, 18)};
    ctx.composer.render(positioned()
                            .width(1440)
                            .height(960)
                            .perspective(1800)
                            .preserve3d()
                            .children(std::move(scene)));
  }

  void update(double elapsed) {
    phase = motion::phase(motion::Duration(elapsed), motion::Duration(9));
  }
};

SIGIL_SKETCH(DeadSpaceRig, "Study · Game UI",
             "Dead Space-inspired suit diagnostics with projected holographic "
             "planes, live vitals and engineering schematics")
