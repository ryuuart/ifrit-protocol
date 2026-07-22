#pragma once
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

#include "GalleryCore.h"

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkPathBuilder.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace compose_gallery {

namespace gerstner {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f,
          a};
}

constexpr SkColor4f kPaper = C(0xEDEAE3);
constexpr SkColor4f kPaperLo = C(0xDCD7CB);
constexpr SkColor4f kInk = C(0x16151A);
constexpr SkColor4f kInkSoft = C(0x55525A);
constexpr SkColor4f kRed = C(0xD8442F);
constexpr SkColor4f kBlue = C(0x2C4CA8); // the non-printing grid blue

// The measure is 58 units. The unit here is a screen unit, not 10pt —
// but every ratio below is Gerstner's.
constexpr int kUnits = 58;
constexpr int kRows = 34;      // the vertical field, same unit
constexpr float kUnit = 12.6f; // 58 * 12.6 = 730.8
constexpr float kFieldW = kUnits * kUnit;
constexpr float kFieldH = kRows * kUnit;
constexpr float kFieldX = 84;
constexpr float kFieldY = 92;

/** One configuration of the programme: n columns of `width` units with
 *  two units of gutter between them, summing to 58. */
struct Config {
  int columns, width, gutter;
  const char *arithmetic;
};
inline constexpr Config kConfigs[] = {
    {1, 58, 2, "58"},
    {2, 28, 2, "2 \xc3\x97 28 + 1 \xc3\x97 2"},
    {3, 18, 2, "3 \xc3\x97 18 + 2 \xc3\x97 2"},
    {4, 13, 2, "4 \xc3\x97 13 + 3 \xc3\x97 2"},
    {5, 10, 2, "5 \xc3\x97 10 + 4 \xc3\x97 2"},
    {6, 8, 2, "6 \xc3\x97 8 + 5 \xc3\x97 2"},
};
inline constexpr int kConfigCount =
    (int)(sizeof(kConfigs) / sizeof(kConfigs[0]));

/** Left edge of column `i`, in units. */
inline float columnUnit(const Config &c, int i) {
  return (float)(i * (c.width + c.gutter));
}

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0, float weight = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  if (weight > 0)
    s.shaping.variations = {sigil::weave::FontVariation("wght", weight)};
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** The copy the programme reflows. Gerstner's own argument, in our words:
 *  a grid is a rule, and a rule that cannot be run is a drawing. */
inline const char *kBody[] = {
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

} // namespace gerstner

struct GerstnerGridScene final : Scene {
  choreograph::Output<float> sweep{0};
  int config = 3; // the four-column setting, Capital's default
  int shownConfig = -1;
  double nextStep = 0.0;

  const char *name() const override { return "gerstner grid"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    sweep = 0;
    config = 3;
    shownConfig = -1;
    // hold the opening configuration for one beat so a still frame
    // lands on a real setting rather than mid-step
    nextStep = 2.6;
    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      // The reading index: one pass down the field every 5.2 s.
      sweep = gerstner::kFieldY +
              gerstner::kFieldH * (float)std::fmod(t / 5.2, 1.0);
      return true;
    });
    composer.render(describe());
  }

  /** Stepping the programme is a DATA change, so it re-describes and the
   *  reconciler diffs — which is also the honest way to show that the six
   *  configurations are six layouts, not six frames of one. */
  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextStep && shownConfig == config)
      return;
    if (elapsed >= nextStep) {
      nextStep = elapsed + 2.6;
      config = (config + 1) % gerstner::kConfigCount;
    }
    shownConfig = config;
    composer.render(describe());
  }

  // ------------------------------------------------------------------

  /** The grid itself, drawn in the blue a grid was drawn in: every unit
   *  column hairline, every fifth baseline, and the field's own frame. */
  Element gridPlate() {
    namespace g = gerstner;
    Element plate = stack()
                        .left(g::kFieldX).top(g::kFieldY)
                        .width(Dim(g::kFieldW)).height(Dim(g::kFieldH));
    for (int u = 0; u <= g::kUnits; ++u)
      plate.child(box().left((float)u * g::kUnit).top(0)
                      .width(Dim(0.5f)).height(Dim(g::kFieldH))
                      .fill(Material::solid({g::kBlue.fR, g::kBlue.fG,
                                             g::kBlue.fB,
                                             u % 2 == 0 ? 0.16f : 0.07f})));
    for (int r = 0; r <= g::kRows; ++r)
      plate.child(box().left(0).top((float)r * g::kUnit)
                      .width(Dim(g::kFieldW)).height(Dim(0.5f))
                      .fill(Material::solid({g::kBlue.fR, g::kBlue.fG,
                                             g::kBlue.fB,
                                             r % 5 == 0 ? 0.20f : 0.07f})));
    return plate;
  }

  /** The configuration: n column bands, each entering on a stagger. */
  Element columns() {
    namespace g = gerstner;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const g::Config &c = g::kConfigs[config];
    const float colW = c.width * g::kUnit;

    Element bands = stack().key("bands")
                        .left(g::kFieldX).top(g::kFieldY)
                        .width(Dim(g::kFieldW)).height(Dim(g::kFieldH))
                        .staggerChildren(52ms);
    for (int i = 0; i < c.columns; ++i) {
      const float x = g::columnUnit(c, i) * g::kUnit;
      // the column's own tint, so the configuration reads at a glance
      Element band =
          box().key("col" + std::to_string(i))
              .left(x).top(0)
              .width(Dim(colW)).height(Dim(g::kFieldH))
              .opacity(withFrom(0.0f, 1.0f, {320ms, &ch::easeOutQuad}))
              .translateY(withFrom(9.0f, 0.0f, {420ms, &ch::easeOutQuint}))
              .fill(Material::solid({g::kRed.fR, g::kRed.fG, g::kRed.fB,
                                     0.045f}));
      // the copy, flowed to this measure
      const char *copy = g::kBody[i % g::kBodyCount];
      const float size = c.columns >= 5 ? 9.0f
                         : c.columns == 4 ? 10.5f
                         : c.columns == 3 ? 12.0f
                         : c.columns == 2 ? 13.5f
                                          : 15.0f;
      // two paragraphs per column, so a narrow measure actually fills its
      // depth and the reflow is visible rather than implied
      const char *copy2 = g::kBody[(i + 1) % g::kBodyCount];
      band.child(box().left(0).top(g::kUnit * 5).right(0)
                     .column().gap(g::kUnit)
                     .child(text(toU8(copy),
                                 g::type(size, g::kInk, 0, 0))
                                .width(Dim(colW)))
                     .child(text(toU8(copy2),
                                 g::type(size, g::kInkSoft, 0, 0))
                                .width(Dim(colW))));
      // a column rule at the head, the way Capital marked its columns
      band.child(box().left(0).top(g::kUnit * 3.4f)
                     .width(Dim(colW)).height(Dim(1.4f))
                     .fill(Material::solid(g::kInk)));
      char label[24];
      std::snprintf(label, sizeof(label), "%02d", i + 1);
      band.child(text(toU8(label), g::type(10, g::kRed, 1.6f, 620))
                     .left(0).top(g::kUnit * 1.7f));
      bands.child(std::move(band));
    }
    return bands;
  }

  /** The headline, set across the full measure regardless of the
   *  configuration — Gerstner's grids always kept one element free. */
  Element headline() {
    namespace g = gerstner;
    using namespace std::chrono_literals;
    const g::Config &c = g::kConfigs[config];
    char count[40];
    std::snprintf(count, sizeof(count), "%d COLUMN%s", c.columns,
                  c.columns == 1 ? "" : "S");
    return box().key("head").column()
        .left(g::kFieldX).top(38)
        .child(box().row().alignItems(Align::End)
                   .child(text(toU8("PROGRAMME"),
                               g::type(30, g::kInk, 3.2f, 680)))
                   .child(text(toU8("58"), g::type(30, g::kRed, 1.0f, 680))
                              .margin(14, 0, 0, 0))
                   .child(text(toU8(count),
                               g::type(11, g::kInkSoft, 3.0f, 600))
                              .margin(18, 0, 0, 6)));
  }

  /** The arithmetic, printed where a caption goes. */
  Element arithmetic() {
    namespace g = gerstner;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const g::Config &c = g::kConfigs[config];
    Element row = box().key("sum").row().alignItems(Align::Center).gap(10)
                      .left(g::kFieldX)
                      .top(g::kFieldY + g::kFieldH + 16)
                      .opacity(withFrom(0.0f, 1.0f, {300ms}))
                      .child(text(toU8("58 ="),
                                  g::type(13, g::kInkSoft, 1.2f, 600)))
                      .child(text(toU8(c.arithmetic),
                                  g::type(15, g::kInk, 0.8f, 640)));
    // the ladder of all six, with the live one marked
    Element ladder = box().row().gap(9).alignItems(Align::Center)
                         .right(84)
                         .top(g::kFieldY + g::kFieldH + 16);
    for (int i = 0; i < g::kConfigCount; ++i) {
      const bool live = i == config;
      char n[4];
      std::snprintf(n, sizeof(n), "%d", g::kConfigs[i].columns);
      ladder.child(box().width(Dim(22.0f)).height(Dim(22.0f))
                       .alignItems(Align::Center).justify(Justify::Center)
                       .fill(Material::solid(live ? g::kRed
                                                  : SkColor4f{0, 0, 0, 0}))
                       .foreground(util::stroke(
                           1.0f, Fill::color(live ? g::kRed : g::kInkSoft)))
                       .child(text(toU8(n),
                                   g::type(12, live ? g::kPaper : g::kInkSoft,
                                           0.6f, 620))));
    }
    return stack().inset(0)
        .child(std::move(row))
        .child(std::move(ladder));
  }

  Element describe() {
    namespace g = gerstner;
    auto root = stack().fill(Material::linear(
        {0, 0}, {0, g::kH}, {{0.0f, g::kPaper}, {1.0f, g::kPaperLo}}));

    // paper tooth
    root.child(box().inset(0)
                   .fill(patterns::noise(0.9f, 3, 5.0f))
                   .opacity(0.05f)
                   .blend(SkBlendMode::kMultiply));

    root.child(gridPlate());
    root.child(columns());
    root.child(headline());
    root.child(arithmetic());

    // the reading index: one hairline sweeping the baseline grid, the
    // only continuous motion on a page of discrete states
    root.child(box().left(g::kFieldX - 22)
                   .width(Dim(g::kFieldW + 44)).height(Dim(1.0f))
                   .top(0).translateY(&sweep)
                   .fill(Material::linear(
                       {0, 0}, {g::kFieldW + 44, 0},
                       {{0.0f, {g::kRed.fR, g::kRed.fG, g::kRed.fB, 0.0f}},
                        {0.12f, {g::kRed.fR, g::kRed.fG, g::kRed.fB, 0.55f}},
                        {0.88f, {g::kRed.fR, g::kRed.fG, g::kRed.fB, 0.55f}},
                        {1.0f, {g::kRed.fR, g::kRed.fG, g::kRed.fB, 0.0f}}}))
                   .zIndex(6));

    root.child(box().column().left(g::kFieldX).bottom(26)
                   .child(text(toU8("KARL GERSTNER \xc2\xb7 CAPITAL "
                                    "\xc2\xb7 1962"),
                               g::type(10, g::kInkSoft, 2.6f, 600)))
                   .child(text(toU8("the mobile grid, run"),
                               g::type(10, g::kInkSoft, 1.2f))
                              .margin(0, 3, 0, 0)));
    return root;
  }
};

} // namespace compose_gallery
