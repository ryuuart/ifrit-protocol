// The exclusion a derived flow takes from another node: the copy wrapped
// around a frame, the target with no shape that excludes by its box, and
// the cycle that is ignored rather than followed.

#include "support/CoreTestSupport.h"

TEST(ComposeDerive, FlowAroundWrapsTextAroundFrame) {
  const std::u8string body =
      u8"the quick brown fox jumps over the lazy dog and keeps running "
      u8"through the tall summer grass until the river bend appears and "
      u8"the evening light settles over the water in long amber bands";

  auto tree = [&](bool flow) {
    auto t = text(body, whiteStyle(18)).key("body");
    if (flow) t.contentFlowAround("frame", 6);
    return stack().children(
        {box()
             .key("frame")
             .width(150)
             .height(140)
             .inset({.top = 10, .right = 10, .bottom = 210, .left = 200})
             .absolute()
             .fill(Fill::color({0, 0.4f, 0, 1})),
         box().inset(0).children({std::move(t)}).zIndex(1)});
  };

  Host plain(360, 420), flowed(360, 420);
  plain.composer.render(tree(false));
  plain.frame();
  flowed.composer.render(tree(true));
  flowed.frame();

  // Without the exclusion, text runs under the frame region; with it,
  // the region stays text-free (frame color only).
  const SkIRect inner = SkIRect::MakeLTRB(215, 25, 345, 135);
  EXPECT_TRUE(anyWhiteIn(plain, inner));
  EXPECT_FALSE(anyWhiteIn(flowed, inner));

  // Displaced words push the flowed paragraph taller.
  auto plainBounds = plain.composer.bounds("body");
  auto flowedBounds = flowed.composer.bounds("body");
  ASSERT_TRUE(plainBounds && flowedBounds);
  EXPECT_GT(flowedBounds->height(), plainBounds->height());
}

namespace {

const std::u8string& flowBody() {
  // Long enough to run past the obstacle on every geometry, so a height
  // comparison reads room-per-line and not "the text stopped early".
  static const std::u8string body = [] {
    std::u8string one =
        u8"the quick brown fox jumps over the lazy dog and keeps running "
        u8"through the tall summer grass until the river bend appears and "
        u8"the evening light settles over the water in long amber bands "
        u8"while the swallows turn above the reeds and the mill wheel "
        u8"grinds on into the blue hour without hurry or complaint ";
    std::u8string all;
    for (int i = 0; i < 4; ++i) all += one;
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

TEST(ComposeDerive, FlowAroundShapelessTargetKeepsItsBox) {
  // The pin: a target with no silhouette of its own is subtracted by its
  // BOX, exactly as it always was. Every line the box crosses is cut to
  // the box's full width, whatever the type does.
  Host host(360, 460);
  host.composer.render(flowScene(obstacleBox({}), 6));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeLTRB(106, 46, 254, 194)));
}

TEST(ComposeDerive, FlowAroundCycleIsIgnored) {
  Host host;
  host.composer.render(box().children({text(u8"self reference", whiteStyle(16))
                                           .key("self")
                                           .contentFlowAround("self")}));
  host.frame();  // must not hang or exclude itself into nothing
  EXPECT_NE(host.composer.paragraphLayout("self"), nullptr);
}
