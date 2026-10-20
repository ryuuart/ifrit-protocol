/** @file
 * A scene3d sketch seen through a window's pane: a zoomed pane forms only its
 * own pixels and shows what the whole canvas shows there, a live session
 * forms no more pixels than it declared, and a still shows the whole picture.
 *
 * Every case that opens a scene3d sketch draws it on the device and SKIPS where
 * the machine has no Vulkan runtime; its suite carries the `gpu` label.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <algorithm>
#include <cstdlib>
#include <glm/geometric.hpp>
#include <memory>

#include "Scene3DTestSupport.h"
#include "support/Fixtures.h"
#include "support/Pixels.h"

namespace {

using namespace sigil::sketch;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;
using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::sketch::test::oneFrame;
using sigil::sketch::test::samePicture;
using sigil::sketch::test::Scene3DWindow;

/** A body on a turntable, performed through a pass the sketch declares
 *  itself and asked back, so a case can read how many pixels the frame
 *  was formed at — which is what a host zooming into a piece of it pays
 *  for. */
struct Measured {
  static inline glm::ivec2 formed{0, 0};
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0.05f, 0.05f, 0.08f, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 90, 260};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
  }
  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    // A rounded box large enough to fill the pane the zoomed cases look
    // through, turning, so its shading moves across every pixel of it.
    world::Frame frame(
        world::group(
            {world::directionalLight(
                 {-0.4f, -0.8f, -0.3f},
                 {.color = sigil::material::Color{1, 1, 1, 1},
                  .intensity = 1.0f})
                 .key("sun"),
             world::mesh(gm::superellipsoid({60, 60, 60}, 2.0f, 24, 16))
                 .key("body")
                 .rotateY(seconds * 90.0f)
                 .fill(sigil::material::surface::program())})
            .key("set"));
    frame
        .pass(world::geometryPass("colour").writes("colour").clear(
            sigil::material::Color{0.05f, 0.05f, 0.08f, 1}))
        .readback(world::readback("colour").then(
            [](const world::Readback::Result& result) {
              formed = result.image.size();
            }));
    return frame;
  }
};

/** How far into the canvas the pane of the zoomed cases looks, in the
 *  pixels of the canvas at four times its declared size: a quarter of
 *  its width and height, off-centre so a part placed at the centre of
 *  the picture would be caught out. */
constexpr int kPaneLeft = 240;
constexpr int kPaneTop = 180;
constexpr float kZoom = 4.0f;

/** @p frames of @p session onto a pane the declared canvas's size,
 *  looking at the canvas magnified four times — what a window zoomed to
 *  4x hands a sketch: a canvas scaled up and clipped to the pane. */
SkBitmap throughThePane(Session& session, int frames) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  canvas.translate(-(float)kPaneLeft, -(float)kPaneTop);
  canvas.scale(kZoom, kZoom);
  for (int f = 0; f < frames; ++f) session.frame(canvas, 1.0 / 60.0);
  return bitmap;
}

TEST_F(Scene3DWindow, AZoomedPaneFormsOnlyThePanesPixels) {
  // A capture is formed at the resolution it is seen at, so at 4x the
  // whole canvas would be 640x480 — sixteen times the pane. What is
  // formed is the part the pane shows, and no larger than the pane.
  Measured::formed = {0, 0};
  std::unique_ptr<Session> session =
      kindOf<Measured>()->open(fonts(), assets(), true);
  // Two frames: a readback is handed over the frame after it was made.
  throughThePane(*session, 2);
  EXPECT_EQ(Measured::formed, glm::ivec2(160, 120));
}

TEST_F(Scene3DWindow, APaneShowsWhatTheWholeCanvasShowsThere) {
  // The part is the whole picture's projection carried off-centre, so
  // each pixel of it is the pixel the whole canvas at 4x holds there.
  // The two projections are the same numbers composed differently, so a
  // pixel may land a rounding apart on an edge: a channel may move by a
  // level or two, and a pixel whose centre stands on a triangle's edge
  // may fall to the other side of it — never more than a sliver of the
  // pane.
  std::unique_ptr<Session> zoomed =
      kindOf<Measured>()->open(fonts(), assets(), true);
  const SkBitmap pane = throughThePane(*zoomed, 30);

  std::unique_ptr<Session> whole =
      kindOf<Measured>()->open(fonts(), assets(), true);
  SkBitmap canvasPixels;
  canvasPixels.allocPixels(SkImageInfo::MakeN32Premul(640, 480));
  SkCanvas canvas(canvasPixels);
  canvas.scale(kZoom, kZoom);
  for (int f = 0; f < 30; ++f) whole->frame(canvas, 1.0 / 60.0);

  int apart = 0;
  int drawn = 0;
  // The whole canvas's corner is the ground; the pane looks at the box.
  const SkColor ground = canvasPixels.getColor(0, 0);
  for (int y = 0; y < pane.height(); ++y)
    for (int x = 0; x < pane.width(); ++x) {
      const SkColor here = pane.getColor(x, y);
      const SkColor there = canvasPixels.getColor(x + kPaneLeft, y + kPaneTop);
      if (here != ground) ++drawn;
      const int difference = std::max(
          {std::abs((int)SkColorGetR(here) - (int)SkColorGetR(there)),
           std::abs((int)SkColorGetG(here) - (int)SkColorGetG(there)),
           std::abs((int)SkColorGetB(here) - (int)SkColorGetB(there))});
      if (difference > 2) ++apart;
    }
  // The pane looks at the box, so a pane showing only the ground would
  // pass the comparison for the wrong reason.
  EXPECT_GT(drawn, pane.width() * pane.height() / 4);
  EXPECT_LE(apart, pane.width() * pane.height() / 500)
      << apart << " pixels differ by more than two levels";
}

TEST_F(Scene3DWindow, AWholeCanvasClipFormsTheWholeCanvas) {
  // A plate's clip is the whole canvas, and what it forms is the
  // declared canvas in the plate's pixels — the frame a scene3d sketch with no
  // part in it forms, unmoved.
  Measured::formed = {0, 0};
  std::unique_ptr<Session> declared =
      kindOf<Measured>()->open(fonts(), assets());
  oneFrame(*declared);
  oneFrame(*declared);
  EXPECT_EQ(Measured::formed, glm::ivec2(160, 120));

  Measured::formed = {0, 0};
  std::unique_ptr<Session> fitted =
      kindOf<Measured>()->open(fonts(), assets(), true);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(640, 480));
  SkCanvas canvas(bitmap);
  canvas.scale(kZoom, kZoom);
  fitted->frame(canvas, 1.0 / 60.0);
  fitted->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(Measured::formed, glm::ivec2(640, 480));
}

TEST_F(Scene3DWindow, ALiveSessionFormsNoMorePixelsThanItDeclared) {
  // A live window's pane on a dense screen, or zoomed in, hands the
  // sketch a canvas scaled up. The lit draw costs by the pixel, so a
  // live session forms the declared canvas and the pane magnifies it;
  // the same canvas handed to a capture forms every pixel (above).
  Measured::formed = {0, 0};
  std::unique_ptr<Session> live =
      kindOf<Measured>()->open(fonts(), assets(), false);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(640, 480));
  SkCanvas canvas(bitmap);
  canvas.scale(kZoom, kZoom);
  live->frame(canvas, 1.0 / 60.0);
  live->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(Measured::formed, glm::ivec2(160, 120));
  // …and the picture is still drawn over the whole canvas: a corner
  // is the ground and the middle is the body.
  EXPECT_NE(bitmap.getColor(320, 240), bitmap.getColor(2, 2));

  // A pane looking at a quarter of the canvas at 4x forms that quarter
  // of the declared pixels, never the pane's own.
  Measured::formed = {0, 0};
  std::unique_ptr<Session> zoomed =
      kindOf<Measured>()->open(fonts(), assets(), false);
  throughThePane(*zoomed, 2);
  EXPECT_EQ(Measured::formed, glm::ivec2(160 / kZoom, 120 / kZoom));
}

TEST_F(Scene3DWindow, AStillAfterAZoomedFrameShowsTheWholePicture) {
  // A still describes nothing and shows the frame standing; a frame
  // standing that was formed over a pane's part of the picture is formed
  // again over what the still shows, at the same moment — so it is the
  // still a session that was never zoomed takes.
  std::unique_ptr<Session> zoomed = kindOf<Measured>()->open(fonts(), assets());
  throughThePane(*zoomed, 12);
  std::unique_ptr<Session> plain = kindOf<Measured>()->open(fonts(), assets());
  for (int f = 0; f < 11; ++f) oneFrame(*plain);
  oneFrame(*plain);

  SkBitmap fromZoomed;
  fromZoomed.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas zoomedCanvas(fromZoomed);
  zoomed->still(zoomedCanvas);
  SkBitmap fromPlain;
  fromPlain.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas plainCanvas(fromPlain);
  plain->still(plainCanvas);
  EXPECT_TRUE(samePicture(fromZoomed, fromPlain));
}

}  // namespace
