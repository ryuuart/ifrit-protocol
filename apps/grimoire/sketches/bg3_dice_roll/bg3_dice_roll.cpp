// Baldur's Gate 3's dialogue ability check. The world drops behind a dark
// vignette and one ornate ring stands in the middle of it: the difficulty
// class hangs above the ring, the d20 tumbles inside it and lands with the
// natural roll square to the viewer, and the bonus chips wait under it.
// Once the die is still the bonuses count in one by one, the total climbs
// past the difficulty class, and the verdict appears.
//
// The ring's inner track is the difficulty read against the die: twenty
// ticks, one per face, and the arc of faces that clear the check once the
// bonuses are added, with the face that landed lit.
//
// The check's numbers — skill, difficulty, roll and bonuses — stand in
// data/check.json.

// TAGS: Interfaces/Game

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Tween.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "D20.h"

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace path = sigil::geometry::path;
namespace weave = sigil::weave;
using material::Paint;
using namespace sigil::compose;
using sigil::material::hexColor;
using namespace sigil::motion;
using namespace std::chrono_literals;

namespace {

constexpr float kWidth = 1200, kHeight = 1200, kCaption = 118;

// The ring, on one centre: the outer rule, the gilt band between it and
// the well, and the well the die sits in.
constexpr glm::vec2 kCentre{600, 470};
constexpr float kRing = 262, kBand = 44, kDie = 150;

const material::Color kScrim = hexColor(0x0B0907);
const material::Color kBone = hexColor(0xE0D6BE);
const material::Color kGilt = hexColor(0xC9A227);
const material::Color kBronze = hexColor(0x5A4312);
const material::Color kUmber = hexColor(0x2A1E0D);
const material::Color kPass = hexColor(0x5AAD8A);
const material::Color kNumeral = hexColor(0x2A1F12);

// The die tumbles to rest over its first 1.1 s; the bonuses count in once
// it is still, one point every 70 ms, and the verdict follows the last.
constexpr double kStill = 1.1, kCountFrom = 1.25, kPerPoint = 0.07;

struct Bonus {
  std::string name, note, chip, amount;
  int adds;
};

struct Check {
  std::string skill, ability;
  int difficulty = 15, roll = 12, discarded = 0;
  std::vector<Bonus> bonuses;
  int total() const {
    int sum = roll;
    for (const Bonus& bonus : bonuses) sum += bonus.adds;
    return sum;
  }
};

glm::vec2 onRing(float radius, float degrees) {
  const float radians = degrees * 3.14159265f / 180;
  return kCentre + radius * glm::vec2(std::cos(radians), std::sin(radians));
}

/** Where face @p number sits on the ring's track: 1 at the top, clockwise. */
float trackAngle(int number) { return -90 + (number - 1) * 18.0f; }

/** A straight rule between two points. */
Element segment(glm::vec2 from, glm::vec2 to, float weight,
                material::Color colour) {
  return pathFigure(path::toPath(path::Polyline{.points = {from, to}}), weight)
      .stroke(stroke(weight, Fill::color(colour)));
}

StyleSheet registers() {
  const material::Color dim = material::withAlpha(kBone, 0.55f);
  return StyleSheet{
      rule(".eyebrow").font({.size = 13, .track = 5}).ink(kGilt),
      rule(".difficulty").font({.size = 52}).ink(kBone),
      rule(".skill").font({.size = 30, .track = 7}).ink(kBone),
      rule(".note").font({.size = 14, .track = 0.4f}).ink(dim),
      rule(".chip").font({.size = 20}).ink(kBone),
      rule(".chip-name").font({.size = 11, .track = 2.5f}).ink(kGilt),
      rule(".bonus").font({.size = 21, .track = 0.6f}).ink(kBone),
      rule(".amount").font({.size = 22}).ink(kBone),
      rule(".total").font({.size = 48}).ink(kBone),
      rule(".verdict").font({.size = 40, .track = 16}).ink(kPass),
  };
}

struct Bg3DiceRoll {
  Check check;
  d20::Die die{0, 12};
  double seconds = 0;
  int shown = 0;

  // ------------------------------------------------------------- the ring

  static Element circle(float radius) {
    return kit::disc({kCentre.x, kCentre.y}, radius).shape(shapes::circle());
  }

  /** The ornate ring: an outer hairline, the gilt band with a four-point
   *  star on it between every pair of faces, and the dark well. */
  static Element ring() {
    const float starRadius = kRing - kBand * 0.5f;
    return box().inset(0).children({
        // A warm light under the ring, so it stands out of the scrim.
        circle(kRing + 120)
            .fill(sigil::material::radialGradient(
                {0.5f, 0.5f}, 1.0f,
                {{0.0f, material::withAlpha(kGilt, 0.16f)},
                 {1.0f, material::withAlpha(kGilt, 0.0f)}},
                {.extent = material::RadialExtent::ClosestSide})),
        circle(kRing + 12)
            .fill(Fill::none())
            .foreground(decorations::border(0.8f, Fill::color(kBronze))),
        circle(kRing)
            .shape(shapes::annulus(1 - kBand / kRing))
            .fill(sigil::material::linearGradient({0.2f, 0.0f}, {0.8f, 1.0f},
                                                  {{0.0f, hexColor(0x6B5218)},
                                                   {0.5f, kUmber},
                                                   {1.0f, hexColor(0x5E4714)}}))
            .foreground(decorations::border(2.0f, Fill::color(kGilt))),
        each(20,
             [&](size_t gap) {
               const glm::vec2 at = onRing(starRadius, trackAngle(gap) + 9);
               return box()
                   .width(18)
                   .height(18)
                   .centerAt({at.x, at.y})
                   .shape(shapes::star(4, 0.34f))
                   .fill(Fill::color(kGilt));
             }),
        circle(kRing - kBand)
            .fill(sigil::material::radialGradient(
                {0.5f, 0.42f}, 0.72f,
                {{0.0f, hexColor(0x241B10)}, {1.0f, hexColor(0x0C0906)}}))
            .foreground(decorations::border(1.0f, Fill::color(kBronze), 6)),
    });
  }

  /** The difficulty read against the die: the faces that clear the check
   *  once every bonus is added, as an arc on the well's rim, and a tick per
   *  face with the landed face lit. */
  Element track() const {
    const float outer = kRing - kBand - 10;
    const int lowest = check.difficulty - (check.total() - check.roll);
    const float start = trackAngle(lowest) - 9;
    const float sweep = (21 - lowest) * 18.0f;
    return box().inset(0).children({
        circle(outer)
            .shape(shapes::sector(start, sweep, 1 - 10 / outer))
            .fill(Fill::color(material::withAlpha(kPass, 0.4f))),
        each(20,
             [&](size_t index) {
               const int face = (int)index + 1;
               const bool landed = face == check.roll;
               const float angle = trackAngle(face);
               return segment(onRing(outer - (landed ? 16 : 10), angle),
                              onRing(outer, angle), landed ? 3 : 1.2f,
                              landed ? kBone
                              : face >= lowest
                                  ? kPass
                                  : material::withAlpha(kBone, 0.3f));
             }),
    });
  }

  // -------------------------------------------------------------- the die

  /** The die at the moment: each face a triangle lit by how squarely it
   *  faces the viewer, the creases in bronze, the silhouette in bone, and
   *  the numbers on the faces that are turned far enough to read. */
  Element d20Figure() const {
    const float settling = (float)std::clamp(seconds / kStill, 0.0, 1.0);
    const d20::View view =
        die.seen(1 - sigil::motion::ease::outQuint(settling), kCentre, kDie);
    return box()
        .inset(0)
        .transformOrigin(Dimension(kCentre.x), Dimension(kCentre.y))
        .scale(sigil::motion::animate(
            {.from = 0.8f,
             .keyframes = {{.to = 1.0f, .duration = 1000ms},
                           {.to = 1.07f, .duration = 80ms},
                           {.to = 0.98f, .duration = 90ms},
                           {.to = 1.0f, .duration = 90ms}}}))
        .children({
            each(view.faces,
                 [](const d20::Face& face) {
                   const float light = 0.34f + 0.66f * face.facing;
                   const material::Color tone = material::scale(kBone, light);
                   const path::Polyline outline{
                       .points = {face.corners[0], face.corners[1],
                                  face.corners[2]},
                       .closed = true};
                   return pathFigure(path::toPath(outline))
                       .fill(sigil::material::linearGradient(
                           {0.3f, 0.0f}, {0.7f, 1.0f},
                           {{0.0f, material::scale(tone, 1.06f)},
                            {1.0f, material::scale(tone, 0.88f)}}));
                 }),
            each(view.edges,
                 [](const d20::Edge& edge) {
                   return edge.silhouette
                              ? segment(edge.from, edge.to, 2.6f, kBone)
                              : segment(edge.from, edge.to, 1.1f,
                                        material::withAlpha(kBronze, 0.9f));
                 }),
            each(view.faces,
                 [&](const d20::Face& face) {
                   const bool result =
                       face.number == check.roll && seconds >= kStill;
                   const float size = kDie * (result ? 0.4f : 0.3f) *
                                      (0.55f + 0.45f * face.facing);
                   return text(std::to_string(face.number))
                       .font({.size = size})
                       .ink(material::withAlpha(kNumeral, face.facing))
                       .centerAt({face.centre.x, face.centre.y})
                       .opacity(face.facing < 0.34f ? 0.0f : 1.0f);
                 }),
        });
  }

  // ------------------------------------------------ above and below the ring

  /** The difficulty class, on a notched plaque between two gilt rules. */
  Element difficulty() const {
    auto flank = [] {
      return kit::line({.length = Dimension(170),
                        .thickness = 1.4f,
                        .fill = Fill::color(kGilt)});
    };
    return kit::at(0, 56, kWidth, 150)
        .column()
        .alignItems(Align::Center)
        .gap(10)
        .children({
            text("DIFFICULTY CLASS").styleClass("eyebrow"),
            box()
                .row()
                .alignItems(Align::Center)
                .gap(18)
                .children({
                    flank(),
                    box()
                        .width(124)
                        .height(84)
                        .shape(shapes::notched(26, 10))
                        .fill(Fill::color(hexColor(0x16110B)))
                        .foreground(decorations::doubleBorder(
                            decorations::border(1.8f, Fill::color(kBone)),
                            decorations::border(0.8f, Fill::color(kGilt), 5)))
                        .alignItems(Align::Center)
                        .justifyContent(Justify::Center)
                        .children({text(std::to_string(check.difficulty))
                                       .styleClass("difficulty")}),
                    flank(),
                }),
        });
  }

  /** One bonus as the chip BG3 hangs under the die: its amount in a gilt
   *  roundel, its source named beneath. */
  static Element chip(const Bonus& bonus) {
    return box()
        .column()
        .alignItems(Align::Center)
        .gap(8)
        .children({
            box()
                .width(60)
                .height(60)
                .shape(shapes::circle())
                .fill(sigil::material::radialGradient(
                    {0.5f, 0.35f}, 0.8f,
                    {{0.0f, hexColor(0x3A2C12)}, {1.0f, hexColor(0x120D07)}}))
                .foreground(decorations::doubleBorder(
                    decorations::border(1.6f, Fill::color(kGilt)),
                    decorations::border(0.7f, Fill::color(kBronze), 4)))
                .alignItems(Align::Center)
                .justifyContent(Justify::Center)
                .children({text(bonus.amount).styleClass("chip")}),
            text(bonus.chip).styleClass("chip-name"),
        });
  }

  /** One line of the bonus list, counting in from the right. */
  static Element bonusLine(const Bonus& bonus) {
    return box()
        .row()
        .alignItems(Align::Center)
        .gap(16)
        .height(50)
        .opacity(sigil::motion::animate({.from = 0.0f,
                                         .to = 1.0f,
                                         .duration = 260ms,
                                         .delay = sigil::motion::stagger(110ms),
                                         .ease = sigil::motion::ease::outQuad}))
        .translateX(
            sigil::motion::animate({.from = 18.0f,
                                    .to = 0.0f,
                                    .duration = 300ms,
                                    .delay = sigil::motion::stagger(110ms),
                                    .ease = sigil::motion::ease::outQuad}))
        .children({
            box().column().flexGrow().gap(2).children({
                text(bonus.name).styleClass("bonus"),
                text(bonus.note).styleClass("note"),
            }),
            text(bonus.amount).styleClass("amount"),
        });
  }

  /** Under the ring: the skill, the chips, and once the die is still the
   *  bonus list, the total and the verdict. The list and the verdict have
   *  slots of their own, so nothing moves when they arrive. */
  Element reckoning() const {
    const bool counting = seconds >= kCountFrom;
    const bool decided = shown == check.total();
    const bool passed = check.total() >= check.difficulty;
    return kit::at(0, kCentre.y + kRing + 18, kWidth, 440)
        .column()
        .alignItems(Align::Center)
        .children({
            text(check.skill).styleClass("skill"),
            text(check.ability + " check  ·  advantage, the other die showed " +
                 std::to_string(check.discarded))
                .styleClass("note")
                .marginTop(6)
                .marginBottom(18),
            box().row().gap(34).children({each(check.bonuses, chip)}),
            box().width(560).height(150).marginTop(20).column().children(
                {counting ? each(check.bonuses, bonusLine)
                          : std::vector<Element>{}}),
            kit::line({.length = Dimension(560),
                       .thickness = 1.4f,
                       .fill = Fill::color(material::withAlpha(kBone, 0.7f))}),
            box()
                .width(560)
                .row()
                .alignItems(Align::Center)
                .justifyContent(Justify::SpaceBetween)
                .children({
                    text("Total").styleClass("bonus"),
                    text(std::to_string(std::max(shown, check.roll)))
                        .styleClass("total"),
                }),
            box()
                .height(64)
                .justifyContent(Justify::Center)
                .children(
                    {decided ? text(passed ? "SUCCESS" : "FAILURE")
                                   .styleClass("verdict")
                                   .opacity(sigil::motion::animate(
                                       {.from = 0.0f,
                                        .to = 1.0f,
                                        .duration = 380ms,
                                        .ease = sigil::motion::ease::outQuad}))
                             : box()}),
        });
  }

  /** The line under the plate, outside the game's own screen. */
  static Element caption() {
    return kit::at(0, kHeight, kWidth, kCaption)
        .column()
        .justifyContent(Justify::Center)
        .padding(0, 56)
        .gap(7)
        .fill(Fill::color(hexColor(0x050403)))
        .children({
            text("BALDUR’S GATE 3 · THE ABILITY CHECK")
                .font({.size = 18, .track = 1.8f})
                .ink(kGilt),
            text("The natural roll lands first; Charisma, proficiency and "
                 "Guidance then lift 12 to 20, clearing a difficulty of 15.")
                .font({.size = 13})
                .ink(material::withAlpha(kBone, 0.85f)),
        });
  }

  Element describe() const {
    return box()
        .inset(0)
        .fill(sigil::material::radialGradient({0.5f, 0.36f}, 0.85f,
                                              {{0.0f, hexColor(0x1C150D)},
                                               {0.6f, kScrim},
                                               {1.0f, hexColor(0x020201)}}))
        .font({.face = weave::ports::face(
                   {"Baskerville", "Palatino", "Hoefler Text", "Georgia"})})
        .applyStyleSheet(registers())
        .children({ring(), track(), d20Figure(), difficulty(), reckoning(),
                   caption()});
  }

  // ---------------------------------------------------------------- setup

  void setup(sketch::SketchContext& context) {
    context.canvas(kWidth, kHeight + kCaption);
    context.background(kScrim);
    context.captureAt(6.0);

    const sketch::kit::Document data(context, "data/check.json");
    check.skill = data["skill"].string();
    check.ability = data["ability"].string();
    check.difficulty = (int)data["difficulty"].number();
    check.roll = (int)data["roll"].number();
    check.discarded = (int)data["advantage"]["discarded"].number();
    for (const auto& bonus : data["bonuses"].array())
      check.bonuses.push_back({std::string(bonus["name"].string()),
                               std::string(bonus["note"].string()),
                               std::string(bonus["chip"].string()),
                               std::string(bonus["amount"].string()),
                               (int)bonus["adds"].number()});
    // The face that lands square to the viewer carries the roll.
    die = d20::Die(0, check.roll);
    context.composer.render(describe());
  }

  /** The die is re-described every frame while it tumbles; after that only
   *  when the list arrives or the counted total moves. */
  void update(double elapsed, sketch::SketchContext& context) {
    const bool tumbling = seconds < kStill + 0.2;
    const bool arriving = (seconds < kCountFrom) != (elapsed < kCountFrom);
    const int counted =
        elapsed < kCountFrom
            ? check.roll
            : std::min(check.total(),
                       check.roll + (int)((elapsed - kCountFrom) / kPerPoint));
    seconds = elapsed;
    if (!tumbling && !arriving && counted == shown) return;
    shown = counted;
    context.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH(Bg3DiceRoll, "Study · Game UI",
             "Baldur's Gate 3's ability check — the d20 lands inside its "
             "ring, then the bonuses lift the roll past the difficulty")
