/** @file
 * The 3D session: what a set declares, and that stepping it to a moment
 * is a function of the moment alone.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilworld/element/Element.h>

#include <cmath>
#include <cstring>

namespace {

using namespace sigil::sketch;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

/** One box on a turntable, and nothing else. */
struct Spun : Set {
  void setup(SetContext& ctx) override {
    ctx.canvas(160, 120);
    ctx.background({0.05f, 0.05f, 0.08f, 1});
    ctx.captureAt(0.5);
    world::Camera lens;
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

SkBitmap steppedTo(int frames) {
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  for (int f = 0; f < frames; ++f) session->frame(canvas, 1.0 / 60.0);
  return bitmap;
}

TEST(SetSession, ReadsTheDeclarationBackAfterSetup) {
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  EXPECT_EQ(session->canvas().size, SkSize::Make(160, 120));
  EXPECT_EQ(session->canvas().captureSeconds, 0.5);
}

TEST(SetSession, TheSameMomentIsTheSamePicture) {
  // The plate contract: a set is a pure function of the scene time, so
  // two runs that step the same number of frames agree on every byte.
  const SkBitmap first = steppedTo(30);
  const SkBitmap second = steppedTo(30);
  ASSERT_EQ(first.computeByteSize(), second.computeByteSize());
  EXPECT_EQ(0, std::memcmp(first.getPixels(), second.getPixels(),
                           first.computeByteSize()));
}

TEST(SetSession, ADifferentMomentIsADifferentPicture) {
  const SkBitmap early = steppedTo(6);
  const SkBitmap late = steppedTo(30);
  EXPECT_NE(0, std::memcmp(early.getPixels(), late.getPixels(),
                           early.computeByteSize()));
}

TEST(SetSession, TakesItsStillAtItsOwnSize) {
  // A set is drawn from shaded vertices: a larger canvas would be a
  // different picture rather than a sharper one.
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  EXPECT_FLOAT_EQ(session->oversample(), 1.0f);
}

TEST(SetSession, OffersAViewpointAHostCanMove) {
  std::unique_ptr<Session> session = kindOf<Spun>()->open(fonts(), assets());
  EXPECT_TRUE(session->hasViewpoint());
  session->viewpoint(45.0f, 20.0f, 300.0f);
}

TEST(SetKind, NamesItsRuntime) { EXPECT_EQ(kindOf<Spun>()->runtime(), "set"); }

}  // namespace
