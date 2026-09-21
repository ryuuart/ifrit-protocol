// Copy flowing around a silhouette: the room a circle and a star each
// give back, the margin that holds the copy off, and the exclusion
// following a target that moves.

#include <string>

#include "support/ShapeTestSupport.h"

namespace {

/** How tall the paragraph came out. It is the one number that reads
 *  "how much room the geometry offered": the same words in the same box
 *  need fewer lines when the exclusion gives room back. */
float flowedHeight(Host& host, const char* key) {
  std::optional<SkRect> bounds = host.composer.bounds(key);
  return bounds ? bounds->height() : 0.0f;
}

/** Words placed on the line band the caller names — the direct reading of
 *  "this line's intervals were longer". */
int inkInBand(Host& host, SkIRect band) {
  int lit = 0;
  for (int y = band.top(); y < band.bottom(); ++y)
    for (int x = band.left(); x < band.right(); ++x)
      if (host.pixel(x, y) == SK_ColorWHITE) ++lit;
  return lit;
}

const std::u8string& flowBody() {
  // Long enough to run past the obstacle on every geometry, so a height
  // comparison reads room-per-line and not "the text stopped early"; and
  // set in words of one letter, so the rag is never further than one
  // word pitch from whatever edge the flow subtracted. At 15 px in the
  // instrument face that pitch is 13.5 px (a 9 px letter and a 4.5 px
  // space): three hundred words are about seventeen lines in the 360 px
  // box, which reaches well past the obstacle and stays well inside the
  // 460 px host with any silhouette on it, and a corner the flow gives
  // back takes a word wherever it is wider than the pitch.
  static const std::u8string body = [] {
    std::u8string all;
    for (int i = 0; i < 300; ++i) all += u8"o ";
    return all;
  }();
  return body;
}

/** One paragraph flowing around one keyed target of the caller's making. */
Element flowScene(Element target, float margin) {
  return stack().children(
      {std::move(target),
       box()
           .inset(0)
           .children({text(flowBody(), whiteStyle(15))
                          .key("body")
                          .contentFlowAround("obstacle", margin)})
           .zIndex(1)});
}

Element obstacleBox(Shape silhouette) {
  Element el = box()
                   .key("obstacle")
                   .width(160)
                   .height(160)
                   .left(100)
                   .top(40)
                   .fill(Fill::color({0, 0.4f, 0, 1}));
  if (silhouette) el.shape(std::move(silhouette));
  return el;
}

}  // namespace

TEST(ComposeDerive, FlowAroundFollowsACircleSilhouette) {
  // A round target gives back the four corners its bounding box was
  // eating, so the same paragraph in the same box gets measurably more
  // room — and the corners themselves take type.
  Host boxed(360, 460), disc(360, 460);
  boxed.composer.render(flowScene(obstacleBox({}), 6));
  boxed.frame();
  disc.composer.render(flowScene(obstacleBox(geometry::shapes::circle()), 6));
  disc.frame();

  EXPECT_LT(flowedHeight(disc, "body"), flowedHeight(boxed, "body"));
  // The corner: inside the box, outside the circle. Type reaches it only
  // under the silhouette.
  const SkIRect corner = SkIRect::MakeLTRB(103, 43, 127, 67);
  EXPECT_FALSE(anyWhiteIn(boxed, corner));
  EXPECT_TRUE(anyWhiteIn(disc, corner));
  // The middle of the disc stays clear either way.
  EXPECT_FALSE(anyWhiteIn(disc, SkIRect::MakeLTRB(150, 100, 210, 140)));
}

TEST(ComposeDerive, FlowAroundFollowsAStarSilhouette) {
  // A concave silhouette is the case a bounding box cannot approximate:
  // text runs INTO the notches between the points.
  Host boxed(360, 460), star(360, 460);
  boxed.composer.render(flowScene(obstacleBox({}), 6));
  boxed.frame();
  star.composer.render(flowScene(obstacleBox(geometry::shapes::star(5)), 6));
  star.frame();

  EXPECT_LT(flowedHeight(star, "body"), flowedHeight(boxed, "body"));
  // The concave half: a star leaves far more of its bounding box open
  // than a disc does, so the notches and corners take more type than the
  // round silhouette can.
  const SkIRect band = SkIRect::MakeLTRB(103, 43, 257, 197);
  EXPECT_GT(inkInBand(star, band), inkInBand(boxed, band));
  // The star's own body still refuses type at its centre.
  EXPECT_FALSE(anyWhiteIn(star, SkIRect::MakeLTRB(165, 105, 195, 135)));
}

TEST(ComposeDerive, FlowAroundMarginHoldsOffTheSilhouette) {
  // The margin means one thing on a silhouette and on a box alike: a
  // standoff from whatever edge is being subtracted. A wider one buys the
  // paragraph less room, never more.
  Host tight(360, 460), wide(360, 460);
  tight.composer.render(flowScene(obstacleBox(geometry::shapes::circle()), 2));
  tight.frame();
  wide.composer.render(flowScene(obstacleBox(geometry::shapes::circle()), 26));
  wide.frame();
  EXPECT_LT(flowedHeight(tight, "body"), flowedHeight(wide, "body"));
}

TEST(ComposeDerive, FlowAroundSilhouetteTracksAMovingTarget) {
  // Moving targets already re-derive; a silhouette target must too.
  auto scene = [](float left) {
    return stack().children(
        {box()
             .key("obstacle")
             .width(160)
             .height(160)
             .left(left)
             .top(40)
             .shape(geometry::shapes::circle())
             .fill(Fill::color({0, 0.4f, 0, 1})),
         box()
             .inset(0)
             .children({text(flowBody(), whiteStyle(15))
                            .key("body")
                            .contentFlowAround("obstacle", 6)})
             .zIndex(1)});
  };
  Host host(360, 460);
  host.composer.render(scene(100));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeLTRB(165, 105, 195, 135)));
  host.composer.render(scene(20));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeLTRB(85, 105, 115, 135)));
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeLTRB(165, 105, 195, 135)));
}
