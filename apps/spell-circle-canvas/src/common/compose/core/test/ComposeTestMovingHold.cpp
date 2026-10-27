// A live composer holds a node as pixels while its own transform or
// opacity moves and nothing it paints moves inside it: the pixels are
// formed once, composited under each new matrix at each new opacity,
// formed again when what the node paints changes, and gone the draw it
// holds still. A composer that is not live paints the same node live under
// every matrix and every opacity.

#include <string>

#include "support/CoreTestSupport.h"

namespace {

using sigil::motion::Animatable;

/** A MARK THAT RESERVES ROOM AND DRAWS NOTHING: the node's paint bounds
 *  grow by what it states, and nothing it paints moves. */
struct Reach {
  float stated = 1;
  bool operator==(const Reach&) const = default;
  float bleed() const { return stated; }
  void paint(sigil::draw::Pen&, const PaintContext&) const {}
};

/** A turning wheel of spokes and lettering about the host's centre: the
 *  node whose own transform moves, with nothing inside it moving; a
 *  positive @p reach adds a mark stating it. */
Element turningWheel(const Animatable<float>& turn, Fill spokeFill = red(),
                     float reach = 0) {
  Element wheel =
      box()
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
  if (reach > 0) wheel.foreground(Reach{reach});
  return box().width(200).height(200).children({std::move(wheel)});
}

/** The same wheel standing still and fading: its opacity bound to
 *  @p fade, its transform fixed. */
Element fadingWheel(const Animatable<float>& fade) {
  Element wheel = box()
                      .absolute()
                      .left(40)
                      .top(40)
                      .width(120)
                      .height(120)
                      .key("wheel")
                      .opacity(fade);
  std::vector<Element> marks;
  for (int index = 0; index < 6; ++index)
    marks.push_back(box()
                        .absolute()
                        .left(55)
                        .top(0)
                        .width(10)
                        .height(60)
                        .transformOrigin(percent(50), percent(100))
                        .rotate((float)index * 60.0f)
                        .fill(red()));
  marks.push_back(text("ROTA").absolute().left(30).top(50));
  wheel.children(std::move(marks));
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
  host.composer.render(box().width(200).height(200).children(
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

TEST(ComposeMovingHold, ALiveComposerFadesAStillSubtreeFromPixelsHeldInPlace) {
  Animatable<float> fade = sigil::motion::animatable(0.5f);
  Host host;
  host.composer.setLive(true);
  host.composer.setProfiling(true);
  host.composer.render(fadingWheel(fade));
  host.frame();
  // The first draw has no opacity to compare with: painted live.
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 0u);
  fade = 0.6f;
  host.frame();
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 1u);
  EXPECT_EQ(stateOf(host.composer), Composer::CacheState::Moving);
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u);
  // Fading on: the same pixels, composited at the new opacity, and the
  // pixels a layer opened for the fade composites at that opacity.
  fade = 0.7f;
  host.frame();
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 1u);
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
  Animatable<float> faded = sigil::motion::animatable(0.7f);
  Host reference;
  reference.composer.render(fadingWheel(faded));
  reference.frame();
  EXPECT_TRUE(identicalPixels(host, reference, 200, 200));
  // Held still: painted live again, and the held pixels go.
  host.frame();
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 0u);
  EXPECT_NE(stateOf(host.composer), Composer::CacheState::Moving);
  EXPECT_TRUE(identicalPixels(host, reference, 200, 200));
}

TEST(ComposeMovingHold, AComposerNotLivePaintsAFadingSubtreeLive) {
  Animatable<float> fade = sigil::motion::animatable(0.2f);
  Host host;
  host.composer.setProfiling(true);
  host.composer.render(fadingWheel(fade));
  for (int step = 0; step < 4; ++step) {
    fade = 0.2f + 0.1f * (float)step;
    host.frame();
    EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 0u);
    EXPECT_NE(stateOf(host.composer), Composer::CacheState::Moving);
  }
}

TEST(ComposeMovingHold, ALoneFillFadesLiveWithNoLayerToSpare) {
  // A lone fill carries its own opacity and opens no layer, so there is
  // nothing for held pixels to save: it is painted live at every opacity.
  Animatable<float> fade = sigil::motion::animatable(0.6f);
  Host host;
  host.composer.setLive(true);
  host.composer.render(
      box().width(200).height(200).children({box()
                                                 .absolute()
                                                 .left(40)
                                                 .top(40)
                                                 .width(120)
                                                 .height(120)
                                                 .key("wheel")
                                                 .opacity(fade)
                                                 .fill(red())}));
  for (int step = 0; step < 4; ++step) {
    fade = 0.6f + 0.1f * (float)step;
    host.frame();
    EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 0u);
    EXPECT_GT(redInk(host), 0);
  }
}

namespace {

/** The turning wheel stating a reach of @p reach, drawn live under a view
 *  that magnifies it by a scale whose texels fall between whole units,
 *  turned once and then held while it turns on. */
SkBitmap heldWheel(float reach) {
  Animatable<float> turn = sigil::motion::animatable(0.0f);
  Element page = turningWheel(turn, red(), reach);
  Host host(260, 260);
  host.composer.setLive(true);
  host.composer.render(page);
  const auto draw = [&host] {
    SkCanvas& canvas = *host.surface->getCanvas();
    canvas.clear(SK_ColorBLACK);
    canvas.save();
    canvas.scale(1.3f, 1.3f);
    host.composer.draw(canvas);
    canvas.restore();
  };
  draw();
  turn = 0.05f;
  draw();
  turn = 0.10f;
  draw();
  EXPECT_EQ(host.composer.stats().nodesHeldWhileMoving, 1u)
      << "the turning wheel stating a reach of " << reach << " was not held";
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(260, 260));
  host.surface->readPixels(pixels.pixmap(), 0, 0);
  return pixels;
}

}  // namespace

TEST(ComposeMovingHold, AHeldTurningNodeBlitsTheSamePixelsWhateverItsBleed) {
  // The held pixels are laid on the node's texel grid, so a mark's bleed
  // that grows the node's bounds grows the pixels with transparent texels
  // and moves none of the picture under the resampled blit.
  const SkBitmap narrow = heldWheel(1);
  const SkBitmap wide = heldWheel(4);
  size_t differing = 0;
  for (int y = 0; y < 260; ++y)
    for (int x = 0; x < 260; ++x)
      if (narrow.getColor(x, y) != wide.getColor(x, y)) ++differing;
  EXPECT_EQ(differing, 0u)
      << "the held wheel blitted other pixels at a reach of four than at "
         "a reach of one";
}
