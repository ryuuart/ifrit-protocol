/** @file
 * The 3D session: that stepping a scene3d sketch to a moment is a function of
 * the moment alone, the resolution a fitted canvas is formed at, the
 * viewpoint a host takes hold of, what every session promises, and that a
 * scene3d sketch with no device to draw on does not open and says why.
 *
 * Every case that opens a scene3d sketch draws it on the device and SKIPS where
 * the machine has no Vulkan runtime; its suite carries the `gpu` label.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Scene3DTestSupport.h"
#include "support/Fixtures.h"
#include "support/Pixels.h"
#include "support/Sessions.h"

namespace {

using namespace sigil::sketch;
namespace world = sigil::world;
namespace camera = sigil::geometry::mesh::camera;
using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::sketch::test::inkBounds;
using sigil::sketch::test::oneFrame;
using sigil::sketch::test::roundedBox;
using sigil::sketch::test::samePicture;
using sigil::sketch::test::Scene3DSession;
using sigil::sketch::test::Spun;

/** THE CAMERA A SCENE3D SKETCH DECLARES IN ITS OWN TREE — nowhere near the
 *  fallback its host hands in, and looking at a point that is not the
 *  origin, so that a viewpoint pivoting on the wrong one of the two is
 *  visible. */
sigil::geometry::mesh::camera::Camera framedLens() {
  sigil::geometry::mesh::camera::Camera lens;
  lens.eye = {180, 150, 320};
  lens.target = {40, 30, -10};
  lens.fovYDegrees = 52;
  return lens;
}

/** One box seen from a camera the TREE carries. */
struct Framed {
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0.05f, 0.05f, 0.08f, 1});
    sigil::geometry::mesh::camera::Camera fallback;
    fallback.eye = {0, 0, 900};
    ctx.camera(fallback);
  }
  world::Frame describe(sigil::motion::Duration) {
    return world::group({world::directionalLight(
                             {-0.4f, -0.8f, -0.3f},
                             {.color = sigil::material::Color{1, 1, 1, 1},
                              .intensity = 1.0f})
                             .key("sun"),
                         world::camera(framedLens()).key("lens"),
                         world::mesh(roundedBox())
                             .key("body")
                             .at({40, 30, -10})
                             .fill(sigil::material::surface::program())})
        .key("set");
  }
};

/** The sketch, stepped @p frames from zero onto a canvas @p scale times its
 *  declared size and scaled to match — which is what a host that fitted
 *  the sketch into a window on a scaled screen hands over. */
SkBitmap steppedOnto(float scale, int frames, bool capture = false) {
  std::unique_ptr<Session> session =
      kindOf<Spun>()->open(fonts(), assets(), capture);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul((int)std::lround(160 * scale),
                                                (int)std::lround(120 * scale)));
  SkCanvas canvas(bitmap);
  canvas.scale(scale, scale);
  for (int f = 0; f < frames; ++f) session->frame(canvas, 1.0 / 60.0);
  return bitmap;
}

SkBitmap steppedTo(int frames) { return steppedOnto(1.0f, frames); }

TEST_F(Scene3DSession, TheSameMomentIsTheSamePicture) {
  // The plate contract: a scene3d sketch is a pure function of the scene time,
  // so two runs that step the same number of frames agree on every byte.
  EXPECT_TRUE(samePicture(steppedTo(30), steppedTo(30)));
}

TEST_F(Scene3DSession, ADifferentMomentIsADifferentPicture) {
  EXPECT_FALSE(samePicture(steppedTo(6), steppedTo(30)));
}

TEST_F(Scene3DSession, AFittedCanvasPutsThePictureInTheSamePlace) {
  // A live host hands over a canvas ALREADY FITTED: the sketch's own
  // canvas scaled up to the window it was letterboxed into, on a screen
  // that may have two pixels for every one of those. A scene3d sketch whose
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

/** How many 2x2 blocks of @p picture hold more than one colour: a
 *  picture magnified twice from a smaller one has none, since every block
 *  came from a single source pixel. */
size_t mixedBlocks(const SkBitmap& picture) {
  size_t mixed = 0;
  for (int y = 0; y + 1 < picture.height(); y += 2)
    for (int x = 0; x + 1 < picture.width(); x += 2) {
      const SkColor first = picture.getColor(x, y);
      if (picture.getColor(x + 1, y) != first ||
          picture.getColor(x, y + 1) != first ||
          picture.getColor(x + 1, y + 1) != first)
        ++mixed;
    }
  return mixed;
}

TEST_F(Scene3DSession, AFittedCaptureIsFormedAtItsOwnResolution) {
  // A scene3d sketch is formed at ONE resolution, so a capture is formed at
  // the canvas's: one formed at the declared size and then magnified onto
  // a canvas twice as wide is a picture of a smaller one. What separates
  // the two is inside the outline rather than around it — a magnified
  // plate is one where every 2x2 of pixels came from a single source
  // pixel, so not one of them holds two colours.
  const SkBitmap fitted = steppedOnto(2.0f, 30, true);
  ASSERT_EQ(fitted.width(), 320);
  ASSERT_EQ(fitted.height(), 240);
  EXPECT_GT(mixedBlocks(fitted), 0u)
      << "every 2x2 block holds one colour, which is what a magnified "
         "plate looks like";
}

TEST_F(Scene3DSession, AFittedLiveSessionIsFormedAtItsDeclaredResolution) {
  // A live window forms no more pixels than the sketch declared and
  // magnifies the picture, so on a canvas twice as wide the same scene
  // is the declared picture doubled: every 2x2 block is one colour.
  const SkBitmap fitted = steppedOnto(2.0f, 30, false);
  ASSERT_EQ(fitted.width(), 320);
  ASSERT_EQ(fitted.height(), 240);
  EXPECT_EQ(mixedBlocks(fitted), 0u)
      << "a live session formed more pixels than its canvas declares";
}

TEST_F(Scene3DSession, DrawsThroughOnePassWhenTheScene3DDeclaresNone) {
  // The runtime is a property of the PROCESS, not of a sketch: a host
  // that brought one up says so once, and every session then reaches it.
  // An executor is only reached THROUGH passes, so a scene3d sketch about the
  // scene is given one geometry pass clearing to its background.
  std::unique_ptr<Session> performed = kindOf<Spun>()->open(fonts(), assets());
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  performed->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(performed->lanes()[3].ms, 1.0);
}

TEST_F(Scene3DSession, OffersAViewpointAHostCanTakeHoldOf) {
  // A host reads this to decide whether to offer the control at all,
  // and seeds the control with where the sketch already stands rather than
  // with a number of its own.
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  EXPECT_TRUE(session->hasViewpoint());
  oneFrame(*session);
  EXPECT_TRUE(session->orbit().has_value());
}

TEST_F(Scene3DSession, IsSeenFromTheCameraItsOwnTreeCarries) {
  // WHAT A LIVE HOST MUST SHOW before anyone touches it: the sketch as its
  // plate shows it. The host hands in a fallback camera nowhere near the
  // one the tree carries, and the tree's is what the session reports.
  std::unique_ptr<Session> session = kindOf<Framed>()->open(fonts(), assets());
  oneFrame(*session);

  const camera::Orbit declared = camera::orbitOf(framedLens());
  const std::optional<camera::Orbit> reported = session->orbit();
  ASSERT_TRUE(reported.has_value());
  EXPECT_NEAR(reported->yawDegrees, declared.yawDegrees, 1e-2f);
  EXPECT_NEAR(reported->pitchDegrees, declared.pitchDegrees, 1e-2f);
  EXPECT_NEAR(reported->distance, declared.distance, 1e-2f);
}

TEST_F(Scene3DSession, ADragThatMovesItByNothingChangesNothing) {
  // Which is what says the orbit pivots on the declared camera's own
  // target and stands at its own distance, rather than on a point and a
  // distance of the host's.
  std::unique_ptr<Session> session = kindOf<Framed>()->open(fonts(), assets());
  oneFrame(*session);
  const camera::Orbit declared = camera::orbitOf(framedLens());

  const SkBitmap standing = oneFrame(*session);
  session->viewpoint(declared.yawDegrees, declared.pitchDegrees,
                     declared.distance);
  const SkBitmap held = oneFrame(*session);
  EXPECT_TRUE(samePicture(standing, held));
}

TEST_F(Scene3DSession, ADragMovesItAboutTheTargetItDeclared) {
  std::unique_ptr<Session> session = kindOf<Framed>()->open(fonts(), assets());
  oneFrame(*session);
  const camera::Orbit declared = camera::orbitOf(framedLens());

  const SkBitmap standing = oneFrame(*session);
  session->viewpoint(declared.yawDegrees + 90.0f, declared.pitchDegrees,
                     declared.distance);
  const SkBitmap moved = oneFrame(*session);
  EXPECT_FALSE(samePicture(standing, moved));
  const std::optional<camera::Orbit> after = session->orbit();
  ASSERT_TRUE(after.has_value());
  EXPECT_NEAR(after->distance, declared.distance, 1e-2f);
  EXPECT_NEAR(after->pitchDegrees, declared.pitchDegrees, 1e-2f);
}

TEST(Scene3DAvailability, AScene3DWithNoDeviceToDrawOnIsUnavailableAndSaysWhy) {
  // ONE MESSAGE at every door: the kind a listing asks, the registry
  // entry a sweep asks, the open a host attempts and the probe a scene2d
  // sketch baking a 3D frame declares all say the same thing, and no body's
  // setup runs.
  useRuntime({});
  std::string why;
  EXPECT_FALSE(runtimeAvailable(&why));
  EXPECT_FALSE(why.empty());
  std::string kindWhy;
  EXPECT_FALSE(kindOf<Spun>()->available(&kindWhy));
  EXPECT_EQ(kindWhy, why);
  Spun::describes = 0;
  try {
    (void)kindOf<Spun>()->open(fonts(), assets());
    ADD_FAILURE() << "a scene3d sketch opened with no device to draw on";
  } catch (const std::runtime_error& error) {
    EXPECT_EQ(std::string(error.what()), why);
  }
  EXPECT_EQ(Spun::describes, 0);
  // A kind stated on no device is unavailable whatever the process holds,
  // which is what a still taken away from the device asks for.
  std::string installed;
  if (sigil::sketch::test::installDeviceRuntime(&installed)) {
    EXPECT_TRUE(kindOf<Spun>()->available(nullptr));
    EXPECT_FALSE(onRuntime(kindOf<Spun>(), {})->available(nullptr));
    useRuntime({});
  }
}

/** The 3D session's answers to what every session promises. */
struct Scene3DTraits {
  /** A scene3d sketch is drawn on a device: the contract installs the device
   *  runtime before each case and takes it away after. */
  static bool prepare(std::string* why) {
    return sigil::sketch::test::installDeviceRuntime(why);
  }
  static void release() { useRuntime({}); }
  static Kind kind() { return kindOf<Spun>(); }
  static SkSize canvas() { return SkSize::Make(160, 120); }
  static double captureSeconds() { return 0.5; }
  static const char* runtime() { return "scene3d"; }
  static std::vector<const char*> lanes() {
    return {"nodes", "drawn", "cooked", "passes", "pending"};
  }
  /** The plate IS the frame just finished, so there is nothing to form
   *  again larger. */
  static float oversample() { return 1.0f; }
  static void reset() { Spun::describes = 0; }
  static int bodyRuns() { return Spun::describes; }
};

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(TheScene3DSession, SessionContract,
                               Scene3DTraits);
