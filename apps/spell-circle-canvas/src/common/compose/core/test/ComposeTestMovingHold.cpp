// A live composer holds a node as pixels while its own transform moves
// and nothing it paints moves inside it: the pixels are formed once,
// composited under each new matrix, formed again when what the node paints
// changes, and gone the draw it holds still. A composer that is not live
// paints the same node live under every matrix.

#include <string>

#include "support/CoreTestSupport.h"

namespace {

using sigil::motion::Animatable;

/** A turning wheel of spokes and lettering about the host's centre: the
 *  node whose own transform moves, with nothing inside it moving. */
Element turningWheel(const Animatable<float>& turn, Fill spokeFill = red()) {
  Element wheel = box()
                      .absolute()
                      .left(40)
                      .top(40)
                      .width(120)
                      .height(120)
                      .key("wheel")
                      .transformOrigin(percent(50), percent(50))
                      .rotate(sigil::motion::bind(turn, {.to = {0.0f, 360.0f}}));
  std::vector<Element> spokes;
  for (int index = 0; index < 6; ++index)
    spokes.push_back(box()
                         .absolute()
                         .left(55)
                         .top(0)
                         .width(10)
                         .height(60)
                         .transformOrigin(percent(50), percent(100))
                         .rotate((float)index * 60.0f)
                         .fill(spokeFill));
  spokes.push_back(text("ROTA").absolute().left(30).top(50));
  wheel.children(std::move(spokes));
  return box().width(200).height(200).children({std::move(wheel)});
}

/** How the node keyed @p key produced its pixels in the last draw. */
Composer::CacheState stateOf(const Composer& composer,
                             const std::string& key = "wheel") {
  for (const Composer::NodeCost& row : composer.profile())
    if (row.label.starts_with(key + " (")) return row.cacheState;
  return Composer::CacheState::Live;
}

}  // namespace

TEST(ComposeMovingHold, ALiveComposerTurnsAStillSubtreeFromHeldPixels) {
  Animatable<float> turn = sigil::motion::animatable(0.0f);
  Host host;
  host.composer.setLive(true);
  host.composer.setProfiling(true);
  host.composer.render(turningWheel(turn));
  host.frame();
  // The first draw has nothing to compare its matrix with: painted live.
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 0u);
  turn = 0.05f;
  host.frame();
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 1u);
  EXPECT_EQ(stateOf(host.composer), Composer::CacheState::Moving);
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u);
  EXPECT_GT(redInk(host), 0);
  // Turning on: the same pixels, composited under the new matrix.
  turn = 0.10f;
  host.frame();
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 1u);
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
  // Held still: painted live again, exactly as a composer that never held
  // it paints the same turn.
  host.frame();
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 0u);
  EXPECT_NE(stateOf(host.composer), Composer::CacheState::Moving);
  Animatable<float> settled = sigil::motion::animatable(0.10f);
  Host reference;
  reference.composer.render(turningWheel(settled));
  reference.frame();
  EXPECT_TRUE(identicalPixels(host, reference, 200, 200));
}

TEST(ComposeMovingHold, AComposerNotLivePaintsATurningSubtreeLive) {
  Animatable<float> turn = sigil::motion::animatable(0.0f);
  Host host;
  host.composer.setProfiling(true);
  host.composer.render(turningWheel(turn));
  for (int step = 0; step < 4; ++step) {
    turn = 0.05f * (float)step;
    host.frame();
    EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 0u);
    EXPECT_NE(stateOf(host.composer), Composer::CacheState::Moving);
  }
  Animatable<float> settled = sigil::motion::animatable(0.15f);
  Host reference;
  reference.composer.render(turningWheel(settled));
  reference.frame();
  EXPECT_TRUE(identicalPixels(host, reference, 200, 200));
}

TEST(ComposeMovingHold, AChangeInsideTheTurningNodeFormsItsPixelsAgain) {
  Animatable<float> turn = sigil::motion::animatable(0.0f);
  Host host;
  host.composer.setLive(true);
  host.composer.render(turningWheel(turn));
  host.frame();
  turn = 0.05f;
  host.frame();
  ASSERT_EQ(host.composer.stats().nodesHeldWhileMoving, 1u);
  host.composer.render(turningWheel(turn, blue()));
  turn = 0.10f;
  host.frame();
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 1u);
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u);
  EXPECT_EQ(redInk(host), 0);
}

TEST(ComposeMovingHold, ANodeWhoseInsideMovesIsNotHeldButItsStillPartIs) {
  Animatable<float> turn = sigil::motion::animatable(0.0f);
  Animatable<float> inner = sigil::motion::animatable(0.0f);
  // A child of the wheel turning on its own clock moves the wheel's pixels
  // in the wheel's own frame, so the wheel has nothing still to hold; the
  // child, turning with nothing moving inside it, is held itself.
  Host host;
  host.composer.setLive(true);
  host.composer.setProfiling(true);
  host.composer.render(
      box().width(200).height(200).children(
          {box()
               .absolute()
               .left(40)
               .top(40)
               .width(120)
               .height(120)
               .key("outer")
               .transformOrigin(percent(50), percent(50))
               .rotate(sigil::motion::bind(turn, {.to = {0.0f, 360.0f}}))
               .children({box()
                              .absolute()
                              .left(10)
                              .top(10)
                              .width(40)
                              .height(40)
                              .key("inner")
                              .fill(red())
                              .rotate(sigil::motion::bind(
                                  inner, {.to = {0.0f, 360.0f}}))})}));
  host.frame();
  for (int step = 1; step < 4; ++step) {
    turn = 0.05f * (float)step;
    inner = 0.1f * (float)step;
    host.frame();
    EXPECT_NE(stateOf(host.composer, "outer"), Composer::CacheState::Moving);
    EXPECT_EQ(stateOf(host.composer, "inner"), Composer::CacheState::Moving);
  }
}
