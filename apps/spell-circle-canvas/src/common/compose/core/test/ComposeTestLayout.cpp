// The box a node is given: the flex line and its spacing, the lengths a
// box is sized by, the text leaf that measures itself, the bounds a
// parent's layer is sized by, the placement longhand beside its
// shorthands, and the hit test that follows paint order, keys and skew.

#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

// A box lays its children out along its line, spaced by its gap, inside its
// padding, each outside its own margin.
TEST(ComposeLayout, AFlexLinePlacesItsChildrenByItsGapPaddingAndMargins) {
  sigil::compose::test::checkRows({
      {"ARowPlacesItsChildrenAcrossTheGap",
       [] {
         Host host;
         host.composer.render(box().row().gap(20).children(
             {box().width(50).height(50).fill(red()),
              box().width(50).height(50).fill(green())}));
         host.frame();
         EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);    // first child
         EXPECT_EQ(host.pixel(60, 25), SK_ColorBLACK);  // the gap
         EXPECT_EQ(host.pixel(95, 25),
                   SK_ColorGREEN);  // second child at 70..120
       }},
      {"FourPaddingsAndFourMarginsRunClockwiseFromTheTop",
       [] {
         Host host;
         host.composer.render(
             box().children({box()
                                 .padding(20, 30, 40, 10)
                                 .key("outer")
                                 .children({box()
                                                .margin(6, 7, 8, 5)
                                                .width(50)
                                                .height(50)
                                                .key("inner")})}));
         host.frame();
         auto inner = host.composer.bounds("inner");
         ASSERT_TRUE(inner.has_value());
         EXPECT_FLOAT_EQ(inner->left(), 10 + 5);  // padding.left + margin.left
         EXPECT_FLOAT_EQ(inner->top(), 20 + 6);   // padding.top + margin.top
       }},
  });
}

// A bare number is pixels, a percentage is of the parent (of the node's own
// box for a radius or a translate), a canvas-relative length is of the
// canvas however deep it is written, and a font-relative one follows the font.
TEST(ComposeLayout, ALengthResolvesAgainstWhatItsUnitIsAFractionOf) {
  sigil::compose::test::checkRows({
      {"APercentLiteralIsOfTheParent",
       [] {
         Host host;
         host.composer.render(box().children({box()
                                                  .width(50_percent)
                                                  .height(25_percent)
                                                  .fill(red())
                                                  .key("half")}));
         host.frame();
         auto rect = host.composer.bounds("half");
         ASSERT_TRUE(rect.has_value());
         EXPECT_FLOAT_EQ(rect->width(), 100.0f);  // 50% of the 200px host
         EXPECT_FLOAT_EQ(rect->height(), 50.0f);  // 25% of 200px
       }},
      {"ACanvasRelativeLengthIsTheCanvasHoweverDeepItIsWritten",
       [] {
         // vw and vh are the CANVAS, where percent is the parent: a half-canvas
         // child three boxes deep is half the canvas, not half of whatever box
         // it sits in, and it follows the canvas when the canvas changes size.
         Host host(400, 200);
         const auto tree = [] {
           // flexShrink(0) on both: what is read back is what each unit
           // RESOLVED to, not how the flex line fitted it afterwards.
           return box().padding(50).children(
               {box().width(100).height(100).children({box()
                                                           .key("canvas")
                                                           .width(50_vw)
                                                           .height(50_vh)
                                                           .flexShrink(0)
                                                           .fill(red()),
                                                       box()
                                                           .key("parent")
                                                           .width(50_percent)
                                                           .height(50_percent)
                                                           .flexShrink(0)
                                                           .fill(green())})});
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
       }},
      {"VminAndVmaxAreTheCanvasSmallerAndLargerSide",
       [] {
         // vmin is a percentage of the canvas's smaller side and vmax of its
         // larger, so a square sized in vmin fits the canvas either way round.
         Host host(400, 200);
         const auto tree = [] {
           return box().children(
               {box().key("small").width(50_vmin).height(50_vmin).flexShrink(0),
                box().key("large").width(50_vmax).height(10).flexShrink(0)});
         };
         host.composer.render(tree());
         host.frame();
         auto small = host.composer.bounds("small");
         auto large = host.composer.bounds("large");
         ASSERT_TRUE(small.has_value());
         ASSERT_TRUE(large.has_value());
         EXPECT_FLOAT_EQ(small->width(), 100.0f)
             << "half the smaller side, 200";
         EXPECT_FLOAT_EQ(small->height(), 100.0f);
         EXPECT_FLOAT_EQ(large->width(), 200.0f) << "half the larger side, 400";

         host.composer.setSize({200, 400});
         host.frame();
         small = host.composer.bounds("small");
         large = host.composer.bounds("large");
         ASSERT_TRUE(small.has_value());
         ASSERT_TRUE(large.has_value());
         EXPECT_FLOAT_EQ(small->width(), 100.0f);
         EXPECT_FLOAT_EQ(large->width(), 200.0f);
       }},
      {"APercentRadiusIsOfTheBoxAcrossAndDown",
       [] {
         // CSS's border-radius: 50% of a 100 by 50 box rounds it into an
         // ellipse, so the top edge is reached only at its middle and the
         // corners are empty.
         Host host;
         host.composer.render(box().children({box()
                                                  .width(100)
                                                  .height(50)
                                                  .flexShrink(0)
                                                  .fill(red())
                                                  .borderRadius(50_percent)}));
         host.frame();
         EXPECT_EQ(host.pixel(50, 25), SK_ColorRED);   // the centre
         EXPECT_EQ(host.pixel(50, 1), SK_ColorRED);    // the top, at its middle
         EXPECT_EQ(host.pixel(20, 3), SK_ColorBLACK);  // inside a 25 px circle,
                                                       // outside the ellipse
         EXPECT_EQ(host.pixel(2, 2), SK_ColorBLACK);   // the corner
       }},
      {"AnEmRadiusFollowsTheFont",
       [] {
         // One em at a 50 px type size rounds a 100 px square into a circle.
         Host host;
         host.composer.render(
             box().children({box()
                                 .fontSize(50)
                                 .width(100)
                                 .height(100)
                                 .flexShrink(0)
                                 .fill(red())
                                 .borderRadius(sigil::weave::em(1))}));
         host.frame();
         EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
         EXPECT_EQ(host.pixel(50, 1), SK_ColorRED);
         EXPECT_EQ(host.pixel(8, 8), SK_ColorBLACK);
       }},
      {"APercentTranslateIsOfTheNodesOwnBox",
       [] {
         // CSS's translate: 50% of a 40 px node is 20 px, whatever its parent
         // is, and a later number in pixels replaces it.
         Host host;
         host.composer.render(box().width(200).children(
             {box().width(40).height(40).flexShrink(0).fill(red()).translateX(
                 50_percent)}));
         host.frame();
         EXPECT_EQ(host.pixel(10, 20), SK_ColorBLACK);  // where it stood
         EXPECT_EQ(host.pixel(50, 20), SK_ColorRED);    // 20 px along

         host.composer.render(box().width(200).children(
             {box().width(40).height(40).flexShrink(0).fill(red()).translateY(
                 -50_percent)}));
         host.frame();
         EXPECT_EQ(host.pixel(10, 30), SK_ColorBLACK);  // 20 px up
         EXPECT_EQ(host.pixel(10, 10), SK_ColorRED);

         host.composer.render(
             box().width(200).children({box()
                                            .width(40)
                                            .height(40)
                                            .flexShrink(0)
                                            .fill(red())
                                            .translateX(50_percent)
                                            .translateX(0)}));
         host.frame();
         EXPECT_EQ(host.pixel(10, 20), SK_ColorRED);
         EXPECT_EQ(host.pixel(50, 20), SK_ColorBLACK);
       }},
  });
}

namespace {

/** The smallest box holding every inked pixel inside @p region — where the
 *  ink STARTS on both axes, which is the only reading of "the words stand
 *  here" that does not depend on which glyph the face put first. White type
 *  on black, so any red channel above the antialiasing floor is ink. */
SkIRect inkBoundsIn(Host& host, const SkRect& region) {
  const SkIRect bounds = region.roundOut();
  SkIRect ink = SkIRect::MakeEmpty();
  for (int row = bounds.top(); row < bounds.bottom(); ++row)
    for (int column = bounds.left(); column < bounds.right(); ++column)
      if (SkColorGetR(host.pixel(column, row)) > 32)
        ink.join(SkIRect::MakeXYWH(column, row, 1, 1));
  return ink;
}

}  // namespace

// A text leaf measures its contents, and its padding insets the box its
// words are set in.
TEST(ComposeLayout, ATextLeafMeasuresItsContents) {
  sigil::compose::test::checkRows({
      {"ASingleLineInARow",
       [] {
         Host host(400, 200);
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
       }},
      {"APaddedLeafSetsItsWordsInsideItsPadding",
       [] {
         // padding() on a text leaf means what it means on a box and in CSS:
         // the content box inset by it. The node grows by the padding on both
         // axes AND the paragraph is laid out in what is left, so a fill on the
         // leaf is the scrim the words stand ON rather than a panel hanging off
         // their corner.
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
         // …and the far edges are one padding clear too, which is what makes
         // the scrim surround the reading instead of hanging off one side of
         // it.
         EXPECT_NEAR(padded.right() - (float)paddedInk.right(),
                     bare.right() - (float)bareInk.right() + pad, 1.0f);
         EXPECT_NEAR(padded.bottom() - (float)paddedInk.bottom(),
                     bare.bottom() - (float)bareInk.bottom() + pad, 1.0f);
       }},
  });
}

TEST(ComposePaintBounds, AParentsLayerHoldsItsChildrenAsTheyAreTransformed) {
  // A parent's bounds hold its children as they are transformed, per-axis
  // scale included: a child whose ONLY transform is a per-axis scale must
  // widen them, or every consumer sized off them (the effect layer here,
  // the opacity layer, the texture bake) truncates the overflow.
  //
  // The parent takes an identity offset() filter purely to force a bounded
  // saveLayer: that layer clips to the bounds, so wrong bounds delete the
  // scaled-out half of the bar instead of merely mis-sizing something.
  Host host(200, 200);
  host.composer.render(box().children(
      {box()
           .absolute()
           .rect(20, 20, 40, 40)
           .filter(
               material::skia::filter(SkImageFilters::Offset(0, 0, nullptr)))
           .children({box()
                          .absolute()
                          .rect(0, 0, 40, 40)
                          .transformOrigin(percent(0), percent(0))
                          .fill(red())
                          .scaleX(3.0f)})}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 40), SK_ColorRED) << "the unscaled part is missing";
  EXPECT_EQ(host.pixel(120, 40), SK_ColorRED)
      << "the scaled-out part of the bar was clipped away: the parent's "
         "bounds did not see scaleX on the child";
  EXPECT_EQ(host.pixel(150, 40), SK_ColorBLACK) << "…and it over-reached";
}

TEST(ComposePlacement, ACoveringNodeGivenASizeStandsInTheFlowAgain) {
  // A canvas fills the box it stands in, and a canvas given a box of its
  // own stands in the flow at that box: what follows it lands after it
  // rather than on top of it, which is what a captioned drawing on a
  // sheet asks for.
  Host host(400, 400);
  host.composer.render(box().column().gap(10).children(
      {custom([](sigil::draw::Pen& pen) {})
           .cover()
           .width(100)
           .height(60)
           .key("drawing"),
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
      {custom([](sigil::draw::Pen& pen) {})
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

// `rect`, `at` and the edge setters are the placement longhand: each
// describes the same node in any unit and prunes against it.
TEST(ComposePlacement, EveryShorthandDescribesTheLonghandsNode) {
  sigil::compose::test::checkRows({
      {"RectIsTheLonghandAndPrunesIdentically",
       [] {
         // rect() is sugar for .absolute().left().top().width().height(), and
         // the entire safety argument is that it describes the SAME node. So a
         // re-describe that swaps one spelling for the other must prune to zero
         // patches and must not move a pixel — anything less and the two
         // spellings are different nodes wearing one name.
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
           return box().children({box()
                                      .key("plate")
                                      .rect(geometry::path::fromSk(r))
                                      .fill(red())});
         };

         host.composer.render(longhand());
         host.frame();
         const auto boundsLonghand = host.composer.bounds("plate");
         ASSERT_TRUE(boundsLonghand.has_value());
         EXPECT_EQ(geometry::path::toSk(*boundsLonghand), r);
         EXPECT_EQ(host.pixel(45, 65), SK_ColorRED);    // inside
         EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);  // above the top edge
         EXPECT_EQ(host.pixel(95, 65),
                   SK_ColorBLACK);  // right of the right edge

         // Re-describe with rect(). Equal properties => the reconciler prunes
         // it.
         host.composer.render(terse());
         host.frame();
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
             << "rect() described a node the reconciler considered DIFFERENT "
                "from "
                "the longhand — it is not writing the same LayoutProps fields";
         EXPECT_EQ(host.composer.bounds("plate"), boundsLonghand);
         EXPECT_EQ(host.pixel(45, 65), SK_ColorRED);
         EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);

         // And back the other way, so neither direction is the privileged one.
         host.composer.render(longhand());
         host.frame();
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u);

         // NEGATIVE CONTROL: without this the two assertions above pass on a
         // composer that never patches anything. A different rect MUST patch.
         host.composer.render(box().children(
             {box().key("plate").rect(41, 60, 50, 30).fill(red())}));
         host.frame();
         EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
             << "the patch counter is not live, so the zeroes above prove "
                "nothing";
         EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);
         EXPECT_EQ(require(host.composer.bounds("plate")).fLeft, 41.0f);
       }},
      {"RectAndAtTakeLengthsInAnyUnit",
       [] {
         // The two shorthands take what the longhand takes: a percent of the
         // parent's box is one re-describe away from the four setters, and
         // prunes against them like the pixel forms do.
         Host host(200, 100);
         auto longhand = [] {
           return box().width(200).height(100).children(
               {box()
                    .key("plate")
                    .left(percent(10))
                    .top(30)
                    .width(percent(25))
                    .height(40)
                    .fill(red()),
                box()
                    .key("pin")
                    .width(10)
                    .height(10)
                    .left(percent(50))
                    .top(percent(20))});
         };
         auto terse = [] {
           return box().width(200).height(100).children(
               {box()
                    .key("plate")
                    .rect(percent(10), 30, percent(25), 40)
                    .fill(red()),
                box().key("pin").width(10).height(10).at(percent(50),
                                                         percent(20))});
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
       }},
      {"AtPinsTheCornerAndLeavesTheNodeToSizeItself",
       [] {
         // The half of the longhand that carries no box: .left().top() on a
         // node that measures itself from its content.
         Host host(300, 200);
         auto longhand = [] {
           return box().children({text(u8"Wm", styleAt(20))
                                      .key("cap")
                                      .absolute()
                                      .left(30)
                                      .top(40)});
         };
         auto terse = [] {
           return box().children(
               {text(u8"Wm", styleAt(20)).key("cap").at({30, 40})});
         };

         host.composer.render(longhand());
         host.frame();
         const auto measured = host.composer.bounds("cap");
         ASSERT_TRUE(measured.has_value());
         EXPECT_FLOAT_EQ(measured->left(), 30.0f);
         EXPECT_FLOAT_EQ(measured->top(), 40.0f);
         // Sized by its content, not by the caller: this is what rect() cannot
         // do and is why at() exists separately.
         EXPECT_GT(measured->width(), 1.0f);

         host.composer.render(terse());
         host.frame();
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
             << "at() is not left().top()";
         EXPECT_EQ(host.composer.bounds("cap"), measured);

         // Negative control, as above.
         host.composer.render(box().children(
             {text(u8"Wm", styleAt(20)).key("cap").at({31, 40})}));
         host.frame();
         EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
       }},
      {"AnEdgeSetterImpliesAbsoluteAndAbsoluteAloneStillLeavesTheFlow",
       [] {
         // Every edge setter sets layout.absolute itself, so `.absolute()`
         // before or after one writes a bool that is already written and can
         // be left out. It cannot be left out of a node that pins no edge.
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
           return box().children(
               {box().key("p").left(30).top(30).width(20).height(20).fill(
                   red())});
         };
         host.composer.render(withRedundant());
         host.frame();
         const auto pinned = host.composer.bounds("p");
         ASSERT_TRUE(pinned.has_value());
         EXPECT_EQ(geometry::path::toSk(*pinned),
                   SkRect::MakeXYWH(30, 30, 20, 20));

         host.composer.render(without());
         host.frame();
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
             << "dropping a redundant .absolute() changed the description";
         EXPECT_EQ(host.composer.bounds("p"), pinned);
         EXPECT_EQ(host.pixel(35, 35), SK_ColorRED);

         // Absolute with a size and no pinned edge: here .absolute() is the
         // only thing taking it out of flow, so without it the node stands
         // behind its sibling.
         Host flow(200, 200);
         flow.composer.render(box().row().children(
             {box().width(60).height(20).fill(green()),
              box().key("q").absolute().width(20).height(20).fill(red())}));
         flow.frame();
         ASSERT_TRUE(flow.composer.bounds("q").has_value());
         EXPECT_FLOAT_EQ(require(flow.composer.bounds("q")).fLeft, 0.0f);

         flow.composer.render(box().row().children(
             {box().width(60).height(20).fill(green()),
              box().key("q").width(20).height(20).fill(red())}));
         flow.frame();
         EXPECT_FLOAT_EQ(require(flow.composer.bounds("q")).fLeft, 60.0f)
             << "a node that pins no edge stays in the flow without "
                ".absolute()";
       }},
  });
}

// -------------------------------------------------------------------------
// Per-edge spacing and Dimension literals; the hit test under paint
// order and keys; and the skew that leans both the paint and the hits.

TEST(ComposeQueries, HitTestRespectsPaintOrderAndKeys) {
  Host host;
  host.composer.render(
      stack().children({box().key("under").inset(0).fill(red()),
                        box()
                            .key("over")
                            .width(60)
                            .height(60)
                            .inset(20, 120, 120, 20)
                            .absolute()
                            .fill(green()),
                        box()
                            .width(30)
                            .height(30)
                            .inset(150, 20, 20, 150)
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

TEST(ComposeQueries, AnOptedOutNodeTakesItsKeylessContentsWithIt) {
  // `hitTestable(false)` is CSS's `pointer-events: none`, which inherits:
  // a keyless dot inside an opted-out overlay answers neither its own key
  // nor the overlay's, so the point reaches the target beneath, while a
  // keyed child of the same overlay still answers for itself.
  Host host;
  host.composer.render(stack().children(
      {box().key("target").inset(0).absolute().fill(red()),
       box().key("probe").inset(0).absolute().hitTestable(false).children(
           {box().absolute().left(40).top(40).width(20).height(20).fill(
                green()),
            box()
                .key("handle")
                .absolute()
                .left(120)
                .top(120)
                .width(20)
                .height(20)
                .fill(blue())})}));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({50, 50}).value_or(""), "target")
      << "a keyless dot inside an opted-out overlay answered for it";
  EXPECT_EQ(host.composer.hitTest({130, 130}).value_or(""), "handle");
  EXPECT_EQ(host.composer.hitTest({90, 90}).value_or(""), "target");
}

// A skew shears the plane as CSS's transform list does, and the hit test
// walks the same shear back.
TEST(ComposeTransform, ASkewLeansThePaintAndTheHitTestAlike) {
  sigil::compose::test::checkRows({
      {"ANegativeSkewXLeansTheTopRight",
       [] {
         // skewX(−12°) leans the card's top to the right about its centre. The
         // point of the case is the second half: hit-testing must walk the
         // shear backwards, so a point that is inside the leaning card but
         // outside its unsheared box still hits it.
         Host host;
         host.composer.render(box().children({box()
                                                  .key("card")
                                                  .width(40)
                                                  .height(40)
                                                  .inset(60, 100, 100, 60)
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
       }},
      {"TwoSkewLanesComposeAsCssTransformList",
       [] {
         // A borrowed CSS name reads as CSS reads it: `skewX(a) skewY(a)` is
         // the x shear with the y shear inside it, [1 + tan²a, tan a; tan a,
         // 1], which widens a 100 px box along x by the product of the
         // tangents. One shear pair [1 tan a; tan a 1] would lean it without
         // the widening.
         Host host(300, 300);
         host.composer.render(box().children({box()
                                                  .key("card")
                                                  .width(100)
                                                  .height(100)
                                                  .inset(100, 100, 100, 100)
                                                  .absolute()
                                                  .fill(red())
                                                  .skewX(-12.5f)
                                                  .skewY(-12.5f)}));
         host.frame();
         int left = 300, right = -1;
         for (int row = 0; row < 300; ++row)
           for (int column = 0; column < 300; ++column)
             if (host.pixel(column, row) == SK_ColorRED) {
               left = std::min(left, column);
               right = std::max(right, column);
             }
         ASSERT_GE(right, left) << "the card painted nothing";
         const double tangent = std::tan(12.5 * std::numbers::pi / 180.0);
         const double cssWidth =
             100.0 * (1.0 + tangent * tangent) + 100.0 * tangent;
         EXPECT_NEAR((double)(right - left + 1), cssWidth, 2.0)
             << "the two lanes did not compose as CSS's transform list";
         // The hit test walks the same product backwards: the widened right end
         // of the top edge is inside the card.
         const double centre = 150.0;
         const double topRightX =
             centre + (1.0 + tangent * tangent) * 50.0 - tangent * -50.0 - 3.0;
         const double topRightY = centre - tangent * 50.0 + -50.0 + 2.0;
         EXPECT_EQ(host.composer.hitTest({(float)topRightX, (float)topRightY})
                       .value_or(""),
                   "card");
       }},
      {"APositiveSkewXLeansTheTopTowardNegativeX",
       [] {
         // THE SIGN PIN. skewX shears about the box centre in screen space, y
         // down, by tan(skewX degrees): a POSITIVE angle displaces the top edge
         // toward NEGATIVE x relative to the bottom edge — the top leans left.
         // The sign is easy to state backwards, so the runtime's answer is
         // pinned here in pixels.
         Host host;
         host.composer.render(box().children({box()
                                                  .key("card")
                                                  .width(40)
                                                  .height(40)
                                                  .inset(60, 100, 100, 60)
                                                  .absolute()
                                                  .fill(red())
                                                  .skewX(30.0f)}));
         host.frame();
         // The unsheared box is x in [60, 100], y in [60, 100], centre (80,
         // 80). At y = 64 (16 above centre) the shift is tan(30) * -16 ~ -9.2,
         // so the top row spans about [50.8, 90.8]; at y = 97 (17 below) the
         // shift is +9.8, spanning about [69.8, 109.8].
         EXPECT_EQ(host.pixel(54, 64),
                   SK_ColorRED);  // top edge left of the box
         EXPECT_EQ(host.pixel(97, 64), SK_ColorBLACK);  // vacated top-right
         EXPECT_EQ(host.pixel(106, 97),
                   SK_ColorRED);  // bottom edge leaned right
         EXPECT_EQ(host.pixel(63, 97), SK_ColorBLACK);  // vacated bottom-left
         // And hit-testing walks the same shear: the leaned top-left corner is
         // inside the card, the vacated top-right is not.
         auto hit = host.composer.hitTest({54, 64});
         ASSERT_TRUE(hit.has_value());
         EXPECT_EQ(*hit, "card");
         EXPECT_FALSE(host.composer.hitTest({97, 64}).has_value());
       }},
  });
}
