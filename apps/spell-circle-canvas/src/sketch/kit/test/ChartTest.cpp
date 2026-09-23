/** @file
 * A frame with scales, and layers as functions of it: what the mapping
 * answers, where a placed element lands, and what dresses it.
 */

#include <gtest/gtest.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Drawn.h"

namespace {

/** The rule of @p sheet whose selector reads as @p cssText, or null. */
const sigil::compose::Rule* ruleFor(const sigil::compose::StyleSheet& sheet,
                                    std::string_view cssText) {
  const sigil::compose::ElementSelector wanted =
      sigil::compose::selector(cssText);
  for (const sigil::compose::Rule& rule : sheet.rules())
    if (rule.selector() == wanted) return &rule;
  return nullptr;
}

namespace compose = sigil::compose;
namespace data = sigil::data;
namespace kit = sigil::sketch::kit;
namespace weave = sigil::weave;
using compose::Element;
using sigil::sketch::kit::test::Drawn;

constexpr float kField = 240;
constexpr float kFieldTall = 160;
constexpr SkSize kBox{kField, kFieldTall};

/** Ten across, a hundred up, eight px of room inside the box. */
kit::Plot plane() {
  return {.x = {.domain = {0, 10}}, .y = {.domain = {0, 100}}, .pad = 8};
}

/** Twelve months round a full turn, the radius going as the root of the
 *  rate — the coxcomb's own two scales. */
kit::Plot wheel() {
  return {.x = {.transform = data::Transform::Band, .steps = 12},
          .y = {.domain = {0, 100}, .transform = data::Transform::Sqrt},
          .polar = kit::Polar{.sweep = {-90, 270}, .inner = 0.2f}};
}

float radians(double degrees) {
  return (float)(degrees * std::numbers::pi / 180.0);
}

/** The plot in a root of the surface's own size, so the composer's space
 *  and the plot's are one and a bound can be read straight back. */
Element sheet(Element field) {
  return compose::box().children({std::move(field)});
}

// The frame's mapping

TEST(SketchKitChart, TheBoxSetsTheRangesAndYRunsUp) {
  const kit::Plot frame = plane();
  const data::Scale across = frame.scale(kit::Axis::X, kBox);
  const data::Scale up = frame.scale(kit::Axis::Y, kBox);
  EXPECT_DOUBLE_EQ(across.range.low, 8.0);
  EXPECT_DOUBLE_EQ(across.range.high, kField - 8.0);
  // Reversed on purpose: the y domain's low end is the box's BOTTOM edge.
  EXPECT_DOUBLE_EQ(up.range.low, kFieldTall - 8.0);
  EXPECT_DOUBLE_EQ(up.range.high, 8.0);
  EXPECT_NEAR(frame.at(0, 0, kBox).fY, kFieldTall - 8.0f, 1e-4f);
  EXPECT_NEAR(frame.at(0, 100, kBox).fY, 8.0f, 1e-4f);
}

TEST(SketchKitChart, ABandedScaleCentresAMarkInItsBand) {
  const kit::Plot frame{.x = {.transform = data::Transform::Band, .steps = 4},
                        .y = {.domain = {0, 1}}};
  const data::Scale across = frame.scale(kit::Axis::X, {200, 100});
  // The band's own extent is read off the scale; the MARK stands in the
  // middle of it, which is what a tick, a dot and a word all want.
  EXPECT_DOUBLE_EQ(across.apply(1), 50.0);
  EXPECT_DOUBLE_EQ(across.bandwidth(), 50.0);
  EXPECT_NEAR(frame.at(1, 0.5, {200, 100}).fX, 75.0f, 1e-4f);
}

// A placed element

TEST(SketchKitChart, AMarkLandsWhereTheScaleSaysInALaidOutBox) {
  const std::vector<kit::Datum> rows{{0, 0}, {2.5, 25}, {10, 100}};
  const auto dot = [](const kit::Datum&, std::size_t index) {
    return compose::box().width(6).height(6).key("dot" + std::to_string(index));
  };
  Drawn drawn(sheet(
      kit::plot(
          "p", plane(),
          {kit::marks(rows, dot, {.x = &kit::Datum::x, .y = &kit::Datum::y})})
          .width(kField)
          .height(kFieldTall)));
  const kit::Plot frame = plane();
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const std::optional<SkRect> box =
        drawn.composer.bounds("dot" + std::to_string(i));
    ASSERT_TRUE(box.has_value()) << i;
    const SkPoint expected = frame.at(rows[i].x, rows[i].y, kBox);
    EXPECT_NEAR(box->centerX(), expected.fX, 0.51f) << i;
    EXPECT_NEAR(box->centerY(), expected.fY, 0.51f) << i;
  }
}

TEST(SketchKitChart, ABandFillsItsOwnBandFromTheBaseToItsValue) {
  const std::vector<kit::Datum> rows{{0, 25}, {1, 50}, {2, 100}, {3, 75}};
  const kit::Plot frame{.x = {.transform = data::Transform::Band, .steps = 4},
                        .y = {.domain = {0, 100}}};
  Drawn drawn(
      sheet(kit::plot("b", frame, {kit::bands(rows, {.y = &kit::Datum::y})})
                .width(kField)
                .height(kFieldTall)));
  const data::Scale across = frame.scale(kit::Axis::X, kBox);
  const data::Scale up = frame.scale(kit::Axis::Y, kBox);
  const std::optional<SkRect> box = drawn.composer.bounds("b-bar0-2");
  ASSERT_TRUE(box.has_value());
  EXPECT_NEAR(box->left(), (float)across.apply(2), 0.51f);
  EXPECT_NEAR(box->width(), (float)across.bandwidth(), 0.51f);
  EXPECT_NEAR(box->bottom(), (float)up.apply(0), 0.51f);
  EXPECT_NEAR(box->top(), (float)up.apply(100), 0.51f);
}

// The wheel

TEST(SketchKitChart, APolarFrameCarriesXOntoTheSweepAndYOntoTheRadius) {
  const kit::Plot frame = wheel();
  const data::Scale angles = frame.scale(kit::Axis::X, SkSize::MakeEmpty());
  // Twelve bands over a full turn: the first spans −90° to −60°.
  EXPECT_DOUBLE_EQ(angles.apply(0), -90.0);
  EXPECT_DOUBLE_EQ(angles.bandwidth(), 30.0);
  EXPECT_DOUBLE_EQ(frame.angle(0), -75.0);
  // A square-root radius, which is what makes a wedge's AREA its value: a
  // quarter of the domain stands half way out of the ring.
  // The inner fraction is a float, so the ladder it seeds is read to a
  // float's own precision.
  EXPECT_NEAR(frame.radiusFraction(0), 0.2, 1e-6);
  EXPECT_DOUBLE_EQ(frame.radiusFraction(100), 1.0);
  EXPECT_NEAR(frame.radiusFraction(25), 0.6, 1e-6);
  // The rim is half the box's shorter side, and the hub its middle.
  EXPECT_NEAR(frame.radius({200, 160}), 80.0f, 1e-4f);
  EXPECT_NEAR(frame.centre({200, 160}).fX, 100.0f, 1e-4f);
  const SkPoint point = frame.at(0, 100, {200, 160});
  EXPECT_NEAR(point.fX, 100.0f + 80.0f * std::cos(radians(-75.0)), 1e-3f);
  EXPECT_NEAR(point.fY, 80.0f + 80.0f * std::sin(radians(-75.0)), 1e-3f);
}

TEST(SketchKitChart, APolarWedgeStandsInTheBoundsOfItsOwnSector) {
  const std::vector<kit::Datum> rows{{0, 25}, {1, 100}, {2, 49}};
  const kit::Plot frame = wheel();
  constexpr SkSize kWheel{200, 160};
  Drawn drawn(
      sheet(kit::plot("w", frame, {kit::bands(rows, {.y = &kit::Datum::y})})
                .width(kWheel.width())
                .height(kWheel.height())));
  const std::optional<SkRect> box = drawn.composer.bounds("w-bar0-1");
  ASSERT_TRUE(box.has_value());

  // The wedge occupies a twelfth of the wheel, and its box is the room
  // that wedge actually takes: its two rim ends stand on it, and the
  // whole of it is inside the disc's own square.
  const data::Scale angles = frame.scale(kit::Axis::X, SkSize::MakeEmpty());
  const double from = angles.apply(1);
  const double to = from + angles.bandwidth();
  const SkPoint hub = frame.centre(kWheel);
  const float outer = (float)frame.radiusFraction(100) * frame.radius(kWheel);
  const float inner = (float)frame.radiusFraction(0) * frame.radius(kWheel);
  for (const double end : {from, to})
    for (const float r : {inner, outer}) {
      const SkPoint on{hub.fX + r * std::cos(radians(end)),
                       hub.fY + r * std::sin(radians(end))};
      EXPECT_NEAR(std::clamp(on.fX, box->left(), box->right()), on.fX, 0.51f);
      EXPECT_NEAR(std::clamp(on.fY, box->top(), box->bottom()), on.fY, 0.51f);
    }
  EXPECT_GT(box->left(), hub.fX - outer - 0.51f);
  EXPECT_LT(box->right(), hub.fX + outer + 0.51f);
  // A 30° wedge inscribed in its whole disc would be a bake nine parts
  // transparent; its own bounds are a fraction of that square.
  EXPECT_LT(box->width() * box->height(), 0.5f * 4 * outer * outer);

  // The shortest wedge still gets the smaller box, because its bounds are
  // its own radius and not the wheel's.
  const std::optional<SkRect> shortest = drawn.composer.bounds("w-bar0-0");
  ASSERT_TRUE(shortest.has_value());
  EXPECT_LT(shortest->width() * shortest->height(),
            box->width() * box->height());
}

TEST(SketchKitChart, BandsAlongTheYScaleAreARowReading) {
  const std::vector<kit::Datum> rows{{25, 0}, {50, 1}, {100, 2}, {75, 3}};
  const kit::Plot frame{.x = {.domain = {0, 100}},
                        .y = {.transform = data::Transform::Band, .steps = 4}};
  Drawn drawn(sheet(kit::plot("r", frame,
                              {kit::bands(rows, {.x = &kit::Datum::x,
                                                 .y = &kit::Datum::y,
                                                 .along = kit::Axis::Y})})
                        .width(kField)
                        .height(kFieldTall)));
  const data::Scale across = frame.scale(kit::Axis::X, kBox);
  const data::Scale down = frame.scale(kit::Axis::Y, kBox);
  const std::optional<SkRect> box = drawn.composer.bounds("r-bar0-2");
  ASSERT_TRUE(box.has_value());
  // The band is handed out DOWN and the bar grows ACROSS, from the base.
  EXPECT_NEAR(box->left(), (float)across.apply(0), 0.51f);
  EXPECT_NEAR(box->right(), (float)across.apply(100), 0.51f);
  EXPECT_NEAR(
      std::min(box->top(), box->bottom()),
      std::min((float)down.apply(2), (float)(down.apply(2) + down.bandwidth())),
      0.51f);
  EXPECT_NEAR(box->height(), std::abs((float)down.bandwidth()), 0.51f);
}

TEST(SketchKitChart, ABaseOffZeroPutsAShortfallOnOneSideOfTheRule) {
  const std::vector<kit::Datum> rows{{0, 0.8}, {1, 1.3}};
  const kit::Plot frame{.x = {.transform = data::Transform::Band, .steps = 2},
                        .y = {.domain = {0.5, 1.5}}};
  Drawn drawn(
      sheet(kit::plot("d", frame,
                      {kit::bands(rows, {.y = &kit::Datum::y, .base = 1.0})})
                .width(kField)
                .height(kFieldTall)));
  const data::Scale up = frame.scale(kit::Axis::Y, kBox);
  const float rule = (float)up.apply(1.0);
  const std::optional<SkRect> under = drawn.composer.bounds("d-bar0-0");
  const std::optional<SkRect> over = drawn.composer.bounds("d-bar0-1");
  ASSERT_TRUE(under.has_value());
  ASSERT_TRUE(over.has_value());
  EXPECT_NEAR(under->top(), rule, 0.51f);
  EXPECT_NEAR(under->bottom(), (float)up.apply(0.8), 0.51f);
  EXPECT_NEAR(over->bottom(), rule, 0.51f);
  EXPECT_NEAR(over->top(), (float)up.apply(1.3), 0.51f);
}

// The recordings

TEST(SketchKitChart, ATracePrunesOnItsKey) {
  const auto tree = [](const char* key) {
    return sheet(
        kit::plot(key, plane(), {kit::trace([](double x) { return x * 10.0; })})
            .width(kField)
            .height(kFieldTall));
  };
  Drawn drawn(tree("t"));
  ASSERT_TRUE(drawn.composer.bounds("t-trace0").has_value());
  // The same key describes the same drawing, so nothing about the node
  // changed — which is the whole reason a recording carries one: a
  // callable compares to nothing.
  drawn.composer.render(tree("t"));
  EXPECT_EQ(drawn.composer.stats().patchedNodes, 0u);
  drawn.composer.render(tree("u"));
  drawn.composer.draw(*drawn.surface->getCanvas());
  EXPECT_TRUE(drawn.composer.bounds("u-trace0").has_value());
}

TEST(SketchKitChart, ARecordedSeriesIsWalkedAtItsOwnIndex) {
  // The samples as they are: the first at the domain's low end, the last
  // at its high one, and the curve prunes on the run's own values.
  const std::vector<double> series{0.0, 50.0, 100.0, 50.0, 0.0};
  const auto tree = [](std::span<const double> run) {
    return sheet(kit::plot("s", plane(), {kit::trace(run)})
                     .width(kField)
                     .height(kFieldTall));
  };
  Drawn drawn(tree(series));
  const std::optional<SkRect> curve = drawn.composer.bounds("s-trace0");
  ASSERT_TRUE(curve.has_value());
  EXPECT_EQ(*curve, SkRect::MakeWH(kField, kFieldTall));
  // A run that did not change is not walked again.
  drawn.composer.render(tree(series));
  EXPECT_EQ(drawn.composer.stats().patchedNodes, 0u);
  // One that did is.
  const std::vector<double> moved{0.0, 60.0, 100.0, 50.0, 0.0};
  drawn.composer.render(tree(moved));
  drawn.composer.draw(*drawn.surface->getCanvas());
  EXPECT_GT(drawn.composer.stats().patchedNodes, 0u);
}

TEST(SketchKitChart, APathWalksOneParameterIntoBothCoordinates) {
  // A locus is what no trace can be: t carries into x AND y, and the
  // curve stands in the whole field as a trace does.
  Drawn drawn(sheet(
      kit::plot(
          "p", plane(),
          {kit::path([](double t) { return kit::Datum{t * 10.0, t * 100.0}; },
                     {.samples = 32, .over = {0.0, 1.0}})})
          .width(kField)
          .height(kFieldTall)));
  const std::optional<SkRect> curve = drawn.composer.bounds("p-path0");
  ASSERT_TRUE(curve.has_value());
  EXPECT_EQ(*curve, SkRect::MakeWH(kField, kFieldTall));
}

TEST(SketchKitChart, ASegmentStandsInTheBoundsOfItsOwnTwoEnds) {
  struct Chord {
    double a0, b0, a1, b1;
  };
  const std::vector<Chord> pairs{{1, 10, 9, 90}, {2, 80, 8, 20}};
  Drawn drawn(sheet(kit::plot("c", plane(),
                              {kit::segments(pairs,
                                             [](const Chord&, std::size_t) {
                                               return compose::box();
                                             },
                                             {.x = &Chord::a0,
                                              .y = &Chord::b0,
                                              .toX = &Chord::a1,
                                              .toY = &Chord::b1})})
                        .width(kField)
                        .height(kFieldTall)));
  // Each child's box is the rectangle its two mapped ends bound, and no
  // more — a run of chords costs its own pixels rather than a field each.
  // The tolerance is a pixel because layout rounds a placed rect onto the
  // pixel grid, which is what every other placed child is rounded by.
  for (std::size_t i = 0; i < pairs.size(); ++i) {
    const std::optional<SkRect> box =
        drawn.composer.bounds("c-segment0-" + std::to_string(i));
    ASSERT_TRUE(box.has_value());
    const SkPoint from = plane().at(pairs[i].a0, pairs[i].b0, kBox);
    const SkPoint to = plane().at(pairs[i].a1, pairs[i].b1, kBox);
    EXPECT_NEAR(box->left(), std::min(from.fX, to.fX), 1.01f);
    EXPECT_NEAR(box->top(), std::min(from.fY, to.fY), 1.01f);
    EXPECT_NEAR(box->width(), std::abs(to.fX - from.fX), 1.01f);
    EXPECT_LT(box->width(), kField);
  }
}

TEST(SketchKitChart, ACurveDrawsItselfOnAlongItsOwnLength) {
  const auto tree = [](std::optional<compose::Spans> gate) {
    compose::StyleSheet dressed =
        kit::houseTheme().styleSheet() +
        compose::StyleSheet{
            compose::rule(".plotTrace")
                .font(weave::Type{.color = sigil::material::skia::toSkColor(
                                      sigil::material::Color{0, 1, 0, 1})})};
    return compose::box().applyStyleSheet(dressed).children(
        {kit::plot("g", plane(),
                   {kit::trace([](double x) { return x * 10.0; },
                               {.pen = {.width = 3}, .along = gate})})
             .width(kField)
             .height(kFieldTall)});
  };
  // Nine tenths along the curve: drawn when the whole of it is, and not
  // drawn when the gate has only reached a third.
  const SkPoint late = plane().at(9, 90, kBox);
  const auto green = [&late](Drawn& drawn) {
    return drawn.pixels()
        .getColor4f((int)std::round(late.fX), (int)std::round(late.fY))
        .fG;
  };
  Drawn whole(tree(std::nullopt));
  Drawn part(tree(compose::spans::upTo(0.33f)));
  EXPECT_GT(green(whole), 0.5f);
  EXPECT_LT(green(part), 0.2f);
}

TEST(SketchKitChart, ARecordingIsPaintedInTheInkItsClassResolvesTo) {
  compose::StyleSheet dressed =
      kit::houseTheme().styleSheet() +
      compose::StyleSheet{
          compose::rule(".plotRule")
              .font(weave::Type{.color = sigil::material::skia::toSkColor(
                                    sigil::material::Color{0, 1, 0, 1})})};
  Drawn drawn(compose::box().applyStyleSheet(dressed).children(
      {kit::plot("r", plane(), {kit::rules({.y = {50}, .pen = {.width = 3}})})
           .width(kField)
           .height(kFieldTall)}));
  const SkPoint on = plane().at(5, 50, kBox);
  const SkColor4f pixel =
      drawn.pixels().getColor4f((int)on.fX, (int)std::round(on.fY));
  EXPECT_NEAR(pixel.fG, 1.0f, 0.05f);
  EXPECT_LT(pixel.fR, 0.2f);
}

TEST(SketchKitChart, AnAxisRunsTheWholeFieldWhateverItsDomainIs) {
  const kit::Plot frame{.x = {.transform = data::Transform::Band, .steps = 5},
                        .y = {.domain = {0, 1}},
                        .pad = 6};
  Drawn drawn(compose::box()
                  .applyStyleSheet(kit::houseTheme().styleSheet())
                  .children({kit::plot("a", frame,
                                       {kit::axis({.of = kit::Axis::X,
                                                   .width = 3,
                                                   .numbers = false})})
                                 .width(kField)
                                 .height(kFieldTall)}));
  const SkBitmap pixels = drawn.pixels();
  // The line stands at the y domain's low end, which is the box's bottom
  // edge less the pad, and it spans the FIELD rather than the domain: five
  // categories have no extent of their own to reach the edges with.
  const int row = (int)std::round(kFieldTall - 6.0f);
  const auto lit = [&](int x) { return pixels.getColor4f(x, row).fG > 0.3f; };
  EXPECT_TRUE(lit(8));
  EXPECT_TRUE(lit((int)kField / 2));
  EXPECT_TRUE(lit((int)kField - 8));
  // And it stops at the pad, which is the room the frame keeps.
  EXPECT_FALSE(lit(2));
}

// The classes, and the table that documents them

/** THE CLASS NAMES THE KIT'S README DOCUMENTS, read off the one table that
 *  names them — the README is the canon, so the test reads it rather than
 *  restating it. */
std::vector<std::string> documentedClasses() {
  std::ifstream file(SIGIL_SKETCH_KIT_README);
  std::vector<std::string> names;
  std::string line;
  bool inTable = false;
  while (std::getline(file, line)) {
    if (line.find("#### The classes a chart draws") != std::string::npos) {
      inTable = true;
      continue;
    }
    if (!inTable) continue;
    if (line.rfind("| `", 0) != 0) {
      if (!names.empty()) break;
      continue;
    }
    const std::size_t open = line.find('`') + 1;
    const std::size_t close = line.find('`', open);
    names.push_back(line.substr(open, close - open));
  }
  return names;
}

TEST(SketchKitChart, TheDocumentedClassTableIsTheThemeSheet) {
  std::vector<std::string> documented = documentedClasses();
  std::sort(documented.begin(), documented.end());
  const std::vector<std::string> drawn{"plotArea",  "plotAxis", "plotBar",
                                       "plotLabel", "plotMark", "plotRule",
                                       "plotTick",  "plotTrace"};
  EXPECT_EQ(documented, drawn);
  const compose::StyleSheet dressed = kit::houseTheme().styleSheet();
  for (const std::string& name : documented)
    EXPECT_NE(ruleFor(dressed, "." + name), nullptr) << name;
}

TEST(SketchKitChart, EveryChartClassCarriesAColour) {
  const kit::Theme& look = kit::houseTheme();
  const compose::StyleSheet dressed = look.styleSheet();
  for (const char* name : {".plotTrace", ".plotMark", ".plotBar"}) {
    const compose::Rule* rule = ruleFor(dressed, name);
    ASSERT_NE(rule, nullptr) << name;
    ASSERT_TRUE(rule->type().color.has_value()) << name;
    EXPECT_EQ(*rule->type().color, look.palette.figure) << name;
  }
  const compose::Rule* rule = ruleFor(dressed, ".plotRule");
  ASSERT_NE(rule, nullptr);
  ASSERT_TRUE(rule->type().color.has_value());
  EXPECT_EQ(*rule->type().color, look.palette.rule);
}

}  // namespace
