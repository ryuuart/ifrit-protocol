// The box a node is given and the order siblings paint in: flex sizing and
// fills, a user layout scheme placing its own cells, z-index and the
// composite an opacity or blend implies, per-axis scale about a transform
// origin, the child-bounds union a parent reports, and the placement
// longhand beside the two shorthands that must describe the same node,
// the per-edge spacing and the Dimension literals a box is sized by, and
// the hit test that follows paint order and skew.

#include "support/CoreTestSupport.h"

TEST(ComposeLayout, FlexRowPositionsAndFills) {
  Host host;
  host.composer.render(
      box().row().gap(20).children({box().width(50).height(50).fill(red()),
                                    box().width(50).height(50).fill(green())}));
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);    // first child
  EXPECT_EQ(host.pixel(60, 25), SK_ColorBLACK);  // the gap
  EXPECT_EQ(host.pixel(95, 25), SK_ColorGREEN);  // second child at 70..120
}

TEST(ComposeLayout, ACanvasRelativeLengthIsTheRootsBoxHoweverDeepItIsWritten) {
  // pw and ph are the CANVAS, where pct is the parent: a half-canvas child
  // three boxes deep is half the canvas, not half of whatever box it sits
  // in, and it follows the canvas when the canvas changes size.
  Host host(400, 200);
  const auto tree = [] {
    // flexShrink(0) on both: what is read back is what each unit RESOLVED to,
    // not how the flex line fitted it afterwards.
    return box().padding(50).children({box().width(100).height(100).children(
        {box().key("canvas").width(50_pw).height(50_ph).flexShrink(0).fill(
             red()),
         box().key("parent").width(50_pct).height(50_pct).flexShrink(0).fill(
             green())})});
  };
  host.composer.render(tree());
  host.frame();
  auto canvas = host.composer.bounds("canvas");
  auto parent = host.composer.bounds("parent");
  ASSERT_TRUE(canvas.has_value());
  ASSERT_TRUE(parent.has_value());
  EXPECT_FLOAT_EQ(canvas->width(), 200.0f) << "half the canvas's 400";
  EXPECT_FLOAT_EQ(canvas->height(), 100.0f) << "half the canvas's 200";
  EXPECT_FLOAT_EQ(parent->width(), 50.0f) << "half the parent's 100";

  // A RESIZE moves them, and nothing but the resize had to say so.
  host.composer.setSize({200, 400});
  host.frame();
  canvas = host.composer.bounds("canvas");
  ASSERT_TRUE(canvas.has_value());
  EXPECT_FLOAT_EQ(canvas->width(), 100.0f);
  EXPECT_FLOAT_EQ(canvas->height(), 200.0f);
}

TEST(ComposeLayout, TextSizesItselfInFlex) {
  Host host(400, 200);
  host.composer.render(
      box().row().padding(10).children({text(u8"Hello compose", styleAt(24))}));
  host.frame();
  auto rect = host.composer.bounds("t");
  (void)rect;
  // The text node measured to a plausible single-line extent.
  host.composer.render(box().row().padding(10).children(
      {text(u8"Hello compose", styleAt(24)).key("t")}));
  host.frame();
  auto measured = host.composer.bounds("t");
  ASSERT_TRUE(measured.has_value());
  EXPECT_GT(measured->width(), 60.0f);
  EXPECT_LT(measured->width(), 380.0f);
  EXPECT_GT(measured->height(), 10.0f);
  EXPECT_LT(measured->height(), 60.0f);
  EXPECT_EQ(measured->left(), 10.0f);
  ASSERT_NE(host.composer.paragraphLayout("t"), nullptr);
}

TEST(ComposeStacking, ZIndexReordersSiblings) {
  Host host;
  // Later sibling has LOWER zIndex → paints first → red wins on top.
  host.composer.render(
      stack().children({box().inset(0).fill(red()).zIndex(1),
                        box().inset(0).fill(green()).zIndex(0)}));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
}

TEST(ComposeStacking, OpacityAndBlendComposite) {
  Host host;
  host.composer.render(stack().children(
      {box().inset(0).fill(red()), box().inset(0).fill(blue()).opacity(0.5f)}));
  host.frame();
  SkColor c = host.pixel(100, 100);
  // Half blue over red: both channels present.
  EXPECT_NEAR(SkColorGetR(c), 128, 10);
  EXPECT_NEAR(SkColorGetB(c), 128, 10);
}

namespace {

/** A lightweight grid, ~20 lines of user code. */
struct Grid {
  int columns = 2;
  float gap = 8;
  float cellHeight = 40;

  std::vector<SkRect> place(const LayoutInput& in) const {
    std::vector<SkRect> rects;
    const float cellWidth =
        (in.container.width() - gap * (float)(columns - 1)) / (float)columns;
    for (size_t i = 0; i < in.childSizes.size(); ++i) {
      const int col = (int)i % columns;
      const int row = (int)i / columns;
      rects.push_back(SkRect::MakeXYWH((cellWidth + gap) * (float)col,
                                       (cellHeight + gap) * (float)row,
                                       cellWidth, cellHeight));
    }
    return rects;
  }
};

}  // namespace

TEST(ComposeLayoutScheme, GridPlacesAndSizesCells) {
  Host host(200, 200);
  auto grid = layout(Grid{.columns = 2, .gap = 10, .cellHeight = 30})
                  .width(190)
                  .height(190);
  for (int i = 0; i < 4; ++i)
    grid.children(
        {box().key("cell" + std::to_string(i)).fill(i % 2 ? green() : red())});
  host.composer.render(box().children({std::move(grid)}));
  host.frame();

  auto c0 = host.composer.bounds("cell0");
  auto c1 = host.composer.bounds("cell1");
  auto c3 = host.composer.bounds("cell3");
  ASSERT_TRUE(c0 && c1 && c3);
  EXPECT_EQ(c0->left(), 0.0f);
  EXPECT_EQ(c0->width(), 90.0f);  // (190 - 10) / 2
  EXPECT_EQ(c0->height(), 30.0f);
  EXPECT_EQ(c1->left(), 100.0f);  // second column
  EXPECT_EQ(c3->top(), 40.0f);    // second row
  EXPECT_EQ(host.pixel(45, 15), SK_ColorRED);
  EXPECT_EQ(host.pixel(145, 15), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(145, 55), SK_ColorGREEN);
}

TEST(ComposeTransform, ScaleXGrowsFromItsOrigin) {
  // The bar primitive. transformOrigin pins the LEFT edge, scaleX carries
  // the fraction, and the fill grows rightward — no clip, no counter-
  // translation, and correct for any fill (the translate-inside-a-clip
  // workaround is only correct for gradients along the other axis).
  Host host(200, 40);
  choreograph::Output<float> fraction{0.25f};
  host.composer.render(
      box().children({box()
                          .width(200)
                          .height(40)
                          .absolute()
                          .left(0)
                          .top(0)
                          .transformOrigin(pct(0), pct(50))
                          .scaleX(&fraction)
                          .fill(material::skia::Paint::solid({1, 0, 0, 1}))}));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(20, 20)), 200u);  // inside the quarter
  EXPECT_LT(SkColorGetR(host.pixel(80, 20)), 60u);   // past it
  fraction = 0.75f;  // bound value moves — no re-render
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(80, 20)), 200u);
  EXPECT_LT(SkColorGetR(host.pixel(180, 20)), 60u);
}

TEST(ComposeTransform, ScaleYIsIndependentOfScaleX) {
  Host host(200, 200);
  host.composer.render(
      box().children({box()
                          .width(200)
                          .height(200)
                          .absolute()
                          .left(0)
                          .top(0)
                          .transformOrigin(pct(0), pct(0))
                          .scaleX(0.25f)
                          .scaleY(0.75f)
                          .fill(material::skia::Paint::solid({0, 1, 0, 1}))}));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(10, 10)), 200u);   // inside both
  EXPECT_LT(SkColorGetG(host.pixel(90, 10)), 60u);    // past x, inside y
  EXPECT_GT(SkColorGetG(host.pixel(10, 140)), 200u);  // inside x, inside y
  EXPECT_LT(SkColorGetG(host.pixel(10, 190)), 60u);   // past y
}

TEST(ComposeTransform, AnOriginIsAPercentageOfTheBoxOrALengthInIt) {
  // One verb, CSS's: a percentage is of the node's own box, any other length
  // is node-local pixels, and the two axes need not share a unit. The box
  // is 80 square at (60, 60) and shrinks to a quarter of its side, so where
  // the red lands says where the pivot stood.
  const auto shrunk = [](Element pivoted) {
    Host host(200, 200);
    host.composer.render(
        box().children({pivoted.absolute()
                            .rect(SkRect::MakeXYWH(60, 60, 80, 80))
                            .fill(red())
                            .scale(0.25f)}));
    host.frame();
    SkIRect ink = SkIRect::MakeEmpty();
    for (int row = 0; row < 200; ++row)
      for (int column = 0; column < 200; ++column)
        if (host.pixel(column, row) == SK_ColorRED)
          ink.join(SkIRect::MakeXYWH(column, row, 1, 1));
    return ink;
  };
  // Unstated is the centre, and fifty percent says the same.
  EXPECT_EQ(shrunk(box()), SkIRect::MakeLTRB(90, 90, 110, 110));
  EXPECT_EQ(shrunk(box().transformOrigin(pct(50), pct(50))),
            SkIRect::MakeLTRB(90, 90, 110, 110));
  // The right and bottom edges: the quarter stands in the far corner.
  EXPECT_EQ(shrunk(box().transformOrigin(pct(100), pct(100))),
            SkIRect::MakeLTRB(120, 120, 140, 140));
  // The same corner in pixels.
  EXPECT_EQ(shrunk(box().transformOrigin(Dimension(80), Dimension(80))),
            SkIRect::MakeLTRB(120, 120, 140, 140));
  // A pivot outside the box: forty pixels left of it, level with its top.
  EXPECT_EQ(shrunk(box().transformOrigin(Dimension(-40), pct(0))),
            SkIRect::MakeLTRB(30, 60, 50, 80));
}

TEST(ComposePaintBounds, PerAxisScaleReachesTheParentsChildBoundsUnion) {
  // `recordBounds()` decides whether a child's transform widens the parent's
  // bounds, and it must recognise exactly the transforms `NodeTransform`
  // applies for paint() and hitInstance() — per-axis scale included. Miss one
  // and a child whose ONLY transform is a per-axis scale hands its parent
  // unscaled bounds, and every consumer sized off them (the effect layer
  // here, the opacity layer, the texture bake) silently truncates the
  // overflow.
  //
  // The parent takes an identity offset() filter purely to force a bounded
  // saveLayer: that layer clips to recordBounds(), so wrong bounds delete the
  // scaled-out half of the bar instead of merely mis-sizing something.
  Host host(200, 200);
  host.composer.render(
      box().children({box()
                          .absolute()
                          .rect(SkRect::MakeXYWH(20, 20, 40, 40))
                          .filter(material::skia::Effect::filter(
                              SkImageFilters::Offset(0, 0, nullptr)))
                          .children({box()
                                         .absolute()
                                         .rect(SkRect::MakeXYWH(0, 0, 40, 40))
                                         .transformOrigin(pct(0), pct(0))
                                         .fill(red())
                                         .scaleX(3.0f)})}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 40), SK_ColorRED) << "the unscaled part is missing";
  EXPECT_EQ(host.pixel(120, 40), SK_ColorRED)
      << "the scaled-out part of the bar was clipped away — recordBounds() "
         "did not see scaleX on the child";
  EXPECT_EQ(host.pixel(150, 40), SK_ColorBLACK) << "…and it over-reached";
}

namespace {

/** The smallest box holding every inked pixel inside @p region — where the
 *  ink STARTS on both axes, which is the only reading of "the words stand
 *  here" that does not depend on which glyph the face put first. White type
 *  on black, so any red channel above the antialiasing floor is ink. */
SkIRect inkBoundsIn(Host& host, const SkRect& region) {
  const SkIRect bounds = region.roundOut();
  SkIRect ink = SkIRect::MakeEmpty();
  for (int y = bounds.top(); y < bounds.bottom(); ++y)
    for (int x = bounds.left(); x < bounds.right(); ++x)
      if (SkColorGetR(host.pixel(x, y)) > 32)
        ink.join(SkIRect::MakeXYWH(x, y, 1, 1));
  return ink;
}

}  // namespace

TEST(ComposeLayout, APaddedTextLeafSetsItsWordsInsideItsPadding) {
  // padding() on a text leaf means what it means on a box and in CSS: the
  // content box inset by it. The node grows by the padding on both axes AND
  // the paragraph is laid out in what is left, so a fill on the leaf is the
  // scrim the words stand ON rather than a panel hanging off their corner.
  //
  // Read against a BARE leaf of the same words rather than against the
  // face's own metrics: what is claimed is the padding's effect, and the
  // difference between the two leaves is the whole of it.
  const float pad = 12.0f;
  Host host(240, 240);
  host.composer.render(
      box().gap(20).children({text(u8"Hi", whiteStyle(32)).key("bare"),
                              text(u8"Hi", whiteStyle(32))
                                  .key("pad")
                                  .padding(pad)
                                  .fill(Fill::color({0, 0, 0.5f, 1}))}));
  host.frame();
  const SkRect bare = require(host.composer.bounds("bare"));
  const SkRect padded = require(host.composer.bounds("pad"));

  EXPECT_FLOAT_EQ(padded.width(), bare.width() + 2 * pad);
  EXPECT_FLOAT_EQ(padded.height(), bare.height() + 2 * pad);

  const SkIRect bareInk = inkBoundsIn(host, bare);
  const SkIRect paddedInk = inkBoundsIn(host, padded);
  ASSERT_FALSE(bareInk.isEmpty());
  ASSERT_FALSE(paddedInk.isEmpty());
  EXPECT_NEAR((float)paddedInk.left() - padded.left(),
              (float)bareInk.left() - bare.left() + pad, 1.0f)
      << "the words did not move one padding in from the left edge";
  EXPECT_NEAR((float)paddedInk.top() - padded.top(),
              (float)bareInk.top() - bare.top() + pad, 1.0f)
      << "the words did not move one padding down from the top edge";
  // …and the far edges are one padding clear too, which is what makes the
  // scrim surround the reading instead of hanging off one side of it.
  EXPECT_NEAR(padded.right() - (float)paddedInk.right(),
              bare.right() - (float)bareInk.right() + pad, 1.0f);
  EXPECT_NEAR(padded.bottom() - (float)paddedInk.bottom(),
              bare.bottom() - (float)bareInk.bottom() + pad, 1.0f);
}

TEST(ComposeLayout, ACoveringNodeGivenASizeStandsInTheFlowAgain) {
  // A canvas fills the box it stands in, and a canvas given a box of its
  // own stands in the flow at that box: what follows it lands after it
  // rather than on top of it, which is what a captioned drawing on a
  // sheet asks for.
  Host host(400, 400);
  host.composer.render(box().column().gap(10).children(
      {custom([](SkCanvas&) {}).cover().width(100).height(60).key("drawing"),
       box().key("after").width(100).height(20).fill(red())}));
  host.frame();
  const auto drawing = host.composer.bounds("drawing");
  const auto after = host.composer.bounds("after");
  ASSERT_TRUE(drawing.has_value());
  ASSERT_TRUE(after.has_value());
  EXPECT_FLOAT_EQ(drawing->height(), 60.0f);
  EXPECT_FLOAT_EQ(after->top(), 70.0f) << "the drawing took its 60 and the gap";
  // A pin stated after the size is a placement, and the node leaves the
  // flow again: what follows it lands where it would have without it.
  host.composer.render(box().column().gap(10).children(
      {custom([](SkCanvas&) {})
           .cover()
           .width(100)
           .height(60)
           .top(5)
           .key("pinned"),
       box().key("next").width(100).height(20).fill(red())}));
  host.frame();
  const auto pinned = host.composer.bounds("pinned");
  const auto next = host.composer.bounds("next");
  ASSERT_TRUE(pinned.has_value());
  ASSERT_TRUE(next.has_value());
  EXPECT_FLOAT_EQ(pinned->top(), 5.0f);
  EXPECT_FLOAT_EQ(next->top(), 0.0f)
      << "a pinned node holds no place in the flow";
}

// -------------------------------------------------------------------------
// The placement longhand and its two shorthands: the rect that names
// the whole box, the corner pin that leaves the node to size itself,
// and the edge setter that makes a node absolute by itself.

TEST(ComposePlacement, RectIsTheLonghandAndPrunesIdentically) {
  // rect() is sugar for .absolute().left().top().width().height(), and the
  // entire safety argument is that it describes the SAME node. So a
  // re-describe that swaps one spelling for the other must prune to zero
  // patches and must not move a pixel — anything less and the two spellings
  // are different nodes wearing one name.
  Host host(200, 200);
  const SkRect r = SkRect::MakeXYWH(40, 60, 50, 30);

  auto longhand = [&] {
    return box().children({box()
                               .key("plate")
                               .absolute()
                               .left(40)
                               .top(60)
                               .width(50)
                               .height(30)
                               .fill(red())});
  };
  auto terse = [&] {
    return box().children({box().key("plate").rect(r).fill(red())});
  };

  host.composer.render(longhand());
  host.frame();
  const auto boundsLonghand = host.composer.bounds("plate");
  ASSERT_TRUE(boundsLonghand.has_value());
  EXPECT_EQ(*boundsLonghand, r);
  EXPECT_EQ(host.pixel(45, 65), SK_ColorRED);    // inside
  EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);  // above the top edge
  EXPECT_EQ(host.pixel(95, 65), SK_ColorBLACK);  // right of the right edge

  // Re-describe with rect(). Equal properties => the reconciler prunes it.
  host.composer.render(terse());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "rect() described a node the reconciler considered DIFFERENT from "
         "the longhand — it is not writing the same LayoutProps fields";
  EXPECT_EQ(host.composer.bounds("plate"), boundsLonghand);
  EXPECT_EQ(host.pixel(45, 65), SK_ColorRED);
  EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);

  // And back the other way, so neither direction is the privileged one.
  host.composer.render(longhand());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);

  // NEGATIVE CONTROL — without this the two assertions above pass on a
  // composer that never patches anything, which is exactly the vacuous
  // shape this program keeps finding. A different rect MUST patch.
  host.composer.render(box().children(
      {box().key("plate").rect(SkRect::MakeXYWH(41, 60, 50, 30)).fill(red())}));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "the patch counter is not live, so the zeroes above prove nothing";
  EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);
  EXPECT_EQ(require(host.composer.bounds("plate")).fLeft, 41.0f);
}

TEST(ComposePlacement, RectAndAtTakeLengthsInAnyUnit) {
  // The two shorthands take what the longhand takes: a percent of the
  // parent's box is one re-describe away from the four setters, and prunes
  // against them like the pixel forms do.
  Host host(200, 100);
  auto longhand = [] {
    return box().width(200).height(100).children(
        {box()
             .key("plate")
             .left(pct(10))
             .top(30)
             .width(pct(25))
             .height(40)
             .fill(red()),
         box().key("pin").width(10).height(10).left(pct(50)).top(pct(20))});
  };
  auto terse = [] {
    return box().width(200).height(100).children(
        {box().key("plate").rect(pct(10), 30, pct(25), 40).fill(red()),
         box().key("pin").width(10).height(10).at(pct(50), pct(20))});
  };

  host.composer.render(longhand());
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("plate")),
            SkRect::MakeXYWH(20, 30, 50, 40));
  EXPECT_EQ(require(host.composer.bounds("pin")),
            SkRect::MakeXYWH(100, 20, 10, 10));

  host.composer.render(terse());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "rect(x, y, width, height) or at(x, y) is not the longhand";
  EXPECT_EQ(require(host.composer.bounds("plate")),
            SkRect::MakeXYWH(20, 30, 50, 40));
  EXPECT_EQ(require(host.composer.bounds("pin")),
            SkRect::MakeXYWH(100, 20, 10, 10));
}

TEST(ComposePlacement, AtPinsTheCornerAndLeavesTheNodeToSizeItself) {
  // The 187-site half of the longhand that carries no box: .left().top()
  // on a node that measures itself from its content.
  Host host(300, 200);
  auto longhand = [] {
    return box().children({text(u8"Wm", styleAt(20))
                               .key("cap")
                               .absolute()
                               .left(30)
                               .top(40)});
  };
  auto terse = [] {
    return box().children({text(u8"Wm", styleAt(20)).key("cap").at({30, 40})});
  };

  host.composer.render(longhand());
  host.frame();
  const auto measured = host.composer.bounds("cap");
  ASSERT_TRUE(measured.has_value());
  EXPECT_FLOAT_EQ(measured->fLeft, 30.0f);
  EXPECT_FLOAT_EQ(measured->fTop, 40.0f);
  // Sized by its content, not by the caller: this is what rect() cannot do
  // and is why at() exists separately (ScenesPersona.h:438-447 is the
  // gallery case that rect() cannot serve at all).
  EXPECT_GT(measured->width(), 1.0f);

  host.composer.render(terse());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "at() is not left().top()";
  EXPECT_EQ(host.composer.bounds("cap"), measured);

  // Negative control, as above.
  host.composer.render(
      box().children({text(u8"Wm", styleAt(20)).key("cap").at({31, 40})}));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
}

TEST(ComposeLayout, AnEdgeSetterMakesANodeAbsoluteAndAloneAbsoluteStillDoes) {
  // Every edge setter sets layout.absolute itself, so `.absolute()` before
  // or after one writes a bool that is already written and can be left
  // out. It cannot be left out of a node that pins no edge, though, which
  // is the shape a blind removal would silently un-absolute.
  Host host(200, 200);

  auto withRedundant = [] {
    return box().children({box()
                               .key("p")
                               .absolute()
                               .left(30)
                               .top(30)
                               .width(20)
                               .height(20)
                               .fill(red())});
  };
  auto without = [] {
    return box().children({box()
                               .key("p")
                               .left(30)
                               .top(30)
                               .width(20)
                               .height(20)
                               .fill(red())});
  };
  host.composer.render(withRedundant());
  host.frame();
  const auto pinned = host.composer.bounds("p");
  ASSERT_TRUE(pinned.has_value());
  EXPECT_EQ(*pinned, SkRect::MakeXYWH(30, 30, 20, 20));

  host.composer.render(without());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "dropping a redundant .absolute() changed the description";
  EXPECT_EQ(host.composer.bounds("p"), pinned);
  EXPECT_EQ(host.pixel(35, 35), SK_ColorRED);

  // The 48-call shape the sweep must NOT touch: absolute with a size and no
  // pinned edge. Here .absolute() is the only thing taking it out of flow,
  // so removing it moves the node behind its sibling.
  Host flow(200, 200);
  flow.composer.render(box().row().children(
      {box().width(60).height(20).fill(green()),
       box().key("q").absolute().width(20).height(20).fill(red())}));
  flow.frame();
  ASSERT_TRUE(flow.composer.bounds("q").has_value());
  EXPECT_FLOAT_EQ(require(flow.composer.bounds("q")).fLeft, 0.0f);

  flow.composer.render(
      box().row().children({box().width(60).height(20).fill(green()),
                            box().key("q").width(20).height(20).fill(red())}));
  flow.frame();
  EXPECT_FLOAT_EQ(require(flow.composer.bounds("q")).fLeft, 60.0f)
      << "if this is still 0 then .absolute() alone is ALSO redundant and "
         "the sweep's predicate is over-cautious; if it is 60 the predicate "
         "is exactly right";
}

// -------------------------------------------------------------------------
// Per-edge spacing and Dimension literals; the hit test under paint
// order and keys; and the skew that leans both the paint and the hits.

TEST(ComposeLayout, PerEdgePaddingAndMargin) {
  Host host;
  host.composer.render(box().children(
      {box()
           .padding({.top = 20, .right = 30, .bottom = 40, .left = 10})
           .key("outer")
           .children(
               {box()
                    .margin({.top = 6, .right = 7, .bottom = 8, .left = 5})
                    .width(50)
                    .height(50)
                    .key("inner")})}));
  host.frame();
  auto inner = host.composer.bounds("inner");
  ASSERT_TRUE(inner.has_value());
  EXPECT_FLOAT_EQ(inner->left(), 10 + 5);  // padding.left + margin.left
  EXPECT_FLOAT_EQ(inner->top(), 20 + 6);   // padding.top + margin.top
}

TEST(ComposeLayout, DimLiteralsResolvePercent) {
  Host host;
  host.composer.render(box().children(
      {box().width(50_pct).height(25_pct).fill(red()).key("half")}));
  host.frame();
  auto rect = host.composer.bounds("half");
  ASSERT_TRUE(rect.has_value());
  EXPECT_FLOAT_EQ(rect->width(), 100.0f);  // 50% of the 200px host
  EXPECT_FLOAT_EQ(rect->height(), 50.0f);  // 25% of 200px
}

TEST(ComposeQueries, HitTestRespectsPaintOrderAndKeys) {
  Host host;
  host.composer.render(stack().children(
      {box().key("under").inset(0).fill(red()),
       box()
           .key("over")
           .width(60)
           .height(60)
           .inset({.top = 20, .right = 120, .bottom = 120, .left = 20})
           .absolute()
           .fill(green()),
       box()
           .width(30)
           .height(30)
           .inset({.top = 150, .right = 20, .bottom = 20, .left = 150})
           .absolute()
           .fill(blue())}));  // keyless → falls to root
  host.frame();
  EXPECT_EQ(host.composer.hitTest({50, 50}).value_or(""), "over");
  EXPECT_EQ(host.composer.hitTest({120, 120}).value_or(""), "under");
  // Keyless box resolves to its nearest keyed ancestor (none here above
  // the stack root, which is keyless) — the "under" sibling is NOT an
  // ancestor, so the keyless box hits nothing of its own and the point
  // falls through to "under".
  EXPECT_EQ(host.composer.hitTest({160, 160}).value_or(""), "under");
  EXPECT_FALSE(host.composer.hitTest({500, 500}).has_value());
}

TEST(ComposeTransform, SkewLeansPaintAndHits) {
  // skewX(−12°) leans the card's top to the right about its centre. The
  // point of the case is the second half: hit-testing must walk the shear
  // backwards, so a point that is inside the leaning card but outside its
  // unsheared box still hits it.
  Host host;
  host.composer.render(box().children(
      {box()
           .key("card")
           .width(40)
           .height(40)
           .inset({.top = 60, .right = 100, .bottom = 100, .left = 60})
           .absolute()
           .fill(red())
           .skewX(-12.0f)}));
  host.frame();
  EXPECT_EQ(host.pixel(101, 64), SK_ColorRED);   // top leaned right
  EXPECT_EQ(host.pixel(61, 64), SK_ColorBLACK);  // vacated top-left
  EXPECT_EQ(host.pixel(58, 97), SK_ColorRED);    // bottom leaned left
  EXPECT_EQ(host.pixel(98, 97), SK_ColorBLACK);  // vacated bottom-right
  auto hit = host.composer.hitTest({101, 64});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, "card");  // transform-aware hit through the shear
  EXPECT_FALSE(host.composer.hitTest({61, 64}).has_value());
}

TEST(ComposeTransform, SkewXPositiveLeansTheTopTowardNegativeX) {
  // THE SIGN PIN. skewX shears about the box centre in screen space, y
  // down, by tan(skewX degrees): a POSITIVE angle displaces the top edge
  // toward NEGATIVE x relative to the bottom edge — the top leans left.
  // The sign is easy to state backwards, so the runtime's answer is
  // pinned here in pixels.
  Host host;
  host.composer.render(box().children(
      {box()
           .key("card")
           .width(40)
           .height(40)
           .inset({.top = 60, .right = 100, .bottom = 100, .left = 60})
           .absolute()
           .fill(red())
           .skewX(30.0f)}));
  host.frame();
  // The unsheared box is x in [60, 100], y in [60, 100], centre (80, 80).
  // At y = 64 (16 above centre) the shift is tan(30) * -16 ~ -9.2, so the
  // top row spans about [50.8, 90.8]; at y = 97 (17 below) the shift is
  // +9.8, spanning about [69.8, 109.8].
  EXPECT_EQ(host.pixel(54, 64), SK_ColorRED);    // top edge left of the box
  EXPECT_EQ(host.pixel(97, 64), SK_ColorBLACK);  // vacated top-right
  EXPECT_EQ(host.pixel(106, 97), SK_ColorRED);   // bottom edge leaned right
  EXPECT_EQ(host.pixel(63, 97), SK_ColorBLACK);  // vacated bottom-left
  // And hit-testing walks the same shear: the leaned top-left corner is
  // inside the card, the vacated top-right is not.
  auto hit = host.composer.hitTest({54, 64});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, "card");
  EXPECT_FALSE(host.composer.hitTest({97, 64}).has_value());
}
