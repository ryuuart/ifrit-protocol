/** @file
 * gerstner grid — Capital 1962, the mobile grid run: a modular
 * typographic system moving through its own configurations.
 */

// The modular-grid study: Karl Gerstner's "mobile grid" for Capital
// magazine (1962), as published in Designing Programmes (1964).
//
// The programme, verbatim: the measure is 58 UNITS wide, the unit being
// 10 points — the size of the base typeface including its lead. 58 is
// the number that works because there are always TWO units between
// columns:
//
//     1 column   58
//     2 columns  2 x 28 + 1 x 2  = 58
//     3 columns  3 x 18 + 2 x 2  = 58
//     4 columns  4 x 13 + 3 x 2  = 58
//     5 columns  5 x 10 + 4 x 2  = 58
//     6 columns  6 x  8 + 5 x 2  = 58
//
// That is the whole design. Gerstner's point was that a grid is not a
// drawing, it is a PROGRAMME — a rule that generates its own layouts —
// and the honest way to demonstrate one in a layout engine is to run it:
// this scene steps through all six configurations on a loop, reflowing
// the same copy into each, with the arithmetic printed underneath.
//
// The step is a SNAP, not a morph, and deliberately: these are six
// alternative layouts, not frames of one. What animates is the entrance —
// each column enters on a stagger — plus the reading index sweeping the
// baseline grid, which is the one continuous thing on the page.
//
// Palette from the period's process: paper, one black, one warm red, and
// the non-printing blue a grid was drawn in.
//
// EDIT THESE FIRST
//   kUnit      — the screen size of Gerstner's 10-point unit. The measure,
//                the field and every column width are multiples of it.
//   kConfigs   — the six configurations, each with the body size its
//                measure is set at and the number of copy blocks that runs
//                that measure to the foot.
//   kHoldSecs  — how long a configuration stands before the programme
//                steps to the next.
//   kSweepSecs — one pass of the reading index down the field.

#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;

namespace field = sigil::material::field;
namespace motion = sigil::motion;
namespace mskia = sigil::material::skia;
namespace pattern = sigil::material::pattern;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace gerstner {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

constexpr SkColor4f kPaper = hex(0xEDEAE3);
constexpr SkColor4f kPaperLo = hex(0xDCD7CB);
constexpr SkColor4f kInk = hex(0x16151A);
constexpr SkColor4f kInkSoft = hex(0x55525A);
constexpr SkColor4f kRed = hex(0xD8442F);
constexpr SkColor4f kBlue = hex(0x2C4CA8);  // the non-printing grid blue

// The measure is 58 units. The unit here is a screen unit, not 10pt —
// but every ratio below is Gerstner's.
constexpr int kUnits = 58;
constexpr int kRows = 34;       // the vertical field, same unit
constexpr float kUnit = 12.6f;  // 58 * 12.6 = 730.8
constexpr float kFieldW = kUnits * kUnit;
constexpr float kFieldH = kRows * kUnit;
constexpr float kFieldX = 84;
constexpr float kFieldY = 92;

/// One pass of the reading index down the field.
constexpr double kSweepSecs = 5.2;
/// How long one configuration holds before the programme steps.
constexpr double kHoldSecs = 2.6;

/** One configuration of the programme: n columns of `width` units with
 *  two units of gutter between them, summing to 58. */
struct Config {
  int columns, width, gutter;
  const char* arithmetic;
  float size;  // the body size this measure is set at
  // How many copy blocks it takes to run this measure to the foot of the
  // field. A wide column swallows a block in three lines and a narrow one
  // takes thirteen, so the number is per configuration; the field clips,
  // the way a page does when the story runs on.
  int blocks;
};
inline constexpr Config kConfigs[] = {
    {1, 58, 2, "58", 15.0f, 9},
    {2, 28, 2, "2 \xc3\x97 28 + 1 \xc3\x97 2", 13.5f, 6},
    {3, 18, 2, "3 \xc3\x97 18 + 2 \xc3\x97 2", 12.0f, 5},
    {4, 13, 2, "4 \xc3\x97 13 + 3 \xc3\x97 2", 10.5f, 5},
    {5, 10, 2, "5 \xc3\x97 10 + 4 \xc3\x97 2", 9.0f, 4},
    {6, 8, 2, "6 \xc3\x97 8 + 5 \xc3\x97 2", 9.0f, 4},
};
inline constexpr int kConfigCount =
    (int)(sizeof(kConfigs) / sizeof(kConfigs[0]));

/** Left edge of column `i`, in units. */
inline float columnUnit(const Config& c, int i) {
  return (float)(i * (c.width + c.gutter));
}

/** The copy the programme reflows. Gerstner's own argument, in our words:
 *  a grid is a rule, and a rule that cannot be run is a drawing. */
inline const char* kBody[] = {
    "A grid is not a drawing. It is a rule, and a rule you cannot run is "
    "only a picture of a rule. Gerstner set the measure at fifty-eight "
    "units because fifty-eight is the number that divides cleanly into "
    "one, two, three, four, five and six columns while always leaving two "
    "units of air between them.",
    "The unit is ten points: the body size of the text with its lead "
    "already counted. Every column width in the programme is therefore an "
    "exact number of lines tall and an exact number of units wide, and no "
    "measurement in the magazine is ever arbitrary.",
    "What the designer chooses is not a layout but a configuration. The "
    "page is set by picking a number from one to six; the arithmetic "
    "underneath the page does the rest, and it does it the same way every "
    "time, which is the only reason a monthly magazine can hold a shape "
    "for years.",
    "The grid looks complicated to anyone who does not know the key. To "
    "the initiate it is simple, and very nearly inexhaustible.",
};
inline constexpr int kBodyCount = (int)(sizeof(kBody) / sizeof(kBody[0]));

}  // namespace gerstner

struct GerstnerGrid final : sketch::Sketch {
  // THE BAKE IS THE IDENTITY, so the two ruled fields are held here rather
  // than minted inside describe(): the programme re-describes on every
  // step, and a freshly constructed Pattern carries no bake, so a
  // per-describe one would re-render its tile six times a loop.
  Pattern unitRule;
  Pattern emphasisRule;
  choreograph::Output<float> sweep{0};
  int config = 3;  // the four-column setting, Capital's default
  int shownConfig = -1;
  double nextStep = 0.0;

  // The scene steps configurations every 2.6 s, so the captured still has to
  // name a moment or it lands wherever the harness happens to stop. This
  // falls in the middle of the opening four-column hold: settled after the
  // entrance, well clear of the first step.

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 1.5,
                             .background = SkColor4f{0, 0, 0, 1}});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    // The grid, in the blue a grid was drawn in: every unit ruled faintly
    // both ways, and over it every second column line and every fifth
    // baseline struck harder. Two tiles rather than ninety-three boxes.
    const auto blue = [](float alpha) {
      return sigil::material::Color{gerstner::kBlue.fR, gerstner::kBlue.fG,
                                    gerstner::kBlue.fB, alpha};
    };
    unitRule = Pattern(pattern::gridLines(gerstner::kUnit, gerstner::kUnit,
                                          0.5f, blue(0.07f)));
    emphasisRule = Pattern(pattern::gridLines(
        gerstner::kUnit * 2.0f, gerstner::kUnit * 5.0f, 0.5f, blue(0.16f)));
    sweep = 0;
    config = 3;
    shownConfig = -1;
    // hold the opening configuration for one beat so a still frame
    // lands on a real setting rather than mid-step
    nextStep = gerstner::kHoldSecs;
    ticker.add([this, &ticker](double) {
      const double t = ticker.elapsed();
      // The reading index: one pass down the field every kSweepSecs.
      sweep = gerstner::kFieldY +
              gerstner::kFieldH * motion::phase(t, gerstner::kSweepSecs);
      return true;
    });
    composer.render(describe());
  }

  /** Stepping the programme is a DATA change, so it re-describes and the
   *  reconciler diffs — which is also the honest way to show that the six
   *  configurations are six layouts, not six frames of one. */
  void update(double elapsed, sketch::SketchContext& ctx) override {
    Composer& composer = ctx.composer;
    if (elapsed < nextStep && shownConfig == config) return;
    if (elapsed >= nextStep) {
      nextStep = elapsed + gerstner::kHoldSecs;
      config = (config + 1) % gerstner::kConfigCount;
    }
    shownConfig = config;
    composer.render(describe());
  }

  // ------------------------------------------------------------------

  /** The grid itself, drawn in the blue a grid was drawn in: every unit
   *  ruled faintly both ways, and over it the emphasis — every second
   *  column line and every fifth baseline — struck harder.
   *
   *  Two materials rather than ninety-three boxes. A ruled field is a
   *  PATTERN, and stating it as one means the pitch is a number the
   *  material reads rather than a loop the tree carries. */
  Element gridPlate() {
    namespace g = gerstner;
    return stack()
        .left(g::kFieldX)
        .top(g::kFieldY)
        .width(Dim(g::kFieldW))
        .height(Dim(g::kFieldH))
        .child(box().inset(0).fill(unitRule.material()))
        .child(box().inset(0).fill(emphasisRule.material()));
  }

  /** The configuration: n column bands, each entering on a stagger. */
  Element columns() {
    namespace g = gerstner;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const g::Config& c = g::kConfigs[config];
    const float colW = c.width * g::kUnit;

    Element bands = stack()
                        .key("bands")
                        .left(g::kFieldX)
                        .top(g::kFieldY)
                        .width(Dim(g::kFieldW))
                        .height(Dim(g::kFieldH))
                        .staggerChildren(52ms);
    for (int i = 0; i < c.columns; ++i) {
      const float x = g::columnUnit(c, i) * g::kUnit;
      // the column's own tint, so the configuration reads at a glance
      Element band =
          box()
              .key("col" + std::to_string(i))
              .left(x)
              .top(0)
              .width(Dim(colW))
              .height(Dim(g::kFieldH))
              .opacity(animate(motion::from(0.0f).to(1.0f),
                               {320ms, &ch::easeOutQuad}))
              .translateY(animate(motion::from(9.0f).to(0.0f),
                                  {420ms, &ch::easeOutQuint}))
              .fill(Fill::color({g::kRed.fR, g::kRed.fG, g::kRed.fB, 0.045f}))
              // The field's foot is the page's foot: the copy that does not
              // fit is cut there, as it is in a magazine.
              .clip();
      // THE COPY RUNS THE MEASURE. A programme that generates a page
      // generates a FULL one: a column that stops a third of the way down
      // is a layout abandoned, not a configuration.
      Element copy =
          box().left(0).top(g::kUnit * 5).right(0).column().gap(g::kUnit);
      for (int b = 0; b < c.blocks; ++b)
        copy.child(
            text(toU8(g::kBody[(i + b) % g::kBodyCount]),
                 weave::textStyle({.size = c.size,
                                   .color = b % 2 == 0 ? g::kInk : g::kInkSoft,
                                   .track = 0,
                                   .weight = 0}))
                .width(Dim(colW)));
      band.child(std::move(copy));
      // a column rule at the head, the way Capital marked its columns
      band.child(box()
                     .left(0)
                     .top(g::kUnit * 3.4f)
                     .width(Dim(colW))
                     .height(Dim(1.4f))
                     .fill(Fill::color(g::kInk)));
      const std::string label = kit::formatted("%02d", i + 1);
      band.child(text(toU8(label), weave::textStyle({.size = 10,
                                                     .color = g::kRed,
                                                     .track = 1.6f,
                                                     .weight = 620}))
                     .left(0)
                     .top(g::kUnit * 1.7f));
      bands.child(std::move(band));
    }
    return bands;
  }

  /** The headline, set across the full measure regardless of the
   *  configuration — Gerstner's grids always kept one element free. */
  Element headline() {
    namespace g = gerstner;
    using namespace std::chrono_literals;
    const g::Config& c = g::kConfigs[config];
    const std::string count =
        kit::formatted("%d COLUMN%s", c.columns, c.columns == 1 ? "" : "S");
    return box()
        .key("head")
        .column()
        .left(g::kFieldX)
        .top(38)
        .child(
            box()
                .row()
                .alignItems(Align::End)
                .child(
                    text(toU8("PROGRAMME"), weave::textStyle({.size = 30,
                                                              .color = g::kInk,
                                                              .track = 3.2f,
                                                              .weight = 680})))
                .child(text(toU8("58"), weave::textStyle({.size = 30,
                                                          .color = g::kRed,
                                                          .track = 1.0f,
                                                          .weight = 680}))
                           .margin(14, 0, 0, 0))
                .child(text(toU8(count), weave::textStyle({.size = 11,
                                                           .color = g::kInkSoft,
                                                           .track = 3.0f,
                                                           .weight = 600}))
                           .margin(18, 0, 0, 6)));
  }

  /** The arithmetic, printed where a caption goes. */
  Element arithmetic() {
    namespace g = gerstner;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const g::Config& c = g::kConfigs[config];
    Element row =
        box()
            .key("sum")
            .row()
            .alignItems(Align::Center)
            .gap(10)
            .left(g::kFieldX)
            .top(g::kFieldY + g::kFieldH + 16)
            .opacity(animate(motion::from(0.0f).to(1.0f), {300ms}))
            .child(text(toU8("58 ="), weave::textStyle({.size = 13,
                                                        .color = g::kInkSoft,
                                                        .track = 1.2f,
                                                        .weight = 600})))
            .child(text(toU8(c.arithmetic), weave::textStyle({.size = 15,
                                                              .color = g::kInk,
                                                              .track = 0.8f,
                                                              .weight = 640})));
    // the ladder of all six, with the live one marked
    Element ladder = box()
                         .row()
                         .gap(9)
                         .alignItems(Align::Center)
                         .right(84)
                         .top(g::kFieldY + g::kFieldH + 16);
    for (int i = 0; i < g::kConfigCount; ++i) {
      const bool live = i == config;
      const std::string n = kit::formatted("%d", g::kConfigs[i].columns);
      ladder.child(
          box()
              .width(Dim(22.0f))
              .height(Dim(22.0f))
              .alignItems(Align::Center)
              .justify(Justify::Center)
              .fill(Fill::color(live ? g::kRed : SkColor4f{0, 0, 0, 0}))
              .foreground(
                  stroke(1.0f, Fill::color(live ? g::kRed : g::kInkSoft)))
              .child(text(toU8(n), weave::textStyle(
                                       {.size = 12,
                                        .color = live ? g::kPaper : g::kInkSoft,
                                        .track = 0.6f,
                                        .weight = 620}))));
    }
    return stack().inset(0).child(std::move(row)).child(std::move(ladder));
  }

  Element describe() {
    namespace g = gerstner;
    auto root = stack().fill(
        linearGradient({0, 0}, {0, g::kH}, {g::kPaper, g::kPaperLo}));

    // Paper tooth: a full-canvas fractal noise that never changes. The
    // library will not bake through an opacity and a blend — it would
    // round the coverage twice — so the bake is asked for here, and the
    // three octaves are evaluated once instead of once a frame.
    root.child(box()
                   .inset(0)
                   .fill(mskia::Paint::recipe(field::noise(0.9f, 3, 5.0f)))
                   .opacity(0.05f)
                   .blend(SkBlendMode::kMultiply)
                   .cache(Cache::Texture));

    root.child(gridPlate());
    root.child(columns());
    root.child(headline());
    root.child(arithmetic());

    // THE READING INDEX: one hairline sweeping the baseline grid, the
    // only continuous motion on a page of discrete states — and named on
    // the page, because an unlabelled red rule across live text reads as a
    // defect rather than as an instrument.
    root.child(
        text(toU8("READING INDEX"),
             weave::textStyle(
                 {.size = 7, .color = g::kRed, .track = 0.6f, .weight = 620}))
            .left(g::kFieldX + g::kFieldW + 6)
            .top(-4)
            .translateY(&sweep)
            .zIndex(6));
    root.child(
        box()
            .left(g::kFieldX - 22)
            .width(Dim(g::kFieldW + 44))
            .height(Dim(1.0f))
            .top(0)
            .translateY(&sweep)
            .fill(linearGradient({0, 0}, {g::kFieldW + 44, 0},
                                 {{g::kRed.fR, g::kRed.fG, g::kRed.fB, 0.0f},
                                  {g::kRed.fR, g::kRed.fG, g::kRed.fB, 0.55f},
                                  {g::kRed.fR, g::kRed.fG, g::kRed.fB, 0.55f},
                                  {g::kRed.fR, g::kRed.fG, g::kRed.fB, 0.0f}},
                                 {0.0f, 0.12f, 0.88f, 1.0f}))
            .zIndex(6));

    root.child(
        box()
            .column()
            .left(g::kFieldX)
            .bottom(26)
            .child(text(toU8("KARL GERSTNER \xc2\xb7 CAPITAL "
                             "\xc2\xb7 1962"),
                        weave::textStyle({.size = 10,
                                          .color = g::kInkSoft,
                                          .track = 2.6f,
                                          .weight = 600})))
            .child(text(toU8("the mobile grid, run"),
                        weave::textStyle(
                            {.size = 10, .color = g::kInkSoft, .track = 1.2f}))
                       .margin(0, 3, 0, 0)));
    return root;
  }
};

}  // namespace

SIGIL_SKETCH_AS(GerstnerGrid, "gerstner grid", "Specimen",
                "Capital 1962 \xe2\x80\x94 the mobile grid, run")
