/** @file
 * The band split a nine-slice decomposes into: divs cut the source into
 * alternating fixed and stretchable intervals starting FIXED, the
 * stretchable ones share whatever destination space is left, and a
 * destination too small for the fixed sum scales the fixed bands down
 * instead of overflowing.
 */

#include <gtest/gtest.h>
#include <sigilskia/draw/Direct.h>

#include <vector>

using namespace sigil::skia::draw;

namespace {

/** The edges for one axis, as the decomposition computes them. */
struct Split {
  std::vector<float> src, dst;
};

Split edgesOf(const std::vector<int>& divs, float srcLen, float dstLen,
              float density = 1.0f) {
  Split s;
  detail::latticeEdges(divs, srcLen, dstLen, s.src, s.dst, density);
  return s;
}

}  // namespace

TEST(LatticeEdges, TheBandsAlternateFromFixedAndSpanTheWholeAxis) {
  // A 90 px source cut at 30 and 60: fixed [0,30), stretch [30,60),
  // fixed [60,90).
  const Split s = edgesOf({30, 60}, 90.0f, 200.0f);
  ASSERT_EQ(s.src.size(), 4u);
  EXPECT_FLOAT_EQ(s.src.front(), 0.0f);
  EXPECT_FLOAT_EQ(s.src.back(), 90.0f);
  ASSERT_EQ(s.dst.size(), 4u);
  EXPECT_FLOAT_EQ(s.dst.front(), 0.0f);
  EXPECT_FLOAT_EQ(s.dst.back(), 200.0f);
  // The two fixed bands keep their source size; the stretch band absorbs
  // the rest, which is the whole point of a nine-slice.
  EXPECT_FLOAT_EQ(s.dst[1] - s.dst[0], 30.0f);
  EXPECT_FLOAT_EQ(s.dst[3] - s.dst[2], 30.0f);
  EXPECT_FLOAT_EQ(s.dst[2] - s.dst[1], 140.0f);
}

TEST(LatticeEdges, ADestinationSmallerThanTheFixedSumScalesTheFixedBands) {
  // 60 px of fixed corner into a 30 px destination: the corners halve
  // rather than overflowing, and the stretch band collapses.
  const Split s = edgesOf({30, 60}, 90.0f, 30.0f);
  EXPECT_FLOAT_EQ(s.dst.back(), 30.0f);
  EXPECT_FLOAT_EQ(s.dst[1] - s.dst[0], 15.0f);
  EXPECT_FLOAT_EQ(s.dst[2] - s.dst[1], 0.0f);
  EXPECT_FLOAT_EQ(s.dst[3] - s.dst[2], 15.0f);
}

TEST(LatticeEdges, DensityShrinksTheFixedBandsAndTheStretchAbsorbsIt) {
  // A frame authored at 2x: its corners land at half their pixel count,
  // sharp on a 2x device instead of twice the intended width.
  const Split s = edgesOf({30, 60}, 90.0f, 200.0f, 2.0f);
  EXPECT_FLOAT_EQ(s.dst[1] - s.dst[0], 15.0f);
  EXPECT_FLOAT_EQ(s.dst[3] - s.dst[2], 15.0f);
  EXPECT_FLOAT_EQ(s.dst[2] - s.dst[1], 170.0f);
  EXPECT_FLOAT_EQ(s.dst.back(), 200.0f);
  // A non-positive density is the caller having no opinion, not a
  // degenerate frame.
  EXPECT_FLOAT_EQ(edgesOf({30, 60}, 90.0f, 200.0f, 0.0f).dst[1], 30.0f);
}

TEST(LatticeEdges, ADivOutsideTheSourceIsClampedIntoIt) {
  // A caller's div past the image's own width would otherwise emit a band
  // that samples nothing.
  const Split s = edgesOf({-10, 500}, 90.0f, 90.0f);
  EXPECT_FLOAT_EQ(s.src[1], 0.0f);
  EXPECT_FLOAT_EQ(s.src[2], 90.0f);
  EXPECT_FLOAT_EQ(s.dst.back(), 90.0f);
}

TEST(LatticeEdges, NoDivsIsOneFixedBandThatKeepsItsAuthoredSize) {
  const Split s = edgesOf({}, 40.0f, 100.0f);
  ASSERT_EQ(s.src.size(), 2u);
  ASSERT_EQ(s.dst.size(), 2u);
  // Every band a lattice with no divs has is fixed, and a fixed band is
  // never stretched: a destination larger than the source leaves the
  // band at the size it was authored at and the rest of the destination
  // unclaimed.
  EXPECT_FLOAT_EQ(s.dst.back(), 40.0f);
}
