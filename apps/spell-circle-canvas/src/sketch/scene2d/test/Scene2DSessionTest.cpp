/** @file
 * The 2D session: what a sketch declares, what a host reads back, and
 * what stepping it does.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/texture/Texture.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Recording.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/paint/Paint.h>
#include <sigilweave/style/PaintLayer.h>
#include <sigilweave/style/Type.h>
#include <sigilworld/World.h>
#include <sigilworld/frame/Frame.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ScratchDir.h"
#include "TextFields.h"
#include "support/Devices.h"
#include "support/Fixtures.h"
#include "support/Pixels.h"
#include "support/Sessions.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;
namespace material = sigil::material;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::sketch::test::pixelsOf;
using sigil::sketch::test::samePicture;

/** THE ONE BODY A BAKE IS MEASURED BY: a white unlit card, its size
 *  stated in world units, square to the camera at the origin. Unlit
 *  because a shaded body's edge is a gradient and this test is about
 *  where the edge IS; a card because a card's silhouette is a rectangle
 *  whose pixels the camera predicts exactly. */
constexpr float kCardW = 100.0f;
constexpr float kCardH = 60.0f;
constexpr float kEyeZ = 300.0f;
constexpr SkISize kBakeSize{128, 96};
/** The ground a bake is cleared to: a colour eight bits hold exactly, so
 *  the picture can be compared with it to the byte. */
constexpr material::Color kGround{26.0f / 255, 26.0f / 255, 26.0f / 255, 1};
/** The entrance the bake's moment is read against: how far a card slides
 *  in, how long one card's own motion lasts, and how far apart the
 *  cascade starts the two of them. */
constexpr float kSlideIn = 90.0f;
constexpr auto kEnter = std::chrono::milliseconds(400);
constexpr auto kStagger = std::chrono::milliseconds(120);

world::Frame cardFrame() {
  return world::Frame(world::group({world::mesh(gm::quad(kCardW, kCardH))
                                        .key("card")
                                        .fill(sigil::material::surface::unlit(
                                            {.baseColor = {1, 1, 1, 1}}))})
                          .key("set"));
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
  const float halfFov = lens.fovYDeg * 0.5f * (float)std::numbers::pi / 180.0f;
  return (float)size.height() /
         (2.0f * std::abs(lens.eye.z) * std::tan(halfFov));
}

/** The box the drawn pixels stand in, against the ground every bake
 *  here is cleared to. */
SkIRect silhouetteOf(const SkBitmap& pixels) {
  return sigil::sketch::test::silhouetteOf(
      pixels, material::skia::toSkColor(kGround).toSkColor());
}

/** A sketch that asks for a texture scene while declaring itself, paints
 *  one red sheet into it, and then LETS GO of it — keeping the image it
 *  took and a weak look at the scene, so that what stands afterwards is
 *  what the session is holding and nothing the sketch is. */
struct Screening {
  static inline sk_sp<SkImage> sheet;
  static inline std::weak_ptr<TextureScene> scene;
  void setup(Scene2DContext& ctx) {
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
struct Declaring {
  static inline int updates = 0;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(320, 200);
    ctx.background({0.1f, 0.2f, 0.3f, 1});
    ctx.captureAt(2.5);
    ctx.composer.render(
        box().width(100).height(50).fill(Fill::color({1, 0, 0, 1})));
  }
  void update(double, Scene2DContext&) { ++updates; }
};

/** The port the listening sketch below opens, and the recording it reads
 *  instead while a capture is being taken. */
constexpr const char* kSkyPort = "udp://:27020";

/** One recorded message carrying @p text. */
std::shared_ptr<const sigil::io::Bytes> recorded(std::string_view text) {
  return std::make_shared<const sigil::io::Bytes>(
      std::as_bytes(std::span(text)));
}

/** A sketch that LISTENS: it opens a feed on a port while declaring
 *  itself, and under a capture it first replays its own recording at
 *  that port, so the door it opens is the file. The feed is kept where
 *  the case can read it, which is what stands for the drawing a real
 *  body would do from what arrived. */
struct Listening {
  static inline sigil::io::Feed sky;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(64, 48);
    sigil::io::Hub& hub = ctx.assets.hub();
    if (ctx.deterministic) hub.replay(kSkyPort, ctx.local("data/sky.feed"));
    sky = hub.listen(kSkyPort);
  }
};

struct CaptureProgress {
  int updates = 0;
  int timerCalls = 0;
  int paints = 0;
  uint64_t arrivals = 0;
  double elapsed = 0;
};

/** Capture state advances in update and a timer; paint builds only the
 *  current image on a separate surface. */
struct UpdateDrivenCapture {
  static inline std::shared_ptr<CaptureProgress> next;
  std::shared_ptr<CaptureProgress> progress = next;
  sigil::motion::Timer timer;
  sigil::io::Feed feed;

  void setup(Scene2DContext& ctx) {
    ctx.canvas(64, 48);
    ctx.paintDiscardedFrames(false);
    ctx.assets.hub().replay(kSkyPort, ctx.local("data/sky.feed"));
    feed = ctx.assets.hub().listen(kSkyPort);
    timer = ctx.engine.timer([this] { ++progress->timerCalls; });
    ctx.composer.render(
        custom("update-driven-capture",
               [this](sigil::draw::Pen& pen) {
                 ++progress->paints;
                 const auto working =
                     SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 48));
                 working->getCanvas()->clear(SK_ColorBLACK);
                 SkPaint ink;
                 ink.setColor4f({float(progress->elapsed),
                                 float(progress->arrivals) / 3, .5f, 1});
                 working->getCanvas()->drawRect(
                     SkRect::MakeXYWH(float(progress->timerCalls) / 3, 8, 8,
                                      12),
                     ink);
                 pen.canvas()->drawImage(working->makeImageSnapshot(), 0, 0);
               })
            .cache(Cache::None)
            .width(64)
            .height(48));
  }

  void update(double elapsed) {
    ++progress->updates;
    progress->elapsed = elapsed;
    progress->arrivals = feed.state().revision;
  }
};

/** The default capture path must preserve pixels accumulated by a pen. */
struct AccumulatingCapture {
  static inline int paints = 0;
  int drawn = 0;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(64, 48);
    ctx.composer.render(graphics("accumulating-capture",
                                 [this](sigil::draw::Pen& pen) {
                                   ++paints;
                                   pen.noStroke();
                                   pen.fill(0, 255, 0);
                                   pen.rect(float(drawn++ * 2), 8, 2, 12);
                                 })
                            .absolute()
                            .inset(0));
  }
};

/** A sketch that PROBES something while declaring itself and keeps the
 *  answer — the shape of every sketch that measures its own faces before
 *  it can lay anything out.
 *
 *  The probe's input is a knob a caller sets before opening, so one
 *  process can stand two sessions of this ONE type over two different
 *  answers; the box each draws is a function of the answer ITS OWN
 *  instance kept. The answer lives on the instance and nowhere else,
 *  which is the property the case below is about: a probe parked in a
 *  function-local static would be one value the two sessions share, and
 *  the second to open would redraw the first. */
struct Probing {
  /** What the next session opened will probe. */
  static inline float nextInk = 40.0f;
  /** This instance's own answer, taken once in setup(). */
  float ink = 0;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(320, 200);
    ctx.background({0, 0, 0, 1});
    ink = nextInk;
    ctx.composer.render(art());
  }
  /** It describes again every frame, which is what makes the probe's
   *  home observable: a sketch whose answer sat in a shared static would
   *  redraw itself from ANOTHER session's answer the moment that session
   *  opened, and only a description taken after that shows it. */
  void update(double, Scene2DContext& ctx) { ctx.composer.render(art()); }
  /** The probed width goes on a CHILD of a root that fills the canvas,
   *  held at the start of its row: the probe has to reach the pixels for
   *  a comparison of them to say anything. */
  [[nodiscard]] Element art() const {
    return box().inset(0).children({box()
                                        .alignSelf(Align::Start)
                                        .flexShrink(0)
                                        .width(Dimension(ink))
                                        .height(Dimension(20))
                                        .fill(Fill::color({1, 0, 0, 1}))});
  }
};

/** What every case here opens: the session over that sketch, and a
 *  raster surface at exactly the canvas it declared. */
class Scene2DSession : public ::testing::Test {
 protected:
  Scene2DSession()
      : session(kindOf<Declaring>()->open(fonts(), assets())),
        surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 200))) {}

  SkCanvas& canvas() { return *surface->getCanvas(); }

  /** What the surface holds now. */
  SkBitmap pixels() { return sigil::sketch::test::plateOf(*surface); }

  std::unique_ptr<Session> session;
  sk_sp<SkSurface> surface;
};

TEST_F(Scene2DSession, PaintsWhatTheSketchDescribed) {
  canvas().clear(SK_ColorBLACK);
  session->frame(canvas(), 1.0 / 60.0);
  EXPECT_EQ(pixels().getColor(10, 10), SK_ColorRED);
  // The declared ground is not the session's to paint: the session hands
  // it to its host as part of the canvas the body declared, and the host
  // clears the surface to it before each frame. So the session reports
  // exactly the ground the body declared…
  const sigil::material::Color& ground = session->canvas().background;
  EXPECT_FLOAT_EQ(ground.r, 0.1f);
  EXPECT_FLOAT_EQ(ground.g, 0.2f);
  EXPECT_FLOAT_EQ(ground.b, 0.3f);
  EXPECT_FLOAT_EQ(ground.a, 1.0f);
  // …and a frame paints the described red box, standing at the size its
  // root states, and nothing past it: the corner keeps whatever the
  // surface held when the frame began.
  EXPECT_EQ(pixels().getColor(300, 190), SK_ColorBLACK);
}

TEST_F(Scene2DSession,
       AStillTakesOneMoreStepSoAPlateIsTheDeclaredMomentPlusOne) {
  // A scene2d runtime re-renders for its still — it is resolution
  // independent, so a plate is described again at the capture scale
  // rather than magnified — and describing again is one more step.
  session->frame(canvas(), 1.0 / 60.0);
  session->frame(canvas(), 1.0 / 60.0);
  const int stepped = Declaring::updates;
  session->still(canvas());
  EXPECT_EQ(Declaring::updates, stepped + 1);
}

TEST(Scene2DCapture, UpdateOnlyPreRollPreservesTimersFeedsAndTheFinalStill) {
  sigil::test::ScratchDir dir("sketch_update_only_capture");
  std::filesystem::create_directories(dir.path / "data");
  {
    sigil::io::RecordingWriter writer(dir.path / "data" / "sky.feed");
    ASSERT_TRUE(writer.good());
    ASSERT_TRUE(writer.append(sigil::io::Message(
        recorded("dawn"), {}, std::chrono::duration<double>(0), 1)));
    ASSERT_TRUE(writer.append(sigil::io::Message(
        recorded("noon"), {}, std::chrono::duration<double>(0.5), 2)));
    ASSERT_TRUE(writer.append(sigil::io::Message(
        recorded("dusk"), {}, std::chrono::duration<double>(5), 3)));
  }
  const auto take = [&](bool discard) {
    SCOPED_TRACE(discard ? "discarded pre-roll" : "painted reference");
    Assets store("");
    store.mountSketch("capture", dir.path);
    UpdateDrivenCapture::next = std::make_shared<CaptureProgress>();
    const auto progress = UpdateDrivenCapture::next;
    const auto session =
        kindOf<UpdateDrivenCapture>()->open(fonts(), store, true, "capture");
    const auto target = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 48));
    SkNoDrawCanvas discarded(64, 48);
    for (int frame = 0; frame < 36; ++frame) {
      if (discard)
        session->discardedFrame(discarded, 1.0 / 60.0);
      else
        session->frame(*target->getCanvas(), 1.0 / 60.0);
    }
    EXPECT_EQ(progress->updates, 36);
    EXPECT_EQ(progress->timerCalls, 36);
    EXPECT_EQ(progress->arrivals, 2u);
    EXPECT_NEAR(progress->elapsed, .6, 1e-9);
    EXPECT_EQ(progress->paints, discard ? 0 : 36);

    session->setProfiling(true);
    session->still(*target->getCanvas());
    const SkBitmap result = sigil::sketch::test::plateOf(*target);
    EXPECT_EQ(progress->updates, 37);
    EXPECT_EQ(progress->timerCalls, 37);
    EXPECT_EQ(progress->paints, discard ? 1 : 37);
    // The still draws once more at the moment the scene stands at.
    EXPECT_NEAR(progress->elapsed, .6, 1e-9);
    EXPECT_NE(result.getColor(14, 12), SK_ColorBLACK);
    EXPECT_FALSE(session->costs(8).empty());

    // Repaint still draws, without advancing the body or its timer.
    session->repaint(*target->getCanvas());
    EXPECT_EQ(progress->updates, 37);
    EXPECT_EQ(progress->timerCalls, 37);
    EXPECT_EQ(progress->paints, discard ? 2 : 38);
    EXPECT_TRUE(samePicture(result, sigil::sketch::test::plateOf(*target)));

    // An omitted draw must not retain the preceding frame's paint costs.
    const int painted = progress->paints;
    session->discardedFrame(discarded, 1.0 / 60.0);
    EXPECT_EQ(progress->updates, 38);
    EXPECT_EQ(progress->paints, painted);
    EXPECT_EQ(session->timing().drawMs, 0);
    for (const LaneCost& lane : session->lanes()) EXPECT_EQ(lane.ms, 0);
    EXPECT_TRUE(session->costs(8).empty());
    session->repaint(*target->getCanvas());
    EXPECT_FALSE(session->costs(8).empty());
    return result;
  };
  const SkBitmap reference = take(false);
  const SkBitmap skipped = take(true);
  EXPECT_TRUE(samePicture(reference, skipped));
}

TEST(Scene2DCapture, DefaultDiscardedFramesPreserveAccumulatedBrushPixels) {
  const auto take = [](bool discard) {
    AccumulatingCapture::paints = 0;
    const auto session = kindOf<AccumulatingCapture>()->open(fonts(), assets());
    const auto target = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 48));
    SkNoDrawCanvas discarded(64, 48);
    EXPECT_TRUE(session->canvas().paintDiscardedFrames);
    for (int frame = 0; frame < 8; ++frame) {
      if (discard)
        session->discardedFrame(discarded, 1.0 / 60.0);
      else
        session->frame(*target->getCanvas(), 1.0 / 60.0);
    }
    EXPECT_EQ(AccumulatingCapture::paints, 8);
    target->getCanvas()->clear(SK_ColorBLACK);
    session->still(*target->getCanvas());
    EXPECT_EQ(AccumulatingCapture::paints, 9);
    const SkBitmap result = sigil::sketch::test::plateOf(*target);
    EXPECT_EQ(result.getColor(1, 12), SK_ColorGREEN);
    EXPECT_EQ(result.getColor(15, 12), SK_ColorGREEN);
    EXPECT_EQ(result.getColor(17, 12), SK_ColorGREEN);
    EXPECT_EQ(result.getColor(24, 12), SK_ColorBLACK);
    return result;
  };
  EXPECT_TRUE(samePicture(take(false), take(true)));
}

/** The 2D session's answers to what every session promises. */
struct Scene2DTraits {
  static Kind kind() { return kindOf<Declaring>(); }
  static SkSize canvas() { return SkSize::Make(320, 200); }
  static double captureSeconds() { return 2.5; }
  static const char* runtime() { return "scene2d"; }
  static std::vector<const char*> lanes() {
    return {"recon", "layout", "volat", "paint"};
  }
  /** A canvas is described again at the capture scale, so a still is
   *  worth taking larger than the canvas the body declared. */
  static float oversample() { return 2.0f; }
  static void reset() { Declaring::updates = 0; }
  static int bodyRuns() { return Declaring::updates; }
};

TEST(Scene2DDoors, DrawsTwoSessionsOfOneSketchFromTheirOwnProbes) {
  const auto shot = [](Session& session, SkSurface& surface) {
    surface.getCanvas()->clear(SK_ColorBLACK);
    session.frame(*surface.getCanvas(), 1.0 / 60.0);
    SkBitmap bitmap;
    bitmap.allocPixels(surface.imageInfo());
    EXPECT_TRUE(surface.readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  };
  const auto same = [](const SkBitmap& a, const SkBitmap& b) {
    return a.computeByteSize() == b.computeByteSize() &&
           std::memcmp(a.getPixels(), b.getPixels(), a.computeByteSize()) == 0;
  };
  const sk_sp<SkSurface> first =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 200));
  const sk_sp<SkSurface> second =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 200));

  Probing::nextInk = 40.0f;
  const std::unique_ptr<Session> a = kindOf<Probing>()->open(fonts(), assets());
  const SkBitmap drawnByA = shot(*a, *first);

  // A SECOND SESSION OF THE SAME TYPE, over a different answer. Both are
  // standing at once, which is what a live host does with two windows on
  // one sketch and what a reload does for the instant the old body and
  // the new one overlap.
  Probing::nextInk = 120.0f;
  const std::unique_ptr<Session> b = kindOf<Probing>()->open(fonts(), assets());
  const SkBitmap drawnByB = shot(*b, *second);
  EXPECT_EQ(drawnByA.getColor(10, 10), SK_ColorRED);
  EXPECT_EQ(drawnByB.getColor(10, 10), SK_ColorRED);
  EXPECT_EQ(drawnByB.getColor(100, 10), SK_ColorRED);
  EXPECT_NE(drawnByA.getColor(100, 10), SK_ColorRED);
  EXPECT_FALSE(same(drawnByA, drawnByB))
      << "the two sessions probed different answers and drew the same box";

  // …and A's picture is still A's. Opening and drawing B moved nothing
  // that A reads, because the answer B probed went onto B's instance.
  EXPECT_TRUE(same(drawnByA, shot(*a, *first)))
      << "one session's probe reached the other's picture";
}

/** A sketch that bakes the card twice while declaring itself: once from
 *  the stated distance, once from twice it. */
struct Baking {
  static inline sk_sp<SkImage> near;
  static inline sk_sp<SkImage> far;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(200, 120);
    near =
        ctx.bakeScene3D(cardFrame(), kBakeSize,
                        {.camera = cardCamera(kEyeZ), .background = kGround});
    far = ctx.bakeScene3D(
        cardFrame(), kBakeSize,
        {.camera = cardCamera(kEyeZ * 2), .background = kGround});
    ctx.composer.render(box().width(10).height(10));
  }
};

/** THE SAME CARD WITH AN ENTRANCE, cascaded: it slides in from the left
 *  over its own tween, and its delay staggers when each of the group's
 *  children starts. A still of it is a claim about that entrance, so the
 *  bake has to be able to reach a moment of it. */
world::Frame enteringFrame() {
  const auto card = [](const char* key, float x) {
    return world::mesh(gm::quad(kCardW * 0.4f, kCardH))
        .key(key)
        .translateX(
            sigil::motion::animate({.from = x - kSlideIn,
                                    .to = x,
                                    .duration = kEnter,
                                    .delay = sigil::motion::stagger(kStagger)}))
        .fill(sigil::material::surface::unlit({.baseColor = {1, 1, 1, 1}}));
  };
  return world::Frame(
      world::group({card("left", -40.0f), card("right", 40.0f)}).key("set"));
}

/** A sketch that bakes the entering card at three moments: before it
 *  starts, part way through, and after the whole cascade has landed. */
struct Entering {
  static inline sk_sp<SkImage> atMount;
  static inline sk_sp<SkImage> partWay;
  static inline sk_sp<SkImage> landed;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(200, 120);
    const auto bake = [&](double seconds) {
      return ctx.bakeScene3D(enteringFrame(), kBakeSize,
                             {.camera = cardCamera(kEyeZ),
                              .background = kGround,
                              .at = sigil::motion::Duration(seconds)});
    };
    atMount = bake(0.0);
    partWay = bake(kEnter.count() / 1000.0 * 0.5);
    landed = bake((kEnter.count() + kStagger.count() * 2) / 1000.0);
    ctx.composer.render(box().width(10).height(10));
  }
};

/** THE CARD UNDER A BLOOM AND NOTHING ELSE: a frame that names only a
 *  post pass still has bodies to paint, so the bake paints them first and
 *  blooms what it painted. */
struct Blooming {
  static inline sk_sp<SkImage> glowing;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(200, 120);
    world::Frame frame = cardFrame();
    frame.pass(world::bloom({.threshold = 0.5f, .intensity = 0.5f}));
    glowing = ctx.bakeScene3D(
        frame, kBakeSize, {.camera = cardCamera(kEyeZ), .background = kGround});
    ctx.composer.render(box().width(10).height(10));
  }
};

/** A sketch that asks for a PAGE while declaring itself, paints it one
 *  green that takes no light, and bakes a card wearing it under no light
 *  at all: what the card shows is the page and nothing the scene lit. The
 *  page is let go after the bake, so what stands afterwards is what the
 *  session is holding. */
struct Paging {
  static inline sk_sp<SkImage> worn;
  static inline std::string name;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(200, 120);
    const Page page = ctx.page({32, 32});
    name = page.name();
    // The page's green states no surface, so it is the card's base colour
    // and takes the sun standing behind the eye.
    world::Frame frame(
        world::group({world::directionalLight({0, 0, -1}).key("sun"),
                      world::mesh(gm::quad(kCardW, kCardH))
                          .key("card")
                          .fill(page.surface())})
            .key("set"));
    frame.pass(page.render(
        box().width(32).height(32).fill(Fill::color({0, 1, 0, 1}))));
    worn = ctx.bakeScene3D(
        frame, kBakeSize, {.camera = cardCamera(kEyeZ), .background = kGround});
    ctx.composer.render(box().width(10).height(10));
  }
};

/** A CASE THAT BAKES A 3D FRAME: the frame is drawn on the device, so the
 *  device runtime is installed for the process before it and taken away
 *  after, or the case skips naming what is missing. */
class Scene2DBakes : public testing::Test {
 protected:
  void SetUp() override {
    std::string why;
    if (!sigil::sketch::test::installDeviceRuntime(&why))
      GTEST_SKIP() << "no Vulkan device: " << why;
  }
  void TearDown() override { useRuntime({}); }
};

TEST(Scene2DDoors, BakesNothingWhereNoDeviceIsInstalled) {
  // A 3D frame is drawn on a device and nowhere else, so a bake with none to
  // draw on is no image rather than a picture no device drew.
  useRuntime({});
  Baking::near = nullptr;
  const std::unique_ptr<Session> session =
      kindOf<Baking>()->open(fonts(), assets());
  EXPECT_FALSE(Baking::near);
}

TEST_F(Scene2DBakes, PaintsTheBodiesUnderAFrameThatNamesOnlyAPostPass) {
  const std::unique_ptr<Session> session =
      kindOf<Blooming>()->open(fonts(), assets());
  ASSERT_TRUE(Blooming::glowing);
  const SkBitmap pixels = pixelsOf(Blooming::glowing);
  // The card is white at the middle of the picture, bloomed or not; a
  // frame that ran its bloom over nothing is the ground everywhere.
  const SkColor middle =
      pixels.getColor(kBakeSize.width() / 2, kBakeSize.height() / 2);
  EXPECT_GT(SkColorGetR(middle), 200u)
      << "the bake bloomed a picture no geometry pass had painted";
  EXPECT_FALSE(silhouetteOf(pixels).isEmpty());
}

TEST_F(Scene2DBakes, BakesTheMomentOfAnEntranceRatherThanAlwaysItsFirstFrame) {
  const std::unique_ptr<Session> session =
      kindOf<Entering>()->open(fonts(), assets());
  ASSERT_TRUE(Entering::landed);
  const SkIRect mounted = silhouetteOf(pixelsOf(Entering::atMount));
  const SkIRect halfway = silhouetteOf(pixelsOf(Entering::partWay));
  const SkIRect settled = silhouetteOf(pixelsOf(Entering::landed));
  ASSERT_FALSE(settled.isEmpty());
  // The cards slide in from the left, so the pair's box travels right as
  // the cascade runs. Three distinct moments, three distinct pictures —
  // and before this argument existed all three were the first.
  EXPECT_LT(mounted.left(), halfway.left())
      << "the bake stood still where the entrance was moving";
  EXPECT_LT(halfway.left(), settled.left());
  // …and the landed one is the scene as described, symmetric about the
  // middle, which is what says the clock actually reached the end.
  EXPECT_NEAR((float)(settled.left() + settled.right()) / 2.0f,
              kBakeSize.width() / 2.0f, 1.5f);
}

TEST_F(Scene2DBakes, PaintsThePageABakedCardWears) {
  const std::unique_ptr<Session> session =
      kindOf<Paging>()->open(fonts(), assets());
  ASSERT_TRUE(Paging::worn);
  EXPECT_EQ(Paging::name, "page");
  const SkBitmap pixels = pixelsOf(Paging::worn);
  // The page's pass ran before the card was drawn: the card's middle is
  // the green the page was painted, lit by the sun, and the corner is the
  // ground the bake was cleared to.
  const SkColor middle =
      pixels.getColor(kBakeSize.width() / 2, kBakeSize.height() / 2);
  EXPECT_GT(SkColorGetG(middle), 100u);
  EXPECT_GT(SkColorGetG(middle), SkColorGetR(middle) + 60);
  EXPECT_GT(SkColorGetG(middle), SkColorGetB(middle) + 60);
  EXPECT_EQ(pixels.getColor(1, 1),
            material::skia::toSkColor(kGround).toSkColor());
  // The session counts the page among what it holds for the body.
  EXPECT_NE(session->counters().find("scenes 1"), std::string::npos)
      << session->counters();
}

TEST(Scene2DDoors, PaintsWhatTheSketchHandedTheTextureScene) {
  const std::unique_ptr<Session> session =
      kindOf<Screening>()->open(fonts(), assets());
  ASSERT_TRUE(Screening::sheet);
  EXPECT_EQ(Screening::sheet->width(), 32);
  EXPECT_EQ(Screening::sheet->height(), 32);
  EXPECT_EQ(pixelsOf(Screening::sheet).getColor(16, 16), SK_ColorRED);
}

TEST(Scene2DDoors, KeepsTheTextureScenesItHandsOutUntilTheBodyDeclaresAgain) {
  std::unique_ptr<Session> session =
      kindOf<Screening>()->open(fonts(), assets());
  // The sketch dropped its own reference at the end of setup; the scene
  // stands because the SESSION holds it — which is what a picture on a
  // device needs, since a scene destroys the texture its image names
  // when it goes.
  EXPECT_FALSE(Screening::scene.expired());

  // Declaring again lets last time's scenes go and keeps this time's.
  const std::weak_ptr<TextureScene> first = Screening::scene;
  session->redeclare();
  EXPECT_TRUE(first.expired());
  EXPECT_FALSE(Screening::scene.expired());

  // …and the session going lets go of the last of them.
  session = {};
  EXPECT_TRUE(Screening::scene.expired());
}

TEST_F(Scene2DBakes, BakesAScene3DAsTheCameraProjectsIt) {
  const std::unique_ptr<Session> session =
      kindOf<Baking>()->open(fonts(), assets());
  ASSERT_TRUE(Baking::near);
  EXPECT_EQ(Baking::near->width(), kBakeSize.width());
  EXPECT_EQ(Baking::near->height(), kBakeSize.height());

  const SkBitmap pixels = pixelsOf(Baking::near);
  // Nothing but the card is in the tree, so the corner is the ground it
  // was cleared to and the middle of the card is the card's own colour —
  // unlit, so it is that colour exactly.
  EXPECT_EQ(pixels.getColor(1, 1),
            material::skia::toSkColor(kGround).toSkColor());
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

TEST_F(Scene2DBakes, BakesTheSameScene3DFromTheCameraItIsGiven) {
  const std::unique_ptr<Session> session =
      kindOf<Baking>()->open(fonts(), assets());
  ASSERT_TRUE(Baking::far);
  // The same frame from twice the distance is the same card at half the
  // pixels — the viewpoint is the caller's, and the picture is a
  // projection of the scene rather than a stamp of it.
  const SkIRect near = silhouetteOf(pixelsOf(Baking::near));
  const SkIRect far = silhouetteOf(pixelsOf(Baking::far));
  EXPECT_NEAR((float)far.width(), near.width() / 2.0f, 2.0f);
  EXPECT_NEAR((float)far.height(), near.height() / 2.0f, 2.0f);
}

/** THE THREE SHAPES A SCENE2D SKETCH IS WRITTEN IN, side by side: the
 *  whole offer, a `setup` that named none of it, and a body stepped
 *  through an `update` that named the seconds and not the context. None
 *  of them derives from anything. */
struct Whole {
  static inline int setups = 0;
  void setup(Scene2DContext& ctx) {
    ++setups;
    ctx.canvas(64, 48);
    ctx.composer.render(box().width(10).height(10));
  }
};

struct Bare {
  static inline int setups = 0;
  void setup() { ++setups; }
};

struct Stepped {
  static inline double last = -1.0;
  void setup(Scene2DContext& ctx) { ctx.canvas(64, 48); }
  void update(double elapsed) { last = elapsed; }
};

/** …and a type that spelled the member some other way, which is what the
 *  registration has to refuse. */
struct Misspelled {
  void setUp(Scene2DContext&) {}
};

TEST(Scene2DBodies, ASketchIsAnyTypeThatNamesSetup) {
  // The concept is what `kindOf` asserts on, so a member spelled some
  // other way is a compile error naming the signatures a body may have.
  // Nothing in this tree compiles a translation unit that must NOT build
  // — the documentation probes are the one generated compile check and
  // they assert that a name EXISTS — so what a case can hold is the
  // answer the assertion reads.
  static_assert(Scene2DSketch<Whole>);
  static_assert(Scene2DSketch<Bare>);
  static_assert(Scene2DSketch<Stepped>);
  static_assert(!Scene2DSketch<Misspelled>);

  Whole::setups = 0;
  const std::unique_ptr<Session> session =
      kindOf<Whole>()->open(fonts(), assets());
  ASSERT_TRUE(session);
  EXPECT_EQ(Whole::setups, 1);
  EXPECT_EQ(session->canvas().size, SkSize::Make(64, 48));
}

TEST(Scene2DBodies, OpensABodyThatNamedNothingItWasOffered) {
  Bare::setups = 0;
  const std::unique_ptr<Session> session =
      kindOf<Bare>()->open(fonts(), assets());
  ASSERT_TRUE(session);
  EXPECT_EQ(Bare::setups, 1);
  // A body that declared no canvas is opened onto the one every session
  // starts with, and one that spells no `update` is stepped by doing
  // nothing at all.
  EXPECT_EQ(session->canvas().size, CanvasSpecification{}.size);
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(16, 16));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
}

TEST(Scene2DBodies, StepsABodyThatNamedTheSecondsAndNotTheContext) {
  Stepped::last = -1.0;
  const std::unique_ptr<Session> session =
      kindOf<Stepped>()->open(fonts(), assets());
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 48));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  // The clock the session steps is the one the body is handed, so two
  // frames of a sixtieth put it two sixtieths in.
  EXPECT_NEAR(Stepped::last, 2.0 / 60.0, 1e-9);
}

/** A recording is a function of the SCENE time, and the session is what
 *  moves it: the frames a host steps carry the feed forward to exactly
 *  the arrivals due by the moment being drawn, with nothing for the
 *  sketch to call and nothing for a host to schedule. */
TEST(Scene2DDoors, CarriesAMountedRecordingToTheSceneTimeTheFramesReach) {
  sigil::test::ScratchDir dir("sketch_canvas_feed");
  std::filesystem::create_directories(dir.path / "data");
  {
    sigil::io::RecordingWriter writer(dir.path / "data" / "sky.feed");
    ASSERT_TRUE(writer.good());
    ASSERT_TRUE(writer.append(sigil::io::Message(
        recorded("dawn"), {}, std::chrono::duration<double>(0.0), 1)));
    ASSERT_TRUE(writer.append(sigil::io::Message(
        recorded("noon"), {}, std::chrono::duration<double>(0.5), 2)));
    ASSERT_TRUE(writer.append(sigil::io::Message(
        recorded("dusk"), {}, std::chrono::duration<double>(5.0), 3)));
  }
  Assets store("");
  store.mountSketch("listening", dir.path);

  Listening::sky = {};
  const std::unique_ptr<Session> session =
      kindOf<Listening>()->open(fonts(), store, true, "listening");
  ASSERT_TRUE(session);
  ASSERT_TRUE(Listening::sky);
  EXPECT_TRUE(Listening::sky.state().error.empty());
  // A session that has drawn no frame has reached no scene time, so the
  // recording has not started.
  EXPECT_EQ(Listening::sky.state().revision, 0u);

  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 48));
  for (int i = 0; i < 36; ++i)
    session->frame(*surface->getCanvas(), 1.0 / 60.0);

  // Thirty-six frames of a sixtieth is six tenths of a scene second: the
  // two arrivals recorded by then have been delivered, and the one at
  // five seconds is still ahead.
  EXPECT_EQ(Listening::sky.state().revision, 2u);
  ASSERT_TRUE(Listening::sky.latest().has_value());
  EXPECT_EQ(Listening::sky.latest()->payload->asText(), "noon");
  EXPECT_NE(Listening::sky.state().readiness, sigil::io::ReadyState::Closed);
  // The store outlives nothing here: the feed is let go before the
  // session that opened it and the hub it was opened on.
  Listening::sky = {};
}

// ── The material a text pass carries ─────────────────────────────────────

/** THE BODY THE TEXT MATERIAL SEAM IS READ FROM: one word set in white
 *  over a flat black card, with one overlay pass also in white, and a
 *  switch deciding whether that pass carries a material. Every pixel of
 *  the unshaded plate is a grey, so a pixel whose channels differ is the
 *  material and nothing else. */
constexpr int kShadedWidth = 200;
constexpr int kShadedHeight = 80;

struct Shaded {
  static inline bool carriesMaterial = true;
  void setup(Scene2DContext& ctx) {
    ctx.canvas(kShadedWidth, kShadedHeight);
    sigil::weave::PaintLayer pass(SK_ColorWHITE);
    if (carriesMaterial)
      pass.material = std::make_shared<const sigil::material::Material>(
          text_fields::meshGradient(SkRect::MakeWH(kShadedWidth, kShadedHeight),
                                    0.0f));
    sigil::weave::Type type;
    type.face = sigil::test::instrument::sans();
    type.size = 56.0f;
    type.color = sigil::material::Color{1, 1, 1, 1};
    type.overlays = std::vector<sigil::weave::PaintLayer>{pass};
    Element word = text(u8"AB");
    word.font(type);
    ctx.composer.render(box()
                            .width(kShadedWidth)
                            .height(kShadedHeight)
                            .fill(Fill::color(material::Color{0, 0, 0, 1}))
                            .children({std::move(word)}));
  }
};

/** The plate one frame of the `Shaded` body leaves, with or without the
 *  material on its overlay pass. */
SkBitmap shadedPlate(bool carriesMaterial) {
  Shaded::carriesMaterial = carriesMaterial;
  const std::unique_ptr<Session> session =
      kindOf<Shaded>()->open(fonts(), assets());
  const sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(kShadedWidth, kShadedHeight));
  surface->getCanvas()->clear(SK_ColorBLACK);
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  return sigil::sketch::test::plateOf(*surface);
}

/** How many pixels of @p plate are not a grey — the reading that tells a
 *  shaded pass from one drawn in its own white. */
int colouredPixels(const SkBitmap& plate) {
  int found = 0;
  for (int y = 0; y < plate.height(); ++y)
    for (int x = 0; x < plate.width(); ++x) {
      const SkColor pixel = plate.getColor(x, y);
      if (SkColorGetR(pixel) != SkColorGetG(pixel) ||
          SkColorGetG(pixel) != SkColorGetB(pixel))
        ++found;
    }
  return found;
}

/** The resolver is process-wide, so a case that installs one puts it back
 *  however it leaves. */
class InstalledResolver {
 public:
  explicit InstalledResolver(sigil::weave::paint::MaterialResolver resolver) {
    sigil::weave::paint::setMaterialResolver(std::move(resolver));
  }
  InstalledResolver(const InstalledResolver&) = delete;
  InstalledResolver& operator=(const InstalledResolver&) = delete;
  ~InstalledResolver() { sigil::weave::paint::setMaterialResolver({}); }
};

TEST(Scene2DDoors, ShadesATextPassesMaterialWithoutAHostInstallingAResolver) {
  // A material on a text pass is shaded by the runtime the sketch drew
  // through: nothing in this binary is a host and nothing here installed
  // a resolver, and the two plates still differ.
  const SkBitmap bare = shadedPlate(false);
  const SkBitmap shaded = shadedPlate(true);
  ASSERT_FALSE(bare.drawsNothing());
  ASSERT_FALSE(shaded.drawsNothing());
  EXPECT_FALSE(samePicture(bare, shaded));
  EXPECT_EQ(colouredPixels(bare), 0)
      << "a white pass over white text inks nothing but greys";
  EXPECT_GT(colouredPixels(shaded), 0)
      << "the pass drew its plain paint, so the material never shaded";
}

TEST(Scene2DDoors, LeavesAResolverAHostInstalledStanding) {
  // A flat, unmistakable colour nothing else in this file draws.
  constexpr SkColor kHostInk = 0xFF00FF00;
  const InstalledResolver installed(
      [](const sigil::material::Material&, const SkRect&) {
        return SkShaders::Color(kHostInk);
      });
  const SkBitmap shaded = shadedPlate(true);
  ASSERT_FALSE(shaded.drawsNothing());
  int hostInked = 0;
  for (int y = 0; y < shaded.height(); ++y)
    for (int x = 0; x < shaded.width(); ++x)
      if (shaded.getColor(x, y) == kHostInk) ++hostInked;
  EXPECT_GT(hostInked, 0) << "the runtime's default replaced the host's";
}

TEST(Scene2DDoors, AMaterialPassNeedsNoHostToShade) {
  // The seam's own claim: opening a scene2d session is what closes it.
  const std::unique_ptr<Session> session =
      kindOf<Bare>()->open(fonts(), assets());
  ASSERT_TRUE(session);
  EXPECT_TRUE(sigil::weave::paint::hasMaterialResolver());
}

struct DeviceScene2D : Bare {
  static constexpr bool needsDevice = true;
};

TEST(Scene2DKinds, DeviceRequirementsFollowTheBody) {
  const Kind ordinary = kindOf<Bare>();
  EXPECT_FALSE(ordinary->needsDevice());
  EXPECT_TRUE(kindOf<DeviceScene2D>()->needsDevice());
  EXPECT_FALSE(ordinary == Kind(Scene2DKind{&makeScene2DBody<Bare>, true}));
}

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(TheScene2DSession, SessionContract,
                               Scene2DTraits);
