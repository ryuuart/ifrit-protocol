// The strip a tile flow lays a mark out along: where each tile's mark
// lands for every flow and facing, what a mirrored tile reads under
// mirrored sampling, and the flattening that changes the ops without
// changing a pixel.

#include <include/core/SkPictureRecorder.h>
#include <sigilskia/draw/Direct.h>

#include <vector>

#include "support/BrushTestSupport.h"

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
    strip.children(
        {box()
             .absolute()
             .left(kMark + (down ? 0.0f : (float)(j * kTileW)))
             .top(kMark + (down ? (float)(j * kTileH) : 0.0f))
             .width(kMarkSize)
             .height(kMarkSize)
             .fill(Fill::color(SkColor4f::FromColor(stripMark(j))))});
  // Shell box: snapshot() sizes by the ROOT's children, not its own dims.
  return snapshot(box().children({std::move(strip)}), fonts());
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

INSTANTIATE_TEST_SUITE_P(ComposeStripTiles, StripTile,
                         testing::Values(TileWindow{"DownForward",
                                                    tiles::Flow::Down,
                                                    tiles::Facing::Forward,
                                                    {kProbe, kProbe},
                                                    {{kFarX, kProbe}}},
                                         TileWindow{"DownMirrored",
                                                    tiles::Flow::Down,
                                                    tiles::Facing::Mirrored,
                                                    {kFarX, kProbe},
                                                    {{kProbe, kProbe},
                                                     {kFarX, kFarY}}},
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
