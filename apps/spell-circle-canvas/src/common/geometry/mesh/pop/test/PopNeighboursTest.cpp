/** @file
 * The operators that read points they do not own: relaxation pushes a
 * scatter apart and stops, clustering groups it in the metric its weights
 * name, a transfer carries one lane over from another cloud, and the
 * connection sink answers the pairs that are near enough to be joined. A
 * device runtime declines each of them by name.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <set>
#include <vector>

#include "sigilgeometry/mesh/pop/Pop.h"
#include "sigilgeometry/path/Neighbours.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

namespace {

/** The closest any two points in a cloud come to each other. */
float closestPair(const Cloud& cloud) {
  const path::Neighbours index(cloud.positions);
  float closest = std::numeric_limits<float>::infinity();
  for (size_t i = 0; i < cloud.size(); ++i) {
    const auto other = index.nearestOther(cloud.positions[i], (uint32_t)i);
    if (!other)
      continue;
    closest = std::min(
        closest, glm::length(cloud.positions[*other] - cloud.positions[i]));
  }
  return closest;
}

/** A cloud at hand-placed positions, which is what lets a case say exactly
 *  what the answer must be. */
Cloud at(std::vector<glm::vec3> positions) {
  Cloud cloud;
  cloud.positions = std::move(positions);
  return cloud;
}

}  // namespace

// ---------------------------------------------------------------------------
// Relax

TEST(Pop, RelaxSeparatesTwoPointsToTheRadiusAndStops) {
  const Cloud pair = at({{0, 0, 0}, {1, 0, 0}});
  const Cloud spread =
      pop::cook(pop::on(pair).relax(10.0f, 40, 0.5f).chain());
  ASSERT_EQ(spread.size(), 2u);
  const float apart =
      glm::length(spread.positions[1] - spread.positions[0]);
  // Two points inside each other's radius push each other out of it and
  // then see nothing at all, so the separation settles AT the radius
  // rather than growing without bound.
  EXPECT_NEAR(apart, 10.0f, 0.5f);

  const Cloud again =
      pop::cook(pop::on(spread).relax(10.0f, 40, 0.5f).chain());
  EXPECT_NEAR(glm::length(again.positions[1] - again.positions[0]), apart,
              1e-3f);
}

TEST(Pop, RelaxSpreadsAClumpedScatter) {
  const Cloud scattered = points::scatterBox({0, 0, 0}, {200, 200, 200}, 500);
  const float before = closestPair(scattered);
  const Cloud settled =
      pop::cook(pop::on(scattered).relax(14.0f, 8, 0.5f).chain());
  ASSERT_EQ(settled.size(), scattered.size());
  EXPECT_GT(closestPair(settled), before * 3.0f);
}

TEST(Pop, RelaxLeavesCoincidentPointsWhereTheyAre) {
  // Two points in the same place have no direction to separate along, and
  // inventing one would make the answer depend on the order.
  const Cloud stacked = at({{5, 5, 5}, {5, 5, 5}});
  const Cloud after = pop::cook(pop::on(stacked).relax(10.0f).chain());
  EXPECT_EQ(after.positions[0], glm::vec3(5, 5, 5));
  EXPECT_EQ(after.positions[1], glm::vec3(5, 5, 5));
}

TEST(Pop, RelaxIsMaskedTheWayEveryFilterIs) {
  pop::Relax op;
  op.radius = 10.0f;
  op.iterations = 20;
  op.mask = "held";
  pop::Chain chain{pop::PointSet{at({{0, 0, 0}, {1, 0, 0}})},
                   pop::Fill{"held", {0, 0, 0, 0}}, op};
  const Cloud held = pop::cook(chain);
  EXPECT_NEAR(glm::length(held.positions[1] - held.positions[0]), 1.0f,
              1e-4f);
}

// ---------------------------------------------------------------------------
// Cluster

TEST(Pop, ClusterFindsTheGroupsThatAreThere) {
  // Three tight knots far apart: any clustering worth the name puts each
  // knot in one group and no group across two knots.
  std::vector<glm::vec3> positions;
  const glm::vec3 knots[3] = {{0, 0, 0}, {500, 0, 0}, {0, 500, 0}};
  for (const glm::vec3 knot : knots)
    for (int i = 0; i < 30; ++i)
      positions.push_back(knot + glm::vec3((float)(i % 5), (float)(i / 5), 0));

  pop::Cluster op;
  op.count = 3;
  const Cloud grouped = pop::cook(pop::Chain{pop::PointSet{at(positions)}, op});
  const std::vector<glm::vec4>* lane = grouped.colorIf("cluster");
  ASSERT_NE(lane, nullptr);
  ASSERT_EQ(lane->size(), positions.size());
  for (int knot = 0; knot < 3; ++knot) {
    std::set<int> groups;
    for (int i = 0; i < 30; ++i)
      groups.insert((int)(*lane)[(size_t)(knot * 30 + i)].x);
    EXPECT_EQ(groups.size(), 1u) << "knot " << knot;
  }
  std::set<int> all;
  for (const glm::vec4 value : *lane) all.insert((int)value.x);
  EXPECT_EQ(all.size(), 3u);
}

TEST(Pop, ClusterWeightsChooseTheMetric) {
  // Two columns far apart in x, each spanning the same small range of y.
  // Weighted on x, the grouping IS the two columns; weighted on y, it
  // cannot be — every group must then hold points from both columns.
  std::vector<glm::vec3> positions;
  for (int i = 0; i < 40; ++i)
    positions.push_back({i < 20 ? 0.0f : 900.0f, (float)(i % 20), 0});

  const auto groupOf = [&](glm::vec4 weights) {
    pop::Cluster op;
    op.count = 2;
    op.weights = weights;
    const Cloud out = pop::cook(pop::Chain{pop::PointSet{at(positions)}, op});
    std::vector<int> groups;
    for (const glm::vec4 value : *out.colorIf("cluster"))
      groups.push_back((int)value.x);
    return groups;
  };

  const std::vector<int> byX = groupOf({1, 0, 0, 0});
  for (int i = 1; i < 20; ++i) {
    EXPECT_EQ(byX[(size_t)i], byX[0]);
    EXPECT_EQ(byX[(size_t)(20 + i)], byX[20]);
  }
  EXPECT_NE(byX[0], byX[20]);

  const std::vector<int> byY = groupOf({0, 1, 0, 0});
  for (int i = 0; i < 20; ++i) EXPECT_EQ(byY[(size_t)i], byY[(size_t)(20 + i)]);
}

TEST(Pop, OneSeedIsOneClustering) {
  const Cloud scattered = points::scatterBox({0, 0, 0}, {100, 100, 100}, 200);
  pop::Cluster op;
  op.count = 5;
  op.seed = 7;
  const pop::Chain chain{pop::PointSet{scattered}, op};
  EXPECT_EQ(pop::cook(chain), pop::cook(chain));

  pop::Cluster other = op;
  other.seed = 8;
  const Cloud elsewhere =
      pop::cook(pop::Chain{pop::PointSet{scattered}, other});
  EXPECT_EQ(elsewhere.size(), scattered.size());
  // Every lane travels with its point: the positions are untouched by a
  // clustering, whatever it grouped them into.
  EXPECT_EQ(elsewhere.positions, scattered.positions);
}

// ---------------------------------------------------------------------------
// Transfer

TEST(Pop, TransferFromAOnePointSourceWritesThatValueEverywhere) {
  Cloud source = at({{0, 0, 0}});
  source.scalar("heat")[0] = 42.0f;

  pop::Transfer op;
  op.source = source;
  op.lane = "heat";
  op.radius = 1000.0f;
  const Cloud out = pop::cook(
      pop::Chain{pop::PointSet{points::scatterBox({-50, -50, -50},
                                                  {50, 50, 50}, 100)},
                 op});
  const std::vector<glm::vec4>* heat = out.colorIf("heat");
  ASSERT_NE(heat, nullptr);
  for (const glm::vec4 value : *heat) EXPECT_NEAR(value.x, 42.0f, 1e-3f);
}

TEST(Pop, TransferAtOneSampleIsAPlainNearestNeighbourLookup) {
  Cloud source = at({{-100, 0, 0}, {100, 0, 0}});
  std::vector<float>& heat = source.scalar("heat");
  heat[0] = 1.0f;
  heat[1] = 9.0f;

  pop::Transfer op;
  op.source = source;
  op.lane = "heat";
  op.radius = 1000.0f;
  op.maxSamples = 1;
  const Cloud out = pop::cook(pop::Chain{
      pop::PointSet{at({{-90, 0, 0}, {90, 0, 0}, {-1, 0, 0}})}, op});
  const std::vector<glm::vec4>& answer = *out.colorIf("heat");
  EXPECT_NEAR(answer[0].x, 1.0f, 1e-4f);
  EXPECT_NEAR(answer[1].x, 9.0f, 1e-4f);
  EXPECT_NEAR(answer[2].x, 1.0f, 1e-4f);
}

TEST(Pop, TransferLeavesAPointWithNothingInRangeAlone) {
  Cloud source = at({{0, 0, 0}});
  source.scalar("heat")[0] = 5.0f;

  pop::Transfer op;
  op.source = source;
  op.lane = "heat";
  op.radius = 10.0f;
  const Cloud out =
      pop::cook(pop::Chain{pop::PointSet{at({{1, 0, 0}, {500, 0, 0}})}, op});
  const std::vector<glm::vec4>& answer = *out.colorIf("heat");
  EXPECT_NEAR(answer[0].x, 5.0f, 1e-4f);
  EXPECT_NEAR(answer[1].x, 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Connect

TEST(Pop, ConnectAdjacentAnswersEachPairOnce) {
  const Cloud line = at({{0, 0, 0}, {10, 0, 0}, {20, 0, 0}, {100, 0, 0}});
  const std::vector<glm::uvec2> pairs =
      pop::connectAdjacent(line, pop::Connect{12.0f});
  ASSERT_EQ(pairs.size(), 2u);
  EXPECT_EQ(pairs[0], glm::uvec2(0, 1));
  EXPECT_EQ(pairs[1], glm::uvec2(1, 2));
}

TEST(Pop, ConnectAdjacentBoundsHowManyEachPointReaches) {
  const Cloud row = at({{0, 0, 0}, {10, 0, 0}, {20, 0, 0}, {30, 0, 0}});
  pop::Connect connect;
  connect.radius = 100.0f;
  connect.maxPerPoint = 1;
  const std::vector<glm::uvec2> pairs = pop::connectAdjacent(row, connect);
  // Each point keeps its single nearest, and the pairs both ends agreed on
  // are one entry: 0-1, 1-2 (from 2's side), 2-3.
  EXPECT_EQ(pairs.size(), 3u);
  for (const glm::uvec2 pair : pairs) EXPECT_EQ(pair.y - pair.x, 1u);
}

TEST(Pop, ConnectAdjacentBridgesPiecesOnly) {
  Cloud two = at({{0, 0, 0}, {1, 0, 0}, {5, 0, 0}, {6, 0, 0}});
  std::vector<float>& piece = two.scalar("piece");
  piece = {0, 0, 1, 1};

  pop::Connect connect;
  connect.radius = 100.0f;
  connect.pieceLane = "piece";
  connect.acrossPiecesOnly = true;
  const std::vector<glm::uvec2> pairs = pop::connectAdjacent(two, connect);
  ASSERT_EQ(pairs.size(), 4u);
  for (const glm::uvec2 pair : pairs)
    EXPECT_NE(piece[pair.x], piece[pair.y]);
}

TEST(Pop, ConnectAdjacentWithNoPieceLaneBridgesNothing) {
  pop::Connect connect;
  connect.radius = 100.0f;
  connect.acrossPiecesOnly = true;
  EXPECT_TRUE(
      pop::connectAdjacent(at({{0, 0, 0}, {1, 0, 0}}), connect).empty());
}

// ---------------------------------------------------------------------------
// The seam

TEST(Pop, TheNeighbourhoodOperatorsAreNamedInAChain) {
  EXPECT_EQ(pop::opName(pop::Op{pop::Smooth{}}), "Smooth");
  EXPECT_EQ(pop::opName(pop::Op{pop::Relax{}}), "Relax");
  EXPECT_EQ(pop::opName(pop::Op{pop::Cluster{}}), "Cluster");
  EXPECT_EQ(pop::opName(pop::Op{pop::Transfer{}}), "Transfer");
}

TEST(Pop, TheNeighbourhoodOperatorsCarryTheirDialsByName) {
  pop::Op op = pop::Relax{};
  EXPECT_TRUE(pop::setField(op, "radius", 12.0f));
  EXPECT_EQ(pop::getField(op, "radius").value(), 12.0f);
  EXPECT_FALSE(pop::getField(op, "strengthiness").has_value());

  pop::Op cluster = pop::Cluster{};
  EXPECT_TRUE(pop::setField(cluster, "count", 6.0f));
  EXPECT_EQ(pop::getField(cluster, "count").value(), 6.0f);
}
