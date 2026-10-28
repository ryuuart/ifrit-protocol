// A decoration cut into bands against the edges it dresses: the
// nine-slice whose fixed corners scale and whose stretch band takes the
// remainder, the edges a rounded corner is split along, and the inner,
// outer and centred strokes that paint their band against the shape —
// whole, and narrowed to the run a span has revealed so far.

#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/Image.h>
#include <sigilskia/draw/Direct.h>

#include <memory>
#include <vector>

#include "support/BrushTestSupport.h"

/** A nine-slice source with a marked top-left corner cell: 24x24, blue, with
 *  the first 8x8 in red, so the corner band's drawn WIDTH is readable off the
 *  red run. */
std::shared_ptr<const sigil::media::Image> cornerMarkedFrame() {
  SkBitmap src;
  src.allocN32Pixels(24, 24);
  src.eraseColor(SK_ColorBLUE);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 8, 8));
  return sigil::media::Image::of(src.asImage());
}

Slice cornerSlice(float density) {
  Slice nine;
  nine.asset = cornerMarkedFrame();
  nine.xDivs = {8, 16};
  nine.yDivs = {8, 16};
  nine.filter = material::Sampling::Nearest;
  nine.density = density;
  return nine;
}

/** The fixed bands are the frame's WEIGHT, and a frame generated oversized so
 *  it stays sharp has to be able to say so — otherwise its corners come out at
 *  their pixel count and swallow the padding the content sits in. */

/** The band arithmetic itself, including the case a density cannot rescue: a
 *  destination smaller than the fixed bands still shrinks them to fit, which
 *  is the rule the stretch bands cannot express. */
TEST(ComposeDecorations,
     ASliceDensityScalesTheFixedBandsAndTheStretchBandTakesTheRest) {
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

TEST(ComposeDecorations,
     AnAlignedStrokeOnAnOpenRunKeepsToTheOutlineTheRunWasCutFrom) {
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

// ---------------------------------------------------------------------------
// The same alignment on a span-REVEALED outline. A span gate narrows the
// boundary to the run shown so far, and a partial run is open exactly as a
// slice's runs are, so the shape it was cut from rides along and the
// alignment clips to that at every fraction of the reveal. `revealBox` is
// a 100 x 100 box at (20, 20): its perimeter is 400 px and its boundary
// starts at the bottom-left corner and runs UP the left edge, so 0.05 of
// the reveal is 20 px of that 100 px edge, 0.20 is 80 px of it, and 0.90
// is three sides and 60 px back along the bottom.
