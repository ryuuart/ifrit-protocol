/** @file
 * The 3D session: what a set declares, and that stepping it to a moment
 * is a function of the moment alone.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/element/Element.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/geometric.hpp>
#include <memory>
#include <optional>
#include <string>

#include "Support.h"

namespace {

using namespace sigil::sketch;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

/** One box on a turntable, and nothing else. */
struct Spun : Set {
  void setup(SetContext& ctx) override {
    ctx.canvas(160, 120);
    ctx.background({0.05f, 0.05f, 0.08f, 1});
    ctx.captureAt(0.5);
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 90, 260};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
  }
  world::Frame describe(float seconds) override {
    return world::Element()
        .key("set")
        .child(world::Element().key("sun").light(
            world::sun({-0.4f, -0.8f, -0.3f}, {1, 1, 1, 1}, 1.0f)))
        .child(world::Element()
                   .key("body")
                   .rotateY(seconds * 90.0f)
                   .mesh(gm::superellipsoid({40, 40, 40}, 0.2f, 24, 16))
                   .fill(sigil::material::kit::surface()));
  }
};

/** A set that paints a compose sheet into a texture the session keeps,
 *  and wears it on one unlit card facing the camera. */
struct Screened : Set {
  std::shared_ptr<sigil::compose::TextureScene> screen;
  void setup(SetContext& ctx) override {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
    screen = ctx.textureScene({64, 64});
  }
  world::Frame describe(float seconds) override {
    using namespace sigil::compose;
    screen->render(
        box().width(64).height(64).fill(Fill::color({0, 1, 0, 1})), seconds);
    sigil::material::Material surface =
        sigil::material::kit::unlit({.baseColor = {1, 1, 1, 1}});
    surface.child(sigil::material::kit::kBaseColorSlot, screen->texture());
    return world::Element().key("set").child(
        world::Element().key("card").mesh(gm::quad(120, 90)).fill(surface));
  }
};

/** THE CAMERA A SET DECLARES IN ITS OWN TREE — nowhere near the
 *  fallback its host hands in, and looking at a point that is not the
 *  origin, so that a viewpoint pivoting on the wrong one of the two is
 *  visible. */
sigil::geometry::mesh::camera::Camera framedLens() {
  sigil::geometry::mesh::camera::Camera lens;
  lens.eye = {180, 150, 320};
  lens.target = {40, 30, -10};
  lens.fovYDeg = 52;
  return lens;
}

/** One box seen from a camera the TREE carries. */
struct Framed : Set {
  void setup(SetContext& ctx) override {
    ctx.canvas(160, 120);
    ctx.background({0.05f, 0.05f, 0.08f, 1});
    sigil::geometry::mesh::camera::Camera fallback;
    fallback.eye = {0, 0, 900};
    ctx.camera(fallback);
  }
  world::Frame describe(float) override {
    return world::Element()
        .key("set")
        .child(world::Element().key("sun").light(
            world::sun({-0.4f, -0.8f, -0.3f}, {1, 1, 1, 1}, 1.0f)))
        .child(world::Element().key("lens").camera(framedLens()))
        .child(world::Element()
                   .key("body")
                   .at({40, 30, -10})
                   .mesh(gm::superellipsoid({40, 40, 40}, 0.2f, 24, 16))
                   .fill(sigil::material::kit::surface()));
  }
};

/** The set, stepped @p frames from zero onto a canvas @p scale times its
 *  declared size and scaled to match — which is what a host that fitted
 *  the sketch into a window on a scaled screen hands over. */
SkBitmap steppedOnto(float scale, int frames) {
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul((int)std::lround(160 * scale),
                                                (int)std::lround(120 * scale)));
  SkCanvas canvas(bitmap);
  canvas.scale(scale, scale);
  for (int f = 0; f < frames; ++f) session->frame(canvas, 1.0 / 60.0);
  return bitmap;
}

SkBitmap steppedTo(int frames) { return steppedOnto(1.0f, frames); }

/** One frame of @p session onto a plate of the declared canvas size. */
SkBitmap oneFrame(Session& session) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  session.frame(canvas, 1.0 / 60.0);
  return bitmap;
}

/** Whether two plates are the same picture, byte for byte. */
bool samePicture(const SkBitmap& a, const SkBitmap& b) {
  return a.computeByteSize() == b.computeByteSize() &&
         std::memcmp(a.getPixels(), b.getPixels(), a.computeByteSize()) == 0;
}

/** WHERE THE PICTURE STANDS, as fractions of the plate, so two plates of
 *  different sizes can be asked whether they show the same thing in the
 *  same place. A set's background is one flat colour and everything
 *  drawn stands against it, so what is not the corner pixel is the
 *  picture. */
SkRect inkBounds(const SkBitmap& plate) {
  const SkColor background = plate.getColor(0, 0);
  int left = plate.width(), top = plate.height(), right = -1, bottom = -1;
  for (int y = 0; y < plate.height(); ++y)
    for (int x = 0; x < plate.width(); ++x) {
      if (plate.getColor(x, y) == background) continue;
      left = std::min(left, x);
      top = std::min(top, y);
      right = std::max(right, x);
      bottom = std::max(bottom, y);
    }
  if (right < 0) return SkRect::MakeEmpty();
  return SkRect::MakeLTRB((float)left / (float)plate.width(),
                          (float)top / (float)plate.height(),
                          (float)(right + 1) / (float)plate.width(),
                          (float)(bottom + 1) / (float)plate.height());
}

TEST(SetSession, ReadsTheDeclarationBackAfterSetup) {
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  EXPECT_EQ(session->canvas().size, SkSize::Make(160, 120));
  EXPECT_EQ(session->canvas().captureSeconds, 0.5);
}

TEST(SetSession, TheSameMomentIsTheSamePicture) {
  // The plate contract: a set is a pure function of the scene time, so
  // two runs that step the same number of frames agree on every byte.
  EXPECT_TRUE(samePicture(steppedTo(30), steppedTo(30)));
}

TEST(SetSession, ADifferentMomentIsADifferentPicture) {
  EXPECT_FALSE(samePicture(steppedTo(6), steppedTo(30)));
}

TEST(SetSession, AFittedCanvasPutsThePictureInTheSamePlace) {
  // A live host hands over a canvas ALREADY FITTED: the sketch's own
  // canvas scaled up to the window it was letterboxed into, on a screen
  // that may have two pixels for every one of those. A set whose
  // projection is read off the surface instead of off the canvas lands
  // at the surface's size inside a box that is still the declared one,
  // which carries most of it off its own edge.
  const SkBitmap declared = steppedOnto(1.0f, 30);
  const SkBitmap fitted = steppedOnto(2.0f, 30);
  ASSERT_EQ(fitted.width(), declared.width() * 2);
  ASSERT_EQ(fitted.height(), declared.height() * 2);

  const SkRect one = inkBounds(declared);
  const SkRect two = inkBounds(fitted);
  ASSERT_FALSE(one.isEmpty());
  ASSERT_FALSE(two.isEmpty());
  // Within a pixel of the smaller plate, which is what an edge two
  // resolutions cover differently is worth.
  constexpr float kSlack = 1.0f / 120.0f;
  EXPECT_NEAR(one.fLeft, two.fLeft, kSlack);
  EXPECT_NEAR(one.fTop, two.fTop, kSlack);
  EXPECT_NEAR(one.fRight, two.fRight, kSlack);
  EXPECT_NEAR(one.fBottom, two.fBottom, kSlack);
}

TEST(SetSession, AFittedCanvasHoldsTheSamePicture) {
  // The same picture INSIDE the outline too, which is what says the
  // projection landed where it belongs rather than merely fitting: a set
  // is formed at ONE resolution, so it has to be formed at the canvas's
  // — one formed at its declared size and then magnified onto the canvas
  // is a picture of a smaller one. Every 2x2 of the fitted plate
  // averages to its own pixel of the declared one, to within what two
  // resolutions do to an edge.
  const SkBitmap declared = steppedOnto(1.0f, 30);
  const SkBitmap fitted = steppedOnto(2.0f, 30);
  ASSERT_EQ(fitted.width(), declared.width() * 2);
  ASSERT_EQ(fitted.height(), declared.height() * 2);

  double total = 0;
  for (int y = 0; y < declared.height(); ++y)
    for (int x = 0; x < declared.width(); ++x) {
      const SkColor4f want = declared.getColor4f(x, y);
      SkColor4f got{0, 0, 0, 0};
      for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
          const SkColor4f pixel = fitted.getColor4f(2 * x + dx, 2 * y + dy);
          got = {got.fR + pixel.fR * 0.25f, got.fG + pixel.fG * 0.25f,
                 got.fB + pixel.fB * 0.25f, 1.0f};
        }
      total += std::abs(want.fR - got.fR) + std::abs(want.fG - got.fG) +
               std::abs(want.fB - got.fB);
    }
  EXPECT_LT(total / (3.0 * declared.width() * declared.height()), 0.02);
}

TEST(SetSession, DrawsThroughThePassesWhenTheProcessInstalledARuntime) {
  // The runtime is a property of the PROCESS, not of a sketch: a host
  // that brought one up says so once, and every session then reaches it.
  // An executor is only reached THROUGH passes, so a set about the scene
  // is given one — and a session that never declared it would draw every
  // surface as the colour extract read off it, whatever runtime the host
  // installed.
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  session->frame(canvas, 1.0 / 60.0);
  ASSERT_EQ(session->lanes().size(), 4u);
  EXPECT_STREQ(session->lanes()[3].name, "passes");
  EXPECT_EQ(session->lanes()[3].ms, 0.0);

  useRuntime(world::Runtime::cpu());
  std::unique_ptr<Session> performed = kindOf<Spun>()->open(fonts(), assets());
  performed->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(performed->lanes()[3].ms, 1.0);
  useRuntime({});
}

TEST(SetSession, TakesItsStillAtItsOwnSize) {
  // The still describes nothing: it presents the frame that stands, so
  // there is nothing here to form again larger and a bigger plate would
  // only magnify what is already made.
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  EXPECT_FLOAT_EQ(session->oversample(), 1.0f);
}

TEST(SetSession, OffersAViewpointAHostCanMove) {
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  EXPECT_TRUE(session->hasViewpoint());
  session->viewpoint(45.0f, 20.0f, 300.0f);
}

TEST(Orbit, ReadsACameraBackAsTheOrbitThatMakesIt) {
  // The two are exact inverses, which is what lets a host seed a control
  // from a set's own lens: moving it by nothing gives that lens back.
  const sigil::geometry::mesh::camera::Camera lens = framedLens();
  const Orbit orbit = orbitOf(lens);
  EXPECT_NEAR(orbit.distance, glm::length(lens.eye - lens.target), 1e-3f);
  const sigil::geometry::mesh::camera::Camera back = cameraAt(lens, orbit);
  EXPECT_NEAR(back.eye.x, lens.eye.x, 1e-3f);
  EXPECT_NEAR(back.eye.y, lens.eye.y, 1e-3f);
  EXPECT_NEAR(back.eye.z, lens.eye.z, 1e-3f);
  // Everything but where the eye stands is the pivot's own.
  EXPECT_EQ(back.target, lens.target);
  EXPECT_EQ(back.up, lens.up);
  EXPECT_FLOAT_EQ(back.fovYDeg, lens.fovYDeg);
}

TEST(SetSession, IsSeenFromTheCameraItsOwnTreeCarries) {
  // WHAT A LIVE HOST MUST SHOW before anyone touches it: the set as its
  // plate shows it. The host hands in a fallback camera nowhere near the
  // one the tree carries, and the tree's is what the session reports.
  std::unique_ptr<Session> session = kindOf<Framed>()->open(fonts(), assets());
  oneFrame(*session);

  const Orbit declared = orbitOf(framedLens());
  const std::optional<Orbit> reported = session->orbit();
  ASSERT_TRUE(reported.has_value());
  EXPECT_NEAR(reported->yawDeg, declared.yawDeg, 1e-2f);
  EXPECT_NEAR(reported->pitchDeg, declared.pitchDeg, 1e-2f);
  EXPECT_NEAR(reported->distance, declared.distance, 1e-2f);
}

TEST(SetSession, ADragThatMovesItByNothingChangesNothing) {
  // Which is what says the orbit pivots on the declared camera's own
  // target and stands at its own distance, rather than on a point and a
  // distance of the host's.
  std::unique_ptr<Session> session = kindOf<Framed>()->open(fonts(), assets());
  oneFrame(*session);
  const Orbit declared = orbitOf(framedLens());

  const SkBitmap standing = oneFrame(*session);
  session->viewpoint(declared.yawDeg, declared.pitchDeg, declared.distance);
  const SkBitmap held = oneFrame(*session);
  EXPECT_TRUE(samePicture(standing, held));
}

TEST(SetSession, ADragMovesItAboutTheTargetItDeclared) {
  std::unique_ptr<Session> session = kindOf<Framed>()->open(fonts(), assets());
  oneFrame(*session);
  const Orbit declared = orbitOf(framedLens());

  const SkBitmap standing = oneFrame(*session);
  session->viewpoint(declared.yawDeg + 90.0f, declared.pitchDeg,
                     declared.distance);
  const SkBitmap moved = oneFrame(*session);
  EXPECT_FALSE(samePicture(standing, moved));
  const std::optional<Orbit> after = session->orbit();
  ASSERT_TRUE(after.has_value());
  EXPECT_NEAR(after->distance, declared.distance, 1e-2f);
  EXPECT_NEAR(after->pitchDeg, declared.pitchDeg, 1e-2f);
}

TEST(SetDoors, PaintsATextureSceneOntoABody) {
  std::unique_ptr<Session> session =
      kindOf<Screened>()->open(fonts(), assets());
  // The card is unlit and wears the sheet, so what the middle of the
  // picture shows is what the compose tree painted: green, and nothing
  // of the black ground or of any lighting.
  const SkBitmap picture = oneFrame(*session);
  const SkColor centre = picture.getColor(80, 60);
  EXPECT_GT(SkColorGetG(centre), 200u);
  EXPECT_LT(SkColorGetR(centre), 40u);
  EXPECT_LT(SkColorGetB(centre), 40u);
  // The set holds the scene too, but the SESSION is what keeps it
  // standing, and says so.
  EXPECT_NE(session->counters().find("screens 1"), std::string::npos);
}

}  // namespace
