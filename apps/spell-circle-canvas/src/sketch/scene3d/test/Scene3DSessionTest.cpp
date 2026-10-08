/** @file
 * The 3D session: what a scene3d sketch declares, that stepping it to a moment
 * is a function of the moment alone, and that a scene3d sketch with no device
 * to draw on does not open and says why.
 *
 * Every case that opens a scene3d sketch draws it on the device and SKIPS where
 * the machine has no Vulkan runtime; its suite carries the `gpu` label.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <glm/geometric.hpp>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "support/Devices.h"
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

/** A CASE THAT OPENS A SCENE3D SKETCH: the device runtime installed for the
 *  process before it, as a host installs it, and taken away after it, or the
 *  case skipped naming what is missing. */
class OnDevice : public testing::Test {
 protected:
  void SetUp() override {
    std::string why;
    if (!sigil::sketch::test::installDeviceRuntime(&why))
      GTEST_SKIP() << "no Vulkan device: " << why;
  }
  void TearDown() override { useRuntime({}); }
};

class Scene3DSession : public OnDevice {};
class Scene3DDoors : public OnDevice {};
class Scene3DWindow : public OnDevice {};
class Scene3DBodies : public OnDevice {};
class Scene3DKinds : public OnDevice {};

/** The body every scene3d sketch here turns: a rounded box, whose flat faces
 *  turn to and from the sun as it spins and whose silhouette a rasteriser
 *  with no antialiasing still draws whole. */
gm::Mesh roundedBox() { return gm::superellipsoid({40, 40, 40}, 6.0f, 24, 16); }

/** One box on a turntable, and nothing else. It counts the times its
 *  body was asked to describe, which is the observable behind the
 *  difference between stepping and repainting. */
struct Spun {
  static inline int describes = 0;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0.05f, 0.05f, 0.08f, 1});
    ctx.captureAt(0.5);
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 90, 260};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
  }
  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    ++describes;
    return world::group({world::directionalLight(
                             {-0.4f, -0.8f, -0.3f},
                             {.color = sigil::material::Color{1, 1, 1, 1},
                              .intensity = 1.0f})
                             .key("sun"),
                         world::mesh(roundedBox())
                             .key("body")
                             .rotateY(seconds * 90.0f)
                             .fill(sigil::material::surface::program())})
        .key("set");
  }
};

/** A scene3d sketch that paints a compose sheet into a texture the session
 *  keeps, and wears it on one unlit card facing the camera. */
struct Screened {
  std::shared_ptr<sigil::compose::TextureScene> screen;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
    screen = ctx.textureScene({64, 64});
  }
  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    using namespace sigil::compose;
    screen->render(box().width(64).height(64).fill(Fill::color({0, 1, 0, 1})),
                   seconds);
    sigil::material::Material surface =
        sigil::material::surface::unlit({.baseColor = {1, 1, 1, 1}});
    surface.slot(sigil::material::surface::kBaseColorSlot, screen->texture());
    return world::group(
               {world::mesh(gm::quad(120, 90)).key("card").fill(surface)})
        .key("set");
  }
};

/** A scene3d sketch that paints a compose page as a surface's maps the session
 *  keeps — a lit panel under a flat green label — and wears it on one card
 *  under a sun. */
struct Surfaced {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    return world::Frame(
               world::group({world::directionalLight({0, 0, -1}).key("sun"),
                             world::mesh(gm::quad(120, 90))
                                 .key("card")
                                 .fill(page.surface())})
                   .key("set"))
        .pass(page.render(
            box()
                .width(64)
                .height(64)
                .fill(material::from(material::Color{.6f, .6f, .6f, 1})
                          .surface({.roughness = .9f}))
                .children({box()
                               .absolute()
                               .rect(16, 16, 32, 32)
                               .fill(material::Color{0, 1, 0, 1})})));
  }
};

/** A page of one colour worn on a card under a single lamp standing off
 *  its left edge, so the card's near side takes more light than its far
 *  side: the colour stated flat when @p Glowing is zero, or as the
 *  emission of a black surface when it is one. */
template <int Glowing>
struct SideLitPage {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    const material::Material colour =
        Glowing ? material::from(material::Color{0, 0, 0, 1})
                      .surface({.roughness = 1.0f,
                                .emission = {0.7f, 0.7f, 0.7f, 1},
                                .emissionStrength = 1.0f})
                : material::Material(material::Color{0.7f, 0.7f, 0.7f, 1});
    return world::Frame(
               world::group({world::pointLight({.intensity = world::intensityAt(
                                                    3.0f, 100.0f)})
                                 .key("lamp")
                                 .at({-90, 0, 40}),
                             world::mesh(gm::quad(120, 90))
                                 .key("card")
                                 .fill(page.surface())})
                   .key("set"))
        .pass(page.render(box().width(64).height(64).fill(colour)));
  }
};

/** A page whose one panel states a clear coat of weight @p Coat over a
 *  rough grey base, worn on a card under a lamp standing between the card
 *  and the eye — where a coat mirrors the lamp into a spot and a rough
 *  base spreads it evenly. */
template <int Coat>
struct CoatedPage {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    return world::Frame(
               world::group({world::camera(
                                 lens, {.toneCurve = world::ToneCurve::Neutral})
                                 .key("camera"),
                             world::pointLight({.intensity = world::intensityAt(
                                                    3.0f, 150.0f)})
                                 .key("lamp")
                                 .at({0, 0, 150}),
                             world::mesh(gm::quad(120, 90))
                                 .key("card")
                                 .fill(page.surface())})
                   .key("set"))
        .pass(page.render(box().width(64).height(64).fill(
            material::from(material::Color{.3f, .3f, .35f, 1})
                .surface({.roughness = 1.0f,
                          .clearcoat = (float)Coat,
                          .clearcoatRoughness = .08f}))));
  }
};

/** A page whose one panel states a glossy clear coat over a rough grey
 *  base, worn on a card under a lamp standing between the card and the eye
 *  as `CoatedPage` is — the coat's own normal facing the viewer across the
 *  page's left half and, where @p Turned, leaning hard across its right
 *  half. */
template <bool Turned>
struct CoatNormalPage {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    const material::Color facing{.5f, .5f, 1, 1};
    const material::Color leaning =
        Turned ? material::Color{.9f, .5f, .6f, 1} : facing;
    const material::Material coatNormal = material::linearGradient(
        {0, 0}, {1, 0},
        {{0.0f, facing}, {0.5f, facing}, {0.5f, leaning}, {1.0f, leaning}});
    return world::Frame(
               world::group({world::camera(
                                 lens, {.toneCurve = world::ToneCurve::Neutral})
                                 .key("camera"),
                             world::pointLight({.intensity = world::intensityAt(
                                                    3.0f, 150.0f)})
                                 .key("lamp")
                                 .at({0, 0, 150}),
                             world::mesh(gm::quad(120, 90))
                                 .key("card")
                                 .fill(page.surface())})
                   .key("set"))
        .pass(page.render(box().width(64).height(64).fill(
            material::from(material::Color{.3f, .3f, .35f, 1})
                .surface({.roughness = 1.0f,
                          .clearcoat = 1.0f,
                          .clearcoatRoughness = .08f,
                          .clearcoatNormal = coatNormal}))));
  }
};

/** A page whose middle is a wide metal strip turned @p Turn degrees
 *  clockwise on the page and brushed along itself, worn on a card under a
 *  lamp standing between the card and the eye. */
template <int Turn>
struct BrushedStripPage {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    return world::Frame(
               world::group(
                   {world::camera(lens, {.toneCurve = world::ToneCurve::Linear})
                        .key("camera"),
                    world::pointLight(
                        {.intensity = world::intensityAt(0.5f, 150.0f)})
                        .key("lamp")
                        .at({0, 0, 150}),
                    world::mesh(gm::quad(120, 90))
                        .key("card")
                        .fill(page.surface())})
                   .key("set"))
        .pass(page.render(
            box()
                .width(64)
                .height(64)
                .fill(material::from(material::Color{.05f, .05f, .05f, 1})
                          .surface({.roughness = 1.0f}))
                .children(
                    {box()
                         .absolute()
                         .rect(-28, 12, 120, 40)
                         .rotate((float)Turn)
                         .fill(material::from(material::Color{.9f, .9f, .9f, 1})
                                   .surface(
                                       {.metallic = 1.0f, .roughness = 0.35f})
                                   .brushed({.strength = .9f}))})));
  }
};

/** A PAGE WHOSE CONTENT STATES AN OPTIONAL LOBE, on a card under a lamp
 *  standing between it and the eye: a dielectric whose specular strength is
 *  @p Specular, brushed along the page's width at @p Brushed tenths. */
template <int Specular, int Brushed>
struct LobedPage {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    const bool metal = Brushed > 0;
    return world::Frame(
               world::group(
                   {world::camera(lens, {.toneCurve = world::ToneCurve::Linear})
                        .key("camera"),
                    world::pointLight({.intensity = world::intensityAt(
                                           metal ? 0.5f : 1.0f, 150.0f)})
                        .key("lamp")
                        .at({0, 0, 150}),
                    world::mesh(gm::quad(120, 90))
                        .key("card")
                        .fill(page.surface())})
                   .key("set"))
        .pass(page.render(box().width(64).height(64).fill(
            material::from(metal ? material::Color{.9f, .9f, .9f, 1}
                                 : material::Color{.3f, .3f, .35f, 1})
                .surface({.metallic = metal ? 1.0f : 0.0f,
                          .roughness = 0.35f,
                          .anisotropy = Brushed / 10.0f,
                          .specular = (float)Specular}))));
  }
};

/** THE CAMERA A SCENE3D SKETCH DECLARES IN ITS OWN TREE — nowhere near the
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

/** One frame of @p session onto a plate of the declared canvas size. */
SkBitmap oneFrame(Session& session) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  session.frame(canvas, 1.0 / 60.0);
  return bitmap;
}

/** ONE FRAME AS ITS SURFACES SHADE IT. A live session draws a body as a
 *  stand-in while its program compiles on a worker, so the same moment
 *  is drawn again once every compile is done, and what is read is the
 *  surface and not its stand-in. It is drawn again whether or not a
 *  compile is still running when the first frame returns: one that
 *  finished while that frame was drawing left its stand-in in the
 *  picture all the same. */
SkBitmap shadedFrame(Session& session) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  session.frame(canvas, 1.0 / 60.0);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (sigil::material::ProgramCache::shared().compiling() > 0 &&
         std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  session.frame(canvas, 0.0);
  return bitmap;
}

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
  EXPECT_NEAR(reported->yawDeg, declared.yawDeg, 1e-2f);
  EXPECT_NEAR(reported->pitchDeg, declared.pitchDeg, 1e-2f);
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
  session->viewpoint(declared.yawDeg, declared.pitchDeg, declared.distance);
  const SkBitmap held = oneFrame(*session);
  EXPECT_TRUE(samePicture(standing, held));
}

TEST_F(Scene3DSession, ADragMovesItAboutTheTargetItDeclared) {
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

TEST_F(Scene3DDoors, PaintsATextureSceneOntoABody) {
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

/** The screen the last `Screened` session made, so a case can ask where
 *  it stands. */
std::weak_ptr<sigil::compose::TextureScene>& lastScreen() {
  static std::weak_ptr<sigil::compose::TextureScene> screen;
  return screen;
}

/** `Screened`, telling the case which screen it was handed. */
struct ScreenedAndSeen : Screened {
  void setup(Scene3DContext& ctx) {
    Screened::setup(ctx);
    lastScreen() = screen;
  }
};

TEST_F(Scene3DDoors, ALiveScene3DPaintsItsScreensOnTheDeviceItIsDrawnOn) {
  // On the CPU every material a screen or a page paints is interpreted
  // pixel by pixel, which for a page of material is seconds a frame; a
  // live scene3d sketch's screens stand on the device it draws on, and a
  // capture that will be diffed keeps the CPU's pixels.
  const sigil::geometry::device::test::OnDevice on =
      sigil::geometry::device::test::onDevice();
  ASSERT_TRUE(on);
  useDevice(on.device);
  for (const bool deterministic : {false, true}) {
    std::unique_ptr<Session> session =
        kindOf<ScreenedAndSeen>()->open(fonts(), assets(), deterministic);
    const SkBitmap picture = oneFrame(*session);
    const std::shared_ptr<sigil::compose::TextureScene> screen =
        lastScreen().lock();
    ASSERT_NE(screen, nullptr);
    EXPECT_EQ((bool)screen->deviceImage(), !deterministic)
        << (deterministic ? "a capture" : "a window");
    const SkColor centre = picture.getColor(80, 60);
    EXPECT_GT(SkColorGetG(centre), 200u);
    EXPECT_LT(SkColorGetR(centre), 40u);
  }
  useDevice(nullptr);
}

TEST_F(Scene3DDoors, PaintsASurfaceSceneThatTheScene3DLights) {
  std::unique_ptr<Session> session =
      kindOf<Surfaced>()->open(fonts(), assets());
  const SkBitmap picture = shadedFrame(*session);
  // The flat label takes the sun as the page's base colour and comes out
  // green; the lit panel around it is grey under the same sun.
  const SkColor label = picture.getColor(80, 60);
  EXPECT_GT(SkColorGetG(label), 100u);
  EXPECT_GT(SkColorGetG(label), SkColorGetR(label) + 60);
  EXPECT_GT(SkColorGetG(label), SkColorGetB(label) + 60);
  const SkColor panel = picture.getColor(48, 60);
  EXPECT_GT(SkColorGetR(panel), 15u);
  EXPECT_NEAR(int(SkColorGetR(panel)), int(SkColorGetG(panel)), 12);
}

TEST_F(Scene3DDoors, AFlatColourOnAPageTakesTheLightAndOnlyEmissionGlows) {
  const auto light = [](SkColor colour) {
    return (int)SkColorGetR(colour) + (int)SkColorGetG(colour) +
           (int)SkColorGetB(colour);
  };
  // A colour that states no surface is the page's base colour: the side of
  // the card nearer the lamp reads lighter than the far side.
  std::unique_ptr<Session> flat =
      kindOf<SideLitPage<0>>()->open(fonts(), assets());
  const SkBitmap lit = shadedFrame(*flat);
  EXPECT_GT(light(lit.getColor(40, 60)), light(lit.getColor(120, 60)) + 30);
  // The same colour stated as emission glows alike on both sides.
  std::unique_ptr<Session> glowing =
      kindOf<SideLitPage<1>>()->open(fonts(), assets());
  const SkBitmap glow = shadedFrame(*glowing);
  EXPECT_GT(light(glow.getColor(120, 60)), 90);
  EXPECT_NEAR(light(glow.getColor(40, 60)), light(glow.getColor(120, 60)), 12);
}

TEST_F(Scene3DDoors, APagesClearcoatReachesTheShaderThroughItsSurface) {
  std::unique_ptr<Session> coated =
      kindOf<CoatedPage<1>>()->open(fonts(), assets());
  std::unique_ptr<Session> bare =
      kindOf<CoatedPage<0>>()->open(fonts(), assets());
  const SkBitmap withCoat = shadedFrame(*coated);
  const SkBitmap without = shadedFrame(*bare);
  // The coat the page painted mirrors the lamp where the reflection meets
  // the eye; the bare page shows only its rough base there.
  const SkColor4f centreCoated = withCoat.getColor4f(80, 60);
  const SkColor4f centreBare = without.getColor4f(80, 60);
  EXPECT_GT(centreCoated.fG, centreBare.fG + 0.15f);
  // A little way off the spot the two pages are nearly alike.
  EXPECT_NEAR(withCoat.getColor4f(40, 60).fG, without.getColor4f(40, 60).fG,
              0.08f);
}

TEST_F(Scene3DDoors, APagesCoatNormalReachesTheShaderThroughItsSurface) {
  const SkBitmap level =
      shadedFrame(*kindOf<CoatNormalPage<false>>()->open(fonts(), assets()));
  const SkBitmap turned =
      shadedFrame(*kindOf<CoatNormalPage<true>>()->open(fonts(), assets()));
  // A coat facing the viewer everywhere mirrors the lamp into one small
  // round spot at the card's middle, as bright a step to either side.
  EXPECT_NEAR(level.getColor4f(80 - 2, 60).fG, level.getColor4f(80 + 2, 60).fG,
              0.06f);
  // Where the coat's normal turns, the reflection breaks: the left half of
  // the spot stands as it was and the right half has left the eye.
  EXPECT_NEAR(turned.getColor4f(80 - 2, 60).fG, level.getColor4f(80 - 2, 60).fG,
              0.03f);
  EXPECT_LT(turned.getColor4f(80 + 2, 60).fG,
            level.getColor4f(80 + 2, 60).fG - 0.5f);
}

TEST_F(Scene3DDoors, APagesSpecularReachesTheShaderThroughItsSurface) {
  // The page learns its content states a specular while it paints, after
  // the frame was described; the first frame already shades with it.
  const SkBitmap stock =
      shadedFrame(*kindOf<LobedPage<1, 0>>()->open(fonts(), assets()));
  const SkBitmap none =
      shadedFrame(*kindOf<LobedPage<0, 0>>()->open(fonts(), assets()));
  EXPECT_GT(stock.getColor4f(80, 60).fG, none.getColor4f(80, 60).fG + 0.1f)
      << "with no specular the page's dielectric shows no highlight";
}

TEST_F(Scene3DDoors, APagesBrushingReachesTheShaderThroughItsSurface) {
  const SkBitmap round =
      shadedFrame(*kindOf<LobedPage<1, 1>>()->open(fonts(), assets()));
  const SkBitmap brushed =
      shadedFrame(*kindOf<LobedPage<1, 9>>()->open(fonts(), assets()));
  // Barely brushed, the highlight is nearly round; brushed hard, it runs
  // out along the page's width and not up it.
  EXPECT_NEAR(round.getColor4f(80 + 18, 60).fG,
              round.getColor4f(80, 60 + 18).fG, 0.03f);
  const float along = brushed.getColor4f(80 + 18, 60).fG;
  const float across = brushed.getColor4f(80, 60 + 18).fG;
  EXPECT_GT(along, across + 0.05f);
}

TEST_F(Scene3DDoors, ABrushedStripStretchesItsHighlightAlongTheStrip) {
  const SkBitmap down =
      shadedFrame(*kindOf<BrushedStripPage<45>>()->open(fonts(), assets()));
  const SkBitmap up =
      shadedFrame(*kindOf<BrushedStripPage<-45>>()->open(fonts(), assets()));
  // The page's y runs down the picture, so a strip turned clockwise on
  // the page runs down to the right in it. Its brushing runs along it: a
  // step down that diagonal stays in the highlight and the same step
  // across it, still on the strip, falls out of it. Turned the other way,
  // the strip and its highlight run up to the right. This holds only
  // where the direction map's green is read up the page end to end.
  for (const int step : {8, 12}) {
    EXPECT_GT(down.getColor4f(80 + step, 60 + step).fG,
              down.getColor4f(80 + step, 60 - step).fG + 0.12f)
        << "step " << step;
    EXPECT_GT(up.getColor4f(80 + step, 60 - step).fG,
              up.getColor4f(80 + step, 60 + step).fG + 0.12f)
        << "step " << step;
  }
}

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

/** A SCENE3D SKETCH THAT SPELLS NOTHING BUT ITS FRAME: no `setup`, and
 *  therefore no plate, no ground and no viewpoint of its own. */
struct Bare {
  static inline int describes = 0;
  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    ++describes;
    return world::group({world::directionalLight(
                             {-0.4f, -0.8f, -0.3f},
                             {.color = sigil::material::Color{1, 1, 1, 1},
                              .intensity = 1.0f})
                             .key("sun"),
                         world::mesh(roundedBox())
                             .key("body")
                             .rotateY(seconds * 90.0f)
                             .fill(sigil::material::surface::program())})
        .key("set");
  }
};

/** …and one that spelled the frame some other way, which is what the
 *  registration has to refuse. */
struct Misspelled {
  void describeIt(float) {}
};

TEST_F(Scene3DBodies, AScene3DIsAnyTypeThatNamesDescribe) {
  // The concept is what `kindOf` asserts on, so a member spelled some
  // other way is a compile error naming the signatures a scene3d sketch may
  // have. Nothing in this tree compiles a translation unit that must NOT build
  // — the documentation probes are the one generated compile check and
  // they assert that a name EXISTS — so what a case can hold is the
  // answer the assertion reads.
  static_assert(Scene3DSketch<Spun>);
  static_assert(Scene3DSketch<Bare>);
  static_assert(!Scene3DSketch<Misspelled>);

  Bare::describes = 0;
  std::unique_ptr<Session> session = kindOf<Bare>()->open(fonts(), assets());
  ASSERT_NE(session, nullptr);
  // A scene3d sketch that declared no plate is opened onto the one every
  // scene3d session starts with, and describing is all it is asked for.
  EXPECT_EQ(session->canvas().size, SkSize::Make(900, 640));
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(90, 64));
  SkCanvas canvas(bitmap);
  session->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(Bare::describes, 1);
}

TEST_F(Scene3DDoors, OpensAnOwnedBodyWithNoKindBehindIt) {
  // The door a host whose scene3d sketch is not a C++ type comes in by: it
  // holds the body itself and names the runtime, and what comes back is the
  // session the registered kind would have opened.
  Spun::describes = 0;
  std::unique_ptr<Session> owned =
      openScene3D(std::make_unique<Scene3DBodyOf<Spun>>(), fonts(), assets(),
                  false, sigil::sketch::runtime());
  ASSERT_NE(owned, nullptr);
  EXPECT_EQ(owned->canvas().size, SkSize::Make(160, 120));
  // Shaded, not first frames: a live session stands a body in while its
  // program compiles, and which of the two sessions meets a compile still
  // under way is a race the picture must not depend on.
  const SkBitmap throughTheDoor = shadedFrame(*owned);
  EXPECT_GE(Spun::describes, 1);

  Spun::describes = 0;
  std::unique_ptr<Session> registered = kindOf<Spun>()->open(fonts(), assets());
  ASSERT_NE(registered, nullptr);
  EXPECT_TRUE(samePicture(throughTheDoor, shadedFrame(*registered)));
}

/** A supplier standing where the address of a factory function would:
 *  what a host whose scene3d sketches are made somewhere else registers. */
class SpunSource final : public Scene3DBodySource {
 public:
  std::unique_ptr<Scene3DBody> open() const override {
    return std::make_unique<Scene3DBodyOf<Spun>>();
  }
};

TEST_F(Scene3DKinds, ASupplierOpensTheBodiesWhereNoFactoryFunctionCan) {
  const auto source = std::make_shared<const SpunSource>();
  const Kind supplied = Scene3DKind{source};

  // Two kinds are the same kind when they open the same body: the same
  // supplier is one kind, a second supplier of the same type is
  // another, and neither is the kind a factory function makes.
  EXPECT_TRUE(supplied == Kind{Scene3DKind{source}});
  EXPECT_FALSE(supplied ==
               Kind{Scene3DKind{std::make_shared<const SpunSource>()}});
  EXPECT_FALSE(supplied == kindOf<Spun>());
  EXPECT_EQ(supplied->runtime(), "scene3d");

  Spun::describes = 0;
  std::unique_ptr<Session> session = supplied->open(fonts(), assets());
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(session->canvas().size, SkSize::Make(160, 120));
  oneFrame(*session);
  EXPECT_EQ(Spun::describes, 1);
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
    return {"nodes", "drawn", "cooked", "passes"};
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
