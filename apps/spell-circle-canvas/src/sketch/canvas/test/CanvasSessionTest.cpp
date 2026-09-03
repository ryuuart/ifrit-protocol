/** @file
 * The 2D session: what a sketch declares, what a host reads back, and
 * what stepping it does.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Frame.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <numbers>
#include <string>

#include "Support.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

/** The pixels of an image, read back. */
SkBitmap pixelsOf(const sk_sp<SkImage>& image) {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(image->width(), image->height()));
  EXPECT_TRUE(image->readPixels(nullptr, bitmap.pixmap(), 0, 0));
  return bitmap;
}

/** THE ONE BODY A BAKE IS MEASURED BY: a white unlit card, its size
 *  stated in world units, square to the camera at the origin. Unlit
 *  because a shaded body's edge is a gradient and this test is about
 *  where the edge IS; a card because a card's silhouette is a rectangle
 *  whose pixels the camera predicts exactly. */
constexpr float kCardW = 100.0f;
constexpr float kCardH = 60.0f;
constexpr float kEyeZ = 300.0f;
constexpr SkISize kBakeSize{128, 96};
constexpr SkColor4f kGround{0.1f, 0.1f, 0.1f, 1};

world::Frame cardFrame() {
  return world::Frame(world::Element().key("set").child(
      world::Element()
          .key("card")
          .mesh(gm::quad(kCardW, kCardH))
          .fill(sigil::material::kit::unlit({.baseColor = {1, 1, 1, 1}}))));
}

gm::camera::Camera cardCamera(float eyeZ) {
  gm::camera::Camera lens;
  lens.eye = {0, 0, eyeZ};
  lens.target = {0, 0, 0};
  return lens;
}

/** HOW MANY PIXELS ONE WORLD UNIT COVERS at the origin, from @p lens, on
 *  a picture @p size across: the projection's own arithmetic, so the
 *  test states the relationship rather than a number someone measured
 *  off a picture. */
float pixelsPerUnit(const gm::camera::Camera& lens, SkISize size) {
  const float halfFov =
      lens.fovYDeg * 0.5f * (float)std::numbers::pi / 180.0f;
  return (float)size.height() /
         (2.0f * std::abs(lens.eye.z) * std::tan(halfFov));
}

/** The box the drawn pixels stand in: every pixel that is not the
 *  background. Empty when nothing was drawn. */
SkIRect silhouetteOf(const SkBitmap& pixels) {
  SkIRect box = SkIRect::MakeEmpty();
  for (int y = 0; y < pixels.height(); ++y)
    for (int x = 0; x < pixels.width(); ++x)
      if (pixels.getColor(x, y) != kGround.toSkColor())
        box.join(SkIRect::MakeXYWH(x, y, 1, 1));
  return box;
}

/** A sketch that asks for a texture scene while declaring itself, paints
 *  one red sheet into it, and then LETS GO of it — keeping the image it
 *  took and a weak look at the scene, so that what stands afterwards is
 *  what the session is holding and nothing the sketch is. */
struct Screening : Sketch {
  static inline sk_sp<SkImage> sheet;
  static inline std::weak_ptr<TextureScene> scene;
  void setup(SketchContext& ctx) override {
    ctx.canvas(200, 120);
    const std::shared_ptr<TextureScene> kept = ctx.textureScene({32, 32});
    kept->render(box().width(32).height(32).fill(Fill::color({1, 0, 0, 1})));
    sheet = kept->image();
    scene = kept;
    ctx.composer.render(box().width(10).height(10));
  }
};

/** A sketch that declares a canvas, draws one box, and counts the times
 *  its body was run. The count is the observable behind the difference
 *  between repainting and stepping. */
struct Declaring : Sketch {
  static inline int updates = 0;
  void setup(SketchContext& ctx) override {
    ctx.canvas(320, 200);
    ctx.background({0.1f, 0.2f, 0.3f, 1});
    ctx.captureAt(2.5);
    ctx.composer.render(
        box().width(100).height(50).fill(Fill::color({1, 0, 0, 1})));
  }
  void update(double, SketchContext&) override { ++updates; }
};

/** What every case here opens: the session over that sketch, and a
 *  raster surface at exactly the canvas it declared. */
class CanvasSession : public ::testing::Test {
 protected:
  CanvasSession()
      : session(kindOf<Declaring>()->open(fonts(), assets())),
        surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 200))) {}

  SkCanvas& canvas() { return *surface->getCanvas(); }

  /** What the surface holds now. */
  SkBitmap pixels() {
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  }

  std::unique_ptr<Session> session;
  sk_sp<SkSurface> surface;
};

TEST_F(CanvasSession, ReadsTheDeclarationBackAfterSetup) {
  EXPECT_EQ(session->canvas().size, SkSize::Make(320, 200));
  EXPECT_EQ(session->canvas().captureSeconds, 2.5);
  EXPECT_FLOAT_EQ(session->canvas().background.fB, 0.3f);
}

TEST_F(CanvasSession, StepsAtTheRateItIsGiven) {
  // The first stepped frame must advance, not be swallowed seeding the
  // clock: a plate is a function of the number of steps taken, so a
  // frame lost at the start moves every one after it.
  for (int i = 0; i < 6; ++i) session->frame(canvas(), 1.0 / 60.0);
  EXPECT_NEAR(session->timing().totalMs,
              session->timing().updateMs + session->timing().drawMs, 1e-6);
}

TEST_F(CanvasSession, ReportsTheLanesTheRuntimeSpent) {
  session->frame(canvas(), 1.0 / 60.0);
  ASSERT_EQ(session->lanes().size(), 4u);
  EXPECT_STREQ(session->lanes()[0].name, "recon");
  EXPECT_STREQ(session->lanes()[3].name, "paint");
  EXPECT_FALSE(session->counters().empty());
}

TEST_F(CanvasSession, PaintsWhatTheSketchDescribed) {
  canvas().clear(SK_ColorBLACK);
  session->frame(canvas(), 1.0 / 60.0);
  EXPECT_EQ(pixels().getColor(10, 10), SK_ColorRED);
}

TEST_F(CanvasSession, RepaintDoesNotAdvanceButStillDoes) {
  session->frame(canvas(), 1.0 / 60.0);
  session->frame(canvas(), 1.0 / 60.0);
  const int stepped = Declaring::updates;

  // Two repaints from the same ground are the same picture: a repaint
  // draws the state the frames left and runs nothing.
  canvas().clear(SK_ColorBLACK);
  session->repaint(canvas());
  const SkBitmap first = pixels();
  canvas().clear(SK_ColorBLACK);
  session->repaint(canvas());
  const SkBitmap second = pixels();
  EXPECT_EQ(Declaring::updates, stepped);
  ASSERT_EQ(first.computeByteSize(), second.computeByteSize());
  EXPECT_EQ(0, std::memcmp(first.getPixels(), second.getPixels(),
                           first.computeByteSize()));

  // The still takes exactly one more step, which is what makes a plate
  // the declared moment plus one.
  session->still(canvas());
  EXPECT_EQ(Declaring::updates, stepped + 1);
}

TEST_F(CanvasSession, TakesTheStillLargerThanItsCanvas) {
  EXPECT_GT(session->oversample(), 1.0f);
}

/** A sketch that bakes the card twice while declaring itself: once from
 *  the stated distance, once from twice it. */
struct Baking : Sketch {
  static inline sk_sp<SkImage> near;
  static inline sk_sp<SkImage> far;
  void setup(SketchContext& ctx) override {
    ctx.canvas(200, 120);
    near = ctx.bakeSet(cardFrame(), cardCamera(kEyeZ), kBakeSize, kGround);
    far =
        ctx.bakeSet(cardFrame(), cardCamera(kEyeZ * 2), kBakeSize, kGround);
    ctx.composer.render(box().width(10).height(10));
  }
};

TEST(CanvasDoors, PaintsWhatTheSketchHandedTheTextureScene) {
  const std::unique_ptr<Session> session =
      kindOf<Screening>()->open(fonts(), assets());
  ASSERT_TRUE(Screening::sheet);
  EXPECT_EQ(Screening::sheet->width(), 32);
  EXPECT_EQ(Screening::sheet->height(), 32);
  EXPECT_EQ(pixelsOf(Screening::sheet).getColor(16, 16), SK_ColorRED);
}

TEST(CanvasDoors, KeepsTheTextureScenesItHandsOutUntilTheBodyDeclaresAgain) {
  std::unique_ptr<Session> session =
      kindOf<Screening>()->open(fonts(), assets());
  // The sketch dropped its own reference at the end of setup; the scene
  // stands because the SESSION holds it — which is what a picture on a
  // device needs, since a scene destroys the texture its image names
  // when it goes.
  EXPECT_FALSE(Screening::scene.expired());
  EXPECT_NE(session->counters().find("scenes 1"), std::string::npos);

  // Declaring again lets last time's scenes go and keeps this time's.
  const std::weak_ptr<TextureScene> first = Screening::scene;
  session->redeclare();
  EXPECT_TRUE(first.expired());
  EXPECT_FALSE(Screening::scene.expired());
  EXPECT_NE(session->counters().find("scenes 1"), std::string::npos);

  // …and the session going lets go of the last of them.
  session.reset();
  EXPECT_TRUE(Screening::scene.expired());
}

TEST(CanvasDoors, BakesASetAsTheCameraProjectsIt) {
  const std::unique_ptr<Session> session =
      kindOf<Baking>()->open(fonts(), assets());
  ASSERT_TRUE(Baking::near);
  EXPECT_EQ(Baking::near->width(), kBakeSize.width());
  EXPECT_EQ(Baking::near->height(), kBakeSize.height());

  const SkBitmap pixels = pixelsOf(Baking::near);
  // Nothing but the card is in the tree, so the corner is the ground it
  // was cleared to and the middle of the card is the card's own colour —
  // unlit, so it is that colour exactly.
  EXPECT_EQ(pixels.getColor(1, 1), kGround.toSkColor());
  EXPECT_EQ(pixels.getColor(kBakeSize.width() / 2, kBakeSize.height() / 2),
            SK_ColorWHITE);

  // THE SILHOUETTE IS THE PROOF. A card of stated size, square to the
  // camera at the point it is aimed at, covers a rectangle the
  // projection names to the pixel; a body that collapsed to a point, or
  // one drawn without its transform, would land inside a colour
  // comparison at the centre and nowhere near this.
  const float scale = pixelsPerUnit(cardCamera(kEyeZ), kBakeSize);
  const SkIRect drawn = silhouetteOf(pixels);
  EXPECT_NEAR((float)drawn.width(), kCardW * scale, 2.0f);
  EXPECT_NEAR((float)drawn.height(), kCardH * scale, 2.0f);
  EXPECT_NEAR((float)(drawn.left() + drawn.right()) / 2.0f,
              kBakeSize.width() / 2.0f, 1.0f);
  EXPECT_NEAR((float)(drawn.top() + drawn.bottom()) / 2.0f,
              kBakeSize.height() / 2.0f, 1.0f);
}

TEST(CanvasDoors, BakesTheSameSetFromTheCameraItIsGiven) {
  const std::unique_ptr<Session> session =
      kindOf<Baking>()->open(fonts(), assets());
  ASSERT_TRUE(Baking::far);
  // The same frame from twice the distance is the same card at half the
  // pixels — the viewpoint is the caller's, and the picture is a
  // projection of the set rather than a stamp of it.
  const SkIRect near = silhouetteOf(pixelsOf(Baking::near));
  const SkIRect far = silhouetteOf(pixelsOf(Baking::far));
  EXPECT_NEAR((float)far.width(), near.width() / 2.0f, 2.0f);
  EXPECT_NEAR((float)far.height(), near.height() / 2.0f, 2.0f);
}

}  // namespace
