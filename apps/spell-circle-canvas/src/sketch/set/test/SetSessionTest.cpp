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
#include <vector>

#include "support/Fixtures.h"
#include "support/Pixels.h"
#include "support/Sessions.h"

namespace {

using namespace sigil::sketch;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::sketch::test::inkBounds;
using sigil::sketch::test::samePicture;

/** One box on a turntable, and nothing else. It counts the times its
 *  body was asked to describe, which is the observable behind the
 *  difference between stepping and repainting. */
struct Spun : Set {
  static inline int describes = 0;
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
    ++describes;
    return world::Element()
        .key("set")
        .child(world::Element().key("sun").light(
            world::light::sun({-0.4f, -0.8f, -0.3f}, {1, 1, 1, 1}, 1.0f)))
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
            world::light::sun({-0.4f, -0.8f, -0.3f}, {1, 1, 1, 1}, 1.0f)))
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

TEST(SetSession, AFittedCanvasIsFormedAtItsOwnResolution) {
  // A set is formed at ONE resolution, so it has to be formed at the
  // canvas's: one formed at the declared size and then magnified onto a
  // canvas twice as wide is a picture of a smaller one. What separates
  // the two is inside the outline rather than around it — a magnified
  // plate is one where every 2x2 of pixels came from a single source
  // pixel, so not one of them holds two colours.
  const SkBitmap fitted = steppedOnto(2.0f, 30);
  ASSERT_EQ(fitted.width(), 320);
  ASSERT_EQ(fitted.height(), 240);

  size_t mixed = 0;
  for (int y = 0; y + 1 < fitted.height(); y += 2)
    for (int x = 0; x + 1 < fitted.width(); x += 2) {
      const SkColor first = fitted.getColor(x, y);
      if (fitted.getColor(x + 1, y) != first ||
          fitted.getColor(x, y + 1) != first ||
          fitted.getColor(x + 1, y + 1) != first)
        ++mixed;
    }
  EXPECT_GT(mixed, 0u) << "every 2x2 block holds one colour, which is what "
                          "a magnified plate looks like";
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
  EXPECT_EQ(session->lanes()[3].ms, 0.0);

  useRuntime(world::Runtime::cpu());
  std::unique_ptr<Session> performed = kindOf<Spun>()->open(fonts(), assets());
  performed->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(performed->lanes()[3].ms, 1.0);
  useRuntime({});
}

TEST(SetSession, OffersAViewpointAHostCanTakeHoldOf) {
  // A host reads this to decide whether to offer the control at all,
  // and seeds the control with where the set already stands rather than
  // with a number of its own.
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  EXPECT_TRUE(session->hasViewpoint());
  oneFrame(*session);
  EXPECT_TRUE(session->orbit().has_value());
}

TEST(SetSession, IsSeenFromTheCameraItsOwnTreeCarries) {
  // WHAT A LIVE HOST MUST SHOW before anyone touches it: the set as its
  // plate shows it. The host hands in a fallback camera nowhere near the
  // one the tree carries, and the tree's is what the session reports.
  std::unique_ptr<Session> session = kindOf<Framed>()->open(fonts(), assets());
  oneFrame(*session);

  const camera::Orbit declared = camera::orbitOf(framedLens());
  const std::optional<camera::Orbit> reported = session->orbit();
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
  const camera::Orbit declared = camera::orbitOf(framedLens());

  const SkBitmap standing = oneFrame(*session);
  session->viewpoint(declared.yawDeg, declared.pitchDeg, declared.distance);
  const SkBitmap held = oneFrame(*session);
  EXPECT_TRUE(samePicture(standing, held));
}

TEST(SetSession, ADragMovesItAboutTheTargetItDeclared) {
  std::unique_ptr<Session> session = kindOf<Framed>()->open(fonts(), assets());
  oneFrame(*session);
  const camera::Orbit declared = camera::orbitOf(framedLens());

  const SkBitmap standing = oneFrame(*session);
  session->viewpoint(declared.yawDeg + 90.0f, declared.pitchDeg,
                     declared.distance);
  const SkBitmap moved = oneFrame(*session);
  EXPECT_FALSE(samePicture(standing, moved));
  const std::optional<camera::Orbit> after = session->orbit();
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
}

/** The 3D session's answers to what every session promises. */
struct SetTraits {
  static Kind kind() { return kindOf<Spun>(); }
  static SkSize canvas() { return SkSize::Make(160, 120); }
  static double captureSeconds() { return 0.5; }
  static const char* runtime() { return "set"; }
  static std::vector<const char*> lanes() {
    return {"nodes", "drawn", "cooked", "passes"};
  }
  /** The plate IS the frame just finished, so there is nothing to form
   *  again larger. */
  static float oversample() { return 1.0f; }
  static void reset() { Spun::describes = 0; }
  static int bodyRuns() { return Spun::describes; }
};

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(TheSetSession, SessionContract, SetTraits);
