/** @file
 * The 3D session: what a set declares, that stepping it to a moment is a
 * function of the moment alone, and that a set with no device to draw on
 * does not open and says why.
 *
 * Every case that opens a set draws it on the device and SKIPS where the
 * machine has no Vulkan runtime; its suite carries the `gpu` label.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/World.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <glm/geometric.hpp>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
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

/** A CASE THAT OPENS A SET: the device runtime installed for the process
 *  before it, as a host installs it, and taken away after it, or the case
 *  skipped naming what is missing. */
class OnDevice : public testing::Test {
 protected:
  void SetUp() override {
    std::string why;
    if (!sigil::sketch::test::installDeviceRuntime(&why))
      GTEST_SKIP() << "no Vulkan device: " << why;
  }
  void TearDown() override { useRuntime({}); }
};

class SetSession : public OnDevice {};
class SetDoors : public OnDevice {};
class SetWindow : public OnDevice {};
class SetBodies : public OnDevice {};
class SetKinds : public OnDevice {};

/** The body every set here turns: a rounded box, whose flat faces turn
 *  to and from the sun as it spins and whose silhouette a rasteriser
 *  with no antialiasing still draws whole. */
gm::Mesh roundedBox() { return gm::superellipsoid({40, 40, 40}, 6.0f, 24, 16); }

/** One box on a turntable, and nothing else. It counts the times its
 *  body was asked to describe, which is the observable behind the
 *  difference between stepping and repainting. */
struct Spun {
  static inline int describes = 0;
  void setup(SetContext& ctx) {
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

/** A set that paints a compose sheet into a texture the session keeps,
 *  and wears it on one unlit card facing the camera. */
struct Screened {
  std::shared_ptr<sigil::compose::TextureScene> screen;
  void setup(SetContext& ctx) {
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

/** A set that paints a compose page as a surface's maps the session keeps
 *  — a lit panel under an unlit green label — and wears it on one card
 *  under a sun. */
struct Surfaced {
  Page page;
  void setup(SetContext& ctx) {
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

/** A page whose one panel states a clear coat of weight @p Coat over a
 *  rough grey base, worn on a card under a lamp standing between the card
 *  and the eye — where a coat mirrors the lamp into a spot and a rough
 *  base spreads it evenly. */
template <int Coat>
struct CoatedPage {
  Page page;
  void setup(SetContext& ctx) {
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
struct Framed {
  void setup(SetContext& ctx) {
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

TEST_F(SetSession, TheSameMomentIsTheSamePicture) {
  // The plate contract: a set is a pure function of the scene time, so
  // two runs that step the same number of frames agree on every byte.
  EXPECT_TRUE(samePicture(steppedTo(30), steppedTo(30)));
}

TEST_F(SetSession, ADifferentMomentIsADifferentPicture) {
  EXPECT_FALSE(samePicture(steppedTo(6), steppedTo(30)));
}

TEST_F(SetSession, AFittedCanvasPutsThePictureInTheSamePlace) {
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

TEST_F(SetSession, AFittedCanvasIsFormedAtItsOwnResolution) {
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

TEST_F(SetSession, DrawsThroughOnePassWhenTheSetDeclaresNone) {
  // The runtime is a property of the PROCESS, not of a sketch: a host
  // that brought one up says so once, and every session then reaches it.
  // An executor is only reached THROUGH passes, so a set about the scene
  // is given one geometry pass clearing to its background.
  std::unique_ptr<Session> performed = kindOf<Spun>()->open(fonts(), assets());
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  performed->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(performed->lanes()[3].ms, 1.0);
}

TEST_F(SetSession, OffersAViewpointAHostCanTakeHoldOf) {
  // A host reads this to decide whether to offer the control at all,
  // and seeds the control with where the set already stands rather than
  // with a number of its own.
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  EXPECT_TRUE(session->hasViewpoint());
  oneFrame(*session);
  EXPECT_TRUE(session->orbit().has_value());
}

TEST_F(SetSession, IsSeenFromTheCameraItsOwnTreeCarries) {
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

TEST_F(SetSession, ADragThatMovesItByNothingChangesNothing) {
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

TEST_F(SetSession, ADragMovesItAboutTheTargetItDeclared) {
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

TEST_F(SetDoors, PaintsATextureSceneOntoABody) {
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

TEST_F(SetDoors, PaintsASurfaceSceneThatTheSetLights) {
  std::unique_ptr<Session> session =
      kindOf<Surfaced>()->open(fonts(), assets());
  const SkBitmap picture = oneFrame(*session);
  // The unlit label comes out as it was painted, its own light; the lit
  // panel around it is grey under the sun.
  const SkColor label = picture.getColor(80, 60);
  EXPECT_GT(SkColorGetG(label), 200u);
  EXPECT_LT(SkColorGetR(label), 50u);
  const SkColor panel = picture.getColor(48, 60);
  EXPECT_GT(SkColorGetR(panel), 15u);
  EXPECT_NEAR(int(SkColorGetR(panel)), int(SkColorGetG(panel)), 12);
}

TEST_F(SetDoors, APagesClearcoatReachesTheShaderThroughItsSurface) {
  std::unique_ptr<Session> coated =
      kindOf<CoatedPage<1>>()->open(fonts(), assets());
  std::unique_ptr<Session> bare =
      kindOf<CoatedPage<0>>()->open(fonts(), assets());
  const SkBitmap withCoat = oneFrame(*coated);
  const SkBitmap without = oneFrame(*bare);
  // The coat the page painted mirrors the lamp where the reflection meets
  // the eye; the bare page shows only its rough base there.
  const SkColor4f centreCoated = withCoat.getColor4f(80, 60);
  const SkColor4f centreBare = without.getColor4f(80, 60);
  EXPECT_GT(centreCoated.fG, centreBare.fG + 0.15f);
  // A little way off the spot the two pages are nearly alike.
  EXPECT_NEAR(withCoat.getColor4f(40, 60).fG, without.getColor4f(40, 60).fG,
              0.08f);
}

/** A body on a turntable, performed through a pass the set declares
 *  itself and asked back, so a case can read how many pixels the frame
 *  was formed at — which is what a host zooming into a piece of it pays
 *  for. */
struct Measured {
  static inline glm::ivec2 formed{0, 0};
  void setup(SetContext& ctx) {
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

TEST_F(SetWindow, AZoomedPaneFormsOnlyThePanesPixels) {
  // A set is formed at the resolution it is seen at, so at 4x the
  // whole canvas would be 640x480 — sixteen times the pane. What is
  // formed is the part the pane shows, and no larger than the pane.
  Measured::formed = {0, 0};
  std::unique_ptr<Session> session =
      kindOf<Measured>()->open(fonts(), assets());
  // Two frames: a readback is handed over the frame after it was made.
  throughThePane(*session, 2);
  EXPECT_EQ(Measured::formed, glm::ivec2(160, 120));
}

TEST_F(SetWindow, APaneShowsWhatTheWholeCanvasShowsThere) {
  // The part is the whole picture's projection carried off-centre, so
  // each pixel of it is the pixel the whole canvas at 4x holds there.
  // The two projections are the same numbers composed differently, so a
  // pixel may land a rounding apart on an edge: a channel may move by a
  // level or two, and a pixel whose centre stands on a triangle's edge
  // may fall to the other side of it — never more than a sliver of the
  // pane.
  std::unique_ptr<Session> zoomed = kindOf<Measured>()->open(fonts(), assets());
  const SkBitmap pane = throughThePane(*zoomed, 30);

  std::unique_ptr<Session> whole = kindOf<Measured>()->open(fonts(), assets());
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

TEST_F(SetWindow, AWholeCanvasClipFormsTheWholeCanvas) {
  // A plate's clip is the whole canvas, and what it forms is the
  // declared canvas in the plate's pixels — the frame a set with no
  // part in it forms, unmoved.
  Measured::formed = {0, 0};
  std::unique_ptr<Session> declared =
      kindOf<Measured>()->open(fonts(), assets());
  oneFrame(*declared);
  oneFrame(*declared);
  EXPECT_EQ(Measured::formed, glm::ivec2(160, 120));

  Measured::formed = {0, 0};
  std::unique_ptr<Session> fitted = kindOf<Measured>()->open(fonts(), assets());
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(640, 480));
  SkCanvas canvas(bitmap);
  canvas.scale(kZoom, kZoom);
  fitted->frame(canvas, 1.0 / 60.0);
  fitted->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(Measured::formed, glm::ivec2(640, 480));
}

TEST_F(SetWindow, AStillAfterAZoomedFrameShowsTheWholePicture) {
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

/** A SET THAT SPELLS NOTHING BUT ITS FRAME: no `setup`, and therefore no
 *  plate, no ground and no viewpoint of its own. */
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

TEST_F(SetBodies, ASetIsAnyTypeThatNamesDescribe) {
  // The concept is what `kindOf` asserts on, so a member spelled some
  // other way is a compile error naming the signatures a set may have.
  // Nothing in this tree compiles a translation unit that must NOT build
  // — the documentation probes are the one generated compile check and
  // they assert that a name EXISTS — so what a case can hold is the
  // answer the assertion reads.
  static_assert(SetSketch<Spun>);
  static_assert(SetSketch<Bare>);
  static_assert(!SetSketch<Misspelled>);

  Bare::describes = 0;
  std::unique_ptr<Session> session = kindOf<Bare>()->open(fonts(), assets());
  ASSERT_NE(session, nullptr);
  // A set that declared no plate is opened onto the one every set
  // session starts with, and describing is all it is asked for.
  EXPECT_EQ(session->canvas().size, SkSize::Make(900, 640));
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(90, 64));
  SkCanvas canvas(bitmap);
  session->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(Bare::describes, 1);
}

TEST_F(SetDoors, OpensAnOwnedBodyWithNoKindBehindIt) {
  // The door a host whose set is not a C++ type comes in by: it holds
  // the body itself and names the runtime, and what comes back is the
  // session the registered kind would have opened.
  Spun::describes = 0;
  std::unique_ptr<Session> owned =
      openSet(std::make_unique<SetBodyOf<Spun>>(), fonts(), assets(), false,
              sigil::sketch::runtime());
  ASSERT_NE(owned, nullptr);
  EXPECT_EQ(owned->canvas().size, SkSize::Make(160, 120));
  const SkBitmap throughTheDoor = oneFrame(*owned);
  EXPECT_EQ(Spun::describes, 1);

  Spun::describes = 0;
  std::unique_ptr<Session> registered = kindOf<Spun>()->open(fonts(), assets());
  ASSERT_NE(registered, nullptr);
  EXPECT_TRUE(samePicture(throughTheDoor, oneFrame(*registered)));
}

/** A supplier standing where the address of a factory function would:
 *  what a host whose sets are made somewhere else registers. */
class SpunSource final : public SetBodySource {
 public:
  std::unique_ptr<SetBody> open() const override {
    return std::make_unique<SetBodyOf<Spun>>();
  }
};

TEST_F(SetKinds, ASupplierOpensTheBodiesWhereNoFactoryFunctionCan) {
  const auto source = std::make_shared<const SpunSource>();
  const Kind supplied = SetKind{source};

  // Two kinds are the same kind when they open the same body: the same
  // supplier is one kind, a second supplier of the same type is
  // another, and neither is the kind a factory function makes.
  EXPECT_TRUE(supplied == Kind{SetKind{source}});
  EXPECT_FALSE(supplied == Kind{SetKind{std::make_shared<const SpunSource>()}});
  EXPECT_FALSE(supplied == kindOf<Spun>());
  EXPECT_EQ(supplied->runtime(), "set");

  Spun::describes = 0;
  std::unique_ptr<Session> session = supplied->open(fonts(), assets());
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(session->canvas().size, SkSize::Make(160, 120));
  oneFrame(*session);
  EXPECT_EQ(Spun::describes, 1);
}

TEST(SetAvailability, ASetWithNoDeviceToDrawOnIsUnavailableAndSaysWhy) {
  // ONE MESSAGE at every door: the kind a listing asks, the registry
  // entry a sweep asks, the open a host attempts and the probe a canvas
  // sketch baking a set declares all say the same thing, and no body's
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
    ADD_FAILURE() << "a set opened with no device to draw on";
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
struct SetTraits {
  /** A set is drawn on a device: the contract installs the device
   *  runtime before each case and takes it away after. */
  static bool prepare(std::string* why) {
    return sigil::sketch::test::installDeviceRuntime(why);
  }
  static void release() { useRuntime({}); }
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
