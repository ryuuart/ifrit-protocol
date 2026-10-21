// The End of Evangelion: MAGI-01 behind the Danang Type-B defensive screen,
// Protect No.666. Three register arms (MELCHIOR-1, BALTHASAR-2, CASPER-3)
// stand inside eight numbered defense rings; outside them a labyrinth of
// double-line channels holds rows of 6s laid in arcs, condensed as they go
// out and glowing in a pulse that travels from the centre outward. Every
// line wears one wash, green at the top to brick red at the bottom, and
// four fixed panels read the screen. The infection runs as one timeline on
// the engine: the twelve registers fill one after another, the screen
// raises and drains them, and from that moment the two counters run, one
// up from the raise and one down to the collapse.
// EDIT THESE FIRST: the palette and the wash, kInfection (when each beat
// lands), the glow's period and lag, and kCollapseSeconds.
// TAGS: Interfaces/Film, Typography/Paths, Motion/Timelines

#include <include/core/SkPath.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "../evangelion/EvangelionUi.h"

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace motion = sigil::motion;
namespace weave = sigil::weave;
using namespace sigil::compose;
using namespace std::chrono_literals;
using sigil::draw::Pen;
using sigil::material::hexColor;
using sigil::material::Paint;

namespace {
constexpr float kWidth = 1428, kHeight = 800;
constexpr SkPoint kCentre{712, 400};
const material::Color kGround = hexColor(0x020704);
const material::Color kOrange = hexColor(0xFF951F);
const material::Color kGreen = hexColor(0x77E9A3);
// The defense lines' wash, read off the film frame: green at the top,
// olive across the middle, brick red at the bottom.
const material::Color kWashTop = hexColor(0x5A8A5A);
const material::Color kWashMiddle = hexColor(0x9A8E50);
const material::Color kWashBottom = hexColor(0x962D18);
const material::Color kInfected = hexColor(0xEB3C30);

/** How far a label may widen to span its measure: the panels' type fills
 *  its box, as the film's lettering does. */
constexpr float kWidest = 4.0f;

/** The program's lockout: the two counters always sum to it. */
constexpr int kCollapseSeconds = 223238;

/** The infection, beat by beat, on the engine's clock. */
struct Infection {
  motion::Duration first = 400ms;    ///< the first register starts to fill
  motion::Duration each = 140ms;     ///< the next one follows this late
  motion::Duration fill = 300ms;     ///< one register filling
  motion::Duration raise = 2600ms;   ///< the screen raises
  motion::Duration raising = 700ms;  ///< how long it takes to stand
};
constexpr Infection kInfection;
/** The field's glow: one pulse from the centre out takes kGlowPeriod, each
 *  row lags the one inside it by kGlowLag of a period, and between pulses
 *  a row rests at kGlowRest of its brightness. */
constexpr motion::Duration kGlowPeriod = 2400ms;
constexpr float kGlowLag = 0.06f, kGlowRest = 0.45f;

/** The defense rings: the innermost one's radius and the pitch out. */
constexpr float kInnerRing = 80, kRingPitch = 38;
/** The field's channels: the first one's radius, the pitch between them,
 *  how far apart a channel's two lines stand, where a channel turns back
 *  above or below the axis, and how deep the turn dips. */
constexpr int kChannels = 5;
constexpr float kFirstChannel = 372, kChannelPitch = 100, kChannelHalf = 4;
constexpr float kTurnHeight = 60, kTurnDepth = 18;

/** The four register bands of an arm, in the arm's own frame: the arm is
 *  drawn pointing right and turned into place. Each band slants with the
 *  arm's leading edge, so a band's corners shift left as it descends. */
constexpr float kBandTop = 343, kBandDepth = 31.75f, kBandShift = 18.5f;
constexpr float kBandLeft = 811, kBandRight = 1015;

struct EvaMagiInterior {
  weave::FontContext* fonts = nullptr;
  Composer* composer = nullptr;
  motion::Animatable<float> glow = motion::animatable(0.0f);
  motion::Animatable<float> raised = motion::animatable(0.0f);
  /** MELCHIOR's four, BALTHASAR's four, CASPER's four: each its own
   *  value, so the stagger reaches them one at a time. */
  std::vector<motion::Animatable<float>> registers = [] {
    std::vector<motion::Animatable<float>> cells;
    for (int cell = 0; cell < 12; ++cell)
      cells.push_back(motion::animatable(0.0f));
    return cells;
  }();
  int sinceRaised = 0;

  /** Condensed bold capitals standing @p capHeight tall with their cap
   *  tops on @p capTop, set to span @p measure: the size is the cap
   *  height's, so only the horizontal scale moves, condensing a long run
   *  and widening a short one as far as kWidest. */
  Text label(Utf8 words, SkPoint capTop, float capHeight, float measure,
             material::Color ink = kOrange) const {
    const weave::TextStyle standing =
        atCapHeight(weave::textStyle({.face = evangelion::condensedBold(),
                                      .size = 100,
                                      .color = ink,
                                      .condense = kWidest}),
                    capHeight, *fonts);
    const weave::TextStyle style =
        fitRun(words.bytes(), standing, measure, *fonts,
               {.minSize = standing.shaping.fontSize, .minCondense = 0.2f});
    return text(words, style)
        .textFirstBaseline(weave::FrameOptions::FirstBaseline::kCapHeight)
        .at({capTop.fX, capTop.fY});
  }

  /** The wash every defense line is stroked in: the film's green at the
   *  top through its olive to its brick red at the bottom, spread over
   *  @p reach px either side of the centre, so a ring changes colour along
   *  its own length. */
  static material::Material wash(float reach) {
    return sigil::material::linearGradient(
        {0, kCentre.fY - reach}, {0, kCentre.fY + reach},
        {{0, kWashTop}, {0.5f, kWashMiddle}, {1, kWashBottom}},
        {.units = material::GradientUnits::Pixels});
  }

  /** Eight heavy defense rings, numbered 8 to 2 along the left radius and
   *  二 to 八 down the bottom, with the seven circles named up the top in
   *  the gaps between them. */
  Element rings() const {
    static constexpr std::array<const char*, 7> kCircles{
        "CIRCLE LUST",   "CIRCLE GLUTTON",  "CIRCLE GREED", "CIRCLE ANGER",
        "CIRCLE HERESY", "CIRCLE VIOLENCE", "CIRCLE FRAUD"};
    static constexpr std::array<const char*, 7> kKanji{"二", "三", "四", "五",
                                                       "六", "七", "八"};
    Element group = box().inset(0).children({pen(
        "protection.rings",
        [](Pen& p) {
          p.noFill();
          p.strokeWeight(10);
          p.stroke(wash(kInnerRing + 7 * kRingPitch));
          for (int ring = 0; ring < 8; ++ring)
            p.circle(kCentre, 2 * (kInnerRing + ring * kRingPitch));
        },
        Cache::Texture)});
    for (int gap = 0; gap < 7; ++gap) {
      // The gap between ring `gap` and the next one out.
      const float radius = kInnerRing + (gap + 0.5f) * kRingPitch;
      group.children({text(kCircles[6 - gap])
                          .font({.face = evangelion::condensedBold(),
                                 .size = 15,
                                 .color = kGreen})
                          .width(radius * 2)
                          .height(radius * 2)
                          .centerAt(sigil::geometry::path::fromSk(kCentre))
                          .textOnPath({.path = shapes::circle(),
                                       .at = 0.75f,
                                       .align = TextPath::Align::Center,
                                       .offset = -5}),
                      text(std::to_string(gap + 2))
                          .font({.face = evangelion::condensedBold(),
                                 .size = 22,
                                 .color = kWashMiddle})
                          .at({kCentre.fX - radius - 6, kCentre.fY - 14}),
                      text(kKanji[gap])
                          .font({.face = evangelion::minchoHeavy(),
                                 .size = 18,
                                 .color = kWashBottom})
                          .at({kCentre.fX - 9, kCentre.fY + radius - 12})});
    }
    return group;
  }

  /** WHERE A ROW OF 6s STANDS on its ring, in degrees clockwise from +x:
   *  the arcs the film fills, never the whole ring. Above the axis a row
   *  runs in three arcs whose cuts step from row to row; below it, where
   *  the defense rings take the middle, an inner row keeps only the ends
   *  by the axis and an outer row two long arcs round the lower corners.
   *  Every row stands clear of the axis, where the channels come in. */
  static std::vector<std::pair<float, float>> arcsOf(int row, float radius) {
    const float clear = std::asin(std::min(1.0f, (kTurnHeight + 20) / radius)) *
                        180 / std::numbers::pi_v<float>;
    const float firstCut = 230 + 17 * ((3 * row) % 5);
    const float secondCut = 296 + 11 * ((2 * row) % 5);
    std::vector<std::pair<float, float>> arcs{{180 + clear, firstCut - 3},
                                              {firstCut + 3, secondCut - 3},
                                              {secondCut + 3, 360 - clear}};
    if (row / 2 >= 2) {
      const float step = 4.0f * (row % 3);
      arcs.push_back({clear, 78 - step});
      arcs.push_back({102 + step, 180 - clear});
    } else {
      arcs.push_back({clear, clear + 26});
      arcs.push_back({154 - clear, 180 - clear});
    }
    return arcs;
  }

  /** The field of 6s: rows in pairs between the channels, each row laid
   *  in the arcs `arcsOf` names and condensed as it goes out, glowing in
   *  a pulse that travels from the centre outward row by row. */
  Element codeField() {
    Element group = box().inset(0);
    for (int row = 0; row < 2 * kChannels; ++row) {
      const float radius =
          kFirstChannel + (row / 2) * kChannelPitch + 10 + (row % 2) * 38;
      const float condense = 0.85f * kFirstChannel / radius;
      const auto brightness =
          motion::bind(glow, {.from = {row * kGlowLag, row * kGlowLag + 1},
                              .envelope = motion::envelope::cosine(),
                              .to = {kGlowRest, 1}});
      for (const auto& [from, to] : arcsOf(row, radius)) {
        const float length =
            radius * (to - from) * std::numbers::pi_v<float> / 180;
        const std::string sixes(
            std::max(1, static_cast<int>(length / (25 * condense + 1.3f))),
            '6');
        group.children({text(sixes)
                            .font({.face = evangelion::condensedRegular(),
                                   .size = 46,
                                   .color = kGreen,
                                   .track = 1.3f,
                                   .condense = condense})
                            .width(radius * 2)
                            .height(radius * 2)
                            .centerAt(sigil::geometry::path::fromSk(kCentre))
                            .opacity(brightness)
                            .textOnPath({.path = shapes::arc(from, to - from),
                                         .autoFlip = false,
                                         .fit = TextPath::Fit::Spacing})});
      }
    }
    group.children({channels()});
    return group;
  }

  /** The labyrinth the field is cut by: double lines on rings between the
   *  pairs of rows, broken at the horizontal axis where each turns back
   *  into its neighbour, alternately above and below the axis and
   *  alternately left and right, and the four straight lines that come in
   *  from either edge along the axis. */
  static Element channels() {
    return pen(
        "protection.channels",
        [](Pen& p) {
          p.noFill();
          p.stroke(wash(kHeight / 2));
          p.strokeWeight(3);
          const auto end = [](float radius, float side, float above) {
            const float across =
                std::sqrt(radius * radius - kTurnHeight * kTurnHeight);
            return SkPoint{kCentre.fX + side * across,
                           kCentre.fY + above * kTurnHeight};
          };
          for (int channel = 0; channel < kChannels; ++channel) {
            const float middle = kFirstChannel + channel * kChannelPitch;
            for (float line : {-kChannelHalf, kChannelHalf}) {
              const float radius = middle + line;
              const float lean = std::asin(kTurnHeight / radius);
              const float diameter = 2 * radius;
              p.arc(kCentre.fX, kCentre.fY, diameter, diameter,
                    std::numbers::pi_v<float> + lean,
                    2 * std::numbers::pi_v<float> - lean);
              p.arc(kCentre.fX, kCentre.fY, diameter, diameter, lean,
                    std::numbers::pi_v<float> - lean);
            }
            if (channel + 1 == kChannels) continue;
            // The turn into the next channel out: on the left above the
            // axis and on the right below it for even channels, the other
            // way round for odd ones.
            const float outer = middle + kChannelPitch;
            for (float side : {-1.0f, 1.0f}) {
              const float above = (channel % 2 == 0) == (side < 0) ? -1 : 1;
              for (float line : {-kChannelHalf, kChannelHalf}) {
                const SkPoint from = end(middle - line, side, above);
                const SkPoint to = end(outer + line, side, above);
                const float dip = -above * kTurnDepth;
                p.bezier(from.fX, from.fY, from.fX, from.fY + dip, to.fX,
                         to.fY + dip, to.fX, to.fY);
              }
            }
          }
          p.noStroke();
          p.fill(kGround);
          p.rect(0, 361, 357, 74);
          p.rect(1067, 361, kWidth - 1067, 74);
          p.noFill();
          p.stroke(wash(kHeight / 2));
          p.strokeWeight(5);
          for (float y : {370.0f, 391.0f, 413.0f, 431.0f}) {
            p.line(0, y, 324, y);
            p.line(1100, y, kWidth, y);
            p.bezier(324, y, 353, y, 353, y - 15, 344, y - 15);
            p.bezier(1100, y, 1069, y, 1069, y - 15, 1078, y - 15);
          }
        },
        Cache::Texture);
  }

  /** One register arm, drawn pointing right and turned by @p angle: the
   *  four registers the infection fills, their outline, their names, and
   *  the machine's own name along the arm's inner edge, turned there by
   *  @p nameTurn so it reads the way the film sets it. */
  Element registerArm(float angle, const char* name, float nameTurn,
                      int first) const {
    static constexpr std::array<const char*, 4> kNames{"CELEBRUM", "CELEBELLUM",
                                                       "CALLOSUM", "OBLONGATE"};
    Element arm = box()
                      .inset(0)
                      .transformOrigin(pct(100 * kCentre.fX / kWidth), pct(50))
                      .rotate(angle);
    arm.children({pen(
        "protection.register.ground",
        [](Pen& p) {
          p.noStroke();
          p.fill(kGround);
          p.quad(786, 343, 1015, 343, 941, 470, 712, 470);
        },
        Cache::Texture)});
    for (int band = 0; band < 4; ++band) {
      const float top = kBandTop + band * kBandDepth;
      const float shift = band * kBandShift;
      arm.children({pen(
                        "protection.register.fill",
                        [=](Pen& p) {
                          p.noStroke();
                          p.fill(kInfected);
                          p.quad(
                              kBandLeft - shift, top, kBandRight - shift, top,
                              kBandRight - shift - kBandShift, top + kBandDepth,
                              kBandLeft - shift - kBandShift, top + kBandDepth);
                        },
                        Cache::Texture)
                        .opacity(registers[first + band])});
    }
    arm.children({pen(
        "protection.register",
        [](Pen& p) {
          p.noFill();
          p.stroke(kOrange);
          p.strokeWeight(3);
          p.quad(786, 343, 1015, 343, 941, 470, 712, 470);
          p.line(811, 343, 737, 470);
          for (int band = 1; band < 4; ++band) {
            const float y = kBandTop + band * kBandDepth;
            const float shift = band * kBandShift;
            p.line(kBandLeft - shift, y, kBandRight - shift, y);
          }
        },
        Cache::Texture)});
    for (int band = 0; band < 4; ++band)
      arm.children(
          {label(kNames[band],
                 {824 - band * kBandShift, 349 + band * kBandDepth}, 20, 166)});
    arm.children({label(name, {0, 0}, 16, 120)
                      .width(120)
                      .height(21)
                      .centerAt({764, 404})
                      .rotate(nameTurn)});
    return arm;
  }

  Element core() const {
    return box().inset(0).children(
        {box().inset(0).fill(kGround).shape([](glm::vec2) {
           return sigil::geometry::path::fromSk(
               SkPath::Polygon({{637, 343}, {786, 343}, {712, 470}}, true));
         }),
         registerArm(0, "BALTHASAR-2", 120, 4),
         registerArm(-120, "MELCHIOR-1", 120, 0),
         registerArm(120, "CASPER-3", -60, 8),
         pen(
             "protection.hub",
             [](Pen& p) {
               p.noFill();
               p.stroke(kOrange);
               p.strokeWeight(3);
               p.triangle(637, 343, 786, 343, 712, 470);
             },
             Cache::Texture),
         label("MAGI", {666, 358}, 32, 93), label("01", {683, 395}, 33, 57),
         label("ORIGINAL", {692, 434}, 7, 40)});
  }

  Element panel(float x, float y, float width, Utf8 heading, Utf8 reading,
                bool timer) const {
    Element plate =
        box()
            .rect(x, y, width, 112)
            .fill(kGround)
            .borderRadius({8})
            .stroke(
                stroke(4, sigil::material::skia::base(Paint::solid(kOrange))));
    if (timer) {
      plate.children(
          {box()
               .rect(sigil::geometry::path::Rect{{8, 39}, {width - 8, 42}})
               .fill(kOrange),
           label(heading, {10, 9}, 24, width - 20),
           label(reading, {15, 52}, 47, width - 100),
           label("sec.", {width - 77, 77}, 23, 63)});
    } else {
      plate.children(
          {label(heading, {9, 12}, 50, width - 18),
           box()
               .rect(sigil::geometry::path::Rect{{6, 72}, {width - 6, 75}})
               .fill(kOrange),
           label(reading, {width * 0.33f, 82}, 21, width * 0.62f)});
    }
    return box().inset(0).children(
        {std::move(plate),
         box().rect(x - 31, y - 2, 17, 116).fill(kOrange).borderRadius({9}),
         box()
             .rect(x + width + 15, y - 2, 17, 116)
             .fill(kOrange)
             .borderRadius({9})});
  }

  Element counters() const {
    const int remaining = kCollapseSeconds - sinceRaised;
    return box().inset(0).children(
        {panel(1014, 12, 365, "TIME REMAINING TO COLLAPSE",
               kit::formatted("%03d,%03d", remaining / 1000, remaining % 1000),
               true),
         panel(1014, 667, 365, "TIME SINCE SCREEN RAISED",
               kit::formatted("%03d,%03d", sinceRaised / 1000,
                              sinceRaised % 1000),
               true)});
  }

  /** The infection on one timeline: the registers fill one after another,
   *  then the screen raises and drains them, last filled first; the
   *  counters run once a second from the raise. */
  void infect(motion::Engine& engine) {
    engine.timeline()
        .add(registers,
             {.from = 0.0f,
              .to = 1.0f,
              .duration = kInfection.fill,
              .delay = motion::stagger(kInfection.each)},
             motion::at(kInfection.first))
        .label("raise", motion::at(kInfection.raise))
        .add(raised, {.from = 0.0f, .to = 1.0f, .duration = kInfection.raising},
             motion::atLabel("raise"))
        .add(registers,
             {.to = 0.0f,
              .duration = kInfection.fill,
              .delay = motion::stagger(kInfection.each / 2,
                                       {.from = motion::StaggerFrom::Last})},
             motion::atLabel("raise"));
    engine.animate(glow, {.from = 0.0f,
                          .to = 1.0f,
                          .duration = kGlowPeriod,
                          .ease = motion::ease::linear,
                          .loop = -1});
    engine.timer(
        [this] {
          ++sinceRaised;
          composer->renderSlot("counters", counters());
        },
        {.stepRate = 1.0, .delay = kInfection.raise});
  }

  void setup(sketch::Scene2DContext& ctx) {
    fonts = ctx.fonts;
    composer = &ctx.composer;
    // The still is the frame the film holds: nine seconds after the raise.
    sketch::kit::stage(ctx, {.size = {kWidth, kHeight},
                             .captureAt = (kInfection.raise + 9.5s).count(),
                             .background = kGround});
    auto tube = evangelion::crtTube(SkRect::MakeWH(kWidth, kHeight));
    tube.set("uBloom", 0.22f);
    // The film frame is drawn flat: this screen's glass does not bend.
    tube.set("uCurvature", 0.0f);
    const auto phosphor = material::Filter::bloom({.sigma = 2.4f,
                                                   .strength = 0.6f,
                                                   .spread = 2.2f,
                                                   .tail = 0.12f,
                                                   .threshold = 0.12f,
                                                   .knee = 0.18f,
                                                   .softness = 0.3f,
                                                   .whitening = 0.06f,
                                                   .dilation = 0.35f,
                                                   .deepening = 1.5f});
    Element screen =
        box()
            .inset(0)
            .transformOrigin(pct(100 * kCentre.fX / kWidth), pct(50))
            .opacity(raised)
            .scale(motion::bind(raised, {.to = {0.94f, 1.0f}}))
            .children({codeField(), rings(),
                       panel(59, 12, 365, "DANANG TYPE-B DEFENSIVE SCREEN",
                             "on MAGI-01 ORIGINAL", false),
                       panel(59, 667, 365, "PROTECT NO.666",
                             "on MAGI-01 ORIGINAL", false),
                       slot("counters")});
    ctx.composer.render(box().inset(0).fill(kGround).children(
        {box()
             .inset(0)
             .overflow(Overflow::Clip)
             .children({std::move(screen), core()})
             .filter(phosphor.then(material::Filter::of(tube, 82.0f)))}));
    ctx.composer.renderSlot("counters", counters());
    infect(ctx.engine);
  }
};
}  // namespace

SIGIL_SKETCH(EvaMagiInterior, "Study · Film",
             "The End of Evangelion — MAGI-01 under the Danang Type-B "
             "defensive screen, Protect No.666: the registers infected, the "
             "screen raised, the 6s glowing outward, the counters running")
