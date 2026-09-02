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

namespace {

/** A cube of six 100 px faces about the centre of a 200×200 canvas, as a
 *  preserve3d host turned by @p yaw about y and @p pitch about x, seen
 *  through a view of @p distance on the host's parent. A face is turned
 *  about its own centre and then moved half an edge along the cube's
 *  axis in the host's frame — `rotateY(90).translateX(50)` for the right
 *  face — because the lanes compose as translate · rotate · scale, with
 *  the translate outermost. Declared FRONT FIRST and BACK LAST, so tree
 *  order would put the back on top; the space's depth order must not. */
Element cubeUnder(float distance, float yaw, float pitch) {
  const auto face = [](const char* key, Fill fill) {
    return box()
        .key(key)
        .absolute()
        .rect(SkRect::MakeXYWH(50, 50, 100, 100))
        .fill(std::move(fill));
  };
  return box().perspective(distance).child(
      box()
          .absolute()
          .rect(SkRect::MakeXYWH(0, 0, 200, 200))
          .preserve3d()
          .rotateX(pitch)
          .rotateY(yaw)
          .child(face("front", red()).translateZ(50))
          .child(face("right", green()).rotateY(90).translateX(50))
          .child(face("top", blue()).rotateX(90).translateY(-50))
          .child(face("left", Fill::color({1, 1, 0, 1}))
                     .rotateY(-90)
                     .translateX(-50))
          .child(face("bottom", Fill::color({0, 1, 1, 1}))
                     .rotateX(-90)
                     .translateY(50))
          .child(face("back", Fill::color({1, 0, 1, 1}))
                     .rotateY(180)
                     .translateZ(-50)));
}

Element cube(float yaw, float pitch) { return cubeUnder(800, yaw, pitch); }

constexpr SkColor kYellow = SkColorSetARGB(255, 255, 255, 0);
constexpr SkColor kMagenta = SkColorSetARGB(255, 255, 0, 255);

}  // namespace

TEST(ComposeDepth, ASharedSpaceDrawsItsPlanesBackToFront) {
  // Facing the cube, the front face (declared first) is nearest and must
  // cover the back face (declared last) at the centre; the side faces are
  // edge-on and draw nothing there. Turn the cube half round and the back
  // face is nearest instead; a quarter turn brings a side to the front;
  // a pitch brings the top. Declaration order decides none of it.
  Host host(200, 200);
  host.composer.render(cube(0, 0));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED) << "the front face is nearest";

  host.composer.render(cube(180, 0));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), kMagenta)
      << "half a turn later the back face is nearest";

  host.composer.render(cube(-90, 0));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN)
      << "a quarter turn brings the right face to the front";

  host.composer.render(cube(0, -90));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE)
      << "pitched down, the top face is nearest";
}

TEST(ComposeDepth, TheHostsOwnPlaneNeverHidesItsSpace) {
  // The host is edge-on at a quarter turn about y: its own plane has no
  // width, yet the faces it hosts stand where the space puts them — the
  // right face square to the viewer, magnified a little by the view. A
  // space needs no inverse of its host.
  Host host(200, 200);
  host.composer.render(cube(-90, 0));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(55, 55), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(145, 145), SK_ColorGREEN);
}

TEST(ComposeDepth, ASpaceIsSeenThroughTheViewOfTheNodeThatDeclaredIt) {
  // Turned 45° about y the cube shows its front face on the right half
  // and its left face on the left half, meeting at the centre column —
  // the edge nearest the viewer. The perspective on the cube's parent
  // reaches the faces: that near edge is taller than the two far edges,
  // so each face is a trapezoid, and a cube seen with no view is not.
  Host host(200, 200);
  host.composer.render(cube(45, 0));
  host.frame();
  EXPECT_EQ(host.pixel(103, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(97, 100), kYellow);
  EXPECT_EQ(host.pixel(103, 47), SK_ColorRED) << "the near edge is taller";
  EXPECT_EQ(host.pixel(97, 47), kYellow);
  EXPECT_EQ(host.pixel(168, 52), SK_ColorRED) << "the far edge is not";
  EXPECT_EQ(host.pixel(168, 47), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(32, 47), SK_ColorBLACK);

  Host orthographic(200, 200);
  orthographic.composer.render(cubeUnder(0, 45, 0));
  orthographic.frame();
  EXPECT_EQ(orthographic.pixel(103, 100), SK_ColorRED);
  EXPECT_EQ(orthographic.pixel(103, 47), SK_ColorBLACK)
      << "with no view every edge is 50 px tall";
  EXPECT_EQ(orthographic.pixel(103, 52), SK_ColorRED);
}

namespace {

// CSS's own matrices, written out by hand so the test's ground truth
// shares no code with the kernel: rotateY(deg) and perspective(d) about a
// point, in a frame with +z toward the viewer.
SkM44 cssRotateY(float degrees) {
  const float r = degrees * 0.017453293f;
  const float c = std::cos(r), s = std::sin(r);
  return SkM44(c, 0, s, 0,   //
               0, 1, 0, 0,   //
               -s, 0, c, 0,  //
               0, 0, 0, 1);
}
SkM44 cssPerspective(float distance, SkPoint origin) {
  SkM44 p;
  p.setRC(3, 2, -1.0f / distance);
  return SkM44::Translate(origin.x(), origin.y()) * p *
         SkM44::Translate(-origin.x(), -origin.y());
}

}  // namespace

TEST(ComposeDepth, AHitLandsWhereTheProjectionPutThePlane) {
  // A 100×100 card at (50, 50) turned 50° about its centre, seen through
  // perspective(500) on the root, whose origin is the canvas centre. The
  // projection is built by hand, and every local point of the card hits
  // the card where that projection puts it — and paints red there.
  Host host(200, 200);
  host.composer.render(box().perspective(500).child(
      box()
          .key("card")
          .absolute()
          .rect(SkRect::MakeXYWH(50, 50, 100, 100))
          .fill(red())
          .rotateY(50)));
  host.frame();
  const SkM44 projection = cssPerspective(500, {100, 100}) *
                           SkM44::Translate(50, 50) * SkM44::Translate(50, 50) *
                           cssRotateY(50) * SkM44::Translate(-50, -50);
  const SkMatrix flat = projection.asM33();
  for (SkPoint local : {SkPoint{10, 10}, SkPoint{90, 50}, SkPoint{50, 90},
                        SkPoint{5, 95}, SkPoint{95, 5}}) {
    const SkPoint on = flat.mapPoint(local);
    EXPECT_EQ(host.composer.hitTest(on).value_or(""), "card")
        << "local (" << local.x() << ", " << local.y() << ") projected to ("
        << on.x() << ", " << on.y() << ")";
    EXPECT_EQ(host.pixel((int)std::lround(on.x()), (int)std::lround(on.y())),
              SK_ColorRED)
        << "…and the pixel there is the card's";
  }
  // Just outside the card's own edge, projected: a miss.
  const SkPoint off = flat.mapPoint({-4, 50});
  EXPECT_FALSE(host.composer.hitTest(off).has_value());
  // Inside the FLAT box but off the projected quad: the far (right) edge
  // recedes, so the right side of where the card used to be is empty.
  const SkPoint farEdge = flat.mapPoint({100, 50});
  EXPECT_LT(farEdge.x(), 145.0f);
  EXPECT_FALSE(host.composer.hitTest({145, 100}).has_value())
      << "the flat box's right side is no longer under the card";
  EXPECT_EQ(host.pixel(145, 100), SK_ColorBLACK);
}

TEST(ComposeDepth, AnEdgeOnPlaneAnswersNoHit) {
  Host host(200, 200);
  host.composer.render(box().child(box()
                                       .key("card")
                                       .absolute()
                                       .rect(SkRect::MakeXYWH(50, 50, 100, 100))
                                       .fill(red())
                                       .rotateY(90)));
  host.frame();
  EXPECT_FALSE(host.composer.hitTest({100, 100}).has_value());
}

TEST(ComposeDepth, AHitInASharedSpaceAnswersTheNearestPlane) {
  // The cube's faces are keyed; the face under the centre is the nearest
  // one, whatever order the faces were declared in — the back face is
  // declared last and painted first, and hits only once it is nearest.
  Host host(200, 200);
  host.composer.render(cube(0, 0));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({100, 100}).value_or(""), "front");
  host.composer.render(cube(180, 0));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({100, 100}).value_or(""), "back");
  host.composer.render(cube(-90, 0));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({100, 100}).value_or(""), "right");
  EXPECT_EQ(host.composer.hitTest({55, 55}).value_or(""), "right")
      << "the space places the face, not the edge-on host";
  host.composer.render(cube(45, 0));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({103, 100}).value_or(""), "front");
  EXPECT_EQ(host.composer.hitTest({97, 100}).value_or(""), "left");
  EXPECT_EQ(host.composer.hitTest({103, 47}).value_or(""), "front")
      << "the near edge is taller under the view, and hits there";
  EXPECT_FALSE(host.composer.hitTest({168, 47}).has_value())
      << "…and the far edge is not";
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
