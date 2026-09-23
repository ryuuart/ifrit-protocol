/** @file
 * The window's pane: a frame drawn through the reader's view of the
 * canvas stays the size of the pane however far the canvas is magnified,
 * and shows the part of the canvas the view says it shows.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/canvas/Sketch.h>

#include <memory>

#include "../CanvasView.h"
#include "support/Fixtures.h"
#include "support/Pixels.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::sketch::test;
using namespace sigil::compose;

constexpr SkColor kGround = SK_ColorBLUE;
constexpr SkColor kMark = SK_ColorRED;

/** A small canvas grounded in blue, with a red mark ten units square in
 *  its top-left corner: where that mark lands is where the canvas's
 *  corner landed. */
struct CornerMark {
  void setup(SketchContext& ctx) {
    ctx.canvas(200, 100);
    ctx.background({0, 0, 1, 1});
    ctx.composer.render(box().inset(0).children({box()
                                                     .alignSelf(Align::Start)
                                                     .flexShrink(0)
                                                     .width(10)
                                                     .height(10)
                                                     .fill(Fill::color(
                                                         {1, 0, 0, 1}))}));
  }
};

/** The pane is exactly the canvas's size, so the view that fits the
 *  canvas is one pane pixel per canvas unit and a zoom of N is a scale
 *  of N. */
constexpr SkISize kPane = {200, 100};

/** The offset that stands the canvas's top-left corner at @p corner on
 *  the pane at @p zoom. */
SkPoint offsetPlacingCorner(float zoom, SkPoint corner) {
  return {corner.x() - kPane.width() * 0.5f + 200.0f * zoom * 0.5f,
          corner.y() - kPane.height() * 0.5f + 100.0f * zoom * 0.5f};
}

TEST(SketchbookPane, AZoomedFrameStaysThePaneAndShowsWhereTheViewSays) {
  std::unique_ptr<Session> session =
      kindOf<CornerMark>()->open(fonts(), assets());
  const SkSize canvas = session->canvas().size;
  ASSERT_EQ(canvas, SkSize::Make(200, 100));
  const SkColor4f ground =
      sigil::material::skia::toSkColor(session->canvas().background);
  const SkPoint corner = {20, 10};
  for (const float zoom : {1.0f, 2.0f, 4.0f}) {
    SCOPED_TRACE(zoom);
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kPane));
    ASSERT_TRUE(surface);
    const CanvasView view{zoom, offsetPlacingCorner(zoom, corner)};
    SkISize drawnInto = {0, 0};
    SkIRect clipped = SkIRect::MakeEmpty();
    float drawnAt = 0.0f;
    drawPane(*surface->getCanvas(), SkSize::Make(kPane), canvas, view,
             [&](SkCanvas& into) {
               drawnInto = into.getBaseLayerSize();
               clipped = into.getDeviceClipBounds();
               drawnAt = into.getTotalMatrix().getScaleX();
               into.clear(ground);
               session->frame(into, 1.0 / 60.0);
             });
    // THE TARGET IS THE PANE AT EVERY ZOOM, and the sketch is handed a
    // clip no larger than it: what lies off the pane is never drawn.
    EXPECT_EQ(drawnInto, kPane);
    EXPECT_TRUE(SkIRect::MakeSize(kPane).contains(clipped));
    // The scale the sketch is drawn at carries the zoom, which is what a
    // bake that reads its resolution off the matrix is taken at.
    EXPECT_FLOAT_EQ(drawnAt, zoom);

    const SkBitmap pixels = plateOf(*surface);
    ASSERT_FALSE(pixels.isNull());
    // The mark stands at the corner the view put the canvas's corner,
    // grown by the zoom; the ground runs on past it.
    const float markEnd = 10.0f * zoom;
    EXPECT_EQ(pixels.getColor((int)(corner.x() + markEnd * 0.5f),
                              (int)(corner.y() + markEnd * 0.5f)),
              kMark);
    EXPECT_EQ(pixels.getColor((int)(corner.x() + markEnd + 3.0f),
                              (int)(corner.y() + 3.0f)),
              kGround);
    // Above and to the left of the canvas the pane has no coverage.
    EXPECT_EQ(SkColorGetA(pixels.getColor((int)corner.x() - 5,
                                          (int)corner.y() - 5)),
              0u);

    // A pointer is read back through the same placement: the pane's
    // point at the corner is the canvas's origin.
    const Placement placement =
        placeCanvas(canvas, SkSize::Make(kPane), view);
    EXPECT_FLOAT_EQ((corner.x() - placement.x) / placement.scale, 0.0f);
    EXPECT_FLOAT_EQ((corner.y() - placement.y) / placement.scale, 0.0f);
  }
}

TEST(SketchbookPane, TheDefaultViewFitsTheWholeCanvas) {
  const SkSize canvas = SkSize::Make(400, 100);
  const SkSize pane = SkSize::Make(200, 200);
  const Placement placement = placeCanvas(canvas, pane, {});
  const Placement fit = fitInto(canvas, SkRect::MakeWH(200, 200));
  EXPECT_FLOAT_EQ(placement.scale, 0.5f);
  EXPECT_FLOAT_EQ(placement.scale, fit.scale);
  EXPECT_FLOAT_EQ(placement.x, fit.x);
  EXPECT_FLOAT_EQ(placement.y, fit.y);
}

TEST(SketchbookPane, WhatASketchLeavesUncoveredShowsTheMatte) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kPane));
  ASSERT_TRUE(surface);
  // Half the canvas's size, centred: a band of pane on every side.
  drawPane(*surface->getCanvas(), SkSize::Make(kPane), SkSize::Make(200, 100),
           {0.5f, {0, 0}}, [](SkCanvas& into) {
             into.clear(SK_ColorTRANSPARENT);
           });
  const SkBitmap pixels = plateOf(*surface);
  ASSERT_FALSE(pixels.isNull());
  EXPECT_EQ(pixels.getColor(100, 50), kPaneMatte);
  EXPECT_EQ(SkColorGetA(pixels.getColor(10, 5)), 0u);
}

}  // namespace
