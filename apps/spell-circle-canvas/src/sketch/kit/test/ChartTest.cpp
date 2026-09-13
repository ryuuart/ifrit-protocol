/** @file
 * A frame with scales, and layers as functions of it: what the mapping
 * answers, where a placed element lands, and what dresses it.
 */

#include <gtest/gtest.h>
#include <sigilcompose/core/Composer.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "Drawn.h"

namespace {

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

TEST(SketchKitChart, APolarWedgeStandsInTheSquareOfItsOwnRadius) {
  const std::vector<kit::Datum> rows{{0, 25}, {1, 100}, {2, 49}};
  const kit::Plot frame = wheel();
  Drawn drawn(
      sheet(kit::plot("w", frame, {kit::bands(rows, {.y = &kit::Datum::y})})
                .width(200)
                .height(160)));
  const std::optional<SkRect> box = drawn.composer.bounds("w-bar0-1");
  ASSERT_TRUE(box.has_value());
  const float reach =
      (float)frame.radiusFraction(100) * frame.radius({200, 160});
  const SkPoint hub = frame.centre({200, 160});
  EXPECT_NEAR(box->centerX(), hub.fX, 0.51f);
  EXPECT_NEAR(box->centerY(), hub.fY, 0.51f);
  EXPECT_NEAR(box->width(), 2 * reach, 0.51f);
  // The shortest wedge is inscribed in the smallest square, so a wedge's
  // box is its own radius and not the wheel's.
  const std::optional<SkRect> shortest = drawn.composer.bounds("w-bar0-0");
  ASSERT_TRUE(shortest.has_value());
  EXPECT_LT(shortest->width(), box->width());
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

TEST(SketchKitChart, ARecordingIsPaintedInTheInkItsClassResolvesTo) {
  weave::StyleSheet dressed = kit::houseTheme().styleSheet();
  dressed.set("rule", weave::Type{.color = SkColor4f{0, 1, 0, 1}});
  Drawn drawn(compose::box().styleSheet(dressed).children(
      {kit::plot("r", plane(), {kit::rules({.y = {50}, .width = 3})})
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
                  .styleSheet(kit::houseTheme().styleSheet())
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
  const std::vector<std::string> drawn{"area", "axis", "bar",  "label",
                                       "mark", "rule", "tick", "trace"};
  EXPECT_EQ(documented, drawn);
  const weave::StyleSheet dressed = kit::houseTheme().styleSheet();
  for (const std::string& name : documented)
    EXPECT_TRUE(dressed.contains(name)) << name;
}

TEST(SketchKitChart, EveryChartClassCarriesAColour) {
  const kit::Theme& look = kit::houseTheme();
  const weave::StyleSheet dressed = look.styleSheet();
  for (const weave::Rule& rule : dressed.rules())
    if (rule.name() == "trace" || rule.name() == "mark" ||
        rule.name() == "bar") {
      ASSERT_TRUE(rule.type().color.has_value()) << rule.name();
      EXPECT_EQ(*rule.type().color, look.palette.figure) << rule.name();
    }
  for (const weave::Rule& rule : dressed.rules())
    if (rule.name() == "rule") {
      ASSERT_TRUE(rule.type().color.has_value());
      EXPECT_EQ(*rule.type().color, look.palette.rule);
    }
}

}  // namespace
