// A decoration cut into bands against the edges it dresses: the
// nine-slice whose fixed corners scale and whose stretch band takes the
// remainder, the edges a rounded corner is split along, and the inner,
// outer and centred strokes that paint their band against the shape —
// whole, and narrowed to the run a span has revealed so far.

#include <sigilimage/asset/ImageAsset.h>
#include <sigilskia/draw/Direct.h>

#include <memory>
#include <vector>

#include "support/BrushTestSupport.h"

/** A nine-slice source with a marked top-left corner cell: 24x24, blue, with
 *  the first 8x8 in red, so the corner band's drawn WIDTH is readable off the
 *  red run. */
std::shared_ptr<sigil::image::ImageAsset> cornerMarkedFrame() {
  SkBitmap src;
  src.allocN32Pixels(24, 24);
  src.eraseColor(SK_ColorBLUE);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 8, 8));
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(src.asImage()));
}

Slice cornerSlice(float density) {
  Slice nine;
  nine.asset = cornerMarkedFrame();
  nine.xDivs = {8, 16};
  nine.yDivs = {8, 16};
  nine.filter = SkFilterMode::kNearest;
  nine.density = density;
  return nine;
}

/** The fixed bands are the frame's WEIGHT, and a frame generated oversized so
 *  it stays sharp has to be able to say so — otherwise its corners come out at
 *  their pixel count and swallow the padding the content sits in. */
TEST(ComposeDecorations, SliceDensityScalesTheFixedBandsOnly) {
  {
    Host host;
    host.composer.render(
        box().width(100).height(100).background(cornerSlice(1.0f)));
    host.frame();
    EXPECT_EQ(host.pixel(6, 6), SK_ColorRED);     // inside the 8-unit corner
    EXPECT_EQ(host.pixel(10, 10), SK_ColorBLUE);  // past it: the stretch band
  }
  {
    Host host;
    host.composer.render(
        box().width(100).height(100).background(cornerSlice(2.0f)));
    host.frame();
    EXPECT_EQ(host.pixel(2, 2), SK_ColorRED);   // the corner is half as wide
    EXPECT_EQ(host.pixel(6, 6), SK_ColorBLUE);  // where density 1 was still red
  }
}

/** The band arithmetic itself, including the case a density cannot rescue: a
 *  destination smaller than the fixed bands still shrinks them to fit, which
 *  is the rule the stretch bands cannot express. */
TEST(ComposeDecorations, SliceDensityLeavesTheStretchBandTheRemainder) {
  std::vector<float> src, dst;
  sigil::skia::draw::detail::latticeEdges({8, 16}, 24.0f, 100.0f, src, dst,
                                          2.0f);
  ASSERT_EQ(dst.size(), 4u);
  EXPECT_FLOAT_EQ(dst[1], 4.0f);    // 8 source px at 2 px per unit
  EXPECT_FLOAT_EQ(dst[2], 96.0f);   // the stretch band takes the rest
  EXPECT_FLOAT_EQ(dst[3], 100.0f);  // and the far corner is 4 again

  sigil::skia::draw::detail::latticeEdges({8, 16}, 24.0f, 100.0f, src, dst,
                                          1.0f);
  EXPECT_FLOAT_EQ(dst[1], 8.0f);  // the default is unchanged
  EXPECT_FLOAT_EQ(dst[2], 92.0f);

  sigil::skia::draw::detail::latticeEdges({8, 16}, 24.0f, 6.0f, src, dst, 2.0f);
  EXPECT_FLOAT_EQ(dst[1], 3.0f);  // too small for either: split even
  EXPECT_FLOAT_EQ(dst[2], 3.0f);
}

TEST(ComposeDecorations, EdgeSliceStrokesSelectedEdgesOnly) {
  Host host;
  host.composer.render(
      box().children({box().width(100).height(100).fill(blue()).foreground(
          onEdges(geometry::path::Edge::Top | geometry::path::Edge::Left,
                  stroke(8, Fill::color({1, 1, 1, 1}))))}));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // top edge stroked
  EXPECT_EQ(host.pixel(1, 50), SK_ColorWHITE);  // left edge stroked
  EXPECT_EQ(host.pixel(98, 50), SK_ColorBLUE);  // right edge bare
  EXPECT_EQ(host.pixel(50, 98), SK_ColorBLUE);  // bottom edge bare
}

TEST(ComposeDecorations, EdgesSplitRoundedCornersDiagonally) {
  // A rounded rect's corner arcs divide between their adjacent edges at
  // the diagonal — the top run must include the upper half of the
  // top-left arc but none of the left flank.
  Host host;
  host.composer.render(box().children(
      {box().width(100).height(100).borderRadius({30}).fill(blue()).foreground(
          onEdges(geometry::path::Edge::Top,
                  stroke(8, Fill::color({1, 1, 1, 1}))))}));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // top run center
  EXPECT_EQ(host.pixel(1, 50), SK_ColorBLUE);   // left flank untouched
  EXPECT_EQ(host.pixel(50, 98), SK_ColorBLUE);  // bottom untouched
}

TEST(ComposeDecorations, ASlicedInnerStrokePaintsTheBandInsideItsOwnEdges) {
  // The runs a slice hands down are OPEN and bound no area. An alignment
  // clips to the outline they were cut from, so the mark is the inner half
  // of the stroke ALONG THOSE RUNS: a 8 px band on the two named edges,
  // nothing on the other two, and nothing outside the silhouette.
  Host host;
  host.composer.render(box().children(
      {box().width(100).height(100).fill(blue()).foreground(onEdges(
          geometry::path::Edge::Bottom | geometry::path::Edge::Right,
          stroke(8, Fill::color({1, 1, 1, 1}), PathFormat::Align::Inner)))}));
  host.frame();
  EXPECT_EQ(host.pixel(50, 95), SK_ColorWHITE);   // in the bottom band
  EXPECT_EQ(host.pixel(95, 50), SK_ColorWHITE);   // in the right band
  EXPECT_EQ(host.pixel(95, 95), SK_ColorWHITE);   // the corner both own
  EXPECT_EQ(host.pixel(50, 88), SK_ColorBLUE);    // one px above the band
  EXPECT_EQ(host.pixel(88, 50), SK_ColorBLUE);    // one px inside of it
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLUE);     // top edge unnamed
  EXPECT_EQ(host.pixel(1, 50), SK_ColorBLUE);     // left edge unnamed
  EXPECT_EQ(host.pixel(101, 50), SK_ColorBLACK);  // nothing fattens the
  EXPECT_EQ(host.pixel(50, 101), SK_ColorBLACK);  // silhouette
}

TEST(ComposeDecorations, ASlicedInnerStrokeIsTheKitsRingMaskedToTheSameEdges) {
  // The same picture, twice: the brush's sliced Inner stroke and the kit's
  // ring masked to those edges. MITRED ends, because that is the ring that
  // runs the whole length of each named side; `Sliced` is that ring with
  // each open end cut back by the depth of the band the mask left out
  // there, which the case below reads off the pixels.
  const SkColor4f tone{1, 1, 1, 1};
  const auto edges = geometry::path::Edge::Bottom | geometry::path::Edge::Right;

  Host stroked;
  stroked.composer.render(box().children(
      {box().width(100).height(100).fill(blue()).foreground(onEdges(
          edges, stroke(8, Fill::color(tone), PathFormat::Align::Inner)))}));
  stroked.frame();

  Host ringed;
  kit::Bevel bevel;
  bevel.depth = 0;  // the outer ring is not part of the claim
  bevel.shadowDepth = 0;
  bevel.inner = kit::BevelInner{.gap = 0,
                                .light = {0, 0, 0, 0},
                                .shadow = tone,
                                .depth = 8,
                                .edges = edges,
                                .ends = styles::BevelEnds::Mitred};
  Element panel = box().width(100).height(100).fill(blue());
  kit::bevelled(panel, bevel);
  ringed.composer.render(box().children({std::move(panel)}));
  ringed.frame();

  EXPECT_TRUE(identicalPixels(stroked, ringed, 200, 200));

  // And with sliced ends the ring is the same band, one depth shorter at
  // each end where the mask left the neighbour out.
  Host cut;
  bevel.inner->ends = styles::BevelEnds::Sliced;
  Element sliced = box().width(100).height(100).fill(blue());
  kit::bevelled(sliced, bevel);
  cut.composer.render(box().children({std::move(sliced)}));
  cut.frame();
  EXPECT_EQ(cut.pixel(4, 95), SK_ColorBLUE);       // bottom band cut at its
  EXPECT_EQ(stroked.pixel(4, 95), SK_ColorWHITE);  // open (left) end
  EXPECT_EQ(cut.pixel(95, 4), SK_ColorBLUE);       // right band cut at its
  EXPECT_EQ(stroked.pixel(95, 4), SK_ColorWHITE);  // open (top) end
  EXPECT_EQ(cut.pixel(50, 95), SK_ColorWHITE);     // the runs themselves stand
  EXPECT_EQ(cut.pixel(95, 50), SK_ColorWHITE);
}

TEST(ComposeDecorations, ASlicedOuterStrokePaintsTheBandOutsideItsOwnEdges) {
  // Outer keeps its meaning on a slice the same way: the half of the
  // stroke OUTSIDE the shape, along the named runs alone.
  Host host;
  host.composer.render(stack().children(
      {box()
           .absolute()
           .left(20)
           .top(20)
           .width(100)
           .height(100)
           .fill(blue())
           .foreground(onEdges(
               geometry::path::Edge::Bottom | geometry::path::Edge::Right,
               stroke(8, Fill::color({1, 1, 1, 1}),
                      PathFormat::Align::Outer)))}));
  host.frame();
  EXPECT_EQ(host.pixel(70, 124), SK_ColorWHITE);  // below the bottom edge
  EXPECT_EQ(host.pixel(124, 70), SK_ColorWHITE);  // right of the right edge
  EXPECT_EQ(host.pixel(70, 115), SK_ColorBLUE);   // inside stays the fill
  EXPECT_EQ(host.pixel(115, 70), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(70, 16), SK_ColorBLACK);  // above the top edge
  EXPECT_EQ(host.pixel(16, 70), SK_ColorBLACK);  // left of the left edge
}

// ---------------------------------------------------------------------------
// The same alignment on a span-REVEALED outline. A span gate narrows the
// boundary to the run shown so far, and a partial run is open exactly as a
// slice's runs are, so the shape it was cut from rides along and the
// alignment clips to that at every fraction of the reveal. `revealBox` is
// a 100 x 100 box at (20, 20): its perimeter is 400 px and its boundary
// starts at the bottom-left corner and runs UP the left edge, so 0.05 of
// the reveal is 20 px of that 100 px edge, 0.20 is 80 px of it, and 0.90
// is three sides and 60 px back along the bottom.

TEST(ComposeDecorations, ARevealedInnerStrokePaintsTheBandInsideTheShapeSoFar) {
  choreograph::Output<float> shown;
  Host host;
  host.composer.render(stack().children({revealBox().fill(blue()).stroke(
      spans::upTo(motion::bind(&shown)),
      stroke(8, Fill::color({1, 1, 1, 1}), PathFormat::Align::Inner))}));

  shown = 0.05f;  // 20 px up the left edge: y in [100, 120]
  host.frame();
  EXPECT_EQ(host.pixel(24, 110), SK_ColorWHITE);  // the band, inside the shape
  EXPECT_EQ(host.pixel(32, 110), SK_ColorBLUE);   // one band-width in: fill
  EXPECT_EQ(host.pixel(16, 110), SK_ColorBLACK);  // nothing outside the shape
  EXPECT_EQ(host.pixel(24, 60), SK_ColorBLUE);    // nothing ahead of the run

  shown = 0.20f;  // 80 px of the 100 px left edge: y in [40, 120]
  host.frame();
  EXPECT_EQ(host.pixel(24, 110), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(24, 50), SK_ColorWHITE);  // on up the edge
  EXPECT_EQ(host.pixel(32, 50), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(16, 50), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(24, 30), SK_ColorBLUE);  // short of the corner
  EXPECT_EQ(host.pixel(60, 24), SK_ColorBLUE);  // the top edge is not yet

  shown = 0.90f;  // three sides, then 60 px back along the bottom
  host.frame();
  EXPECT_EQ(host.pixel(60, 24), SK_ColorWHITE);    // top edge, inner half
  EXPECT_EQ(host.pixel(116, 60), SK_ColorWHITE);   // right edge
  EXPECT_EQ(host.pixel(100, 116), SK_ColorWHITE);  // bottom, from the right
  EXPECT_EQ(host.pixel(40, 116), SK_ColorBLUE);    // …but not all the way back
  EXPECT_EQ(host.pixel(60, 16), SK_ColorBLACK);    // and never outside
  EXPECT_EQ(host.pixel(124, 60), SK_ColorBLACK);
}

TEST(ComposeDecorations, AFullyRevealedInnerStrokeIsTheUnspannedOne) {
  // A settled reveal draws exactly what no reveal at all draws — the
  // property the whole gate rests on, read here through an alignment.
  const PathFormat mark =
      stroke(8, Fill::color({1, 1, 1, 1}), PathFormat::Align::Inner);

  Host revealed;
  revealed.composer.render(
      stack().children({revealBox()
                            .fill(blue())
                            .mask(by::spans(spans::upTo(1.0f)))
                            .stroke(mark)}));
  revealed.frame();

  Host plain;
  plain.composer.render(
      stack().children({revealBox().fill(blue()).stroke(mark)}));
  plain.frame();

  EXPECT_TRUE(identicalPixels(revealed, plain, 200, 200));
  EXPECT_EQ(plain.pixel(24, 60), SK_ColorWHITE);  // and both drew the band
}

TEST(ComposeDecorations, ARevealedOuterStrokePaintsTheBandOutsideTheShape) {
  choreograph::Output<float> shown;
  Host host;
  host.composer.render(stack().children({revealBox().fill(blue()).stroke(
      spans::upTo(motion::bind(&shown)),
      stroke(8, Fill::color({1, 1, 1, 1}), PathFormat::Align::Outer))}));

  shown = 0.20f;  // the whole left edge
  host.frame();
  EXPECT_EQ(host.pixel(16, 60), SK_ColorWHITE);  // the half outside the shape
  EXPECT_EQ(host.pixel(24, 60), SK_ColorBLUE);   // the inside stays the fill
  EXPECT_EQ(host.pixel(60, 24), SK_ColorBLUE);   // the top is not yet shown
  EXPECT_EQ(host.pixel(60, 16), SK_ColorBLACK);
}

TEST(ComposeDecorations, ARevealedCentredStrokeStraddlesTheRunAsItAlwaysDid) {
  // Centre never clipped, so the silhouette is nothing to it: the mark
  // stands half inside the shape and half outside, along the shown run.
  choreograph::Output<float> shown;
  Host host;
  host.composer.render(stack().children({revealBox().fill(blue()).stroke(
      spans::upTo(motion::bind(&shown)),
      stroke(8, Fill::color({1, 1, 1, 1}), PathFormat::Align::Center))}));

  shown = 0.20f;
  host.frame();
  EXPECT_EQ(host.pixel(18, 60), SK_ColorWHITE);  // outside the shape
  EXPECT_EQ(host.pixel(22, 60), SK_ColorWHITE);  // and inside it
  EXPECT_EQ(host.pixel(28, 60), SK_ColorBLUE);   // half a width in: fill
  EXPECT_EQ(host.pixel(60, 24), SK_ColorBLUE);   // the top is not yet shown
}
