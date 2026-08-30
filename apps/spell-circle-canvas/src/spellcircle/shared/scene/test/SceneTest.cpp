// Tests for the Qt-free SpellCircle scene core. Wire payloads are built with
// the generated FlatBuffers API and run through the same
// verify -> decode -> resolve path a UDP datagram takes, so what is asserted
// here is the behavior both receiver apps share. Where geometry only exists
// at draw time (box placement depends on the measured label width), the test
// observes SceneRenderer through a recording canvas that captures rectangle
// draws instead of rasterizing pixels.

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <sigilgeometry/path/Contour.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "SceneGeometry.h"
#include "SceneLabels.h"
#include "SceneModel.h"
#include "SceneRenderer.h"
#include "SpellCircle_generated.h"

namespace {

using spellcircle::SceneDocument;

std::vector<uint8_t> finishScene(
    flatbuffers::FlatBufferBuilder& fbb,
    flatbuffers::Offset<SpellCircle::Scene> scene) {
  SpellCircle::FinishSceneBuffer(fbb, scene);
  return {fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize()};
}

/** One box whose anchor point rides a radius-0 circle at (anchorX, anchorY),
 *  so the box's anchor resolves to exactly that coordinate. */
std::vector<uint8_t> sceneWithBox(float anchorX, float anchorY,
                                  const std::string& label) {
  flatbuffers::FlatBufferBuilder fbb;
  const SpellCircle::Vec2 pos(anchorX, anchorY);
  const auto circle = SpellCircle::CreateCircleDirect(fbb, &pos, nullptr,
                                                      /*radius=*/0);
  const auto point = SpellCircle::CreatePointDirect(fbb, nullptr, circle);
  const auto box = SpellCircle::CreateBoxDirect(fbb, label.c_str(), point);
  const std::vector<flatbuffers::Offset<SpellCircle::Box>> boxes{box};
  return finishScene(
      fbb, SpellCircle::CreateSceneDirect(fbb, nullptr, nullptr, &boxes));
}

SceneDocument decodeScene(const std::vector<uint8_t>& bytes) {
  EXPECT_TRUE(spellcircle::verifyScenePayload(bytes.data(), bytes.size()));
  SceneDocument document;
  document.decode(bytes.data(), bytes.size());
  return document;
}

/** A no-device canvas that records every rectangle drawn on it. The scene
 *  renderer draws each box's clear/fill/stroke as drawRect calls with one
 *  shared bounds, so the recorded rects are the boxes' resolved geometry;
 *  all other primitives fall through to the base no-op canvas. */
class RectRecordingCanvas : public SkCanvas {
 public:
  RectRecordingCanvas(int width, int height) : SkCanvas(width, height) {}

  std::vector<SkRect> rects;

 protected:
  void onDrawRect(const SkRect& rect, const SkPaint&) override {
    rects.push_back(rect);
  }
};

}  // namespace

// ── SceneModel ─────────────────────────────────────────────────────────────

TEST(SceneModel, RejectsTruncatedPayload) {
  const uint8_t garbage[] = {0xde, 0xad, 0xbe, 0xef};
  EXPECT_FALSE(spellcircle::verifyScenePayload(garbage, sizeof(garbage)));
}

TEST(SceneModel, SharedPointTablesDecodeToOneEntity) {
  // FlatBuffers is zero-copy, so one Point table referenced by both an Edge
  // and a Box must decode to a single shared entity, not one per reference.
  flatbuffers::FlatBufferBuilder fbb;
  const SpellCircle::Vec2 pos(10.0f, 20.0f);
  const auto circle = SpellCircle::CreateCircleDirect(fbb, &pos, nullptr, 0);
  const auto shared = SpellCircle::CreatePointDirect(fbb, nullptr, circle);
  const auto other = SpellCircle::CreatePointDirect(fbb, nullptr, circle);
  const auto edge = SpellCircle::CreateEdge(fbb, shared, other);
  const auto box = SpellCircle::CreateBoxDirect(fbb, "b", shared);
  const std::vector<flatbuffers::Offset<SpellCircle::Edge>> edges{edge};
  const std::vector<flatbuffers::Offset<SpellCircle::Box>> boxes{box};
  const auto bytes = finishScene(
      fbb, SpellCircle::CreateSceneDirect(fbb, nullptr, &edges, &boxes));

  ASSERT_TRUE(spellcircle::verifyScenePayload(bytes.data(), bytes.size()));
  SceneDocument document;
  const spellcircle::SceneStats stats =
      document.decode(bytes.data(), bytes.size());

  EXPECT_EQ(stats.circles, 0);  // circles embedded in points are not top-level
  EXPECT_EQ(stats.edges, 1);
  EXPECT_EQ(stats.boxes, 1);
  EXPECT_EQ(document.registry().view<spellcircle::PointComponent>().size(), 2u);
}

// ── SceneGeometry ──────────────────────────────────────────────────────────

TEST(SceneGeometry, PointFractionsMeasureClockwiseFromTwelveOClock) {
  // Pins the wire convention shared by Point.position and Circle.text_start:
  // a fraction of one full turn measured CLOCKWISE from 12 o'clock, so 0.0
  // is the topmost point of the circle and 0.25 is 3 o'clock.
  flatbuffers::FlatBufferBuilder fbb;
  const SpellCircle::Vec2 center(500.0f, 500.0f);
  const auto circle =
      SpellCircle::CreateCircleDirect(fbb, &center, nullptr, /*radius=*/400);
  const auto north =
      SpellCircle::CreatePointDirect(fbb, "north", circle, /*position=*/0.0f);
  const auto east =
      SpellCircle::CreatePointDirect(fbb, "east", circle, /*position=*/0.25f);
  const auto edge = SpellCircle::CreateEdge(fbb, north, east);
  const std::vector<flatbuffers::Offset<SpellCircle::Edge>> edges{edge};
  const auto bytes =
      finishScene(fbb, SpellCircle::CreateSceneDirect(fbb, nullptr, &edges));

  const SceneDocument document = decodeScene(bytes);
  const spellcircle::ResolvedScene resolved =
      spellcircle::resolveScene(document, 1000.0f, 1000.0f);

  ASSERT_EQ(resolved.pointLabels.size(), 2u);
  const auto labelNamed =
      [&](std::string_view value) -> const spellcircle::ResolvedBox* {
    const auto found =
        std::find_if(resolved.pointLabels.begin(), resolved.pointLabels.end(),
                     [&](const auto& label) { return label.value == value; });
    return found != resolved.pointLabels.end() ? &*found : nullptr;
  };

  const auto* top = labelNamed("north");
  ASSERT_NE(top, nullptr);
  EXPECT_NEAR(top->anchor.x, 500.0f, 0.01f);
  EXPECT_NEAR(top->anchor.y, 100.0f, 0.01f);

  const auto* right = labelNamed("east");
  ASSERT_NE(right, nullptr);
  EXPECT_NEAR(right->anchor.x, 900.0f, 0.01f);
  EXPECT_NEAR(right->anchor.y, 500.0f, 0.01f);
}

TEST(SceneGeometry, TextStartSharesThePointOriginAndResolvesToTheContour) {
  // text_start rides the wire in the same convention as Point.position —
  // clockwise from 12 o'clock — and resolveScene() converts it into the
  // drawn contour's own parameterisation, whose start is 3 o'clock (pinned
  // by SceneLabels.CircleContoursStartAtThreeOClock). So wire 0.0 (top)
  // must resolve to contour fraction 0.75, and wire 0.25 (3 o'clock) to
  // the contour's own start at 0.0.
  flatbuffers::FlatBufferBuilder fbb;
  const SpellCircle::Vec2 center(500.0f, 500.0f);
  const auto top = SpellCircle::CreateCircleDirect(
      fbb, &center, "top", /*radius=*/400, /*text_start=*/0.0f);
  const auto east = SpellCircle::CreateCircleDirect(
      fbb, &center, "east", /*radius=*/400, /*text_start=*/0.25f);
  const std::vector<flatbuffers::Offset<SpellCircle::Circle>> circles{top,
                                                                      east};
  const auto bytes =
      finishScene(fbb, SpellCircle::CreateSceneDirect(fbb, &circles));

  const SceneDocument document = decodeScene(bytes);
  const spellcircle::ResolvedScene resolved =
      spellcircle::resolveScene(document, 1000.0f, 1000.0f);

  ASSERT_EQ(resolved.circles.size(), 2u);
  const auto circleNamed =
      [&](std::string_view name) -> const spellcircle::ResolvedCircle* {
    const auto found =
        std::find_if(resolved.circles.begin(), resolved.circles.end(),
                     [&](const auto& c) { return c.name == name; });
    return found != resolved.circles.end() ? &*found : nullptr;
  };
  const auto* topCircle = circleNamed("top");
  ASSERT_NE(topCircle, nullptr);
  EXPECT_NEAR(topCircle->textStart, 0.75f, 1e-6f);
  const auto* eastCircle = circleNamed("east");
  ASSERT_NE(eastCircle, nullptr);
  EXPECT_NEAR(eastCircle->textStart, 0.0f, 1e-6f);

  // The conversion is the same one point resolution uses, so a label
  // anchored at fraction f and a point placed at fraction f agree — one
  // origin on the wire, one quarter-turn, one home for it.
  EXPECT_NEAR(spellcircle::ringFractionFromTwelve(0.0f), 0.75f, 1e-6f);
  EXPECT_NEAR(spellcircle::ringFractionFromTwelve(0.25f), 0.0f, 1e-6f);
  EXPECT_NEAR(spellcircle::ringFractionFromTwelve(-0.25f), 0.5f, 1e-6f);
}

TEST(SceneGeometry, RadiiScaleWithTheHorizontalAxisOnly) {
  // Centers scale per axis; radii use the horizontal factor alone so circles
  // stay round on a canvas whose aspect ratio differs from the author's.
  flatbuffers::FlatBufferBuilder fbb;
  const SpellCircle::Vec2 center(100.0f, 100.0f);
  const auto circle =
      SpellCircle::CreateCircleDirect(fbb, &center, "ring", /*radius=*/50);
  const std::vector<flatbuffers::Offset<SpellCircle::Circle>> circles{circle};
  const auto bytes = finishScene(
      fbb, SpellCircle::CreateSceneDirect(fbb, &circles, nullptr, nullptr,
                                          /*width=*/500.0f,
                                          /*height=*/250.0f));

  const SceneDocument document = decodeScene(bytes);
  const spellcircle::ResolvedScene resolved =
      spellcircle::resolveScene(document, 1000.0f, 1000.0f);

  ASSERT_EQ(resolved.circles.size(), 1u);
  EXPECT_NEAR(resolved.circles[0].center.x, 200.0f, 0.01f);  // x2 horizontally
  EXPECT_NEAR(resolved.circles[0].center.y, 400.0f, 0.01f);  // x4 vertically
  EXPECT_NEAR(resolved.circles[0].radius, 100.0f, 0.01f);    // x2, horizontal
}

// ── SceneRenderer: box placement ───────────────────────────────────────────

TEST(SceneRendererBox, InnerEdgeSitsAtConfiguredDistance) {
  // Anchor at (800, 500) on a 1000x1000 canvas resolves the outward ray to
  // (1, 0), so the box's near face is its left edge and the anchor-to-edge
  // gap must equal boxDistance exactly.
  const SceneDocument document = decodeScene(sceneWithBox(800.0f, 500.0f, "N"));
  const spellcircle::ResolvedScene resolved =
      spellcircle::resolveScene(document, 1000.0f, 1000.0f);
  ASSERT_EQ(resolved.boxes.size(), 1u);
  ASSERT_NEAR(resolved.boxes[0].direction.x, 1.0f, 1e-4f);
  ASSERT_NEAR(resolved.boxes[0].direction.y, 0.0f, 1e-4f);

  spellcircle::SceneStyle style;
  style.boxDistance = 40.0f;
  spellcircle::SceneRenderer renderer;
  RectRecordingCanvas canvas(1000, 1000);
  renderer.draw(&canvas, resolved, style);

  ASSERT_FALSE(canvas.rects.empty());
  EXPECT_NEAR(canvas.rects.front().left() - 800.0f, style.boxDistance, 0.01f);
}

TEST(SceneRendererBox, GapIndependentOfLabelLength) {
  // The configured distance is a gap to the box's near face, so a label long
  // enough to widen the box past its minimum width must grow the box away
  // from its anchor, leaving the gap untouched.
  spellcircle::SceneStyle style;
  spellcircle::SceneRenderer renderer;

  const auto boxRectFor = [&](const std::string& label) {
    const SceneDocument document =
        decodeScene(sceneWithBox(800.0f, 500.0f, label));
    const spellcircle::ResolvedScene resolved =
        spellcircle::resolveScene(document, 1000.0f, 1000.0f);
    RectRecordingCanvas canvas(1000, 1000);
    renderer.draw(&canvas, resolved, style);
    EXPECT_FALSE(canvas.rects.empty());
    return canvas.rects.empty() ? SkRect::MakeEmpty() : canvas.rects.front();
  };

  const SkRect shortRect = boxRectFor("N");
  const SkRect longRect = boxRectFor(std::string(120, 'W'));

  // The long label must actually widen the box beyond the configured
  // minimum, or the invariance below would hold trivially.
  ASSERT_GT(longRect.width(), shortRect.width());
  EXPECT_NEAR(shortRect.left() - 800.0f, style.boxDistance, 0.01f);
  EXPECT_NEAR(longRect.left() - 800.0f, style.boxDistance, 0.01f);
}

TEST(SceneRendererBox, GapHoldsAlongDiagonalRay) {
  // On a diagonal ray no whole edge faces the anchor; the guarantee is that
  // the box clears the supporting line perpendicular to the ray at
  // boxDistance — the smallest projection of any corner onto the ray equals
  // the configured distance, continuously in the ray's direction.
  const SceneDocument document =
      decodeScene(sceneWithBox(800.0f, 800.0f, "SE"));
  const spellcircle::ResolvedScene resolved =
      spellcircle::resolveScene(document, 1000.0f, 1000.0f);
  ASSERT_EQ(resolved.boxes.size(), 1u);
  const spellcircle::Vec2 direction = resolved.boxes[0].direction;
  ASSERT_NEAR(direction.x, direction.y, 1e-4f);  // 45 degrees outward

  spellcircle::SceneStyle style;
  style.boxDistance = 40.0f;
  spellcircle::SceneRenderer renderer;
  RectRecordingCanvas canvas(1000, 1000);
  renderer.draw(&canvas, resolved, style);
  ASSERT_FALSE(canvas.rects.empty());
  const SkRect rect = canvas.rects.front();

  const auto projection = [&](float x, float y) {
    return direction.x * (x - 800.0f) + direction.y * (y - 800.0f);
  };
  const float nearestCorner =
      std::min(std::min(projection(rect.left(), rect.top()),
                        projection(rect.right(), rect.top())),
               std::min(projection(rect.left(), rect.bottom()),
                        projection(rect.right(), rect.bottom())));
  EXPECT_NEAR(nearestCorner, style.boxDistance, 0.05f);
}

// ── SceneLabels ────────────────────────────────────────────────────────────

TEST(SceneLabels, CircleContoursStartAtThreeOClock) {
  // Pins the assumption Circle.text_start is built on: a Skia circle path's
  // contour begins at 3 o'clock and winds clockwise in y-down screen space,
  // which is why an anchor fraction of 0.75 centers a label at the top.
  const std::vector<sigil::geometry::path::Contour> rings =
      sigil::geometry::path::Contour::of(SkPath::Circle(0.0f, 0.0f, 100.0f));
  ASSERT_EQ(rings.size(), 1u);

  const auto start = rings[0].at(0.0f);
  ASSERT_TRUE(start);
  EXPECT_NEAR(start->position.x, 100.0f, 0.01f);
  EXPECT_NEAR(start->position.y, 0.0f, 0.01f);
  // Leaving the start point the contour heads downward (+y), which is
  // clockwise on screen.
  EXPECT_GT(start->tangent.y, 0.9f);
}

TEST(RingLabelGeometryCache, DegenerateRadiusReturnsNullEveryTime) {
  spellcircle::RingLabelGeometryCache cache;
  EXPECT_FALSE(cache.ringForRadius(0.0f).valid());
  EXPECT_FALSE(cache.ringForRadius(0.0f).valid());
  EXPECT_FALSE(cache.ringForRadius(-5.0f).valid());
}

TEST(RingLabelGeometryCache, ValidRadiiShareOneMeasurement) {
  spellcircle::RingLabelGeometryCache cache;
  const sigil::geometry::path::Contour first = cache.ringForRadius(200.0f);
  ASSERT_TRUE(first.valid());
  EXPECT_EQ(cache.ringForRadius(200.0f), first);
}

TEST(RingLabelGeometryCache, DegenerateRequestsDoNotOccupySlots) {
  spellcircle::RingLabelGeometryCache cache(/*maximumEntries=*/4);
  EXPECT_FALSE(cache.ringForRadius(0.0f).valid());

  const float radii[] = {10.0f, 20.0f, 30.0f, 40.0f};
  sigil::geometry::path::Contour held[4];
  for (int i = 0; i < 4; ++i) {
    held[i] = cache.ringForRadius(radii[i]);
    ASSERT_TRUE(held[i].valid());
  }

  // Had the degenerate request been stored, it would have counted toward the
  // four-entry capacity and inserting the fourth ring would have flushed the
  // earlier ones; every radius still answering with the SAME measurement
  // proves it occupied nothing. The held copies keep the originals alive,
  // so a re-measure could not be mistaken for them.
  for (int i = 0; i < 4; ++i) EXPECT_EQ(cache.ringForRadius(radii[i]), held[i]);
  EXPECT_FALSE(cache.ringForRadius(0.0f).valid());
}
