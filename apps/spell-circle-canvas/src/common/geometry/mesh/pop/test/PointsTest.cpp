/** @file
 * Point clouds: the generators write their conventional lanes, the
 * modifiers move positions exactly as the operators of the same name do,
 * a stamp instanced at every point is scaled and turned by the lanes it
 * names, and a splat takes the atlas cell the cloud carries.
 *
 * A cloud of billboards is ONE canvas draw, so the properties a sequence
 * of draws used to carry — the back-to-front order, each point's tint,
 * size and cell, and the requested blend — are read here off the pixels
 * the single batch lays down.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/utils/SkNoDrawCanvas.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/pop/Points.h"
#include "sigilgeometry/mesh/pop/Pop.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

TEST(Points, EveryGeneratorWritesTheConventionalLanesForItsFigure) {
  curve::Spline3 line;
  line.type = curve::Spline3::Type::Linear;
  line.points = {{0, 0, 0}, {90, 0, 0}};
  Cloud onCurve = points::onSpline(line, 10);
  EXPECT_EQ(onCurve.size(), 10u);
  ASSERT_TRUE(onCurve.scalarIf("t"));
  EXPECT_NEAR(onCurve.scalarIf("t")->back(), 1, 1e-4);
  ASSERT_TRUE(onCurve.vectorIf("normal"));

  Cloud lattice = points::grid({0, 0, 0}, {90, 0, 0}, {0, 60, 0}, 4, 3);
  EXPECT_EQ(lattice.size(), 12u);
  EXPECT_NEAR(lattice.vectorIf("normal")->front().z, 1, 1e-4);

  Cloud circle = points::ring({0, 0, 0}, 50, 8);
  EXPECT_EQ(circle.size(), 8u);
  // A ring is laid out in the plane perpendicular to its axis, and the
  // default axis is +y, so an unaxised ring lives in the xz plane at y = 0.
  for (const glm::vec3& p : circle.positions) {
    EXPECT_NEAR(glm::length(p), 50, 1e-2);
    EXPECT_NEAR(p.y, 0, 1e-3);
  }

  Cloud box = points::scatterBox({0, 0, 0}, {10, 10, 10}, 100, 3);
  EXPECT_EQ(box.size(), 100u);
  for (const glm::vec3& p : box.positions) {
    EXPECT_GE(p.x, 0);
    EXPECT_LE(p.x, 10);
  }
}

// ONE VERB IS ONE FIELD. `jitter` and `displaceNoise` are the `Jitter`
// and `Noise` operators reached for without a chain, so a cloud moved
// each way has to land on the same floats — not near them. Two
// arithmetics under one name would mean nobody could say which of them
// a picture came from.
TEST(Points, AModifierMovesPointsExactlyAsItsOperatorDoes) {
  Cloud seeded = points::scatterBox({-60, -20, -40}, {60, 20, 40}, 250, 3);

  Cloud jittered = seeded;
  points::jitter(jittered, 14.0f, 21u);
  const Cloud chained = pop::cook(pop::Chain{
      pop::PointSet{seeded}, pop::Jitter{pop::Attribute::P, 14.0f, 21u}});
  ASSERT_EQ(chained.size(), jittered.size());
  for (size_t i = 0; i < jittered.size(); ++i)
    EXPECT_EQ(chained.positions[i], jittered.positions[i]) << "point " << i;

  Cloud drifted = seeded;
  points::displaceNoise(drifted, 9.0f, 0.02f, 5u);
  const Cloud driftedChain = pop::cook(pop::Chain{
      pop::PointSet{seeded}, pop::Noise{pop::Attribute::P, 9.0f, 0.02f, 5.0f}});
  ASSERT_EQ(driftedChain.size(), drifted.size());
  for (size_t i = 0; i < drifted.size(); ++i)
    EXPECT_EQ(driftedChain.positions[i], drifted.positions[i]) << "point " << i;
}

TEST(Points, ScatteringOnAMeshPutsEveryPointOnItsSurface) {
  const Mesh quad = mesh::quad(100, 100);  // z = 0 plane
  Cloud cloud = points::onMesh(quad, 64, 5);
  ASSERT_EQ(cloud.size(), 64u);
  for (const glm::vec3& p : cloud.positions) {
    EXPECT_NEAR(p.z, 0, 1e-4);
    EXPECT_LE(std::abs(p.x), 50.01f);
  }
  ASSERT_TRUE(cloud.vectorIf("normal"));
  EXPECT_NEAR((*cloud.vectorIf("normal"))[0].z, 1, 1e-3);
}

TEST(Points, AStampIsScaledOrientedAndTintedByTheLanesItIsToldTo) {
  Cloud cloud = points::ring({0, 0, 0}, 80, 6);
  std::vector<float>& size = cloud.scalar("size", 1);
  size[0] = 2;
  std::vector<glm::vec4>& tint = cloud.color("tint");
  tint[0] = {1, 0, 0, 1};
  const Mesh stamp = mesh::quad(10, 10);
  points::InstanceOptions options;
  options.scaleLane = "size";
  options.tintLane = "tint";
  options.orientLane = "normal";
  const Mesh merged = points::instance(cloud, stamp, options);
  EXPECT_EQ(merged.vertexCount(), 6u * stamp.vertexCount());
  EXPECT_EQ(merged.triangleCount(), 6u * stamp.triangleCount());
  ASSERT_EQ(merged.colors.size(), merged.vertexCount());
  EXPECT_NEAR(merged.colors[0].r, 1, 1e-4);
  EXPECT_NEAR(merged.colors[0].g, 0, 1e-4);
  // The scale lane multiplies the stamp about its own point. An unscaled
  // 10x10 quad has a 14.1 diagonal, so the first stamp — the only one whose
  // "size" was set to 2 — has to measure more than that.
  glm::vec3 lo, hi;
  Mesh first;
  first.positions.assign(merged.positions.begin(),
                         merged.positions.begin() + 4);
  first.bounds(&lo, &hi);
  EXPECT_GT(glm::length(hi - lo), 14.0f);
}

TEST(Points, AppendPadsLanesWithConventionalDefaults) {
  // When a lane exists on only one side of an append, the other side is
  // padded by what the lane NAME means, not by a generic zero: "size" pads
  // with 1, because 0 would make those instances invisible, and "Tex" pads
  // with the identity uv window (0,0,1,1) rather than white.
  Cloud a;
  a.positions = {{0, 0, 0}, {1, 0, 0}};
  Cloud b;
  b.positions = {{2, 0, 0}, {3, 0, 0}};
  b.scalar("size", 2);
  b.color("Tex", {0.5f, 0.5f, 0.5f, 0.5f});
  a.append(b);
  ASSERT_EQ(a.size(), 4u);
  const std::vector<float>* size = a.scalarIf("size");
  ASSERT_TRUE(size);
  ASSERT_EQ(size->size(), 4u);
  EXPECT_FLOAT_EQ((*size)[0], 1.0f);  // a's side: scale 1, visible
  EXPECT_FLOAT_EQ((*size)[1], 1.0f);
  EXPECT_FLOAT_EQ((*size)[2], 2.0f);  // b's actual values
  EXPECT_FLOAT_EQ((*size)[3], 2.0f);
  const std::vector<glm::vec4>* tex = a.colorIf("Tex");
  ASSERT_TRUE(tex);
  ASSERT_EQ(tex->size(), 4u);
  for (size_t i = 0; i < 2; ++i) {  // a's side: identity uv window
    EXPECT_FLOAT_EQ((*tex)[i].x, 0.0f);
    EXPECT_FLOAT_EQ((*tex)[i].y, 0.0f);
    EXPECT_FLOAT_EQ((*tex)[i].z, 1.0f);
    EXPECT_FLOAT_EQ((*tex)[i].w, 1.0f);
  }
  EXPECT_FLOAT_EQ((*tex)[2].x, 0.5f);  // b's actual window
}

namespace {

/** A FLAT sprite: one opaque white rectangle, square unless asked
 *  otherwise. The default soft dot fades to nothing at its rim, so every
 *  assertion about a colour would be an assertion about a threshold; a
 *  flat sprite carries its tint exactly. */
sk_sp<SkImage> flatSprite(int width = 32, int height = 32) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  bitmap.eraseColor(SK_ColorWHITE);
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** The canvas a cloud splats onto, over black, and its pixels read back. */
struct Plate {
  explicit Plate(int width = 200, int height = 200)
      : surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height))) {
    surface->getCanvas()->clear(SK_ColorBLACK);
  }
  SkCanvas& canvas() { return *surface->getCanvas(); }
  SkBitmap pixels() const {
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  }
  sk_sp<SkSurface> surface;
};

/** Every draw a splat makes, counted with no device and no pixels. */
class CountingCanvas final : public SkNoDrawCanvas {
 public:
  using SkNoDrawCanvas::SkNoDrawCanvas;
  int vertexLists = 0;
  int imageRectangles = 0;
  int atlases = 0;

 protected:
  void onDrawVerticesObject(const SkVertices*, SkBlendMode,
                            const SkPaint&) override {
    ++vertexLists;
  }
  void onDrawImageRect2(const SkImage*, const SkRect&, const SkRect&,
                        const SkSamplingOptions&, const SkPaint*,
                        SrcRectConstraint) override {
    ++imageRectangles;
  }
  void onDrawAtlas2(const SkImage*, const SkRSXform[], const SkRect[],
                    const SkColor[], int, SkBlendMode, const SkSamplingOptions&,
                    const SkRect*, const SkPaint*) override {
    ++atlases;
  }
};

}  // namespace

TEST(Points, ACloudSplattedAsBillboardsReachesTheCanvas) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 150));
  surface->getCanvas()->clear(SK_ColorBLACK);
  Cloud cloud = points::ring({0, 0, 0}, 40, 12, {0, 0, 1});
  camera::Camera camera;
  camera.eye = {0, 0, 200};
  points::BillboardStyle style;
  style.size = 24;
  style.tint = {0, 1, 0, 1};
  points::drawBillboards(*surface->getCanvas(), cloud, camera, {200, 150},
                         style);
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  // Twelve 24-pixel billboards cover thousands of pixels; the threshold is
  // set low because the point is only that the cloud reached the canvas at
  // all — an empty or entirely off-screen draw is what it must catch.
  int lit = 0;
  for (int y = 0; y < 150; ++y)
    for (int x = 0; x < 200; ++x)
      if (SkColorGetG(bm.getColor(x, y)) > 30) ++lit;
  EXPECT_GT(lit, 200);
}

TEST(Points, BillboardsSplatTheAtlasCellTheCloudCarries) {
  // A sprite SHEET splats as a field of different sprites, and which one
  // each point takes is the window a pop::AtlasCell operation wrote into "Tex".
  // Without the lane every point takes the whole sheet, which is the
  // failure this reads: a splat showing all four quadrants at once.
  //
  // The sheet: four quadrants, one colour each, no antialiasing anywhere.
  constexpr int kSheet = 64;
  constexpr int kHalf = kSheet / 2;
  sk_sp<SkImage> sheet;
  {
    sk_sp<SkSurface> sheetSurface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSheet, kSheet));
    SkCanvas* c = sheetSurface->getCanvas();
    const auto cell = [&](int x, int y, SkColor colour) {
      SkPaint p;
      p.setColor(colour);
      c->drawIRect(SkIRect::MakeXYWH(x, y, kHalf, kHalf), p);
    };
    cell(0, 0, SK_ColorWHITE);
    cell(kHalf, 0, SK_ColorRED);
    cell(0, kHalf, SK_ColorGREEN);
    cell(kHalf, kHalf, SK_ColorBLUE);
    sheet = sheetSurface->makeImageSnapshot();
  }

  // One point, dead centre, carrying the BOTTOM-LEFT cell — green.
  Cloud cloud;
  cloud.positions = {{0, 0, 0}};
  cloud.color("Tex") = {{0.0f, 0.5f, 0.5f, 0.5f}};
  camera::Camera camera;
  camera.eye = {0, 0, 200};

  const auto splatted = [&](const std::string& lane) {
    Plate plate(120, 120);
    points::BillboardStyle style;
    style.sprite = sheet;
    style.size = 60;
    style.additive = false;
    style.perspective = false;
    style.textureLane = lane;
    points::drawBillboards(plate.canvas(), cloud, camera, {120, 120}, style);
    return plate.pixels();
  };
  const auto centrePixel = [&](const std::string& lane) {
    return splatted(lane).getColor(60, 60);
  };
  // The cell is one flat colour, so its centre IS its colour.
  EXPECT_EQ(centrePixel("Tex"), SK_ColorGREEN) << "the window was not read";
  // Unnamed, the splat is the whole sheet and its centre is the seam
  // between four different colours — anything but the cell's own.
  EXPECT_NE(centrePixel(""), SK_ColorGREEN);

  // ...AND NOT ONE TEXEL OF A NEIGHBOUR ANYWHERE. One shader samples the
  // whole sheet, so the cell's own edges are where a linear filter would
  // reach the cell next door: a 60-pixel splat centred on a 120-pixel
  // plate spans 30 to 90, and its outermost pixels are the ones that
  // would show the blue cell to the right or the white one above.
  const SkBitmap cell = splatted("Tex");
  EXPECT_EQ(cell.getColor(30, 60), SK_ColorGREEN) << "the left edge";
  EXPECT_EQ(cell.getColor(89, 60), SK_ColorGREEN) << "the right edge bled";
  EXPECT_EQ(cell.getColor(60, 30), SK_ColorGREEN) << "the top edge bled";
  EXPECT_EQ(cell.getColor(60, 89), SK_ColorGREEN) << "the bottom edge";

  // A degenerate window is not a cell: an atlas operation that never ran, or a
  // lane padded with zeros, takes the whole image rather than splatting
  // a sliver of one texel over everything.
  cloud.color("Tex") = {{0.0f, 0.0f, 0.0f, 0.0f}};
  EXPECT_EQ(centrePixel("Tex"), centrePixel(""));
}

TEST(Points, BillboardsInOneBatchKeepTheirBackToFrontOrder) {
  // Two opaque splats at the same place, one behind the other: the near
  // one is what shows. The whole cloud is one draw, so the order is
  // carried by the vertex list rather than by a sequence of draws —
  // a batch built in cloud order, or one whose quads are reordered,
  // reads the far splat here.
  camera::Camera camera;
  camera.eye = {0, 0, 200};
  points::BillboardStyle style;
  style.sprite = flatSprite();
  style.size = 40;
  style.additive = false;
  style.perspective = false;
  style.tintLane = "tint";

  const auto centreOf = [&](float firstDepth, float secondDepth) {
    Cloud cloud;
    cloud.positions = {{0, 0, firstDepth}, {0, 0, secondDepth}};
    cloud.color("tint") = {{1, 0, 0, 1}, {0, 1, 0, 1}};
    Plate plate;
    points::drawBillboards(plate.canvas(), cloud, camera, {200, 200}, style);
    return plate.pixels().getColor(100, 100);
  };
  // The second point stands nearer the eye, so green covers red...
  EXPECT_EQ(centreOf(-30.0f, 30.0f), SK_ColorGREEN);
  // ...and the same two points with their depths exchanged read the
  // other way about.
  EXPECT_EQ(centreOf(30.0f, -30.0f), SK_ColorRED);
}

TEST(Points, EverySplatTakesItsOwnTintFromTheLane) {
  // One batch, one sheet, two colours: the tint is per sprite, not per
  // draw. An all-white lane is dropped rather than carried, so the drop
  // has to be identity — the sprite's own colour, not a changed one.
  camera::Camera camera;
  camera.eye = {0, 0, 200};
  Cloud cloud;
  cloud.positions = {{-30, 0, 0}, {30, 0, 0}};
  points::BillboardStyle style;
  style.sprite = flatSprite();
  style.size = 20;
  style.additive = false;
  style.perspective = false;
  style.tintLane = "tint";

  const std::optional<SkPoint> left =
      camera.project(cloud.positions[0], {200, 200});
  const std::optional<SkPoint> right =
      camera.project(cloud.positions[1], {200, 200});
  ASSERT_TRUE(left && right);
  const auto splatted = [&]() {
    Plate plate;
    points::drawBillboards(plate.canvas(), cloud, camera, {200, 200}, style);
    const SkBitmap pixels = plate.pixels();
    return std::pair{
        pixels.getColor((int)std::lround(left->fX), (int)std::lround(left->fY)),
        pixels.getColor((int)std::lround(right->fX),
                        (int)std::lround(right->fY))};
  };

  cloud.color("tint") = {{1, 0, 0, 1}, {0, 0, 1, 1}};
  auto [red, blue] = splatted();
  EXPECT_EQ(red, SK_ColorRED);
  EXPECT_EQ(blue, SK_ColorBLUE);

  cloud.color("tint") = {{1, 1, 1, 1}, {1, 1, 1, 1}};
  auto [firstWhite, secondWhite] = splatted();
  EXPECT_EQ(firstWhite, SK_ColorWHITE);
  EXPECT_EQ(secondWhite, SK_ColorWHITE);
}

TEST(Points, EverySplatTakesItsOwnSizeFromTheLane) {
  // With perspective off a splat is its lane's multiple of the style's
  // size, in pixels, so the second covers exactly twice the width of the
  // first. One uniform scale for the batch reads two equal splats here.
  camera::Camera camera;
  camera.eye = {0, 0, 200};
  Cloud cloud;
  cloud.positions = {{-40, 0, 0}, {40, 0, 0}};
  cloud.scalar("size", 1)[1] = 2;
  points::BillboardStyle style;
  style.sprite = flatSprite();
  style.size = 20;
  style.sizeLane = "size";
  style.additive = false;
  style.perspective = false;

  Plate plate;
  points::drawBillboards(plate.canvas(), cloud, camera, {200, 200}, style);
  const std::optional<SkPoint> narrowAt =
      camera.project(cloud.positions[0], {200, 200});
  const std::optional<SkPoint> wideAt =
      camera.project(cloud.positions[1], {200, 200});
  ASSERT_TRUE(narrowAt && wideAt);
  const SkBitmap pixels = plate.pixels();
  const int row = (int)std::lround(narrowAt->fY);
  int narrow = 0, wide = 0;
  for (int x = 0; x < 200; ++x)
    if (SkColorGetR(pixels.getColor(x, row)) > 128) ++(x < 100 ? narrow : wide);
  const auto down = [&](const SkPoint& at) {
    int lit = 0;
    for (int y = 0; y < 200; ++y)
      if (SkColorGetR(pixels.getColor((int)std::lround(at.fX), y)) > 128) ++lit;
    return lit;
  };
  // BOTH AXES, because the lane scales a splat as a square: a width read
  // on its own cannot tell that from a splat stretched along one axis.
  // An edge pixel the splat only partly covers may fall either side of
  // the threshold, which is the whole of the tolerance.
  EXPECT_NEAR(narrow, 20, 1);
  EXPECT_NEAR(wide, 40, 1);
  EXPECT_NEAR(down(*narrowAt), 20, 1);
  EXPECT_NEAR(down(*wideAt), 40, 1);
}

TEST(Points, ASplatIsSquareWhateverTheAspectOfItsCell) {
  // A cell need not be square — a sheet wider than it is tall, or an
  // atlas grid with more columns than rows — but the splat it draws is.
  // The batch's uniform scale answers the cell's width and its size lane
  // answers the height, so a batch that dropped the lane draws this
  // sprite four times as wide as it is tall.
  camera::Camera camera;
  camera.eye = {0, 0, 200};
  Cloud cloud;
  cloud.positions = {{0, 0, 0}};
  cloud.color("Tex") = {{0.0f, 0.0f, 0.5f, 1.0f}};  // the sheet's left half
  points::BillboardStyle style;
  style.sprite = flatSprite(64, 16);
  style.size = 40;
  style.additive = false;
  style.perspective = false;

  const auto extent = [&](const std::string& lane) {
    style.textureLane = lane;
    Plate plate;
    points::drawBillboards(plate.canvas(), cloud, camera, {200, 200}, style);
    const SkBitmap pixels = plate.pixels();
    int across = 0, down = 0;
    for (int i = 0; i < 200; ++i) {
      if (SkColorGetR(pixels.getColor(i, 100)) > 128) ++across;
      if (SkColorGetR(pixels.getColor(100, i)) > 128) ++down;
    }
    return std::pair{across, down};
  };
  // The whole sheet as one four-to-one cell...
  const auto [wholeAcross, wholeDown] = extent("");
  EXPECT_NEAR(wholeAcross, 40, 1);
  EXPECT_NEAR(wholeDown, 40, 1);
  // ...and a two-to-one window of it, which the half-texel inset leaves
  // unsquare as well.
  const auto [windowAcross, windowDown] = extent("Tex");
  EXPECT_NEAR(windowAcross, 40, 1);
  EXPECT_NEAR(windowDown, 40, 1);
}

TEST(Points, AdditiveSplatsAccumulateWhereTheyOverlap) {
  // Additive is the whole colour model of a glow: two half-bright splats
  // over black are brighter where they meet. A batch that lost the
  // requested blend paints the second over the first instead, and the
  // overlap reads the same as either splat alone.
  camera::Camera camera;
  camera.eye = {0, 0, 200};
  Cloud cloud;
  cloud.positions = {{-5, 0, 0}, {5, 0, 0}};
  points::BillboardStyle style;
  style.sprite = flatSprite();
  style.size = 40;
  style.tint = {0.5f, 0.5f, 0.5f, 1};
  style.additive = true;
  style.perspective = false;

  Plate plate;
  points::drawBillboards(plate.canvas(), cloud, camera, {200, 200}, style);
  const SkBitmap pixels = plate.pixels();
  const int row = 100;
  const int alone = SkColorGetR(pixels.getColor(80, row));
  const int overlap = SkColorGetR(pixels.getColor(100, row));
  const int alsoAlone = SkColorGetR(pixels.getColor(120, row));
  EXPECT_NEAR(alone, 128, 2);
  EXPECT_NEAR(alsoAlone, 128, 2);
  EXPECT_GT(overlap, alone + 64);
}

TEST(Points, EverySplatSurvivesTheBatchChunkBoundary) {
  // One vertex list indexes a bounded number of sprites, so a bigger
  // cloud is cut into chunks — and the LAST chunk is the one an
  // off-by-one loses. The cloud is one point past the cut, with that
  // point somewhere else, so a lost tail is a place that is not lit.
  constexpr size_t kCount = 16001;
  camera::Camera camera;
  camera.eye = {0, 0, 200};
  Cloud cloud;
  cloud.positions.assign(kCount, {-20, 0, 0});
  cloud.positions.back() = {20, 0, 0};
  points::BillboardStyle style;
  style.sprite = flatSprite();
  style.size = 8;
  style.additive = false;
  style.depthSort = false;
  style.perspective = false;

  Plate plate;
  points::drawBillboards(plate.canvas(), cloud, camera, {200, 200}, style);
  const std::optional<SkPoint> pile = camera.project({-20, 0, 0}, {200, 200});
  const std::optional<SkPoint> tail = camera.project({20, 0, 0}, {200, 200});
  ASSERT_TRUE(pile && tail);
  const SkBitmap pixels = plate.pixels();
  EXPECT_EQ(
      pixels.getColor((int)std::lround(pile->fX), (int)std::lround(pile->fY)),
      SK_ColorWHITE)
      << "the first chunk";
  EXPECT_EQ(
      pixels.getColor((int)std::lround(tail->fX), (int)std::lround(tail->fY)),
      SK_ColorWHITE)
      << "…and the tail";
}

TEST(Points, ADenseCloudIsOneCanvasDraw) {
  // THE POINT OF THE BATCH, read without a device: however many points a
  // cloud holds, and whatever cells they take, the canvas is asked for
  // one draw. A splat per point still paints the same picture, so no
  // pixel test catches the regression this one does.
  Cloud cloud = points::scatterBox({-60, -60, -60}, {60, 60, 60}, 1000, 3);
  camera::Camera camera;
  camera.eye = {0, 0, 400};
  points::BillboardStyle style;
  style.sprite = flatSprite();
  style.size = 6;

  CountingCanvas counting(200, 200);
  points::drawBillboards(counting, cloud, camera, {200, 200}, style);
  EXPECT_EQ(counting.vertexLists, 1);
  EXPECT_EQ(counting.imageRectangles, 0);
  EXPECT_EQ(counting.atlases, 0) << "the native atlas op draws nothing on "
                                    "some backends";

  // Sixteen different cells of one sheet are still one draw: a cell is
  // per sprite in the batch, not per draw.
  std::vector<glm::vec4>& windows = cloud.color("Tex");
  for (size_t i = 0; i < windows.size(); ++i)
    windows[i] = {(float)(i % 4) * 0.25f, (float)(i / 4 % 4) * 0.25f, 0.25f,
                  0.25f};
  style.textureLane = "Tex";
  CountingCanvas withCells(200, 200);
  points::drawBillboards(withCells, cloud, camera, {200, 200}, style);
  EXPECT_EQ(withCells.vertexLists, 1);
  EXPECT_EQ(withCells.imageRectangles, 0);
}

TEST(Points, AnInstancedFacingLaneAgreesWithTheCameraFacingTransform) {
  // The two ways of orienting a quad must agree: a one-point cloud whose
  // "facing" lane holds the eye direction, stamped through points::quads(),
  // produces the same vertices as transforming a quad by faceCamera. If
  // they drift apart, a scene mixing billboards with instanced panels shows
  // two different orientations for the same direction.
  const glm::vec3 at = {40, -25, 60};
  const glm::vec3 eye = {0, 200, 1150};
  Cloud one;
  one.positions = {at};
  one.vector("facing") = {glm::normalize(eye - at)};
  points::InstanceOptions options;
  options.orientLane = "facing";
  const Mesh stamped = points::quads(one, 170, 112, options);
  const Mesh quad = mesh::quad(170, 112);
  const glm::mat4 m = camera::faceCamera(eye, at);
  ASSERT_EQ(stamped.positions.size(), quad.positions.size());
  for (size_t i = 0; i < quad.positions.size(); ++i) {
    const glm::vec3 viaMatrix = glm::vec3(m * glm::vec4(quad.positions[i], 1));
    EXPECT_NEAR(glm::length(viaMatrix - stamped.positions[i]), 0.0f, 1e-4f)
        << "vertex " << i;
  }
}
