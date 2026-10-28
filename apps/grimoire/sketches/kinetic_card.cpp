/** @file
 * kinetic_card — the fx preset specimen: every stock text effect on one
 * sheet, each caught mid-cascade with its own schedule drawn under it.
 */

// NINE PRESETS, ONE CLOCK. `kit/Kinetic.h` ships nine comparable
// `TextEffect` values over the kernel's `textFx()` seam, and a still of a
// moving effect is worth nothing unless it is caught while the effect is
// moving — so every cell here is driven by ONE wrapping phase, and the
// declared moment falls where every cascade is mid-flight.
//
// The rows, in the order the header names them:
//
//   · RISE, SLIDE, POP, SPIN IN, SCATTER — the five that MOVE glyphs off
//     the pen positions the layout gave them, which is what puts a live
//     run on the subpixel grid.
//   · TYPE ON, AXIS SWEEP, TINT — the three that do not: they touch
//     coverage, an outline coordinate and colour, so their letters keep
//     whole-pixel origins however hard they run.
//   · WAVE LOOP — the one that never lands. It reads the same wrapping
//     phase as a loop rather than as an entrance, which is why its meter
//     never fills.
//   · MIX, then THREE SCHEDULES — `textFx::mix` evaluates every operand at
//     the same local time and composes them by the algebra stacked tracks
//     use: offsets and rotation ADD, scale and alpha MULTIPLY, so a
//     scatter and a tint run at once rather than one after the other.
//     The three after it keep the scatter and change only WHO GETS A BEAT
//     WHEN: the stagger's origin (`StaggerFrom::Last`, `Edges`) and an
//     `ease` passing the linear ramp of delays through a curve, which
//     crowds the early units and spreads the tail. The per-unit motion is
//     untouched by either.
//
// THE METER UNDER EACH CELL is `kit::trackMeter`: one cell per beat of the
// track, at that beat's own laid-out rect, filled by that beat's local
// time. It is `Composer::beatsOf` drawn with nothing in between, so the
// bars and the letters cannot disagree about the schedule — which is the
// only way to read a cascade off a still.
//
// AXIS SWEEP IS GATED. A driven axis is honoured only where it does not
// move advances: `wght` does, so the runtime would refuse it and draw at
// the shaped face. `GRAD` is the axis that exists for exactly this — a
// grade is weight without width — and the cell is set in a face that
// carries one.
//
// TINT READS BACKWARDS ON PURPOSE. `GlyphModifier::colorMultiplier` multiplies,
// and a multiplier only takes a colour toward black, so the element is set in
// the DESTINATION and the effect divides down toward the origin. The
// arguments still read in time order.
//
// EDIT THESE FIRST
//   kPeriod    — seconds per pass of the shared phase. Every cascade's
//                master maps onto exactly one of these.
//   kCascade   — the spread every preset beats on, so the nine differ in
//                their effect and in nothing else; the last three cells
//                lay its delays out from another origin.
//   kSpecimen  — the size the specimens are set at.

// TAGS: Typography/Effects, Motion/Transitions

#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Presets.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Face.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kSceneSize{1180, 1180};

constexpr float kMargin = 56;
constexpr float kSpecimen = 29;
constexpr float kBodyH = 100;

/// Seconds per pass of the shared phase.
constexpr double kPeriod = 3.0;

constexpr material::Color kGround{0.043f, 0.043f, 0.058f, 1};
constexpr material::Color kAccent{0.980f, 0.360f, 0.250f, 1};
/// Where `textFx::tint` wipes FROM. The specimen is set in kAccent, its
/// DESTINATION, and the effect multiplies down to this — so every channel
/// here has to be darker than the destination's, since a multiplier
/// cannot brighten and a channel it could not reach simply holds.
constexpr material::Color kPale{0.180f, 0.090f, 0.060f, 1};

/** The one cascade every preset beats on, so the nine differ in their
 *  effect and in nothing else. Its span is what the shared phase maps
 *  onto — one wrap is exactly one pass of the schedule. */
const sigil::motion::Tween<float> kCascade{
    .duration = std::chrono::duration<double, std::milli>(620),
    .delay =
        sigil::motion::stagger(std::chrono::duration<double, std::milli>(110))};

/** The cascade with its delays laid out from @p origin, through
 *  @p distribution when one is named: the spread every cell beats on with
 *  only its order changed. */
sigil::motion::Tween<float> cascadeFrom(
    sigil::motion::StaggerFrom origin,
    sigil::motion::Easing distribution = nullptr) {
  return {.duration = kCascade.duration,
          .delay = sigil::motion::stagger(
              std::chrono::duration<double, std::milli>(110),
              {.from = origin, .ease = std::move(distribution)})};
}

/** The cells, in the order the header reads them. `key` is what the
 *  meter under the cell resolves the schedule from; `over` is what this
 *  cell's specimen changes about the register — a face, a colour — and
 *  nothing for most. */
struct Row {
  const char* key;
  const char* call;
  const char* note;
  const char* word;
  TextEffect effect;
  weave::Type over;
  sigil::motion::Tween<float> schedule = kCascade;
};

sigil::weave::Face display() {
  return weave::ports::face({"Helvetica Neue", "Inter", "Helvetica", "Arial"},
                            sigil::weave::FaceStyle{.weight = 700});
}
/** The face the axis cell is set in: San Francisco carries a GRAD axis,
 *  which is advance-invariant and therefore the one a draw-time drive is
 *  allowed to move. */
sigil::weave::Face graded() {
  return sketch::kit::houseFace(sketch::kit::Voice::Interface, 500);
}

/** The specimen's own class: the display face at kSpecimen, tracked, in
 *  whatever ink is in force. */
sigil::compose::StyleSheet sheetClasses() {
  return sigil::compose::StyleSheet{
      sigil::compose::rule(".specimen")
          .font({.face = display(), .size = kSpecimen, .letterSpacing = 1.5f})};
}

}  // namespace

namespace {

struct KineticCard {
  /// The one clock: a wrapping [0,1) every cascade's master reads.
  motion::Animatable<float> phase = motion::animatable(0.0f);

  void setup(sketch::Scene2DContext& ctx) {
    // The feature sheet with room for the animated type specimens.
    sketch::kit::stage(
        ctx,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .sheet = rule(":root")
                      .var("spacing-margin-horizontal", kMargin)
                      .var("spacing-margin-top", kMargin - 12)
                      .var("spacing-margin-bottom", 30)
                      .var("spacing-caption-gap", 8),
         .size = kSceneSize,
         .captureAt = kPeriod * 0.5});
    // MID-CASCADE. The master maps onto each track's OWN span, so one
    // fraction of the period is the same fraction of every schedule
    // however many letters a word has. At half way the head of every word
    // has landed and its tail is still in flight — the ladder a meter
    // exists to show, and late enough that each word is legible as the
    // word it is.
    phase = 0;
    ctx.engine.timer([this, &ticker = ctx.engine] {
      const double t = ticker.elapsed().count();
      phase = motion::phase(motion::Duration(t), motion::Duration(kPeriod));
    });
    // THE METERS ARE A READ-BACK: they resolve from the layout the last
    // draw left standing, so the page is described once for the specimens
    // and again for the instruments beside them.
    ctx.composer.render(describe(ctx));
    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::Scene2DContext& ctx) {
    // A meter that moves with its cascade is re-read every frame; that is
    // the whole cost of an instrument, and it is why one is never in the
    // paint loop of anything that ships.
    ctx.composer.render(describe(ctx));
  }

  /** One cell: the specimen wearing its own effect on the shared
   *  cascade, captioned with the call that made it. The meter is not here
   *  — it is drawn over the whole composition, because `beatsOf` answers
   *  in the composer's space. */
  sketch::kit::ComparisonCase cell(const Row& row, float width) {
    return {.title = row.word,
            .control = row.call,
            .figure = box().width(width).height(kBodyH).padding(22, 4).children(
                {text(row.word)
                     .styleClass("specimen")
                     .font(row.over)
                     .key(row.key)
                     .width(width - 8)
                     .textFx({.effect = row.effect,
                              .tween = row.schedule,
                              .progress = phase})}),
            .note = row.note};
  }

  Element describe(sketch::Scene2DContext& ctx) {
    const Composer& composer = ctx.composer;

    static const Row kRows[13] = {
        {"rise", "textFx::enter(textFx::rise(26))",
         "up from below, fading in over the first "
         "third of its beat",
         "RISE", textFx::enter(textFx::rise(26))},
        {"slide", "textFx::enter(textFx::slide(-32))",
         "in from the side; negative is from the "
         "left",
         "SLIDE", textFx::enter(textFx::slide(-32))},
        {"pop", "textFx::enter(textFx::pop(0.35, 1.70158))",
         "scale overshoot — "
         "back.out(1.7)",
         "POP", textFx::enter(textFx::pop(0.35f, 1.70158f))},
        {"spin", "textFx::enter(textFx::spinIn(70, 14))",
         "a tumble: rotation and a rise, "
         "eased out together",
         "SPIN IN", textFx::enter(textFx::spinIn(70, 14))},
        {"scatter", "textFx::enter(textFx::scatter(40, 24))",
         "each glyph from its own seeded "
         "offset and lean",
         "SCATTER", textFx::enter(textFx::scatter(40, 24))},
        {"typeon", "textFx::typeOn()",
         "absent, then simply there — "
         "coverage only, no displacement",
         "TYPE ON", textFx::enter(textFx::typeOn())},
        {"axis",
         "GRAD axis · 400 → 1000",
         "a grade swept at draw time; advance-invariant, so nothing moves",
         "AXIS SWEEP",
         textFx::enter(textFx::variableAxisSweep("GRAD", 400, 1000)),
         {.face = graded()}},
        {"tint",
         "textFx::tint(pale, accent)",
         "the element is set in the destination and the effect multiplies "
         "down to the origin",
         "TINT",
         textFx::tint({.from = kPale, .to = kAccent}),
         {.ink = kAccent}},
        {"wave", "textFx::waveLoop(0.10, 0.5)",
         "the one that never lands: a loop on the same wrapping phase, so "
         "its meter never fills",
         // The loop reads the master as a phase rather than as an
         // entrance, so every glyph takes the SAME master and the
         // travelling wave comes from the glyph's own index inside the
         // effect.
         "WAVE LOOP", textFx::waveLoop(0.10f, 0.5f)},
        {"mix",
         "textFx::mix(scatter, tint)",
         "both at the same local time: the offset adds, the colour "
         "multiplies",
         "MIX",
         textFx::mix(textFx::enter(textFx::scatter(40, 24)),
                     textFx::tint({.from = kPale, .to = kAccent})),
         {.ink = kAccent}},
        {"last",
         "stagger(110, {.from = Last})",
         "the final glyph starts first; the cascade runs backward",
         "BACKWARD",
         textFx::enter(textFx::scatter(40, 24)),
         {},
         cascadeFrom(motion::StaggerFrom::Last)},
        {"edges",
         "stagger(110, {.from = Edges})",
         "both ends start together and meet in the middle",
         "EDGES",
         textFx::enter(textFx::scatter(40, 24)),
         {},
         cascadeFrom(motion::StaggerFrom::Edges)},
        {"eased",
         "stagger(110, {.ease = t²})",
         "the delays crowd early and spread late; each motion is unchanged",
         "EASED",
         textFx::enter(textFx::scatter(40, 24)),
         {},
         cascadeFrom(motion::StaggerFrom::First,
                     [](float progress) { return progress * progress; })},
    };

    std::vector<sketch::kit::ComparisonCase> moving;
    for (int row = 0; row < 5; ++row) moving.push_back(cell(kRows[row], 200));
    std::vector<sketch::kit::ComparisonCase> stationary;
    for (int row = 5; row < 8; ++row)
      stationary.push_back(cell(kRows[row], 340));
    std::vector<sketch::kit::ComparisonCase> rescheduled;
    for (int row = 9; row < 13; ++row)
      rescheduled.push_back(cell(kRows[row], 250));
    Element sheet = sketch::kit::page(
        {.title = "A vocabulary of moving type",
         .subtitle = "Nine presets, a mix and three schedules on one "
                     "three-second clock · each meter reads the glyph's "
                     "actual share of the cascade",
         .footer = "Each entrance spans its own word, so half a cycle means "
                   "half a cascade. The grade axis changes outlines without "
                   "changing advances."},
        box().column().gap(26).children(
            {document::h2("01 / MOVE THE GLYPHS"),
             sketch::kit::comparison(
                 {.cases = std::move(moving), .measure = 1068, .gap = 17}),
             document::h2("02 / KEEP THE PEN POSITIONS"),
             sketch::kit::comparison(
                 {.cases = std::move(stationary), .measure = 1068, .gap = 24}),
             document::h2("03 / KEEP MOVING"),
             box().row().gap(28).children(
                 {sketch::kit::comparison(
                      {.cases = {cell(kRows[8], 340)}, .measure = 340}),
                  box().width(480).column().gap(16).children(
                      {document::h2("READING THE CASCADE"),
                       document::caption(
                           "A full meter means that glyph has completed its "
                           "beat. A partial meter marks a glyph in motion; an "
                           "empty one is still waiting.")
                           .width(440),
                       document::caption(
                           "The wave reads the shared clock as a loop. It "
                           "changes neither the entrance schedule nor the "
                           "positions chosen by paragraph layout.")
                           .width(440)})}),
             document::h2("04 / MIX, THEN CHANGE WHO STARTS"),
             sketch::kit::comparison({.cases = std::move(rescheduled),
                                      .measure = 1068,
                                      .gap = 22})}));

    Element root = stack()
                       .applyStyleSheet(sheetClasses())
                       .fill(Fill::color(kGround))
                       .children({std::move(sheet)});
    // One meter per cell, over the whole composition: the rects are in the
    // composer's space, so the bars land on the letters wherever the sheet
    // put them.
    for (const Row& row : kRows)
      root.children(
          {kit::trackMeter(composer, row.key, 0, kAccent,
                           {kAccent.r, kAccent.g, kAccent.b, 0.14f},
                           {.where = kit::MeterPlacement::Where::Under,
                            .thickness = 3.0f,
                            .gap = 7.0f,
                            .trim = 1.5f})
               .inset(0)});
    return root;
  }
};

}  // namespace

SIGIL_SKETCH_AS(KineticCard, "kinetic_card", "Specimen",
                "every stock text effect, caught mid-cascade with its "
                "schedule under it, then a scatter mixed with a tint and run "
                "from the end, from both edges and through eased delays")
