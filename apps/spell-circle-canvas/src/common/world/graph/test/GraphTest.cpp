/** @file
 * The ordering: what runs when and why, what a cycle says, which
 * resources share a surface, the hazards between them, and how each
 * pass's selection is realised.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilworld/graph/Plan.h>

#include <memory>
#include <string>
#include <vector>

#include "TestMaterial.h"

using namespace sigil;
using namespace sigil::world;
using namespace sigil::world::test;

namespace {

constexpr SkISize kExtent{64, 64};

Frame framed() { return Frame().extent(kExtent); }

std::vector<std::string> namesOf(const graph::Plan& plan) {
  std::vector<std::string> names;
  for (const PassWork& work : plan.steps()) names.push_back(work.pass->name());
  return names;
}

const PassWork* stepNamed(const graph::Plan& plan, const std::string& name) {
  for (const PassWork& work : plan.steps())
    if (work.pass->name() == name) return &work;
  return nullptr;
}

}  // namespace

TEST(WorldGraph, AReaderRunsAfterTheWriterWhateverOrderTheyWereDeclaredIn) {
  const Frame forward =
      framed()
          .pass(geometryPass("main").writes("colour"))
          .pass(postPass("grade").reads("colour").writes("final"));
  const Frame backward =
      framed()
          .pass(postPass("grade").reads("colour").writes("final"))
          .pass(geometryPass("main").writes("colour"));
  const std::vector<std::string> expected = {"main", "grade"};
  EXPECT_EQ(namesOf(graph::build(forward)), expected);
  EXPECT_EQ(namesOf(graph::build(backward)), expected);
}

TEST(WorldGraph, PassesThatDependOnNothingKeepTheirDeclaredOrder) {
  const Frame frame = framed()
                          .pass(geometryPass("b").writes("b"))
                          .pass(geometryPass("a").writes("a"))
                          .pass(geometryPass("c").writes("c"));
  const std::vector<std::string> expected = {"b", "a", "c"};
  EXPECT_EQ(namesOf(graph::build(frame)), expected);
}

TEST(WorldGraph, AWriteRunsAfterTheWriteBeforeItAndAfterThatVersionsReaders) {
  const Frame frame = framed()
                          .pass(geometryPass("first").writes("colour"))
                          .pass(postPass("read").reads("colour").writes("copy"))
                          .pass(postPass("again").writes("colour"));
  const std::vector<std::string> expected = {"first", "read", "again"};
  EXPECT_EQ(namesOf(graph::build(frame)), expected);
}

TEST(WorldGraph, ASecondGeometryPassOverAWrittenTargetIsAnErrorNamingBoth) {
  // A geometry pass CLEARS its target, so this one does not stand over
  // the picture "main" painted — it throws it away and keeps its own
  // bodies. Refused while the plan is read, because looking at the
  // result shows a plausible picture and says nothing about the one
  // that is missing.
  const Frame frame =
      framed()
          .pass(geometryPass("main").writes("colour"))
          .pass(geometryPass("motes").reads("motes").writes("colour"));
  const graph::Plan plan = graph::build(frame);
  EXPECT_FALSE((bool)plan);
  EXPECT_TRUE(plan.steps().empty());
  EXPECT_NE(plan.error().find("main"), std::string::npos);
  EXPECT_NE(plan.error().find("motes"), std::string::npos);
  EXPECT_NE(plan.error().find("colour"), std::string::npos);
  EXPECT_EQ(plan.error(), graph::build(frame).error());
}

TEST(WorldGraph, APostPassMayWriteWhatAGeometryPassWrote) {
  // Laying one picture over another is what a post pass is: it writes
  // the result of reading its layers rather than clearing and painting,
  // so nothing is lost and the plan stands.
  const Frame frame =
      framed()
          .pass(geometryPass("main").writes("colour"))
          .pass(postPass("grade").reads("colour").writes("colour"));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan) << plan.error();
  const std::vector<std::string> expected = {"main", "grade"};
  EXPECT_EQ(namesOf(plan), expected);
}

TEST(WorldGraph, AGeometryPassCarryingABodyMayWriteWhatWasWritten) {
  // A body runs INSTEAD of the stage and keeps only its declarations, so
  // it clears nothing: what it makes of what already stands in the
  // target is the body's own business and the rule does not reach it.
  const Frame frame = framed()
                          .pass(geometryPass("main").writes("colour"))
                          .pass(geometryPass("hand").writes("colour").body(
                              [](const View&, Targets&) {}));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan) << plan.error();
  const std::vector<std::string> expected = {"main", "hand"};
  EXPECT_EQ(namesOf(plan), expected);
}

TEST(WorldGraph, ACycleIsAnErrorNamingThePassesOnIt) {
  const Frame frame = framed()
                          .pass(postPass("ping").reads("b").writes("a"))
                          .pass(postPass("pong").reads("a").writes("b"));
  const graph::Plan plan = graph::build(frame);
  EXPECT_FALSE((bool)plan);
  EXPECT_TRUE(plan.steps().empty());
  EXPECT_NE(plan.error().find("ping"), std::string::npos);
  EXPECT_NE(plan.error().find("pong"), std::string::npos);
  // The report is the same every run, so a failing build says one thing.
  EXPECT_EQ(plan.error(), graph::build(frame).error());
}

TEST(WorldGraph, APreviousReadBreaksTheCycleItWouldOtherwiseMake) {
  const Frame frame =
      framed()
          .pass(geometryPass("main").writes("colour"))
          .pass(postPass("trail").reads("colour").previous("trail").writes(
              "trail"));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  const std::vector<std::string> expected = {"main", "trail"};
  EXPECT_EQ(namesOf(plan), expected);
  // …and the resource it reads from last frame is kept for it.
  const std::span<const std::string> kept = plan.kept();
  ASSERT_EQ(kept.size(), 1u);
  EXPECT_EQ(kept.front(), "trail");
  const graph::Resource* trail = plan.resource("trail");
  ASSERT_NE(trail, nullptr);
  EXPECT_TRUE(trail->persistent);
}

TEST(WorldGraph, TwoResourcesWhoseLivesDoNotOverlapShareOneSurface) {
  const Frame frame =
      framed()
          .pass(geometryPass("main").writes("colour"))
          .pass(postPass("half").reads("colour").writes("half"))
          .pass(postPass("quarter").reads("half").writes("quarter"));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  EXPECT_EQ(plan.resources().size(), 3u);
  // Nothing can be reused here: colour is still live while half is
  // written, and quarter is the picture and stands alone.
  EXPECT_EQ(plan.present(), "quarter");
  EXPECT_EQ(plan.surfaces(), 3);
  EXPECT_EQ(plan.aliased(), 0);

  const Frame longer =
      framed()
          .pass(geometryPass("main").writes("colour"))
          .pass(postPass("half").reads("colour").writes("half"))
          .pass(postPass("quarter").reads("half").writes("quarter"))
          .pass(postPass("eighth").reads("quarter").writes("eighth"));
  const graph::Plan reuse = graph::build(longer);
  ASSERT_TRUE((bool)reuse);
  EXPECT_EQ(reuse.resources().size(), 4u);
  // One step longer and colour is free again by the time quarter is
  // written, so the two of them take turns on one surface and the
  // frame needs no more surfaces than the shorter one did.
  EXPECT_EQ(reuse.surfaces(), 3);
  EXPECT_EQ(reuse.aliased(), 2);
}

TEST(WorldGraph, AResourceReadBackIsNeverAliased) {
  const Frame frame =
      framed()
          .pass(geometryPass("main").writes("colour"))
          .pass(postPass("half").reads("colour").writes("half"))
          .pass(postPass("quarter").reads("half").writes("quarter"))
          .pass(postPass("eighth").reads("quarter").writes("eighth"))
          .readback(readback("colour"));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  const graph::Resource* colour = plan.resource("colour");
  ASSERT_NE(colour, nullptr);
  EXPECT_TRUE(colour->persistent);
  EXPECT_FALSE(colour->aliased);
  EXPECT_EQ(colour->slot, -1);
}

TEST(WorldGraph, EveryWriteFollowedByAReadIsAStatedHazard) {
  const Frame frame =
      framed()
          .pass(geometryPass("main").writes("colour"))
          .pass(postPass("grade").reads("colour").writes("final"));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  ASSERT_EQ(plan.barriers().size(), 1u);
  const graph::Barrier& barrier = plan.barriers().front();
  EXPECT_EQ(barrier.resource, "colour");
  EXPECT_EQ(barrier.after, 0u);
  EXPECT_EQ(barrier.before, 1u);
  EXPECT_EQ(barrier.from, graph::Access::Write);
  EXPECT_EQ(barrier.to, graph::Access::Read);
}

TEST(WorldGraph, TwoReadsOfOneResourceAreNoHazard) {
  const Frame frame = framed()
                          .pass(geometryPass("main").writes("colour"))
                          .pass(postPass("a").reads("colour").writes("a"))
                          .pass(postPass("b").reads("colour").writes("b"));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  int onColour = 0;
  for (const graph::Barrier& barrier : plan.barriers())
    if (barrier.resource == "colour") ++onColour;
  // One from the write to the first read; the second read adds none.
  EXPECT_EQ(onColour, 1);
}

TEST(WorldGraph, ANarrowedGeometryPassIsCulled) {
  const Frame frame = framed().pass(
      geometryPass("glow").only(sel::tag("glow")).writes("colour"));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  ASSERT_EQ(plan.steps().size(), 1u);
  EXPECT_EQ(plan.steps().front().realisation, Selection::Cull);
}

TEST(WorldGraph, APassThatNarrowsNothingAddressesEveryBody) {
  const Frame frame = framed().pass(geometryPass("main").writes("colour"));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  EXPECT_EQ(plan.steps().front().realisation, Selection::None);
}

TEST(WorldGraph, ANarrowedPostPassIsMaskedAndTheGeometryPassWritesItsCoverage) {
  const Frame frame =
      framed()
          .pass(geometryPass("main").writes("colour"))
          .pass(postPass("bloom").reads("colour").writes("lit").only(
              sel::tag("glow")));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  const PassWork* bloom = stepNamed(plan, "bloom");
  const PassWork* main = stepNamed(plan, "main");
  ASSERT_NE(bloom, nullptr);
  ASSERT_NE(main, nullptr);
  EXPECT_EQ(bloom->realisation, Selection::Mask);
  EXPECT_FALSE(bloom->coverageIn.empty());
  EXPECT_EQ(main->coverageOut, bloom->coverageIn);
  EXPECT_EQ(main->coverageOf, sel::tag("glow"));
  // …and the coverage is a resource of the frame like any other.
  EXPECT_NE(plan.resource(bloom->coverageIn), nullptr);
}

TEST(WorldGraph, AMaskWithNothingPaintingBodiesAheadOfItIsAnError) {
  const Frame frame = framed().pass(
      postPass("bloom").reads("colour").writes("lit").only(sel::tag("glow")));
  const graph::Plan plan = graph::build(frame);
  EXPECT_FALSE((bool)plan);
  EXPECT_NE(plan.error().find("bloom"), std::string::npos);
}

TEST(WorldGraph, AVariantSurfaceIsRedrawn) {
  const material::Material white = paint({1, 1, 1, 1});
  const Frame frame = framed().pass(geometryPass("main")
                                        .writes("colour")
                                        .only(sel::tag("glow"))
                                        .variant(white));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  EXPECT_EQ(plan.steps().front().realisation, Selection::Variant);
}

TEST(WorldGraph, APassThatKnowsBetterOverridesTheRule) {
  const Frame frame = framed().pass(geometryPass("cover")
                                        .writes("mask")
                                        .only(sel::tag("glow"))
                                        .realise(Selection::Mask));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  EXPECT_EQ(plan.steps().front().realisation, Selection::Mask);
}

TEST(WorldGraph, AComputePassWritesPointsAndEverythingElseWritesPixels) {
  const std::vector<glm::vec3> path = {{0, 0, 0}, {1, 0, 0}};
  const Frame frame =
      framed()
          .pass(computePass("cook").writes("motes").chain(
              geometry::mesh::pop::on(path).count(8)))
          .pass(geometryPass("beads").reads("motes").writes("colour"));
  const graph::Plan plan = graph::build(frame);
  ASSERT_TRUE((bool)plan);
  const std::vector<std::string> expected = {"cook", "beads"};
  EXPECT_EQ(namesOf(plan), expected);
  ASSERT_NE(plan.resource("motes"), nullptr);
  EXPECT_EQ(plan.resource("motes")->kind, graph::Kind::Points);
  ASSERT_NE(plan.resource("colour"), nullptr);
  EXPECT_EQ(plan.resource("colour")->kind, graph::Kind::Image);
  // A point set holds no surface, so it never takes one.
  EXPECT_EQ(plan.resource("motes")->slot, -1);
  EXPECT_EQ(plan.present(), "colour");
}

TEST(WorldGraph, AFrameWithNoPassesHasNoPlanAndNoError) {
  const graph::Plan plan = graph::build(Frame());
  EXPECT_TRUE((bool)plan);
  EXPECT_TRUE(plan.steps().empty());
  EXPECT_TRUE(plan.present().empty());
}
