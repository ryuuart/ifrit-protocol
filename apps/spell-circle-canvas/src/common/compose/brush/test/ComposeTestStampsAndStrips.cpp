// Art repeated along a boundary: the stamp a decoration walks a contour
// with, the nine-slice strip a tile flow lays out, the derived routes
// between two nodes, and the shape values they are cut against.

#include <include/core/SkBBHFactory.h>
#include <include/core/SkFont.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkStream.h>
#include <sigilcompose/core/Feed.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilskia/draw/Direct.h>

#include <numeric>

#include "support/BrushTestSupport.h"

TEST(ComposeDerive, ConnectorTracksMovedEndpoints) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});

  auto tree = [&](float bLeft) {
    return stack()
        .child(box()
                   .key("a")
                   .width(20)
                   .height(20)
                   .inset(10, 10, 170, 170)
                   .absolute()
                   .fill(red()))
        .child(box()
                   .key("b")
                   .width(20)
                   .height(20)
                   .inset(bLeft, 160, 180 - bLeft, 20)
                   .absolute()
                   .fill(green()))
        .child(connector("a", "b").inset(0).foreground(wire).zIndex(-1));
  };

  host.composer.render(tree(10.0f));
  host.frame();
  // Vertical wire at x=20 between the stacked boxes.
  EXPECT_EQ(host.pixel(20, 100), SK_ColorYELLOW);

  host.composer.render(tree(160.0f));  // move b to the right
  host.frame();
  EXPECT_EQ(host.pixel(20, 100), SK_ColorBLACK);  // old route gone
  EXPECT_NE(host.pixel(95, 95), SK_ColorBLACK);   // new diagonal route
}

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
      box().child(box().width(100).height(100).fill(blue()).foreground(
          onEdges(geometry::path::Edge::Top | geometry::path::Edge::Left,
                  stroke(8, Fill::color({1, 1, 1, 1}))))));
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
  host.composer.render(box().child(
      box().width(100).height(100).corners({30}).fill(blue()).foreground(
          onEdges(geometry::path::Edge::Top,
                  stroke(8, Fill::color({1, 1, 1, 1}))))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // top run center
  EXPECT_EQ(host.pixel(1, 50), SK_ColorBLUE);   // left flank untouched
  EXPECT_EQ(host.pixel(50, 98), SK_ColorBLUE);  // bottom untouched
}

// ---------------------------------------------------------------------------
// Element stamps + snapshot().

TEST(ComposeStamps, SnapshotBakesIntrinsicSize) {
  sk_sp<SkPicture> pic =
      snapshot(box()
                   .row()
                   .gap(4)
                   .child(box().width(20).height(12).fill(red()))
                   .child(box().width(20).height(12).fill(green())),
               fonts());
  ASSERT_NE(pic, nullptr);
  EXPECT_FLOAT_EQ(pic->cullRect().width(), 44.0f);   // 20 + 4 + 20
  EXPECT_FLOAT_EQ(pic->cullRect().height(), 12.0f);  // content height
}

namespace {

constexpr int kTileW = 40;

constexpr int kTileH = 20;

constexpr int kTileCount = 3;

// The mark sits near the tile's top-left and is small enough that its own
// mirror image never overlaps it — on EITHER axis, which is what lets a
// single sample point tell a flip from no flip.
constexpr float kMark = 3.0f;

constexpr float kMarkSize = 6.0f;

constexpr int kProbe = 5;  ///< inside the mark
constexpr int kFarX = kTileW - kProbe;

constexpr int kFarY = kTileH - kProbe;

SkColor stripMark(int index) {
  static const SkColor marks[3] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE};
  return marks[index % 3];
}

/** A strip whose every tile carries ONE mark, near the tile's top-LEFT and
 *  in a per-tile colour — so a rendered tile reports its index by colour and
 *  its handedness by which side the mark landed on. `flow` picks whether the
 *  strip runs down (a column of tiles) or across (a row). */
sk_sp<SkPicture> markedStrip(tiles::Flow flow) {
  const bool down = flow == tiles::Flow::Down;
  const float w = (float)(down ? kTileW : kTileW * kTileCount);
  const float h = (float)(down ? kTileH * kTileCount : kTileH);
  auto strip = box().width(w).height(h);
  for (int j = 0; j < kTileCount; ++j)
    strip.child(box()
                    .absolute()
                    .left(kMark + (down ? 0.0f : (float)(j * kTileW)))
                    .top(kMark + (down ? (float)(j * kTileH) : 0.0f))
                    .width(kMarkSize)
                    .height(kMarkSize)
                    .fill(Fill::color(SkColor4f::FromColor(stripMark(j)))));
  // Shell box: snapshot() sizes by the ROOT's children, not its own dims.
  return snapshot(box().child(std::move(strip)), fonts());
}

sk_sp<SkSurface> renderTile(const sk_sp<SkPicture>& pic, int index,
                            tiles::Flow flow, tiles::Facing facing) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kTileW, kTileH));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  canvas->concat(tiles::window({kTileW, kTileH}, index, flow, facing));
  canvas->drawPicture(pic);
  return surface;
}

SkColor tilePixel(SkSurface& surface, int x, int y) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  surface.readPixels(bm.pixmap(), x, y);
  return bm.getColor(0, 0);
}

}  // namespace

namespace {

/** A window onto the strip, and where the tile's own mark must land in
 *  it. The mark's colour names its tile, so an off-by-one or a reversed
 *  step reads as the wrong colour rather than as a missing one; the bare
 *  points are the places a mirror the caller did not ask for would put
 *  it. Facing::Mirrored reflects ACROSS the flow and never along it,
 *  which is why the axis of the reflection changes with the flow. */
struct TileWindow {
  const char* what;
  tiles::Flow flow;
  tiles::Facing facing;
  SkIPoint mark;
  std::vector<SkIPoint> bare;
};

class StripTile : public testing::TestWithParam<TileWindow> {};

}  // namespace

TEST_P(StripTile, EachTilesMarkLandsWhereItsFlowAndFacingPutIt) {
  const TileWindow& window = GetParam();
  const sk_sp<SkPicture> strip = markedStrip(window.flow);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> tile = renderTile(strip, k, window.flow, window.facing);
    EXPECT_EQ(tilePixel(*tile, window.mark.x(), window.mark.y()), stripMark(k))
        << "tile " << k;
    for (const SkIPoint& p : window.bare)
      EXPECT_EQ(tilePixel(*tile, p.x(), p.y()), SK_ColorBLACK)
          << "tile " << k << " is marked at " << p.x() << "," << p.y();
  }
}

INSTANTIATE_TEST_SUITE_P(
    ComposeStripTiles, StripTile,
    testing::Values(
        TileWindow{"DownForward",
                   tiles::Flow::Down,
                   tiles::Facing::Forward,
                   {kProbe, kProbe},
                   {{kFarX, kProbe}}},
        TileWindow{"DownMirrored",
                   tiles::Flow::Down,
                   tiles::Facing::Mirrored,
                   {kFarX, kProbe},
                   {{kProbe, kProbe}, {kFarX, kFarY}}},
        TileWindow{"AcrossForward",
                   tiles::Flow::Across,
                   tiles::Facing::Forward,
                   {kProbe, kProbe},
                   {{kProbe, kFarY}}},
        TileWindow{"AcrossMirrored",
                   tiles::Flow::Across,
                   tiles::Facing::Mirrored,
                   {kProbe, kFarY},
                   {{kProbe, kProbe}}}),
    [](const testing::TestParamInfo<TileWindow>& info) {
      return info.param.what;
    });

TEST(ComposeStripTiles, MirroredTileReadsForwardUnderMirroredSampling) {
  // What Facing::Mirrored actually promises: bake mirrored, sample mirrored,
  // and the strip reads exactly as the forward bake does. A caller drawing
  // the back face of a ribbon relies on this being an exact reflection
  // rather than an approximately-similar one.
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> forward =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Forward);
    sk_sp<SkSurface> mirrored =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    for (int y = 1; y < kTileH; y += 3)
      for (int x = 1; x < kTileW; x += 3)
        ASSERT_EQ(tilePixel(*forward, x, y),
                  tilePixel(*mirrored, kTileW - 1 - x, y))
            << "tile " << k << " at " << x << "," << y;
  }
}

TEST(ComposeStripTiles, SliceableFlattensTheOpsAndChangesNoPixel) {
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  const sk_sp<SkPicture> sliced = tiles::sliceable(strip);
  ASSERT_NE(sliced, nullptr);
  // The trap this verb exists for: drawPicture() into the recorder would
  // store ONE nested op the hierarchy cannot index into. Counting
  // non-nested ops is what tells the two apart.
  EXPECT_EQ(sliced->approximateOpCount(false), strip->approximateOpCount(false))
      << "sliceable() nested the picture instead of flattening it";
  EXPECT_GT(sliced->approximateOpCount(false), 3);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> plain =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    sk_sp<SkSurface> fast =
        renderTile(sliced, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    for (int y = 0; y < kTileH; ++y)
      for (int x = 0; x < kTileW; ++x)
        ASSERT_EQ(tilePixel(*plain, x, y), tilePixel(*fast, x, y))
            << "tile " << k << " at " << x << "," << y;
  }
}

TEST(ComposeStamps, StampRecordsOnceReplaysPerSample) {
  static int stampDescribes;
  stampDescribes = 0;
  Host host;

  ContourWalk vine;
  vine.spacing = 25.0f;
  vine.stamp =
      custom([](SkCanvas& c, const PaintContext& ctx) {
        ++stampDescribes;
        SkPaint p;
        p.setColor(SK_ColorYELLOW);
        c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
      })
          .width(12)
          .height(12);

  host.composer.render(box().child(box()
                                       .width(100)
                                       .height(100)
                                       .inset(50, 50, 50, 50)
                                       .absolute()
                                       .fill(blue())
                                       .foreground(vine)));
  host.frame();
  host.frame();
  EXPECT_EQ(stampDescribes, 1);  // baked once, replayed at every sample

  // Stamps are centered on the outline: the top-left corner sample
  // lands half outside the box.
  EXPECT_EQ(host.pixel(50, 50), SK_ColorYELLOW);   // corner sample center
  EXPECT_EQ(host.pixel(100, 46), SK_ColorYELLOW);  // top edge, above box
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE);   // interior untouched
}

TEST(ComposeStamps, RecursiveStampWalksItsOwnContour) {
  // Two levels of recursion: the stamp is itself decorated by a ContourWalk
  // dotting its own outline. This terminates because a stamp may only
  // decorate art that is already baked, never itself — so the nesting is
  // finite by construction rather than by a depth limit.
  Host host;
  ContourWalk dots;
  dots.spacing = 6.0f;
  dots.draw = [](SkCanvas& c, const PathSample&, const PaintContext&) {
    SkPaint p;
    p.setColor(SK_ColorCYAN);
    c.drawRect(SkRect::MakeXYWH(-1, -1, 2, 2), p);
  };

  ContourWalk outer;
  outer.spacing = 40.0f;
  outer.stamp = box().width(16).height(16).fill(red()).foreground(dots);

  host.composer.render(box().child(box()
                                       .width(120)
                                       .height(120)
                                       .inset(40, 40, 40, 40)
                                       .absolute()
                                       .foreground(outer)));
  host.frame();
  int redPx = 0, cyanPx = 0;
  for (int x = 0; x < 200; x += 2)
    for (int y = 0; y < 200; y += 2) {
      const SkColor c = host.pixel(x, y);
      if (c == SK_ColorRED)
        redPx++;
      else if (c == SK_ColorCYAN)
        cyanPx++;
    }
  EXPECT_GT(redPx, 20);   // stamps landed
  EXPECT_GT(cyanPx, 10);  // and their own walked borders too
}

TEST(ComposeStamps, CustomLeafDrawsNestedComposer) {
  // A custom() leaf hosting an entire nested Composer: the recursion closes
  // at the paint phase, with the inner composer owning its own ticker and
  // size and drawing straight onto the outer canvas.
  Host host;
  auto nestedTicker = std::make_shared<sigil::motion::Ticker>();
  auto nested = std::make_shared<Composer>(*nestedTicker, fonts());
  nested->setSize({60, 60});
  nested->render(
      box().padding(10).fill(green()).child(box().grow(1).fill(red())));

  host.composer.render(box().child(
      custom([nested, nestedTicker](SkCanvas& c, const PaintContext&) {
        nested->draw(c);
      })
          .width(60)
          .height(60)
          .cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN);  // nested padding ring
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);  // nested content
}

// ---------------------------------------------------------------------------
// Routers (connector route library).

TEST(ComposeDerive, OrthogonalRouterRunsManhattan) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});
  host.composer.render(stack()
                           .child(box()
                                      .key("a")
                                      .width(20)
                                      .height(20)
                                      .inset(10, 10, 170, 170)
                                      .absolute()
                                      .fill(red()))
                           .child(box()
                                      .key("b")
                                      .width(20)
                                      .height(20)
                                      .inset(160, 160, 20, 20)
                                      .absolute()
                                      .fill(green()))
                           .child(connector("a", "b", routers::orthogonal())
                                      .inset(0)
                                      .foreground(wire)
                                      .zIndex(-1)));
  host.frame();
  // Centers (20,20) and (170,170); midX = 95: H leg at y=20, V leg at
  // x=95, H leg at y=170.
  EXPECT_EQ(host.pixel(60, 20), SK_ColorYELLOW);    // first horizontal leg
  EXPECT_EQ(host.pixel(95, 100), SK_ColorYELLOW);   // vertical run
  EXPECT_EQ(host.pixel(130, 170), SK_ColorYELLOW);  // final horizontal leg
  EXPECT_EQ(host.pixel(60, 100), SK_ColorBLACK);    // nowhere near diagonal
}

TEST(ComposeDerive, ArcRouterBowsOffTheChord) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});
  host.composer.render(stack()
                           .child(box()
                                      .key("a")
                                      .width(10)
                                      .height(10)
                                      .inset(20, 95, 170, 95)
                                      .absolute()
                                      .fill(red()))
                           .child(box()
                                      .key("b")
                                      .width(10)
                                      .height(10)
                                      .inset(170, 95, 20, 95)
                                      .absolute()
                                      .fill(green()))
                           .child(connector("a", "b", routers::arc(0.3f))
                                      .inset(0)
                                      .foreground(wire)
                                      .zIndex(-1)));
  host.frame();
  // Horizontal chord from (25,100) to (175,100), bulge 0.3×150 = 45 px
  // toward +normal (downward-left convention: normal of (+x,0) is
  // (0,+y) → the bow lands at y ≈ 145).
  EXPECT_EQ(host.pixel(100, 145), SK_ColorYELLOW);  // bowed midpoint
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);   // chord midpoint empty
}

TEST(ComposeDerive, ConnectorGapPullsTheWireOffTheEndpoints) {
  // A route runs to the node BOX's centre, and a box is often much larger
  // than the shape drawn inside it — an sdf:: panel, for instance, reserves
  // room for its glow. Without a terminal gap the wire is drawn straight
  // through the visible terminal to a centre nobody can see. The gap is the
  // same pull-back Anchor takes, spelled on connector().
  const auto scene = [](float gap) {
    PathFormat wire;
    wire.width = 4;
    wire.strokeFill = Fill::color({1, 1, 0, 1});
    return stack()
        .child(box()
                   .key("a")
                   .width(20)
                   .height(20)
                   .inset(10, 90, 170, 90)
                   .absolute()
                   .fill(red()))
        .child(box()
                   .key("b")
                   .width(20)
                   .height(20)
                   .inset(170, 90, 10, 90)
                   .absolute()
                   .fill(green()))
        .child(
            connector("a", "b", {}, gap).inset(0).foreground(wire).zIndex(1));
  };
  // Control: with gap 0 the wire runs centre to centre, (20,100) → (180,100),
  // and paints OVER both terminal boxes. Without this arm, "the gapped wire
  // does not reach the terminal" would also pass on a wire that was never
  // drawn.
  Host flush;
  flush.composer.render(scene(0.0f));
  flush.frame();
  EXPECT_EQ(flush.pixel(25, 100), SK_ColorYELLOW)
      << "the gapless wire must still reach into its near terminal";
  EXPECT_EQ(flush.pixel(175, 100), SK_ColorYELLOW)
      << "…and pierce the far one (that IS the complaint)";
  EXPECT_EQ(flush.pixel(100, 100), SK_ColorYELLOW);
  // The gap: 30 px pulled back at EACH end — the wire now runs x ∈
  // [50, 150], both terminals show their own fill, the middle survives.
  Host gapped;
  gapped.composer.render(scene(30.0f));
  gapped.frame();
  EXPECT_EQ(gapped.pixel(25, 100), SK_ColorRED)
      << "the gap did not pull the wire off its near terminal";
  EXPECT_EQ(gapped.pixel(175, 100), SK_ColorGREEN)
      << "the gap did not pull the wire off its far terminal";
  EXPECT_EQ(gapped.pixel(145, 100), SK_ColorYELLOW)
      << "the wire ends before its gap says to";
  EXPECT_EQ(gapped.pixel(160, 100), SK_ColorBLACK)
      << "the wire overran its gap";
  EXPECT_EQ(gapped.pixel(100, 100), SK_ColorYELLOW);  // the run survives
}

TEST(ComposeMask, WrapWindowCrossesTheSeam) {
  // A wrap window crossing the cycle seam must paint exactly the union of
  // its two clamped pieces — direction-agnostic pixel containment.
  auto strokedBox = [](Spans where) {
    return box().child(box()
                           .absolute()
                           .inset(50, 50, 50, 50)
                           .mask(by::spans(std::move(where)))
                           .foreground(stroke(6, green())));
  };
  Host wrap, pieceA, pieceB;
  wrap.composer.render(strokedBox(spans::wrap(0.9f, 1.15f)));
  pieceA.composer.render(strokedBox(spans::range(0.9f, 1.0f)));
  pieceB.composer.render(strokedBox(spans::range(0.0f, 0.15f)));
  wrap.frame();
  pieceA.frame();
  pieceB.frame();
  int unionCount = 0, wrapCount = 0, missing = 0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2) {
      const bool inUnion = pieceA.pixel(x, y) == SK_ColorGREEN ||
                           pieceB.pixel(x, y) == SK_ColorGREEN;
      const bool inWrap = wrap.pixel(x, y) == SK_ColorGREEN;
      unionCount += inUnion;
      wrapCount += inWrap;
      missing += inUnion && !inWrap;
    }
  EXPECT_GT(unionCount, 50);  // the pieces really painted
  EXPECT_LE(missing, 4);      // wrap covers the union (AA slack)
  EXPECT_NEAR(wrapCount, unionCount, unionCount / 5.0 + 8);
}

TEST(ComposeMask, WrapOffsetBindingMarchesTheWindow) {
  Host host;
  choreograph::Output<float> phase{0.0f};
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .mask(by::spans(spans::wrap(0.0f, 0.25f).offset(&phase)))
                      .foreground(stroke(6, green()))));
  host.frame();
  std::vector<SkIPoint> lit0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2)
      if (host.pixel(x, y) == SK_ColorGREEN) lit0.push_back({x, y});
  ASSERT_GT(lit0.size(), 10u);

  phase = 0.5f;  // march half the cycle — no render()
  host.frame();
  int still = 0;
  for (const SkIPoint& p : lit0)
    still += host.pixel(p.x(), p.y()) == SK_ColorGREEN;
  // The window moved to the far side: (almost) none of the old pixels stay.
  EXPECT_LT((float)still, 0.2f * (float)lit0.size());
}

TEST(ComposeShapeValues, ABandWithAComparableSpinePrunes) {
  // A band's authored spine is a Shape too, so deriveEqual must compare it
  // rather than refusing any authored spine outright.
  Host host;
  auto tree = [] {
    return box().child(band(geometry::shapes::circle(), across(8.0f))
                           .width(100)
                           .height(100)
                           .fill(red()));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical authored band spine re-patched";
}
