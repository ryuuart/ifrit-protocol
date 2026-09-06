/** @file
 * The three things a field is walked, repeated or stepped by: a
 * streamline through a vector field, the copies a symmetry stands for,
 * and the cell sheet a rule steps.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <numbers>
#include <vector>

#include "sigilgeometry/path/Cells.h"
#include "sigilgeometry/path/Symmetry.h"
#include "sigilgeometry/path/Trace.h"

using namespace sigil::geometry::path;

namespace {

/** A field that points the same way everywhere: the one case whose
 *  streamline can be written down. */
VectorField uniformField(glm::vec2 direction) {
  return [direction](glm::vec2) { return direction; };
}

}  // namespace

// ---------------------------------------------------------------------------
// Streamlines

TEST(Streamline, AConstantFieldIsAStraightLineOfTheAskedLength) {
  TraceOptions options;
  options.step = 2.0f;
  options.length = 100.0f;
  const Polyline line = streamline(uniformField({1, 0}), {0, 0}, options);
  ASSERT_EQ(line.points.size(), 51u);  // the seed plus fifty steps
  EXPECT_NEAR(line.length(), 100.0f, 1e-2f);
  EXPECT_EQ(line.points.front(), glm::vec2(0, 0));
  EXPECT_NEAR(line.points.back().x, 100.0f, 1e-2f);
  EXPECT_NEAR(line.points.back().y, 0.0f, 1e-3f);
}

TEST(Streamline, TheStepAdvancesByArcLengthWhateverTheFieldsMagnitude) {
  TraceOptions options;
  options.step = 5.0f;
  options.length = 50.0f;
  // A field a thousand times as strong walks the same distance: what the
  // field says is which WAY, and the step says how far.
  const Polyline slow = streamline(uniformField({1, 0}), {0, 0}, options);
  const Polyline fast = streamline(uniformField({1000, 0}), {0, 0}, options);
  ASSERT_EQ(slow.points.size(), fast.points.size());
  EXPECT_NEAR(slow.points.back().x, fast.points.back().x, 1e-2f);
}

TEST(Streamline, ACircularFieldComesBackRound) {
  // A field perpendicular to the radius carries a point round a circle,
  // and a fourth-order step keeps it on one: after a whole turn the walk
  // is back where it started and the radius has not drifted.
  const float radius = 100.0f;
  const VectorField swirl = [](glm::vec2 p) { return glm::vec2{-p.y, p.x}; };
  TraceOptions options;
  options.step = 1.0f;
  options.length = 2.0f * std::numbers::pi_v<float> * radius;
  const Polyline line = streamline(swirl, {radius, 0}, options);
  for (const glm::vec2 point : line.points)
    EXPECT_NEAR(glm::length(point), radius, 0.5f);
  EXPECT_LT(glm::length(line.points.back() - line.points.front()), 2.0f);
}

TEST(Streamline, BoundsStopTheWalkAndTheSeedMustBeInside) {
  TraceOptions options;
  options.step = 1.0f;
  options.length = 1000.0f;
  options.bounds = SkRect::MakeLTRB(0, -10, 40, 10);
  const Polyline line = streamline(uniformField({1, 0}), {0, 0}, options);
  EXPECT_LT(line.points.size(), 60u);
  EXPECT_LE(line.points.back().x, 41.0f);

  EXPECT_TRUE(
      streamline(uniformField({1, 0}), {900, 0}, options).points.empty());
}

TEST(Streamline, BothWaysPutsTheSeedInTheMiddle) {
  TraceOptions options;
  options.step = 2.0f;
  options.length = 20.0f;
  options.bothWays = true;
  const Polyline line = streamline(uniformField({1, 0}), {0, 0}, options);
  ASSERT_EQ(line.points.size(), 21u);
  EXPECT_NEAR(line.points.front().x, -20.0f, 1e-2f);
  EXPECT_NEAR(line.points.back().x, 20.0f, 1e-2f);
  EXPECT_NEAR(line.points[10].x, 0.0f, 1e-3f);
}

TEST(Streamline, AStillFieldEndsTheWalkAtTheSeed) {
  const Polyline line = streamline(uniformField({0, 0}), {7, 7});
  ASSERT_EQ(line.points.size(), 1u);
  EXPECT_EQ(line.points[0], glm::vec2(7, 7));
}

TEST(Streamline, OneLinePerSeedInSeedOrder) {
  const std::vector<glm::vec2> seeds = {{0, 0}, {0, 10}, {0, 20}};
  TraceOptions options;
  options.length = 10.0f;
  const std::vector<Polyline> lines =
      streamlines(uniformField({1, 0}), seeds, options);
  ASSERT_EQ(lines.size(), 3u);
  for (size_t i = 0; i < seeds.size(); ++i)
    EXPECT_EQ(lines[i].points.front(), seeds[i]);
}

TEST(Streamline, AGradientFieldClimbsAndACurlFieldCirculates) {
  sigil::core::noise::Field field;
  field.frequency = 0.004f;
  field.seed = 5;
  const VectorField climb = flow(field, Flow::Gradient, 1.0f, 2.0f);
  const VectorField circulate = flow(field, Flow::Curl, 1.0f, 2.0f);
  // The curl is the gradient turned a quarter turn, everywhere: that is
  // what makes one of them run uphill and the other run along a contour.
  for (int i = 0; i < 20; ++i) {
    const glm::vec2 at{(float)(i * 37), (float)(i * 61)};
    const glm::vec2 up = climb(at), around = circulate(at);
    EXPECT_NEAR(glm::dot(up, around), 0.0f, 1e-4f);
    EXPECT_NEAR(glm::length(around), 1.0f, 1e-4f);
  }
  // Climbing raises the field it climbs; circulating does not.
  const Polyline climbed = streamline(climb, {200, 200}, {1.0f, 60.0f});
  EXPECT_GT(field.at(climbed.points.back().x, climbed.points.back().y),
            field.at(200.0f, 200.0f));
}

TEST(Streamline, AnAngleFieldIsAUnitDirection) {
  sigil::core::noise::Field field;
  field.frequency = 0.01f;
  const VectorField angled = flow(field, Flow::Angle, 2.0f);
  for (int i = 0; i < 20; ++i)
    EXPECT_NEAR(glm::length(angled({(float)i * 13, (float)i * 7})), 1.0f,
                1e-5f);
}

// ---------------------------------------------------------------------------
// Symmetry

TEST(Symmetry, TheDefaultIsOneCopyWhereTheFigureAlreadyIs) {
  const std::vector<SkMatrix> matrices = copies(Symmetry{});
  ASSERT_EQ(matrices.size(), 1u);
  EXPECT_TRUE(matrices[0].isIdentity());
}

TEST(Symmetry, RotationalOrderIsHowManyCopies) {
  Symmetry symmetry;
  symmetry.order = 6;
  const std::vector<glm::vec2> spokes =
      copies(symmetry, std::vector<glm::vec2>{{100, 0}});
  ASSERT_EQ(spokes.size(), 6u);
  for (size_t i = 0; i < spokes.size(); ++i) {
    EXPECT_NEAR(glm::length(spokes[i]), 100.0f, 1e-3f);
    const float angle = 2.0f * std::numbers::pi_v<float> * (float)i / 6.0f;
    EXPECT_NEAR(spokes[i].x, 100.0f * std::cos(angle), 1e-2f);
    EXPECT_NEAR(spokes[i].y, 100.0f * std::sin(angle), 1e-2f);
  }
}

TEST(Symmetry, TheCentreIsWhatTheTurnTurnsAbout) {
  Symmetry symmetry;
  symmetry.order = 2;
  symmetry.centre = {50, 0};
  const std::vector<glm::vec2> pair =
      copies(symmetry, std::vector<glm::vec2>{{60, 0}});
  ASSERT_EQ(pair.size(), 2u);
  EXPECT_NEAR(pair[0].x, 60.0f, 1e-3f);
  EXPECT_NEAR(pair[1].x, 40.0f, 1e-3f);
}

TEST(Symmetry, AMirrorDoublesTheCopiesAcrossItsAxis) {
  Symmetry symmetry;
  symmetry.mirror = true;
  symmetry.mirrorAngle = 0;  // the x axis: y flips
  const std::vector<glm::vec2> pair =
      copies(symmetry, std::vector<glm::vec2>{{30, 40}});
  ASSERT_EQ(pair.size(), 2u);
  EXPECT_NEAR(pair[0].x, 30.0f, 1e-3f);
  EXPECT_NEAR(pair[0].y, 40.0f, 1e-3f);
  EXPECT_NEAR(pair[1].x, 30.0f, 1e-3f);
  EXPECT_NEAR(pair[1].y, -40.0f, 1e-3f);
}

TEST(Symmetry, TheLatticeStepsTheWholeFigure) {
  Symmetry symmetry;
  symmetry.order = 4;
  symmetry.cellU = {100, 0};
  symmetry.cellV = {0, 100};
  symmetry.repeatU = 3;
  symmetry.repeatV = 2;
  EXPECT_EQ(copies(symmetry).size(), 4u * 3u * 2u);

  // The lattice is outermost, so a caller drawing the copies in order
  // lays a whole rosette down per cell rather than one spoke per cell.
  const std::vector<glm::vec2> placed =
      copies(symmetry, std::vector<glm::vec2>{{0, 0}});
  for (int i = 0; i < 4; ++i) EXPECT_EQ(placed[(size_t)i], glm::vec2(0, 0));
  EXPECT_EQ(placed[4], glm::vec2(100, 0));
}

TEST(Symmetry, APathCarriesEveryCopy) {
  Symmetry symmetry;
  symmetry.order = 8;
  const SkPath one = SkPath::Circle(100, 0, 10);
  const SkPath all = copies(symmetry, one);
  const SkRect bounds = all.getBounds();
  EXPECT_NEAR(bounds.width(), 220.0f, 1.0f);
  EXPECT_NEAR(bounds.height(), 220.0f, 1.0f);
}

TEST(Symmetry, AWallpaperGroupIsAStockValue) {
  const Symmetry p4m = wallpaper(Wallpaper::P4m, {100, 0}, {0, 100}, 2, 2);
  EXPECT_EQ(p4m.order, 4);
  EXPECT_TRUE(p4m.mirror);
  EXPECT_EQ(copies(p4m).size(), 4u * 2u * 2u * 2u);

  const Symmetry p1 = wallpaper(Wallpaper::P1, {100, 0}, {0, 100}, 3, 3);
  EXPECT_EQ(copies(p1).size(), 9u);
  // A group is a value like any other: it is edited rather than replaced.
  Symmetry edited = p1;
  edited.order = 3;
  EXPECT_EQ(copies(edited).size(), 27u);
}

// ---------------------------------------------------------------------------
// The cell sheet

TEST(Cells, ARuleReadsTheSheetAsItWasAndNotAsItIsBecoming) {
  // A rule that copies its left neighbour. On one buffer the values would
  // all slide to whatever the first column held; on two, each column
  // takes what its neighbour HAD, so one step shifts the sheet by one.
  Cells<int> sheet(5, 1, 0);
  for (int x = 0; x < 5; ++x) sheet.at(x, 0) = x;
  sheet.step(
      [](const Cells<int>& from, int x, int y) { return from.read(x - 1, y); });
  EXPECT_EQ(sheet.at(0, 0), 0);  // clamped: its own value
  EXPECT_EQ(sheet.at(1, 0), 0);
  EXPECT_EQ(sheet.at(2, 0), 1);
  EXPECT_EQ(sheet.at(3, 0), 2);
  EXPECT_EQ(sheet.at(4, 0), 3);
}

TEST(Cells, TheEdgeRuleIsWhatAReadOutsideAnswers) {
  Cells<int> sheet(3, 3, 0);
  sheet.at(0, 0) = 7;
  sheet.at(2, 2) = 9;

  sheet.setEdge(Edge::Clamp);
  EXPECT_EQ(sheet.read(-5, -5), 7);
  sheet.setEdge(Edge::Wrap);
  EXPECT_EQ(sheet.read(-1, -1), 9);
  EXPECT_EQ(sheet.read(3, 3), 7);
  sheet.setEdge(Edge::Constant);
  sheet.setOutside(-1);
  EXPECT_EQ(sheet.read(-1, 0), -1);
  EXPECT_EQ(sheet.read(0, 0), 7);
}

TEST(Cells, TheLifeGlidersMoves) {
  // Conway's rule, written by the caller as it should be: a glider walks
  // one cell diagonally every four steps, which is the check that the
  // substrate steps every cell at once and wraps where it says it does.
  Cells<uint8_t> sheet(16, 16, 0);
  sheet.setEdge(Edge::Wrap);
  for (const auto& [x, y] :
       std::vector<std::pair<int, int>>{{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}})
    sheet.at(x, y) = 1;

  const auto life = [](const Cells<uint8_t>& from, int x, int y) -> uint8_t {
    int alive = 0;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
        if (dx != 0 || dy != 0) alive += from.read(x + dx, y + dy);
    const bool here = from.read(x, y) != 0;
    return (uint8_t)((here && (alive == 2 || alive == 3)) ||
                     (!here && alive == 3));
  };
  for (int step = 0; step < 4; ++step) sheet.step(life);

  EXPECT_EQ(sheet.at(2, 1), 1);
  EXPECT_EQ(sheet.at(3, 2), 1);
  EXPECT_EQ(sheet.at(1, 3), 1);
  EXPECT_EQ(sheet.at(2, 3), 1);
  EXPECT_EQ(sheet.at(3, 3), 1);
  int total = 0;
  for (const uint8_t cell : sheet.values()) total += cell;
  EXPECT_EQ(total, 5);
}

TEST(Cells, AnEmptySheetStepsWithoutIncident) {
  Cells<float> sheet;
  EXPECT_TRUE(sheet.empty());
  sheet.step([](const Cells<float>&, int, int) { return 1.0f; });
  EXPECT_EQ(sheet.size(), 0u);
}

TEST(Cells, TwoSheetsAreEqualCellForCell) {
  Cells<int> a(4, 4, 3);
  Cells<int> b(4, 4, 3);
  EXPECT_EQ(a, b);
  b.at(2, 2) = 4;
  EXPECT_NE(a, b);
  b.at(2, 2) = 3;
  b.setEdge(Edge::Wrap);
  EXPECT_NE(a, b);
}
