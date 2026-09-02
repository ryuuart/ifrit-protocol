/** @file
 * The frame's declarations and the CPU executor that performs them: a
 * pass compares as a value, a geometry pass paints what its realisation
 * leaves it, a post pass reads what stands and what stood last frame, a
 * compute pass cooks, and a declared body is handed the extracted view.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilworld/element/Node.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/frame/Runtime.h>

#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::world;

namespace {

constexpr SkISize kExtent{80, 80};

Mesh square(float size) {
  Mesh m;
  m.positions = {
      {-size, -size, 0}, {size, -size, 0}, {size, size, 0}, {-size, size, 0}};
  m.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  m.uvs = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}

Camera frontCamera() {
  Camera camera;
  camera.eye = {0, 0, 260};
  camera.target = {0, 0, 0};
  return camera;
}

/** Two squares side by side, the right one tagged "glow". */
struct Bodies {
  Mesh mesh = square(40.0f);
  std::vector<std::string> none;
  std::vector<std::string> glow = {"glow"};
  std::vector<Draw> draws;
  std::vector<Light> lights;

  Bodies() {
    Draw left;
    left.mesh = &mesh;
    left.world = glm::translate(glm::mat4(1.0f), {-52.0f, 0.0f, 0.0f});
    left.baseColor = {0.2f, 0.4f, 0.9f, 1.0f};
    left.key = "left";
    left.tags = none;
    Draw right = left;
    right.world = glm::translate(glm::mat4(1.0f), {52.0f, 0.0f, 0.0f});
    right.baseColor = {0.9f, 0.5f, 0.2f, 1.0f};
    right.key = "right";
    right.tags = glow;
    draws = {left, right};
  }

  View view() const {
    View v;
    v.draws = draws;
    v.lights = lights;
    v.camera = frontCamera();
    v.extent = kExtent;
    return v;
  }
};

int painted(const sk_sp<SkImage>& image) {
  if (!image) return 0;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(image->width(), image->height()));
  if (!image->readPixels(bitmap.pixmap(), 0, 0)) return 0;
  int count = 0;
  for (int y = 0; y < bitmap.height(); ++y)
    for (int x = 0; x < bitmap.width(); ++x)
      if (SkColorGetA(bitmap.getColor(x, y)) > 0) ++count;
  return count;
}

/** How much ink stands in the left or the right half of the plate. */
int paintedIn(const sk_sp<SkImage>& image, bool leftHalf) {
  if (!image) return 0;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(image->width(), image->height()));
  if (!image->readPixels(bitmap.pixmap(), 0, 0)) return 0;
  int count = 0;
  const int mid = bitmap.width() / 2;
  for (int y = 0; y < bitmap.height(); ++y)
    for (int x = 0; x < bitmap.width(); ++x) {
      if (leftHalf != (x < mid)) continue;
      if (SkColorGetA(bitmap.getColor(x, y)) > 0) ++count;
    }
  return count;
}

PassWork workOf(const Pass& pass, Selection realisation = Selection::None) {
  PassWork work;
  work.pass = &pass;
  work.realisation = realisation;
  return work;
}

Targets targetsAt(SkISize extent) {
  Targets targets;
  targets.extent(extent);
  return targets;
}

}  // namespace

TEST(WorldFrame, APassComparesFieldByField) {
  const Pass a = geometryPass("main").writes("colour", "depth");
  const Pass b = geometryPass("main").writes("colour", "depth");
  EXPECT_EQ(a, b);
  EXPECT_NE(a, geometryPass("main").writes("colour"));
  EXPECT_NE(a, computePass("main").writes("colour", "depth"));
  EXPECT_NE(geometryPass("main").only(sel::tag("glow")),
            geometryPass("main").only(sel::tag("lit")));
  EXPECT_EQ(postPass("bloom").reads("colour").blur(4.0f),
            postPass("bloom").reads("colour").blur(4.0f));
  EXPECT_NE(postPass("bloom").reads("colour").blur(4.0f),
            postPass("bloom").reads("colour").blur(5.0f));
}

TEST(WorldFrame, ALambdaBodyIsEqualToNothingButItsOwnCopies) {
  const Pass a = geometryPass("hand").body([](const View&, Targets&) {});
  const Pass b = geometryPass("hand").body([](const View&, Targets&) {});
  EXPECT_NE(a, b);
  EXPECT_EQ(a, Pass(a));
}

TEST(WorldFrame, ADeclaredNameIsNamedOnce) {
  const Pass pass = geometryPass("main").writes("colour").writes("colour");
  EXPECT_EQ(pass.writes().size(), 1u);
}

TEST(WorldFrame, AGeometryPassPaintsEveryBody) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  const Pass pass = geometryPass("main").writes("colour");
  Runtime::cpu()->execute(workOf(pass), bodies.view(), targets);

  const sk_sp<SkImage> colour = targets.image("colour");
  EXPECT_GT(paintedIn(colour, /*leftHalf=*/true), 0);
  EXPECT_GT(paintedIn(colour, /*leftHalf=*/false), 0);
}

TEST(WorldFrame, ACulledGeometryPassPaintsOnlyItsSelection) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  const Pass pass =
      geometryPass("glow").only(sel::tag("glow")).writes("colour");
  Runtime::cpu()->execute(workOf(pass, Selection::Cull), bodies.view(),
                          targets);

  const sk_sp<SkImage> colour = targets.image("colour");
  EXPECT_EQ(paintedIn(colour, /*leftHalf=*/true), 0);
  EXPECT_GT(paintedIn(colour, /*leftHalf=*/false), 0);
}

TEST(WorldFrame, AMaskRealisationPaintsCoverageAndNothingElse) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  const Pass pass = geometryPass("cover").only(sel::tag("glow")).writes("mask");
  Runtime::cpu()->execute(workOf(pass, Selection::Mask), bodies.view(),
                          targets);

  const sk_sp<SkImage> mask = targets.image("mask");
  EXPECT_EQ(paintedIn(mask, /*leftHalf=*/true), 0);
  EXPECT_GT(paintedIn(mask, /*leftHalf=*/false), 0);
}

TEST(WorldFrame, AVariantRealisationDrawsTheSelectionAgain) {
  Bodies bodies;
  Targets plain = targetsAt(kExtent);
  Targets varied = targetsAt(kExtent);

  struct Paint {
    glm::vec4 baseColor{1, 1, 1, 1};
  };
  const auto recipe = std::make_shared<const material::Recipe>(
      material::Recipe::of<Paint>("world.test.paint"));
  const material::Material white(recipe, Paint{{1.0f, 1.0f, 1.0f, 1.0f}});

  const Pass ordinary = geometryPass("main").writes("colour");
  Runtime::cpu()->execute(workOf(ordinary), bodies.view(), plain);
  const Pass over = geometryPass("main")
                        .writes("colour")
                        .only(sel::tag("glow"))
                        .variant(white);
  Runtime::cpu()->execute(workOf(over, Selection::Variant), bodies.view(),
                          varied);

  // The left body is untouched and the right one is repainted.
  EXPECT_EQ(paintedIn(plain.image("colour"), true),
            paintedIn(varied.image("colour"), true));
  SkBitmap a;
  SkBitmap b;
  a.allocPixels(SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  b.allocPixels(SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  ASSERT_TRUE(plain.image("colour")->readPixels(a.pixmap(), 0, 0));
  ASSERT_TRUE(varied.image("colour")->readPixels(b.pixmap(), 0, 0));
  const int x = kExtent.width() * 3 / 4;
  const int y = kExtent.height() / 2;
  EXPECT_NE(a.getColor(x, y), b.getColor(x, y));
}

TEST(WorldFrame, APostPassGradesWhatItReads) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  const Pass main = geometryPass("main").writes("colour");
  Runtime::cpu()->execute(workOf(main), bodies.view(), targets);

  const Pass dark =
      postPass("dark").reads("colour").writes("graded").levels(0.25f, 0.0f);
  Runtime::cpu()->execute(workOf(dark), bodies.view(), targets);

  SkBitmap before;
  SkBitmap after;
  before.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  after.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  ASSERT_TRUE(targets.image("colour")->readPixels(before.pixmap(), 0, 0));
  ASSERT_TRUE(targets.image("graded")->readPixels(after.pixmap(), 0, 0));
  const int x = kExtent.width() / 4;
  const int y = kExtent.height() / 2;
  EXPECT_GT(
      SkColorGetR(before.getColor(x, y)) + SkColorGetG(before.getColor(x, y)) +
          SkColorGetB(before.getColor(x, y)),
      SkColorGetR(after.getColor(x, y)) + SkColorGetG(after.getColor(x, y)) +
          SkColorGetB(after.getColor(x, y)));
}

TEST(WorldFrame, AMaskedPostPassReachesOnlyItsCoverage) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  const Pass main = geometryPass("main").writes("colour");
  PassWork paint = workOf(main);
  paint.coverageOut = "bloom.coverage";
  paint.coverageOf = sel::tag("glow");
  Runtime::cpu()->execute(paint, bodies.view(), targets);

  const Pass bloom = postPass("bloom")
                         .reads("colour")
                         .writes("lit")
                         .only(sel::tag("glow"))
                         .levels(0.0f, 0.0f);
  PassWork masked = workOf(bloom, Selection::Mask);
  masked.coverageIn = "bloom.coverage";
  Runtime::cpu()->execute(masked, bodies.view(), targets);

  SkBitmap before;
  SkBitmap after;
  before.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  after.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  ASSERT_TRUE(targets.image("colour")->readPixels(before.pixmap(), 0, 0));
  ASSERT_TRUE(targets.image("lit")->readPixels(after.pixmap(), 0, 0));
  const int y = kExtent.height() / 2;
  // Outside the coverage the picture stands; inside it the grade landed.
  EXPECT_EQ(before.getColor(kExtent.width() / 4, y),
            after.getColor(kExtent.width() / 4, y));
  EXPECT_NE(before.getColor(kExtent.width() * 3 / 4, y),
            after.getColor(kExtent.width() * 3 / 4, y));
}

TEST(WorldFrame, APreviousReadIsWhatStoodAtTheEndOfTheFrameBefore) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  targets.bind("colour", -1);
  targets.bind("trail", -1);
  targets.keep("trail");

  const Pass main = geometryPass("main").writes("colour");
  const Pass trail = postPass("trail")
                         .reads("colour")
                         .previous("trail")
                         .writes("trail")
                         .composite(SkBlendMode::kPlus, 1.0f);

  // The first frame has no previous, so the trail is only the picture.
  Runtime::cpu()->execute(workOf(main), bodies.view(), targets);
  Runtime::cpu()->execute(workOf(trail), bodies.view(), targets);
  const int first = painted(targets.image("trail"));
  targets.endFrame();

  // The second adds last frame's trail to it, and the ink can only grow.
  Runtime::cpu()->execute(workOf(main), bodies.view(), targets);
  Runtime::cpu()->execute(workOf(trail), bodies.view(), targets);
  EXPECT_GE(painted(targets.image("trail")), first);

  SkBitmap once;
  SkBitmap twice;
  once.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  twice.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  ASSERT_TRUE(targets.previous("trail")->readPixels(once.pixmap(), 0, 0));
  ASSERT_TRUE(targets.image("trail")->readPixels(twice.pixmap(), 0, 0));
  const int x = kExtent.width() / 4;
  const int y = kExtent.height() / 2;
  EXPECT_NE(once.getColor(x, y), twice.getColor(x, y));
}

TEST(WorldFrame, AComputePassCooksItsChainIntoThePointSetItWrites) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  const std::vector<glm::vec3> path = {
      {-40, 0, 0}, {0, 30, 0}, {40, 0, 0}, {0, -30, 0}};
  const Pass cook = computePass("cook").writes("motes").chain(
      geometry::mesh::pop::on(path).count(48).spread(3.0f));
  Runtime::cpu()->execute(workOf(cook), bodies.view(), targets);

  const Cloud* motes = targets.points("motes");
  ASSERT_NE(motes, nullptr);
  EXPECT_EQ(motes->size(), 48u);
}

TEST(WorldFrame, AGeometryPassStampsThePointSetsItReads) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  const std::vector<glm::vec3> path = {
      {-40, 0, 0}, {0, 30, 0}, {40, 0, 0}, {0, -30, 0}};
  const Pass cook = computePass("cook").writes("motes").chain(
      geometry::mesh::pop::on(path).count(48).spread(3.0f));
  Runtime::cpu()->execute(workOf(cook), bodies.view(), targets);

  View empty = bodies.view();
  empty.draws = {};
  const Pass beads =
      geometryPass("beads").reads("motes").writes("colour").stamp(square(3.0f));
  Runtime::cpu()->execute(workOf(beads), empty, targets);
  EXPECT_GT(painted(targets.image("colour")), 0);
}

// A STAMPED SET IS FORMED ONCE per distinct (cloud, stamp). A pass
// draws the stamps of the sets it reads every frame, and a set that has
// not moved between two of them must not be instanced a second time —
// the whole cloud times the stamp's vertices is what forming one costs,
// and paying it per frame is paying it for nothing.
TEST(WorldFrame, AStillPointSetIsStampedOnceHoweverManyFramesDrawIt) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  const std::vector<glm::vec3> path = {
      {-40, 0, 0}, {0, 30, 0}, {40, 0, 0}, {0, -30, 0}};
  const Pass cook = computePass("cook").writes("motes").chain(
      geometry::mesh::pop::on(path).count(48).spread(3.0f));
  Runtime::cpu()->execute(workOf(cook), bodies.view(), targets);

  View empty = bodies.view();
  empty.draws = {};
  const Pass beads =
      geometryPass("beads").reads("motes").writes("colour").stamp(square(3.0f));

  for (int frame = 0; frame < 3; ++frame) {
    Runtime::cpu()->execute(workOf(beads), empty, targets);
    EXPECT_EQ(targets.stampings(), 1u) << "after frame " << frame;
    targets.endFrame();
  }

  // …and a set that HAS moved is a second stamping, which is what makes
  // the first assertion a statement about the content and not about the
  // cache never missing.
  Cloud* motes = targets.points("motes");
  ASSERT_NE(motes, nullptr);
  motes->positions.front().x += 1.0f;
  Runtime::cpu()->execute(workOf(beads), empty, targets);
  EXPECT_EQ(targets.stampings(), 2u);
}

TEST(WorldFrame, ADeclaredBodyIsHandedTheExtractedViewAndTheTargets) {
  Bodies bodies;
  Targets targets = targetsAt(kExtent);
  size_t seen = 0;
  std::string firstKey;
  SkISize extent{0, 0};
  const Pass hand = geometryPass("hand").writes("colour").body(
      [&](const View& view, Targets& into) {
        seen = view.draws.size();
        if (!view.draws.empty()) firstKey = std::string(view.draws.front().key);
        extent = view.extent;
        if (SkCanvas* canvas = into.canvas("colour"))
          canvas->clear(SkColor4f{1.0f, 0.0f, 0.0f, 1.0f});
      });
  Runtime::cpu()->execute(workOf(hand), bodies.view(), targets);

  EXPECT_EQ(seen, 2u);
  EXPECT_EQ(firstKey, "left");
  EXPECT_EQ(extent, kExtent);
  EXPECT_EQ(painted(targets.image("colour")),
            kExtent.width() * kExtent.height());
}

TEST(WorldFrame, TwoNamesOnOneSlotShareTheSurface) {
  Targets targets = targetsAt(kExtent);
  targets.bind("first", 0);
  targets.bind("second", 0);
  targets.canvas("first")->clear(SkColor4f{0.0f, 1.0f, 0.0f, 1.0f});
  EXPECT_EQ(targets.surfaces(), 1);
  EXPECT_EQ(painted(targets.image("second")),
            kExtent.width() * kExtent.height());
}

TEST(WorldFrame, AFrameWithNoPassesIsItsScene) {
  const Frame frame = Element().key("set");
  EXPECT_TRUE(frame.passes().empty());
  EXPECT_EQ(frame.scene().node()->key, "set");
}
