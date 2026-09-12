// The tether: a box hung off another one's finished geometry at a pair of
// normalized points, and the list of places it tries when the first will
// not fit.

#include "support/CoreTestSupport.h"

namespace {

/** A tether tying a badge to the anchor, hanging the badge's @p at point
 *  on the anchor's @p on point. */
Tether hung(SkPoint on, SkPoint at, SkVector offset = {0, 0}) {
  return Tether{.key = "anchor", .on = on, .at = at, .offset = offset};
}

/** An anchor of 40×20 at (@p x, @p y) in a full-canvas root, with a 60×20
 *  badge hung off it however @p tether says. */
Element scene(SkPoint anchorAt, Tether tether) {
  return box()
      .absolute()
      .inset(0)
      .child(box()
                 .key("anchor")
                 .absolute()
                 .left(Dimension(anchorAt.x()))
                 .top(Dimension(anchorAt.y()))
                 .width(Dimension(40.0f))
                 .height(Dimension(20.0f))
                 .fill(green()))
      .child(box()
                 .key("badge")
                 .width(Dimension(60.0f))
                 .height(Dimension(20.0f))
                 .fill(red())
                 .tether(std::move(tether)));
}

}  // namespace

TEST(ComposeTether, ThePairOfPointsIsTheWholePosition) {
  Host host(300, 300);
  // Centred above: the badge's bottom-centre lands on the anchor's
  // top-centre.
  host.composer.render(scene({100, 100}, hung({0.5f, 0.0f}, {0.5f, 1.0f})));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("badge")),
            SkRect::MakeXYWH(100 + 20 - 30, 100 - 20, 60, 20));
}

TEST(ComposeTether, TheOffsetIsInTheCompositionsAxes) {
  Host host(300, 300);
  host.composer.render(
      scene({100, 100}, hung({1.0f, 0.5f}, {0.0f, 0.5f}, {8, -4})));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("badge")),
            SkRect::MakeXYWH(140 + 8, 110 - 4 - 10, 60, 20));
}

TEST(ComposeTether, TheFirstFallbackThatFitsIsTheOneTaken) {
  Host host(300, 300);
  // Above would put the badge off the top of the canvas; below fits, and
  // is the first fallback that does. The one after it is never reached.
  Tether above = hung({0.5f, 0.0f}, {0.5f, 1.0f});
  above.fallbacks = {hung({0.5f, 1.0f}, {0.5f, 0.0f}),
                     hung({0.0f, 0.5f}, {1.0f, 0.5f})};
  host.composer.render(scene({100, 4}, std::move(above)));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("badge")),
            SkRect::MakeXYWH(100 + 20 - 30, 4 + 20, 60, 20));
}

TEST(ComposeTether, TheORDEROfTheFallbacksIsTheOrderTheyWereWrittenIn) {
  Host host(300, 300);
  // The same two places, listed the other way round: "to the left" fits
  // too, so listing it first is what makes it the answer.
  Tether above = hung({0.5f, 0.0f}, {0.5f, 1.0f});
  above.fallbacks = {hung({0.0f, 0.5f}, {1.0f, 0.5f}),
                     hung({0.5f, 1.0f}, {0.5f, 0.0f})};
  host.composer.render(scene({100, 4}, std::move(above)));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("badge")),
            SkRect::MakeXYWH(100 - 60, 4 + 10 - 10, 60, 20));
}

TEST(ComposeTether, WhenNothingFitsTheStatedPlaceStands) {
  Host host(120, 40);
  // A canvas too small for the badge to sit anywhere clear of its edges:
  // the badge is still placed, and it is placed where it was asked for.
  Tether above = hung({0.5f, 0.0f}, {0.5f, 1.0f});
  above.fallbacks = {hung({0.5f, 1.0f}, {0.5f, 0.0f})};
  host.composer.render(scene({40, 10}, std::move(above)));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("badge")),
            SkRect::MakeXYWH(40 + 20 - 30, 10 - 20, 60, 20));
}

TEST(ComposeTether, AWithinNarrowerThanTheCanvasIsWhatFitsIsJudgedAgainst) {
  Host host(300, 300);
  Tether above = hung({0.5f, 0.0f}, {0.5f, 1.0f});
  // The badge would clear the canvas, but not the panel it must stay in.
  above.within = SkRect::MakeXYWH(0, 90, 300, 200);
  above.fallbacks = {hung({0.5f, 1.0f}, {0.5f, 0.0f})};
  host.composer.render(scene({100, 100}, std::move(above)));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("badge")),
            SkRect::MakeXYWH(100 + 20 - 30, 120, 60, 20));
}

TEST(ComposeTether, AnUnknownKeyIsSilentAndTheBoxStaysWhereLayoutLeftIt) {
  Host host(300, 300);
  host.composer.render(
      box().absolute().inset(0).child(box()
                                          .key("badge")
                                          .width(Dimension(60.0f))
                                          .height(Dimension(20.0f))
                                          .fill(red())
                                          .tether({.key = "nobody"})));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("badge")),
            SkRect::MakeXYWH(0, 0, 60, 20));
}

TEST(ComposeTether, ABoxCannotHangOffItsOwnDescendant) {
  Host host(300, 300);
  // The child's box is derived from this one's, so tying this one to the
  // child would feed the position its own output; the tie is refused and
  // the box stays where layout left it.
  host.composer.render(box().absolute().inset(0).child(
      box()
          .key("badge")
          .width(Dimension(60.0f))
          .height(Dimension(20.0f))
          .tether({.key = "inner"})
          .child(box()
                     .key("inner")
                     .width(Dimension(10.0f))
                     .height(Dimension(10.0f)))));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("badge")),
            SkRect::MakeXYWH(0, 0, 60, 20));
}

TEST(ComposeTether, TheBoxFollowsTheAnchorWhenTheAnchorMoves) {
  Host host(300, 300);
  const auto at = [&](float y) {
    host.composer.render(scene({100, y}, hung({0.5f, 1.0f}, {0.5f, 0.0f})));
    host.frame();
    return require(host.composer.bounds("badge"));
  };
  const SkRect first = at(100);
  const SkRect moved = at(160);
  EXPECT_FLOAT_EQ(moved.top() - first.top(), 60);
}
