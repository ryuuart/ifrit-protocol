/** @file
 * chladni_tab1 — "Tab. I", the first plate of sound figures in Ernst
 * Florens Friedrich Chladni's *Entdeckungen über die Theorie des Klanges*
 * (Leipzig, 1787), engraved by Capieux and signed "Capieux. sculps. 1786."
 * at its foot.
 *
 * Twelve metal discs seen from above, strewn with sand, bowed at the rim
 * and damped with a finger: the grains bounce off the parts that move and
 * pile along the lines that stay still. Every number in the words file
 * was measured off a scan of the plate — the frame, each disc's centre
 * with the engraver's hand wander kept, every star's point count and hub,
 * every compass line, every reference letter — and the plate is drawn
 * from those numbers alone.
 *
 * What the measurements showed, and the drawing obeys:
 *  - the hub of a star grows with its mode, from a bare needle in figure 1
 *    to a solid boss with teeth in figure 12, and its flanks are engraved
 *    concave, narrowing off the hub and running out as needles;
 *  - figures 3 and 5 are figures 2 and 4 drawn as valleys, a blank star
 *    channel through a disc of sand combed outward;
 *  - figures 7, 9 and 10, which no eigenmode formula describes, are
 *    compass work: every line is a true circular arc, its point often set
 *    on or beyond the rim;
 *  - the plate is laid out in ascending pitch, so the bow passes the
 *    figures in reading order and the sand of a higher figure finds its
 *    line sooner;
 *  - every reference letter stands upright, never turned to the rim.
 *
 * The leaf is printed from a copper plate: the plate's edge is pressed
 * into the paper as a recess, the ink the wiping left behind lies as a
 * film inside it, and the leaf curls into the book's gutter at the left.
 * Wherever the bow touches, the disc sounds: two wavefronts leave the
 * rim and fade as they spread, and the grains hop under the bow, on its
 * first pass as they gather and on every round after.
 */
// TAGS: Drawing/Generative

#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/core/Mask.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Ground.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Transition.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <memory>

#include "Figures.h"

using namespace std::chrono_literals;

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace field = sigil::material::field;
using namespace sigil::compose;
using namespace sigil::motion;
using namespace sigil::weave::literals;
using material::Paint;

namespace {

/** The star's tips stop just short of the rim, as engraved. */
constexpr float kTip = 0.985f;
/** How far a figure's cell reaches from its centre, in radii: far enough
 *  for its numeral, its letters and the ring of sound. */
constexpr float kCellReach = 1.36f;
/** Room around a disc for a grain still hopping outside it. */
constexpr float kSandMargin = 10;
/** The signature's clearance from the inner rule, in pixels. */
constexpr float kCreditClear = 12;

// The reading order, in seconds: the frame is ruled, the title engraved,
// the numerals and rims drawn, the sand strewn, then the bow passes each
// figure in pitch order and its sand finds its lines. After the first pass
// the bow keeps making its round, slower, one figure at a time.
constexpr float kFrameAt = 0.15f;
constexpr float kTitleAt = 0.70f;
constexpr float kNumeralAt = 1.00f;
constexpr float kRimAt = 1.10f;
constexpr float kScatterAt = 1.35f;
constexpr float kFirstBow = 1.75f;
constexpr float kBowStep = 0.30f;
constexpr float kSettle = 1.42f;
constexpr float kRoundAt = 7.30f;
constexpr float kRoundStep = 0.44f;
constexpr float kCreditAt = 7.60f;

float bowAt(size_t index) { return kFirstBow + kBowStep * (float)index; }
float roundAt(size_t index) { return kRoundAt + kRoundStep * (float)index; }

/** One figure's share of the round: the bow's pressure rises and falls
 *  across the first twelfth of the round's phase and is off for the rest. */
float roundSwell(float phase) {
  return phase < 1.0f / 12 ? std::sin(kPi * phase * 12) : 0.0f;
}
/** How far along the rim the bow has drawn in that twelfth. */
float roundTravel(float phase) { return std::min(phase * 12, 1.0f); }
/** How far the figure's sound has spread: it leaves the rim with the bow
 *  and dies away across two twelfths of the round. */
float roundSound(float phase) { return std::min(phase * 6, 1.0f); }
constexpr float kSoundSeconds = 2 * kRoundStep;

/** THE SOUND IS TWO WAVEFRONTS, the second leaving the rim a little
 *  after the first. Each spreads fast and slows as it goes, and its
 *  loudness falls with the distance it has spread, not with the bow's
 *  pressure: a ring that faded with the pressure alone would stand at its
 *  widest at full strength, a second rim rather than a sound. */
constexpr float kFrontLag = 0.26f;
constexpr float kSoundReach = 0.24f;
float frontSpread(float sound, int front) {
  const float lag = kFrontLag * (float)front;
  return std::clamp((sound - lag) / (1 - lag), 0.0f, 1.0f);
}
template <int Front>
float frontReach(float sound) {
  return sigil::motion::ease::outCubic(frontSpread(sound, Front));
}
template <int Front>
float frontLoudness(float sound) {
  const float spread = frontSpread(sound, Front);
  if (spread <= 0 || spread >= 1) return 0;
  return std::min(spread * 14, 1.0f) * std::pow(1 - spread, 2.4f);
}
/** A wavefront is a band of pressure, not a line: it is stroked wide and
 *  inked by a gradient across its width that is clear at both edges, so
 *  it never reads as a second rim ruled beside the first. */
struct Front {
  sigil::motion::Easing reach, loudness;
  float width, peak;
};
const std::array<Front, 2> kFronts{
    Front{frontReach<0>, frontLoudness<0>, 8.0f, 0.9f},
    Front{frontReach<1>, frontLoudness<1>, 5.0f, 0.65f}};

/** HOW THE PLATE IS SET. The tokens are its inks; the lettering is three
 *  hands — italic figures for the numerals, the same italic for the
 *  reference letters, an engraver's copperplate for the title and the
 *  signature — and every step of the scale is a multiple of the reference
 *  letter's size. */
StyleSheet plateSheet(const data::Json& inks) {
  Rule root = rule(":root");
  for (const auto& [name, colour] : inks.fields())
    root.var(name, colourOf(colour));
  return StyleSheet{
      root.fontFamily("Didot, Bodoni 72, serif")
          .fontStyle(FontStyle::Italic)
          .fontSize(33)
          .ink(var("ink")),
      rule("numeral").fontSize(1.15_rem).letterSpacing(0.5),
      rule("h1, footer")
          .fontFamily("Snell Roundhand, Apple Chancery, Baskerville, serif")
          .fontStyle(FontStyle::Normal)
          .fontWeight(400),
      rule("h1").fontSize(1.9_rem).letterSpacing(1),
      rule("footer").fontSize(0.62_rem).letterSpacing(0.3).ink(var("ink-soft")),
  };
}

}  // namespace

struct ChladniTab1 {
  sketch::kit::Document plate;
  StyleSheet sheet;
  std::vector<Figure> figures;
  float scale = 1, radius = 0;
  SkSize canvas{0, 0};

  /** THE ONE CLOCK. Everything that moves by itself reads it through a
   *  binding; only the sand is stepped, because a grain's hop depends on
   *  the bow as well as on its own flight. */
  sigil::motion::Animatable<float> clock = sigil::motion::animatable(0.0f);

  /** One figure's sand: its pool, the phase each grain shivers at, and
   *  which of its two stampings shows — 1 the baked one, 0 the live one. */
  struct Sand {
    std::shared_ptr<instancing::Pool> pool =
        std::make_shared<instancing::Pool>();
    std::vector<float> shiver;
    std::unique_ptr<sigil::motion::Animatable<float>> baked =
        std::make_unique<sigil::motion::Animatable<float>>(0.0f);
  };
  std::vector<Sand> sand;
  std::shared_ptr<instancing::CellSheet> marks;
  /** ONE PASS OF THE BOW, as the clock reads it: the pressure, 0 to 1,
   *  read by the drawing and by the sand alike; how far along the rim the
   *  contact has drawn; how far its sound has spread. */
  struct BowPass {
    Bound pressure, travel, sound;
  };
  std::vector<BowPass> firstPass, roundPass;
  /** THE SAND IS STAMPED TWICE, live and baked, and one of the two
   *  shows: the live stamping while the sand gathers, the baked one once
   *  every grain has landed, and the live one again for the figure the
   *  round is bowing, so its grains hop, until the bow moves on. The
   *  switch is a stepped value rather than a new description, because a
   *  node filled with a material recipe takes its bake again on every
   *  describe, however unchanged; the tree is described once more only
   *  when the sand has landed, so the baked stamping holds the grains at
   *  rest. */
  static constexpr size_t kNoFigure = SIZE_MAX;
  float allLanded = 0;
  bool gathering = true, describeAgain = false;
  size_t hopping = kNoFigure;

  Paint ink;
  Pattern foxing, foxingLow;
  /** Built once and held: a gradient or grained fill made afresh compares
   *  unequal to the one before, so a node described again with it would
   *  have its bake taken again. */
  Element paper;
  std::vector<Fill> fans, frontInks;

  // =========================================================================

  /** THE LEAF: rag paper with a tooth, foxed, darkening toward its edges
   *  and into the gutter, with the copper plate's recess pressed into it
   *  and the film of ink the wiping left inside. None of it moves, so it
   *  is baked once. */
  Element leaf() const {
    const data::Json& mark = plate["platemark"];
    const float left = (float)mark["left"].number() * scale;
    const float top = (float)mark["top"].number() * scale;
    const material::Color edge = colourOf(plate["ink"]["paper-edge"]);
    return box()
        .key("leaf")
        .cover()
        .fill(kit::grained(colourOf(plate["ink"]["paper"]), 0.16f, 0.013f))
        .children({
            box().cover().fill(foxing.material()),
            kit::at(box().fill(foxingLow.material()), 0, canvas.height() * 0.5f,
                    canvas.width() * 0.52f, canvas.height() * 0.5f),
            kit::at(box(), left, top,
                    (float)mark["right"].number() * scale - left,
                    (float)mark["bottom"].number() * scale - top)
                .fill(Fill::var("plate-tone"))
                // Lit from the upper left, a recess shades its upper-left
                // wall and catches the light on its lower-right one.
                .foreground(
                    styles::BevelEmboss{.depth = 2,
                                        .size = 3,
                                        .angleDeg = 305,
                                        .highlight = {1, 0.98f, 0.92f, 0.55f},
                                        .shadow = {edge.r * 0.6f, edge.g * 0.6f,
                                                   edge.b * 0.6f, 0.5f}}),
            box().cover().fill(
                kit::vignette(canvas, {edge.r, edge.g, edge.b, 0.26f}, 0.62f)),
            box().cover().fill(material::Paint::linearGradient(
                {0, 0}, {canvas.width() * 0.09f, 0},
                {{edge.r * 0.5f, edge.g * 0.5f, edge.b * 0.5f, 0.22f},
                 {edge.r, edge.g, edge.b, 0}},
                {.units = material::GradientUnits::Pixels})),
        })
        .cache(Cache::Texture);
  }

  /** THE FRAME'S DOUBLE RULE, the second hairline inside the first. */
  Element frame() const {
    const data::Json& rule = plate["frame"];
    const float left = (float)rule["left"].number() * scale;
    const float top = (float)rule["top"].number() * scale;
    const float right = (float)rule["right"].number() * scale;
    const float bottom = (float)rule["bottom"].number() * scale;
    const float gap = (float)rule["gap"].number() * scale;
    return box().cover().children(
        each(std::array{2.0f, 1.3f}, [&](float weight, size_t line) {
          const float inset = (float)line * gap;
          return kit::at(box(), left + inset, top + inset,
                         right - left - 2 * inset, bottom - top - 2 * inset)
              .key("frame" + std::to_string(line))
              .fill(Fill::none())
              .stroke(
                  spans::upTo(sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 880ms, .delay = std::chrono::duration<double, std::milli>(kFrameAt * 1000 + (float)line * 90), .ease = sigil::motion::ease::outQuint})),
                  stroke(weight, Fill::var("ink-line")));
        }));
  }

  /** The figure's own settle, from the bow's stroke to the sand at rest,
   *  as the fraction @p from to @p to of it. */
  Bound settled(size_t index, float from, float to) const {
    return sigil::motion::bind(clock, {.from = {bowAt(index) + from * kSettle, bowAt(index) + to * kSettle}, .clampFrom = true});
  }

  /** WHAT THE SAND DRAWS once it has found the still lines. */
  Element drawing(size_t index) const {
    const Figure& figure = figures[index];
    const std::string tag = "figure" + std::to_string(figure.number);
    switch (figure.kind) {
      case Kind::Star:
        return kit::disc(middle(), radius * kTip)
            .key(tag + "star")
            .shape(shapes::star(figure.points, figure.inner, figure.waist))
            .fill(ink)
            .opacity(settled(index, 0.58f, 0.94f))
            .cache(Cache::Texture);
      case Kind::Valleys: {
        // The engraved tone is a radial fan of combed strokes under the
        // sand, and the star's channel is cut out of it. The fan's spokes
        // are evenly spaced and open out toward the rim, where they would
        // read as a ruled band, so the fan fades out before the rim and
        // the combed grains alone carry the fur there.
        return kit::disc(middle(), radius)
            .key(tag + "fan")
            .shape(shapes::circle())
            .fill(Fill::none())
            .background(lines::RadialHatch{.strokeFill = fans[index],
                                           .spokes = figure.points * 120,
                                           .width = 0.85f,
                                           .holeFraction = figure.inner})
            .mask(by::outside(Region::path(
                shapes::star(figure.points, figure.inner,
                             figure.waist)(SkSize{2 * radius, 2 * radius}))))
            .opacity(settled(index, 0.52f, 0.98f))
            .cache(Cache::Texture);
      }
      case Kind::Arcs:
        // One nodal line per line the plate draws, each drawn on as the
        // figure settles.
        return box().cover().children(each(figure.linien, [&](const Linie&,
                                                              size_t line) {
          return kit::disc(middle(), radius)
              .key(tag + "line" + std::to_string(line))
              .shape(outlineOf(figure, line))
              .fill(Fill::none())
              .stroke(spans::upTo(settled(index, 0.55f, 0.98f)), stroke(2.6f))
              .opacity(settled(index, 0.58f, 0.94f));
        }));
    }
    return box();
  }

  /** ONE PASS OF THE BOW over a figure: the contact drawn along the rim
   *  under its pressure, and the sound the disc sends out, 0 at the rim
   *  and 1 where it has died away. */
  Element bowed(const std::string& key, const BowPass& pass) const {
    PathFormat contact =
        stroke(3.0f, Fill::currentInk(), PathFormat::Align::Inner);
    contact.trimEnd = 0.065f;
    contact.trimPhase = pass.travel;
    return box()
        .cover()
        .children({kit::disc(middle(), radius)
                       .key(key + "contact")
                       .shape(shapes::circle())
                       .fill(Fill::none())
                       .stroke(contact)
                       .opacity(Bound(pass.pressure).target(0, 0.66f))
                       .cache(Cache::None)})
        .children(each(kFronts, [&](const Front& front, size_t at) {
          return kit::disc(middle(), radius)
              .key(key + "sound" + std::to_string(at))
              .shape(shapes::circle())
              .fill(Fill::none())
              .stroke(stroke(front.width, frontInks[at]))
              .scale(
                  Bound(pass.sound).map(front.reach).target(1, 1 + kSoundReach))
              .opacity(
                  Bound(pass.sound).map(front.loudness).target(0, front.peak))
              .cache(Cache::None);
        }));
  }

  bool stamping(size_t index) const { return gathering || index == hopping; }

  /** A figure's centre in its own cell. */
  SkPoint middle() const { return {kCellReach * radius, kCellReach * radius}; }

  /** ONE CELL OF THE PLATE: rim, drawing, sand, the bow, the numeral and
   *  the reference letters, which rise into place as the figure settles. */
  Element figureCell(size_t index) const {
    const Figure& figure = figures[index];
    const std::string tag = "figure" + std::to_string(figure.number);
    const float side = 2 * kCellReach * radius;
    return kit::at(box(), figure.centre.fX - side * 0.5f,
                   figure.centre.fY - side * 0.5f, side, side)
        .key(tag)
        .children({
            kit::disc(middle(), radius)
                .key(tag + "rim")
                .shape(shapes::circle())
                .fill(Fill::none())
                .stroke(
                    spans::upTo(sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 620ms, .delay = std::chrono::duration<double, std::milli>(kRimAt * 1000 + (float)index * 26), .ease = sigil::motion::ease::outQuad})),
                    stroke(1.5f, Fill::var("ink-line"))),
            drawing(index),
            kit::disc(middle(), radius + kSandMargin)
                .key(tag + "sand")
                .opacity(
                    sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 400ms, .delay = std::chrono::duration<double, std::milli>(kScatterAt * 1000)}))
                .children({
                    box()
                        .cover()
                        .key(tag + "sandbaked")
                        .opacity(sigil::motion::bind(sand[index].baked.get()))
                        .children({instancing::instances(
                            marks, sand[index].pool, instancing::Mode::Data)})
                        .cache(Cache::Texture),
                    box()
                        .cover()
                        .key(tag + "sandlive")
                        .opacity(sigil::motion::bind(sand[index].baked.get(), {.to = {1.0f, 0.0f}}))
                        .children({instancing::instances(
                            marks, sand[index].pool, instancing::Mode::Live)}),
                }),
            bowed(tag + "first", firstPass[index]),
            // The round's waves repeat on a folded phase, so the pass is
            // shown only from its first turn on.
            box()
                .cover()
                .opacity(sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 1ms, .delay = std::chrono::duration<double, std::milli>(roundAt(index) * 1000)}))
                .children({bowed(tag + "round", roundPass[index])}),
            text(std::to_string(figure.number) + ".")
                .role("numeral")
                .key(tag + "numeral")
                .centerAt({middle().fX - 0.82f * radius,
                           middle().fY - 1.15f * radius})
                .opacity(
                    sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 360ms, .delay = std::chrono::duration<double, std::milli>(kNumeralAt * 1000 + (float)index * 22)})),
            box().cover().children(
                each(figure.letters,
                     [&](const Letter& letter, size_t at) {
                       return text(letter.glyph)
                           .role("letter")
                           .key(tag + "letter" + std::to_string(at))
                           .centerAt(kUnit.about(middle()).px(
                               letter.bearing, radius * letter.radius))
                           .opacity(settled(index, 0.84f, 0.99f))
                           .translateY(settled(index, 0.84f, 0.99f)
                                           .map(sigil::motion::ease::outQuad)
                                           .invert()
                                           .target(0, 7));
                     })),
        });
  }

  Element describe() const {
    const data::Json& title = plate["title"];
    const data::Json& credit = plate["credit"];
    const data::Json& rule = plate["frame"];
    const float gap = (float)rule["gap"].number() * scale;
    const SkRect innerFrame =
        SkRect::MakeLTRB((float)rule["left"].number() * scale + gap,
                         (float)rule["top"].number() * scale + gap,
                         (float)rule["right"].number() * scale - gap,
                         (float)rule["bottom"].number() * scale - gap);
    const auto at = [&](const data::Json& place) {
      return SkPoint{(float)place["centre"][0].number() * scale,
                     (float)place["centre"][1].number() * scale};
    };
    return box()
        .width(canvas.width())
        .height(canvas.height())
        .applyStyleSheet(sheet)
        .children({
            paper,
            frame(),
            document::h1(Utf8(title["words"].text()))
                .key("title")
                .textFx(Track{
                    .effect = textFx::typeOn(),
                    .delay = sigil::motion::stagger({0ms, 520ms}), .duration = 60ms,
                    .progress = sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 620ms, .delay = std::chrono::duration<double, std::milli>(kTitleAt * 1000), .ease = sigil::motion::ease::linear})})
                .centerAt(at(title)),
        })
        .children(each(
            figures,
            [&](const Figure&, size_t index) { return figureCell(index); }))
        .children({
            // The signature is small, as an engraver's is, and stands in the
            // frame's lower right corner, in from the inner rule and clear
            // of the curve of the last disc above it.
            document::footer(Utf8(credit["words"].text()))
                .key("credit")
                .right(canvas.width() - innerFrame.right() + kCreditClear)
                .bottom(canvas.height() - innerFrame.bottom() + kCreditClear)
                .opacity(
                    sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 700ms, .delay = std::chrono::duration<double, std::milli>(kCreditAt * 1000)})),
        });
  }

  // =========================================================================

  /** THE SAND, stepped: every grain's flight toward its line, then the
   *  hop no flight holds — a shiver that decays across the walk, and a
   *  nudge while the bow is on the figure's rim. Sand lies still until
   *  the bow first reaches its disc, and after it has landed only the
   *  figure the round is bowing moves. */
  void stepSand(float seconds) {
    static const sigil::motion::Easing bounce = ease::outBounce();
    if (gathering && seconds >= allLanded) {
      // One last step lands every grain exactly on its line.
      for (Sand& grains : sand) {
        grains.pool->fly(allLanded + 1, bounce);
        grains.pool->commit();
      }
      for (Sand& grains : sand) *grains.baked = 1.0f;
      gathering = false;
      describeAgain = true;
    }
    if (!gathering) {
      const size_t bowing =
          seconds < kRoundAt
              ? kNoFigure
              : (size_t)((seconds - kRoundAt) / kRoundStep) % figures.size();
      if (bowing != hopping) {
        if (hopping != kNoFigure) {
          sand[hopping].pool->fly(allLanded + 1, bounce);
          *sand[hopping].baked = 1.0f;
        }
        hopping = bowing;
        if (hopping != kNoFigure) *sand[hopping].baked = 0.0f;
      }
    }
    for (size_t index = 0; index < sand.size(); ++index) {
      if (!stamping(index)) continue;
      Sand& grains = sand[index];
      grains.pool->fly(seconds, bounce);
      const auto flights = std::as_const(*grains.pool).flights();
      auto positions = grains.pool->positions();
      const float pressure =
          firstPass[index].pressure.value().apply(seconds) +
          (seconds >= roundAt(index)
               ? roundPass[index].pressure.value().apply(seconds)
               : 0.0f);
      const float nudge = pressure * 1.8f;
      for (size_t at = 0; at < flights.size(); ++at) {
        const instancing::Pool::Flight& flight = flights[at];
        const float walked =
            std::clamp((seconds - flight.start) / flight.duration, 0.0f, 1.0f);
        const bool walking = walked > 0 && walked < 1;
        const float hop = (walking ? (1 - walked) * 5.6f : 0.0f) + nudge;
        const float phase = grains.shiver[at];
        positions[at].offset(
            std::sin(seconds * 21 + phase) * hop,
            std::cos(seconds * 17 + phase * 1.7f) * hop * 0.8f);
      }
    }
  }

  void setup(sketch::SketchContext& ctx) {
    plate = sketch::kit::Document(ctx, "data/plate.json");
    const data::Json& scan = plate["scan"];
    scale = (float)scan["scale"].number();
    radius = (float)scan["radius"].number() * scale;
    canvas = {(float)scan["width"].number() * scale,
              (float)scan["height"].number() * scale};
    figures = readFigures(plate["figures"], scale);
    sheet = plateSheet(plate["ink"]);

    // The still is the settled plate, the signature in, and the round's
    // bow on figure 8's rim with both wavefronts of its sound spreading.
    sketch::kit::stage(ctx, {.size = canvas,
                             .captureAt = 10.72,
                             .background = colourOf(plate["ink"]["paper"])});

    // Ink on rag paper is never flat: luminance noise shades the fill
    // without moving its hue.
    ink = Paint::blend(
        {{Paint::solid(colourOf(plate["ink"]["ink"])), material::BlendMode::Source},
         {Paint::recipe(field::grain(0.09f, 3, 4.0f, 0.35f)),
          material::BlendMode::SoftLight}});
    // Sparse, and on a tile large enough that its repeat is not the
    // strongest mark on the page.
    foxing =
        material::pattern::speckle(640, 22, 1.4f, 5.0f, {colourOf(plate["ink"]["fox"])});
    foxing.seed(17);
    foxingLow = material::pattern::speckle(520, 14, 2.0f, 7.0f,
                                  {colourOf(plate["ink"]["fox-low"])});
    foxingLow.seed(53);

    // The four engraved marks the sand is stamped with, baked once. A bake
    // inherits nothing, so each states its own ink.
    const material::Color grain = colourOf(plate["ink"]["ink"]);
    const auto mark = [&](float width, float height, float alpha) {
      return box()
          .width(width)
          .height(height)
          .borderRadius({height * 0.5f})
          .fill(Fill::color({grain.r, grain.g, grain.b, alpha}));
    };
    marks = std::make_shared<instancing::CellSheet>(3.0f);
    marks->cell(mark(8.6f, 2.3f, 0.94f), {10, 4});
    marks->cell(mark(6.0f, 1.7f, 0.88f), {8, 3});
    marks->cell(mark(3.1f, 3.1f, 0.90f), {5, 5});
    marks->cell(mark(16.0f, 1.1f, 0.86f), {18, 3});

    paper = leaf();
    const material::Color fur = colourOf(plate["ink"]["fur"]);
    for (const Figure& figure : figures) {
      const auto across = [&](float share) {
        return figure.inner + share * (1 - figure.inner);
      };
      fans.push_back(toFill(material::Paint::radialGradient(
          {radius, radius}, radius,
          {{across(0.2f), material::withAlpha(fur, 0)},
           {across(0.6f), fur},
           {0.95f, material::withAlpha(fur, 0)}},
          {.units = material::GradientUnits::Pixels})));
    }

    const material::Color line = colourOf(plate["ink"]["ink-line"]);
    for (const Front& front : kFronts) {
      const float outer = radius + front.width * 0.5f;
      frontInks.push_back(toFill(material::Paint::radialGradient(
          {radius, radius}, outer,
          {{(radius - front.width * 0.5f) / outer,
            material::withAlpha(line, 0)},
           {radius / outer, line},
           {1.0f, material::withAlpha(line, 0)}},
          {.units = material::GradientUnits::Pixels})));
    }

    sand.resize(figures.size());
    for (size_t index = 0; index < figures.size(); ++index) {
      Sand& grains = sand[index];
      seedSand(figures[index], radius, bowAt(index),
               {radius + kSandMargin, radius + kSandMargin}, *grains.pool,
               grains.shiver);
      for (const instancing::Pool::Flight& flight :
           std::as_const(*grains.pool).flights())
        allLanded = std::max(allLanded, flight.start + flight.duration);
      const float first = bowAt(index) - 0.22f;
      firstPass.push_back(
          {.pressure = sigil::motion::bind(clock, {.from = {first, first + 0.62f}, .clampFrom = true, .envelope = sigil::motion::envelope::cosine()}),
           .travel = sigil::motion::bind(clock, {.from = {first, first + 0.62f}, .clampFrom = true}),
           .sound = sigil::motion::bind(clock, {.from = {first, first + kSoundSeconds}, .clampFrom = true})});
      const float round = roundAt(index);
      const float cycle = round + 12 * kRoundStep;
      roundPass.push_back(
          {.pressure = sigil::motion::bind(clock, {.from = {round, cycle}, .envelope = sigil::motion::envelope::shaped(roundSwell)}),
           .travel = sigil::motion::bind(clock, {.from = {round, cycle}, .envelope = sigil::motion::envelope::shaped(roundTravel)}),
           .sound = sigil::motion::bind(clock, {.from = {round, cycle}, .envelope = sigil::motion::envelope::shaped(roundSound)})});
    }

    ctx.engine.add([this, &ticker = ctx.engine] {
      const float seconds = (float)ticker.elapsed();
      clock = seconds;
      stepSand(seconds);
    });
    for (Sand& grains : sand) grains.pool->fly(0);
    ctx.composer.render(describe());
  }

  /** Described once more, when the sand has landed. */
  void update(double, sketch::SketchContext& ctx) {
    if (!describeAgain) return;
    describeAgain = false;
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(ChladniTab1, "Study · Science",
             "Chladni's Tab. I (1786) — twelve bowed discs whose sand finds "
             "their nodal lines, printed from the copper plate")
