/** @file
 * What a scene3d body is: any type that names `describe`, opened through a
 * registered kind, an owned body with no kind behind it, or a supplier
 * standing where a factory function would.
 *
 * Every case that opens a scene3d sketch draws it on the device and SKIPS where
 * the machine has no Vulkan runtime; its suite carries the `gpu` label.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

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
using sigil::sketch::test::roundedBox;
using sigil::sketch::test::samePicture;
using sigil::sketch::test::Scene3DBodies;
using sigil::sketch::test::Scene3DDoors;
using sigil::sketch::test::Scene3DKinds;
using sigil::sketch::test::shadedFrame;
using sigil::sketch::test::Spun;

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

}  // namespace
