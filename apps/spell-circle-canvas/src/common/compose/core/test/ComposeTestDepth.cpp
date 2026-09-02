// THE DEPTH LANES — a node as a plane turned and moved in depth, the view
// an ancestor declares, the shared space a preserve3d node hosts, and the
// two faces of a plane. Every case here reads pixels off a raster host
// and hits through the composer, so what is asserted is what a reader of
// the verbs would predict from CSS's model: a quarter turn has no width, a
// plane nearer the viewer is larger, a nearer face covers a farther one,
// a hidden back is neither drawn nor hit, and a point lands on a tilted
// plane where the projection put it.

#include <include/core/SkM44.h>

#include "support/CoreTestSupport.h"

namespace {

bool anyRedIn(Host& host, int w, int h) {
  for (int y = 0; y < h; y += 2)
    for (int x = 0; x < w; x += 2)
      if (host.pixel(x, y) == SK_ColorRED) return true;
  return false;
}

/** A 100×100 red plane centred on a 200×200 canvas, turned about y. */
Element turnedAboutY(float degrees) {
  return box().child(box()
                         .absolute()
                         .rect(SkRect::MakeXYWH(50, 50, 100, 100))
                         .fill(red())
                         .rotateY(degrees));
}

}  // namespace

TEST(ComposeDepth, AQuarterTurnAboutYHasNoWidth) {
  // The plane's own lanes place it: at 90° its projection is a line and
  // it draws nothing; at 60° it is cos(60°) of its width about its centre,
  // with no perspective above it to widen one edge over the other.
  Host host(200, 200);
  host.composer.render(turnedAboutY(90));
  host.frame();
  EXPECT_FALSE(anyRedIn(host, 200, 200))
      << "a plane turned edge-on still drew pixels";

  host.composer.render(turnedAboutY(60));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(80, 100), SK_ColorRED);   // inside the 50 px band
  EXPECT_EQ(host.pixel(120, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(60, 100), SK_ColorBLACK);  // where the flat box was
  EXPECT_EQ(host.pixel(140, 100), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(100, 60), SK_ColorRED);  // the height is untouched
}

TEST(ComposeDepth, PerspectiveScalesAPlaneMovedInDepth) {
  // The view is the PARENT's: perspective(400) on the root, whose origin
  // is the canvas centre, projects a child at depth z by 1 / (1 − z/400).
  // A 50 px plane centred on that origin doubles at z = +200 and halves at
  // z = −400; at z = 0 the view leaves it exactly where it was.
  Host host(200, 200);
  const auto scene = [](float z) {
    return box().perspective(400).child(box()
                                            .absolute()
                                            .rect(SkRect::MakeXYWH(75, 75, 50,
                                                                   50))
                                            .fill(red())
                                            .translateZ(z));
  };
  host.composer.render(scene(0));
  host.frame();
  EXPECT_EQ(host.pixel(80, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(70, 100), SK_ColorBLACK);

  host.composer.render(scene(200));  // toward the viewer: 50..150
  host.frame();
  EXPECT_EQ(host.pixel(60, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(140, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(45, 100), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(155, 100), SK_ColorBLACK);

  host.composer.render(scene(-400));  // away: 87.5..112.5
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(90, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(110, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(84, 100), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(116, 100), SK_ColorBLACK);
}

TEST(ComposeDepth, AViewOnTheParentLeavesAFlatChildAlone) {
  // A child at z = 0 with no turn divides by 1: the parent's perspective
  // moves nothing about it, and the pixels are the flat child's.
  Host flat(200, 200);
  flat.composer.render(box().child(
      box().absolute().rect(SkRect::MakeXYWH(30, 40, 80, 60)).fill(red())));
  flat.frame();
  Host viewed(200, 200);
  viewed.composer.render(box().perspective(300).child(
      box().absolute().rect(SkRect::MakeXYWH(30, 40, 80, 60)).fill(red())));
  viewed.frame();
  EXPECT_TRUE(identicalPixels(flat, viewed, 200, 200));
}

TEST(ComposeDepth, ADepthLaneRampsLikeAnyOtherLane) {
  // The lanes are Instance::Slot rows, so a re-described rotateY with a
  // transition ramps from the turn it is at: halfway through a 0 → 90
  // ramp the plane is mid-turn — narrower than flat, still visible.
  Host host(200, 200);
  host.composer.render(box().child(box()
                                       .absolute()
                                       .rect(SkRect::MakeXYWH(50, 50, 100, 100))
                                       .fill(red())
                                       .rotateY(0)));
  host.frame();
  host.composer.render(box().child(
      box()
          .absolute()
          .rect(SkRect::MakeXYWH(50, 50, 100, 100))
          .fill(red())
          .rotateY(animate(motion::to(90.0f), motion::Transition{
                                          .duration = 200ms,
                                          .ease = &choreograph::easeNone}))));
  host.frame(0.1);  // 45°: cos(45°) · 100 ≈ 71 px about the centre
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(70, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(55, 100), SK_ColorBLACK);
  EXPECT_TRUE(host.ticker.active());
  host.frame(0.2);  // landed at 90°: gone
  EXPECT_FALSE(anyRedIn(host, 200, 200));
}
