// vertigo_titles — the title sequence Saul Bass designed for Alfred
// Hitchcock's VERTIGO (Paramount, 1958, 1.85:1), with the spirals John
// Whitney drew for it: an eye, a red stain, the title opening out of the
// pupil, and Lissajous figures traced by a point of light on black.
//
// THE RECORD, read for this study:
//   * artofthetitle.com/title/vertigo — Bass's design, Whitney's spirals,
//     Herrmann's score, Paramount, 1.85:1; the spirals called Lissajous
//     figures.
//   * typotheque.com, Emily King, "Taking Credit: Film Title Sequences
//     1955–1965", chapter 5 on Vertigo — the order (a face, the eyes, "the
//     screen is suddenly stained red", the title expanding out of the
//     pupil); the two type registers: display titles in outline capitals
//     through which the image shows, body credits in solid capitals of the
//     same serif face; a score only loosely synchronised with the cuts.
//   * patrycerichter.wordpress.com (2016), a shot breakdown — the spirals
//     in cool tones, blue and purple, and warm tones, orange and red.
//   * hitchcocksvertigo.substack.com "Saul Bass and John Whitney", with
//     rhizome.org and diyphotography.net — Whitney's rig: a pendulum over a
//     turning plate, built on a converted WWII M-5 anti-aircraft gun
//     director; the curves are Lissajous's parametric equations.
//   * fontsinuse.com "Vertigo Opening Titles" — the face is Clarendon; this
//     machine's SuperClarendon is Apple's cut of it.
//
// THIS STUDY'S OWN: the drawn eye stands for the photographed one; every
// frequency ratio, damping, turn and colour in data/figures.csv; every
// time in data/figures.csv and data/credits.csv; the grain.
//
// THE RIG, translated. The pendulum swings in two harmonic axes and its pen
// lays ink on a plate that turns underneath it:
//
//     pendulum(s) = R · envelope(s) · (sin(a·s + phase), sin(b·s))
//     laid(s)     = rotate(−precession · s) · pendulum(s)   on the plate
//     plate(t)    = rotate(+precession · s(t))              as seen
//
// so each figure is a canvas that is KEPT (`graphics`) — every frame lays
// only the stretch of curve drawn since the last, in the plate's frame —
// turned by one bound rotation, while the point of light rides the
// pendulum itself. The plate keeps turning after the pen lifts, as
// Whitney's did under the camera.

// TAGS: Motion/Trajectories

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigildata/table/Table.h>
#include <sigildraw/Color.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Face.h>
#include <sigilweave/style/Style.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkColorFilter.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace data = sigil::data;
namespace draw = sigil::draw;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;
using material::Color;
using material::hexColor;

namespace {

// The frame is the film's own 1.85:1, and the pupil stands at its centre.
constexpr float kWidth = 1480;
constexpr float kHeight = 800;
constexpr glm::vec2 kPupil{kWidth / 2, kHeight / 2};
constexpr float kIrisRadius = 232;
constexpr float kPupilRadius = 64;
/** The whole sequence, after which it begins again. */
constexpr float kLoopLength = 32;
/** How finely each turn of the pendulum is laid down. */
constexpr int kSamplesPerTurn = 900;

/** A DUOTONE: every value in the eye is a luminance, and the tint is what
 *  a luminance is printed in — so the red stain is the same eye in
 *  another ink rather than a colour laid over it. */
struct Tint {
  Color shadow, middle, light;
  Color at(float luminance, float alpha = 1) const {
    const Color colour = luminance < 0.5f
                             ? material::mixLinear(shadow, middle, luminance * 2)
                             : material::mixLinear(middle, light, luminance * 2 - 1);
    return material::withAlpha(colour, alpha);
  }
};
constexpr Tint kGrey{hexColor(0x060708), hexColor(0x6d6f6a), hexColor(0xe8e6de)};
constexpr Tint kStained{hexColor(0x120003), hexColor(0xb0121e), hexColor(0xff9a7e)};

/** One of Whitney's figures: the pendulum's ratios, how it dies away and
 *  how fast the plate turns under it, and when it is drawn. */
struct Figure {
  Color ink, glow;
  float a = 3, b = 2, phaseDegrees = 0;
  float damping = 0.03f;
  /** How long the swing takes to build from rest, in pendulum time; zero
   *  starts it at full swing. The first figure opens out of the pupil. */
  float rise = 0;
  /** Plate turn per unit of pendulum time, in radians. */
  float precession = 0.1f;
  float turns = 3;
  float radius = 280;
  float start = 0, drawn = 5, end = 10;

  float length() const { return turns * 2 * std::numbers::pi_v<float>; }
  int samples() const { return (int)(turns * kSamplesPerTurn); }
  /** Where the pen is in pendulum time at @p seconds into the loop. */
  float pendulumTime(float seconds) const {
    return std::clamp((seconds - start) / drawn, 0.0f, 1.0f) * length();
  }
  /** The plate's turn, in degrees per second — constant, since a motor
   *  does not ease. */
  float plateDegreesPerSecond() const {
    return precession * length() / drawn * 180 / std::numbers::pi_v<float>;
  }
  glm::vec2 pendulum(float time) const {
    float envelope = std::exp(-damping * time);
    if (rise > 0) envelope *= 1 - std::exp(-time / rise);
    const float phase = phaseDegrees * std::numbers::pi_v<float> / 180;
    return radius * envelope * glm::vec2{std::sin(a * time + phase), std::sin(b * time)};
  }
  /** Where the ink lands on the plate, which has turned by then. */
  glm::vec2 laid(float time) const {
    const glm::vec2 swing = pendulum(time);
    const float turn = -precession * time;
    return {swing.x * std::cos(turn) - swing.y * std::sin(turn),
            swing.x * std::sin(turn) + swing.y * std::cos(turn)};
  }
  /** The plate's side: the swing reaches its corners, R·√2, on any turn. */
  float plateSize() const { return 2 * (radius * std::numbers::sqrt2_v<float> + 12); }
};

/** One credit card of the sequence, in one of its two registers. */
struct Credit {
  std::string words;
  bool outline = false;
  float size = 24;
  glm::vec2 at = kPupil;
  Color ink;
  float start = 0, end = 1;
  /** Seconds the card takes to open out of the pupil; zero fades it in. */
  float grow = 0;
};

glm::vec2 cubic(glm::vec2 from, glm::vec2 pull, glm::vec2 push, glm::vec2 to, float along) {
  const float rest = 1 - along;
  return rest * rest * rest * from + 3 * rest * rest * along * pull +
         3 * rest * along * along * push + along * along * along * to;
}

// The eye's aperture: the corners and the lids' pulls.
constexpr glm::vec2 kInnerCorner{150, 444};
constexpr glm::vec2 kOuterCorner{1330, 384};
constexpr glm::vec2 kUpperPull{380, 116}, kUpperPush{1066, 86};
constexpr glm::vec2 kLowerPull{1040, 684}, kLowerPush{420, 700};

glm::vec2 upperLid(float along) { return cubic(kInnerCorner, kUpperPull, kUpperPush, kOuterCorner, along); }
glm::vec2 lowerLid(float along) { return cubic(kOuterCorner, kLowerPull, kLowerPush, kInnerCorner, along); }

void aperture(draw::Pen& pen) {
  pen.beginShape();
  pen.vertex(kInnerCorner.x, kInnerCorner.y);
  pen.bezierVertex(kUpperPull.x, kUpperPull.y, kUpperPush.x, kUpperPush.y, kOuterCorner.x, kOuterCorner.y);
  pen.bezierVertex(kLowerPull.x, kLowerPull.y, kLowerPush.x, kLowerPush.y, kInnerCorner.x, kInnerCorner.y);
  pen.endShape(draw::CLOSE);
}

void lidCurve(draw::Pen& pen, glm::vec2 (*lid)(float), float lift = 0) {
  pen.beginShape();
  for (int step = 0; step <= 64; ++step) {
    const glm::vec2 at = lid((float)step / 64);
    pen.vertex(at.x, at.y - lift);
  }
  pen.endShape();
}

/** THE EYE, in one tint: skin in shadow, the lids and lashes, the white,
 *  and an iris of radial fibres round a pupil with a window in it. */
void drawEye(draw::Pen& pen, const Tint& tint) {
  pen.noStroke();
  pen.fill(material::radialGradient(kPupil, 820,
                                    {{0.00f, tint.at(0.30f)},
                                     {0.55f, tint.at(0.17f)},
                                     {1.00f, tint.at(0.03f)}},
                                    {.units = material::GradientUnits::Pixels}));
  pen.rect(0, 0, kWidth, kHeight);

  // The crease and the brow's shadow above it.
  pen.noFill();
  for (int band = 0; band < 20; ++band) {
    pen.stroke(tint.at(0.05f, 0.035f));
    pen.strokeWeight(10.0f + band * 6);
    lidCurve(pen, upperLid, 112 + band * 1.5f);
  }
  pen.stroke(tint.at(0.08f, 0.7f));
  pen.strokeWeight(3);
  lidCurve(pen, upperLid, 96);

  pen.push();
  pen.clip([&] { aperture(pen); });
  pen.noStroke();
  pen.fill(material::radialGradient(kPupil, 640,
                                    {{0.00f, tint.at(0.86f)},
                                     {0.60f, tint.at(0.66f)},
                                     {1.00f, tint.at(0.34f)}},
                                    {.units = material::GradientUnits::Pixels}));
  pen.rect(0, 0, kWidth, kHeight);

  // The iris: a ramp from the pupil's dark collar to a pale inner ring and
  // out to the limbus, crossed by its fibres.
  std::vector<material::ColorStop> fibres;
  for (int fibre = 0; fibre <= 120; ++fibre) {
    const float jitter = 0.015f * sigil::core::noise::hash(29u, (uint32_t)fibre);
    const float value = (fibre % 2 == 0 ? 0.44f : 0.56f) + jitter;
    fibres.push_back({(float)fibre / 120, {value, value, value, 1}});
  }
  pen.fill(material::from(material::radialGradient(
                              kPupil, kIrisRadius,
                              {{0.00f, tint.at(0.04f)},
                               {0.27f, tint.at(0.10f)},
                               {0.33f, tint.at(0.62f)},
                               {0.55f, tint.at(0.46f)},
                               {0.86f, tint.at(0.38f)},
                               {1.00f, tint.at(0.12f)}},
                              {.units = material::GradientUnits::Pixels}))
               .layer(material::conicGradient(kPupil, fibres,
                                              {.units = material::GradientUnits::Pixels}),
                      {.blend = material::BlendMode::SoftLight}));
  pen.circle(kPupil.x, kPupil.y, kIrisRadius * 2);

  // The collarette: a ragged ring where the fibres gather.
  pen.noFill();
  pen.stroke(tint.at(0.12f, 0.55f));
  pen.strokeWeight(5);
  pen.beginShape();
  for (int step = 0; step < 180; ++step) {
    const float angle = (float)step / 180 * 2 * std::numbers::pi_v<float>;
    const float reach = 108 + 6 * sigil::core::noise::hash(7u, (uint32_t)step);
    pen.vertex(kPupil.x + reach * std::cos(angle), kPupil.y + reach * std::sin(angle));
  }
  pen.endShape(draw::CLOSE);
  pen.stroke(tint.at(0.05f, 0.85f));
  pen.strokeWeight(12);
  pen.circle(kPupil.x, kPupil.y, kIrisRadius * 2 - 6);

  pen.noStroke();
  pen.fill(tint.at(0.0f));
  pen.circle(kPupil.x, kPupil.y, kPupilRadius * 2);

  // The upper lid's shadow on the eye, then the window caught in it.
  pen.noFill();
  for (int band = 0; band < 24; ++band) {
    pen.stroke(tint.at(0.02f, 0.05f));
    pen.strokeWeight(8.0f + band * 7);
    lidCurve(pen, upperLid);
  }
  constexpr glm::vec2 kWindow{kPupil.x + 78, kPupil.y - 70};
  pen.noStroke();
  pen.fill(material::radialGradient(kWindow, 46,
                                    {{0.00f, tint.at(1.0f, 0.95f)},
                                     {0.30f, tint.at(1.0f, 0.55f)},
                                     {1.00f, tint.at(1.0f, 0.0f)}},
                                    {.units = material::GradientUnits::Pixels}));
  pen.circle(kWindow.x, kWindow.y, 92);
  pen.pop();

  // The lid rims, the wet line under the eye and the lashes.
  pen.noFill();
  pen.stroke(tint.at(0.04f));
  pen.strokeWeight(6);
  lidCurve(pen, upperLid);
  pen.stroke(tint.at(0.62f, 0.8f));
  pen.strokeWeight(2.5f);
  lidCurve(pen, lowerLid);
  pen.stroke(tint.at(0.03f, 0.95f));
  pen.strokeCap(draw::ROUND);
  // A lash is a curl that thins to its tip: segments of falling weight.
  const auto lash = [&](glm::vec2 root, glm::vec2 lean, glm::vec2 curl, float weight) {
    glm::vec2 last = root;
    for (int step = 1; step <= 10; ++step) {
      const float along = step / 10.0f;
      const glm::vec2 at = root + lean * along + curl * along * along;
      pen.strokeWeight(weight * (1.1f - along));
      pen.line(last.x, last.y, at.x, at.y);
      last = at;
    }
  };
  for (int count = 0; count < 150; ++count) {
    const float scatter = sigil::core::noise::hash(3u, (uint32_t)count);
    const float along = 0.05f + 0.92f * (count + scatter) / 150.0f;
    const glm::vec2 root = upperLid(along);
    const float temple = 0.2f + 0.8f * along;  // lashes lean toward the temple
    const float length = (26 + 46 * std::sin(along * std::numbers::pi_v<float>)) *
                         (0.85f + 0.25f * sigil::core::noise::hash(4u, (uint32_t)count));
    lash(root, {(scatter - 0.3f) * 14, -length * 0.55f},
         {length * 0.7f * temple, -length * 0.25f}, 2.6f);
  }
  for (int count = 0; count < 44; ++count) {
    const float scatter = sigil::core::noise::hash(5u, (uint32_t)count);
    const float along = 0.10f + 0.8f * (count + scatter) / 44.0f;
    const glm::vec2 root = lowerLid(along) + glm::vec2{0, 3};
    const float length = 18 + 8 * scatter;
    lash(root, {-3, length * 0.8f}, {-length * 0.4f * (1 - along), length * 0.2f}, 1.5f);
  }
}

/** The outline register: the capitals' contour carries the ink, with a
 *  soft dark underlay so it holds over a bright field. */
material::Material hollowInk(Color colour, float width) {
  const auto underlay = material::Filter::dilate(1.5f)
                            .then(material::skia::filter(SkColorFilters::Blend(
                                SkColorSetARGB(0x70, 0, 0, 0), SkBlendMode::kSrcIn)))
                            .then(material::Filter::blur(3.0f));
  return material::Material{Color{0, 0, 0, 0}}.effects(
      material::Filter::stroke(colour, {.width = width, .position = material::StrokePosition::Center})
          .then(material::Filter{}.emit(underlay, material::BlendMode::DestinationOver)));
}

}  // namespace

struct VertigoTitles {
  std::vector<Figure> figures;
  std::vector<Credit> credits;
  weave::Face clarendon, clarendonBody;

  /** Seconds into the loop: every card, fade and turn is a binding of it. */
  motion::Animatable<float> seconds = motion::animatable(0.0f);
  /** The point of light: where the pendulum hangs, and how bright. */
  motion::Animatable<float> lightX = motion::animatable(kPupil.x);
  motion::Animatable<float> lightY = motion::animatable(kPupil.y);
  motion::Animatable<float> lightLevel = motion::animatable(0.0f);
  /** Where the grain sheet stands this film frame. */
  motion::Animatable<float> grainX = motion::animatable(0.0f);
  motion::Animatable<float> grainY = motion::animatable(0.0f);
  /** How many samples of each figure are on its plate already. */
  std::vector<int> laid;

  /** On from @p from to @p to, fading in over @p rise and out over @p fall. */
  motion::Animatable<float> during(float from, float to, float rise, float fall) const {
    const float span = to - from;
    return motion::bind(seconds, {.from = {from, to},
                                  .clampFrom = true,
                                  .envelope = motion::envelope::trapezoid(
                                      0, rise / span, 1 - fall / span, 1)});
  }

  /** THE PLATE, run each frame: lay down the curve drawn since the last run
   *  — a wide faint pass for the light's spread and a fine bright one for
   *  the line, both added, so where the figure crosses itself it burns. */
  void layPlate(draw::Pen& pen, size_t index) {
    const Figure& figure = figures[index];
    const int total = figure.samples();
    const int reached = (int)std::floor(
        figure.pendulumTime(seconds.value()) / figure.length() * (float)total);
    if (reached < laid[index]) {
      pen.clear();
      laid[index] = 0;
    }
    if (reached <= laid[index]) return;
    const glm::vec2 centre{pen.width / 2, pen.height / 2};
    const auto stretch = [&] {
      pen.beginShape();
      for (int sample = laid[index]; sample <= reached; ++sample) {
        const glm::vec2 at = centre + figure.laid(figure.length() * (float)sample / (float)total);
        pen.vertex(at.x, at.y);
      }
      pen.endShape();
    };
    pen.blendMode(draw::ADD);
    pen.noFill();
    pen.strokeCap(draw::SQUARE);
    pen.stroke(material::withAlpha(figure.glow, 0.14f));
    pen.strokeWeight(9);
    stretch();
    pen.stroke(material::withAlpha(figure.ink, 0.9f));
    pen.strokeWeight(1.3f);
    stretch();
    laid[index] = reached;
  }

  Element plate(size_t index) {
    const Figure& figure = figures[index];
    const float side = figure.plateSize();
    return graphics("plate" + std::to_string(index),
                    [this, index](draw::Pen& pen) { layPlate(pen, index); })
        .width(side)
        .height(side)
        .centerAt(kPupil)
        .rotate(motion::bind(seconds, {.from = {figure.start, figure.end},
                                       .clampFrom = true,
                                       .to = {0, figure.plateDegreesPerSecond() *
                                                     (figure.end - figure.start)}}))
        .opacity(during(figure.start, figure.end, 0.3f, 1.6f));
  }

  Element card(const Credit& credit, size_t index) {
    const std::string key = "credit" + std::to_string(index);
    Element words =
        credit.outline
            ? text(credit.words, weave::textStyle({.face = clarendon,
                                                   .size = credit.size,
                                                   .color = Color{0, 0, 0, 0},
                                                   .track = credit.size * 0.03f}))
                  .decorationOutline(Boundary::Glyphs)
                  .ink(hollowInk(credit.ink, std::max(1.4f, credit.size / 70)))
                  .cache(Cache::Texture)
            : text(credit.words)
                  .font({.face = clarendonBody, .size = credit.size, .track = credit.size * 0.16f})
                  .ink(credit.ink);
    words = std::move(words).key(key).centerAt(credit.at);
    if (credit.grow > 0)
      words = std::move(words).scale(
          motion::bind(seconds, {.from = {credit.start, credit.start + credit.grow},
                                 .clampFrom = true,
                                 .ease = motion::ease::outCubic,
                                 .to = {0.02f, 1}}));
    return std::move(words).opacity(
        during(credit.start, credit.end, credit.grow > 0 ? 0.25f : 0.4f, 0.6f));
  }

  Element describe() {
    const Figure& first = figures.front();
    return box().inset(0).fill(Color{0, 0, 0, 1}).children({
        // The face in its own grey, then — suddenly — stained red.
        pen("eye", [](draw::Pen& pen) { drawEye(pen, kGrey); }, Cache::Texture)
            .opacity(during(0, 2.4f, 0.8f, 0.1f)),
        pen("eye-stained", [](draw::Pen& pen) { drawEye(pen, kStained); }, Cache::Texture)
            .opacity(during(2.0f, first.start + 5.0f, 0.2f, 1.4f)),
        each(figures, [this](const Figure&, size_t index) { return plate(index); }),
        box()
            .key("light")
            .width(64)
            .height(64)
            .centerAt({0, 0})
            .translateX(lightX)
            .translateY(lightY)
            .opacity(lightLevel)
            .fill(material::radialGradient({32, 32}, 32,
                                           {{0.00f, hexColor(0xffffff)},
                                            {0.08f, hexColor(0xffffff)},
                                            {0.22f, hexColor(0xfff2e4, 0.55f)},
                                            {0.50f, hexColor(0xffe4cc, 0.14f)},
                                            {1.00f, hexColor(0xffe4cc, 0)}},
                                           {.units = material::GradientUnits::Pixels}))
            .cache(Cache::Texture),
        each(credits, [this](const Credit& credit, size_t index) { return card(credit, index); }),
        // Film grain: one sheet, larger than the frame, moved to a new
        // place every film frame.
        box()
            .key("grain")
            .left(-64)
            .top(-64)
            .width(kWidth + 128)
            .height(kHeight + 128)
            .flexShrink(0)
            .translateX(grainX)
            .translateY(grainY)
            .children({box()
                           .inset(0)
                           .fill(material::field::grain(0.7f, 2, 3.0f, 1.0f))
                           .opacity(0.06f)
                           .cache(Cache::Texture)}),
    });
  }

  template <class Read>
  static void rows(sketch::SketchContext& context, const char* file, Read read) {
    if (const auto table = context.assets.hub().load<data::Table>(
            context.local(std::string("data/") + file)))
      read(*table);
  }

  void setup(sketch::SketchContext& context) {
    rows(context, "figures.csv", [this](const data::Table& table) {
      const auto number = [&](const char* name) { return table.column<double>(name); };
      const auto ink = table.column<std::string>("ink");
      const auto glow = table.column<std::string>("glow");
      for (size_t row = 0; row < ink.size(); ++row)
        figures.push_back({.ink = draw::parseColor(ink[row]),
                           .glow = draw::parseColor(glow[row]),
                           .a = (float)number("a")[row],
                           .b = (float)number("b")[row],
                           .phaseDegrees = (float)number("phase")[row],
                           .damping = (float)number("damping")[row],
                           .rise = (float)number("rise")[row],
                           .precession = (float)number("precession")[row],
                           .turns = (float)number("turns")[row],
                           .radius = (float)number("radius")[row],
                           .start = (float)number("start")[row],
                           .drawn = (float)number("drawn")[row],
                           .end = (float)number("end")[row]});
    });
    rows(context, "credits.csv", [this](const data::Table& table) {
      const auto number = [&](const char* name) { return table.column<double>(name); };
      const auto words = table.column<std::string>("words");
      const auto registers = table.column<std::string>("register");
      const auto ink = table.column<std::string>("ink");
      for (size_t row = 0; row < words.size(); ++row)
        credits.push_back({.words = words[row],
                           .outline = registers[row] == "outline",
                           .size = (float)number("size")[row],
                           .at = {(float)number("x")[row], (float)number("y")[row]},
                           .ink = draw::parseColor(ink[row]),
                           .start = (float)number("start")[row],
                           .end = (float)number("end")[row],
                           .grow = (float)number("grow")[row]});
    });
    if (figures.empty()) return;
    laid.assign(figures.size(), 0);

    // Captured as the title has opened out of the stained pupil and the
    // first figure has begun to spiral from it.
    sketch::kit::stage(context, {.size = SkSize::Make(kWidth, kHeight),
                                 .captureAt = 5.2,
                                 .background = hexColor(0x000000)});
    clarendon = weave::ports::face({"SuperClarendon", "Super Clarendon", "Rockwell", "Bodoni 72"},
                                   weave::FaceStyle{.weight = 700});
    clarendonBody = weave::ports::face({"SuperClarendon", "Super Clarendon", "Rockwell", "Bodoni 72"},
                                       weave::FaceStyle{.weight = 400});
    context.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext&) {
    const float now = (float)std::fmod(elapsed, (double)kLoopLength);
    seconds = now;
    lightLevel = 0.0f;
    for (const Figure& figure : figures) {
      const float drawn = (now - figure.start) / figure.drawn;
      if (drawn <= 0 || drawn >= 1) continue;
      const glm::vec2 at = kPupil + figure.pendulum(figure.pendulumTime(now));
      lightX = at.x;
      lightY = at.y;
      lightLevel = std::min({1.0f, drawn / 0.02f, (1 - drawn) / 0.04f});
    }
    // Twenty-four film frames a second, each its own place in the sheet.
    const auto filmFrame = (uint32_t)(elapsed * 24);
    grainX = std::round(60 * sigil::core::noise::hash(11u, filmFrame));
    grainY = std::round(60 * sigil::core::noise::hash(13u, filmFrame));
  }
};

SIGIL_SKETCH(VertigoTitles, "Study · Motion",
             "Bass and Whitney's Vertigo titles (1958) — the stained eye, the "
             "title out of the pupil, Lissajous figures traced in light")
