/** @file
 * The frame's declarations and the resources its passes leave: a pass
 * compares as a value, a body compares only with its own copies, a
 * stamped point set is formed once while it stands still, and a frame
 * with no passes is its scene. What an executor draws is the device's,
 * and its cases stand beside it.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/advanced/Runtime.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/node/Group.h>

#include <glm/vec3.hpp>
#include <string>
#include <vector>

#include "TestMaterial.h"

using namespace sigil;
using namespace sigil::world;

namespace {

/** 48 motes spread along one closed path, as a point set a compute pass
 *  would have written. */
geometry::mesh::Cloud motes() {
  geometry::mesh::Cloud cloud;
  for (int i = 0; i < 48; ++i)
    cloud.positions.push_back({(float)i * 2.0f - 48.0f, (float)(i % 7), 0.0f});
  return cloud;
}

}  // namespace

TEST(WorldFrame, APassComparesFieldByField) {
  const Pass a = geometryPass("main").writes("colour", "depth");
  const Pass b = geometryPass("main").writes("colour", "depth");
  EXPECT_EQ(a, b);
  EXPECT_NE(a, geometryPass("main").writes("colour"));
  EXPECT_NE(a, computePass("main").writes("colour", "depth"));
  EXPECT_NE(geometryPass("main").only("glow"),
            geometryPass("main").only("lit"));
  EXPECT_EQ(postPass("bloom").reads("colour").blur(4.0f),
            postPass("bloom").reads("colour").blur(4.0f));
  EXPECT_NE(postPass("bloom").reads("colour").blur(4.0f),
            postPass("bloom").reads("colour").blur(5.0f));
}

TEST(WorldFrame, ALambdaBodyIsEqualToNothingButItsOwnCopies) {
  const Pass a = geometryPass("hand").body([] {});
  const Pass b = geometryPass("hand").body([] {});
  EXPECT_NE(a, b);
  EXPECT_EQ(a, Pass(a));
}

TEST(WorldFrame, ABodyNamesOnlyWhatItReadsAndTheValueFormStillCompares) {
  // The view and the targets are both offered to a body, which names the
  // ones it reads: a body that only has to run names neither. The comparable
  // seam value is untouched by that — it is still the form a frame prunes
  // on, and a callable still compares equal to nothing but its own copies.
  int ran = 0;
  const geometry::mesh::Mesh mesh = geometry::mesh::quad(80.0f, 80.0f);
  std::vector<Draw> draws(2);
  for (Draw& draw : draws) draw.mesh = &mesh;
  View view;
  view.draws = draws;
  Targets targets;
  const Pass bare = geometryPass("bare").body([&ran] { ++ran; });
  bare.body()->run(view, targets);
  EXPECT_EQ(ran, 1);
  size_t seen = 0;
  const Pass viewed = geometryPass("viewed").body(
      [&seen](const View& handed) { seen = handed.draws.size(); });
  viewed.body()->run(view, targets);
  EXPECT_EQ(seen, 2u);
  const Pass both =
      geometryPass("both").body([&seen](const View& handed, Targets& into) {
        seen = handed.draws.size() + (into.points("motes") ? 1 : 0);
      });
  both.body()->run(view, targets);
  EXPECT_EQ(seen, 3u);
  EXPECT_NE(bare, geometryPass("bare").body([&ran] { ++ran; }));
  EXPECT_EQ(bare, Pass(bare));
}

TEST(WorldFrame, ADeclaredNameIsNamedOnce) {
  const Pass pass = geometryPass("main").writes("colour").writes("colour");
  EXPECT_EQ(pass.writes().size(), 1u);
}

// A STAMPED SET IS FORMED ONCE per distinct (cloud, stamp). A pass
// draws the stamps of the sets it reads every frame, and a set that has
// not moved between two of them must not be instanced a second time —
// the whole cloud times the stamp's vertices is what forming one costs,
// and paying it per frame is paying it for nothing.
TEST(WorldFrame, AStillPointSetIsStampedOnceHoweverManyFramesDrawIt) {
  Targets targets;
  geometry::mesh::Cloud cloud = motes();
  const geometry::mesh::Mesh bead = geometry::mesh::quad(6.0f, 6.0f);

  for (int frame = 0; frame < 3; ++frame) {
    ASSERT_NE(targets.stamped(cloud, bead), nullptr);
    EXPECT_EQ(targets.stampings(), 1u) << "after frame " << frame;
    targets.endFrame();
  }

  // …and a set that HAS moved is a second stamping, which is what makes
  // the first assertion a statement about the content and not about the
  // cache never missing.
  cloud.positions.front().x += 1.0f;
  ASSERT_NE(targets.stamped(cloud, bead), nullptr);
  EXPECT_EQ(targets.stampings(), 2u);
}

TEST(WorldFrame, AStampingNoFrameAsksForIsLetGo) {
  Targets targets;
  const geometry::mesh::Cloud cloud = motes();
  const geometry::mesh::Mesh bead = geometry::mesh::quad(6.0f, 6.0f);
  uint64_t first = 0;
  ASSERT_NE(targets.stamped(cloud, bead, &first), nullptr);
  targets.endFrame();
  // A frame that drew no stamps lets the stamping go, so the next ask
  // forms it again under the same number.
  targets.endFrame();
  uint64_t again = 0;
  ASSERT_NE(targets.stamped(cloud, bead, &again), nullptr);
  EXPECT_EQ(targets.stampings(), 2u);
  EXPECT_EQ(again, first);
}

TEST(WorldFrame, AnImageIsWhatTheExecutorHoldingItAnswers) {
  Targets targets;
  EXPECT_EQ(targets.image("colour"), nullptr);
  std::string asked;
  targets.source([&asked](std::string_view name) {
    asked = std::string(name);
    return sk_sp<SkImage>();
  });
  EXPECT_EQ(targets.image("colour"), nullptr);
  EXPECT_EQ(asked, "colour");
}

TEST(WorldFrame, AFrameNamesNoRuntimeUntilItIsGivenOne) {
  const Frame frame = group().key("set");
  EXPECT_FALSE(frame.runtime());
}

TEST(WorldFrame, AFrameWithNoPassesIsItsScene) {
  const Frame frame = group().key("set");
  EXPECT_TRUE(frame.passes().empty());
  EXPECT_EQ(frame.scene().node()->key, "set");
}
