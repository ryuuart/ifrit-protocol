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

/** One box's scene resolved and drawn: the placement resolveScene gave
 *  it, and the bounds the renderer drew it at. */
struct DrawnBox {
  spellcircle::ResolvedBox box;
  SkRect rect;
};

/** The one box of a scene whose anchor rides a radius-0 circle at
 *  (@p anchorX, @p anchorY), decoded, resolved onto a 1000x1000 canvas
 *  and drawn with @p style. */
DrawnBox drawBox(float anchorX, float anchorY, const std::string& label,
                 const spellcircle::SceneStyle& style) {
  const SceneDocument document =
      decodeScene(sceneWithBox(anchorX, anchorY, label));
  const spellcircle::ResolvedScene resolved =
      spellcircle::resolveScene(document, 1000.0f, 1000.0f);
  EXPECT_EQ(resolved.boxes.size(), 1u);
  spellcircle::SceneRenderer renderer;
  RectRecordingCanvas canvas(1000, 1000);
  renderer.draw(&canvas, resolved, style);
  EXPECT_FALSE(canvas.rects.empty());
  return DrawnBox{
      resolved.boxes.empty() ? spellcircle::ResolvedBox{}
                             : resolved.boxes.front(),
      canvas.rects.empty() ? SkRect::MakeEmpty() : canvas.rects.front()};
}

}  // namespace

// ── SceneModel ─────────────────────────────────────────────────────────────

TEST(SceneModel, RejectsTruncatedPayload) {
  const uint8_t garbage[] = {0xde, 0xad, 0xbe, 0xef};
  EXPECT_FALSE(spellcircle::verifyScenePayload(garbage, sizeof(garbage)));
}

TEST(SceneModel, RejectsAnEmptyOrOversizedPayload) {
  // Nothing at all is not a scene, and neither is a buffer bigger than the
  // one datagram a scene arrives in: both are refused before any offset in
  // them is followed.
  EXPECT_FALSE(spellcircle::verifyScenePayload(nullptr, 0));
  const uint8_t byte = 0;
  EXPECT_FALSE(spellcircle::verifyScenePayload(&byte, 0));

  flatbuffers::FlatBufferBuilder fbb;
  const std::vector<uint8_t> scene =
      finishScene(fbb, SpellCircle::CreateScene(fbb));
  ASSERT_LT(scene.size(), spellcircle::kMaximumScenePayload);
  EXPECT_TRUE(spellcircle::verifyScenePayload(scene.data(), scene.size()));

  // The same valid scene with trailing bytes that push it past the ceiling
  // is refused for its size alone: a payload that large never came off the
  // wire.
  std::vector<uint8_t> padded = scene;
  padded.resize(spellcircle::kMaximumScenePayload + 1, 0);
  EXPECT_FALSE(spellcircle::verifyScenePayload(padded.data(), padded.size()));
}

TEST(SceneModel, RejectsASceneTruncatedToTheDatagramCeiling) {
  // A sender that builds a scene larger than one datagram loses the tail:
  // what reaches the app is a prefix that still starts with a well-formed
  // Scene root. Its vectors and strings then point past the bytes that
  // arrived, and verification must refuse the whole payload rather than let
  // decode read there.
  flatbuffers::FlatBufferBuilder fbb;
  const SpellCircle::Vec2 center(100.0f, 100.0f);
  std::vector<flatbuffers::Offset<SpellCircle::Circle>> circles;
  for (int i = 0; i < 4000; ++i)
    circles.push_back(SpellCircle::CreateCircleDirect(
        fbb, &center, ("ring " + std::to_string(i)).c_str(),
        /*radius=*/static_cast<uint32_t>(i)));
  const std::vector<uint8_t> whole =
      finishScene(fbb, SpellCircle::CreateSceneDirect(fbb, &circles));
  ASSERT_GT(whole.size(), spellcircle::kMaximumScenePayload);

  const std::vector<uint8_t> delivered(
      whole.begin(), whole.begin() + spellcircle::kMaximumScenePayload);
  EXPECT_FALSE(
      spellcircle::verifyScenePayload(delivered.data(), delivered.size()));
}

TEST(SceneModel, DenselySharedTablesStayInsideTheVerifiersBudget) {
  // The verifier gives up past a nesting depth and a number of visited
  // tables. Neither ceiling can be reached from a datagram: the schema
  // nests Scene -> Edge -> Point -> Circle and no deeper, and one vector
  // element is a four-byte offset naming at most that chain of four
  // tables, so a full-size payload asks for far fewer visits than the
  // verifier refuses at. Raising the payload ceiling breaks this.
  static_assert(
      spellcircle::kMaximumScenePayload / sizeof(uint32_t) * 4 < 1000000,
      "a datagram must not be able to exhaust the verifier's "
      "table budget");

  // Densest sharing a sender can express: one Edge table named by every
  // element of the edges vector, so the verifier walks the same tables
  // thousands of times over a few kilobytes.
  flatbuffers::FlatBufferBuilder fbb;
  const SpellCircle::Vec2 pos(10.0f, 20.0f);
  const auto circle = SpellCircle::CreateCircleDirect(fbb, &pos, nullptr, 0);
  const auto first = SpellCircle::CreatePointDirect(fbb, nullptr, circle);
  const auto second = SpellCircle::CreatePointDirect(fbb, nullptr, circle);
  const auto edge = SpellCircle::CreateEdge(fbb, first, second);
  constexpr int kAliases = 4000;
  const std::vector<flatbuffers::Offset<SpellCircle::Edge>> edges(kAliases,
                                                                  edge);
  const std::vector<uint8_t> bytes =
      finishScene(fbb, SpellCircle::CreateSceneDirect(fbb, nullptr, &edges));
  ASSERT_LT(bytes.size(), spellcircle::kMaximumScenePayload);

  ASSERT_TRUE(spellcircle::verifyScenePayload(bytes.data(), bytes.size()));
  SceneDocument document;
  const spellcircle::SceneStats stats =
      document.decode(bytes.data(), bytes.size());

  // Every element decodes to its own Edge entity, but the two Point tables
  // behind them are one pair of entities however often they are named.
  EXPECT_EQ(stats.edges, kAliases);
  EXPECT_EQ(document.registry().view<spellcircle::PointComponent>().size(), 2u);
  const spellcircle::ResolvedScene resolved =
      spellcircle::resolveScene(document, 1000.0f, 1000.0f);
  EXPECT_EQ(resolved.edges.size(), static_cast<size_t>(kAliases));
}

TEST(SceneModel, MissingReferencesDecodeToNothingAndResolveSafely) {
  // Every reference in the schema is optional, so a sender can leave one
  // out (or emit a table it never filled in): a Point with no Circle, an
  // Edge with no endpoints, a Box with no Point. None of them names an
  // entity that exists, and resolution must answer a coordinate for each
  // rather than follow a dangling handle.
  flatbuffers::FlatBufferBuilder fbb;
  const auto circleless = SpellCircle::CreatePointDirect(fbb, "loose");
  const auto danglingEdge = SpellCircle::CreateEdge(fbb);
  const auto halfEdge = SpellCircle::CreateEdge(fbb, circleless);
  const auto anchorless = SpellCircle::CreateBoxDirect(fbb, "floating");
  const std::vector<flatbuffers::Offset<SpellCircle::Edge>> edges{danglingEdge,
                                                                  halfEdge};
  const std::vector<flatbuffers::Offset<SpellCircle::Box>> boxes{anchorless};
  const std::vector<uint8_t> bytes = finishScene(
      fbb, SpellCircle::CreateSceneDirect(fbb, nullptr, &edges, &boxes));

  const SceneDocument document = decodeScene(bytes);
  // The point that carries no circle still decodes, as the anchor a
  // radius-0 circle describes: the origin of author space.
  ASSERT_EQ(document.registry().view<spellcircle::PointComponent>().size(), 1u);

  const spellcircle::ResolvedScene resolved =
      spellcircle::resolveScene(document, 1000.0f, 1000.0f);
  ASSERT_EQ(resolved.edges.size(), 2u);
  for (const spellcircle::ResolvedEdge& edge : resolved.edges) {
    EXPECT_EQ(edge.first.x, 0.0f);
    EXPECT_EQ(edge.first.y, 0.0f);
    EXPECT_EQ(edge.second.x, 0.0f);
    EXPECT_EQ(edge.second.y, 0.0f);
  }
  ASSERT_EQ(resolved.boxes.size(), 1u);
  EXPECT_EQ(resolved.boxes.front().anchor.x, 0.0f);
  EXPECT_EQ(resolved.boxes.front().anchor.y, 0.0f);

  // Drawing it is the last thing that could read through a missing
  // reference, so the whole path runs.
  spellcircle::SceneRenderer renderer;
  RectRecordingCanvas canvas(1000, 1000);
  renderer.draw(&canvas, resolved, spellcircle::SceneStyle{});
}

TEST(SceneModel, AnEmptySceneClearsWhateverWasDrawnBefore) {
  // A Scene with no vectors at all is valid on the wire — it is how a
  // sender says "nothing" — and must decode to an empty document rather
  // than leave the previous scene standing.
  const std::vector<uint8_t> withBox = sceneWithBox(500.0f, 500.0f, "one");
  SceneDocument document;
  ASSERT_TRUE(spellcircle::verifyScenePayload(withBox.data(), withBox.size()));
  ASSERT_TRUE(document.decode(withBox.data(), withBox.size()).hasGeometry());

  flatbuffers::FlatBufferBuilder fbb;
  const std::vector<uint8_t> empty =
      finishScene(fbb, SpellCircle::CreateScene(fbb));
  ASSERT_TRUE(spellcircle::verifyScenePayload(empty.data(), empty.size()));
  const spellcircle::SceneStats stats =
      document.decode(empty.data(), empty.size());

  EXPECT_EQ(stats.circles, 0);
  EXPECT_EQ(stats.edges, 0);
  EXPECT_EQ(stats.boxes, 0);
  EXPECT_FALSE(stats.hasGeometry());
  EXPECT_EQ(document.registry().view<spellcircle::PointComponent>().size(), 0u);
  EXPECT_EQ(document.sceneWidth(), 0.0f);
  EXPECT_EQ(document.sceneHeight(), 0.0f);

  const spellcircle::ResolvedScene resolved =
      spellcircle::resolveScene(document, 1000.0f, 1000.0f);
  EXPECT_TRUE(resolved.circles.empty());
  EXPECT_TRUE(resolved.edges.empty());
  EXPECT_TRUE(resolved.boxes.empty());
  EXPECT_TRUE(resolved.pointLabels.empty());
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
  spellcircle::SceneStyle style;
  style.boxDistance = 40.0f;
  const DrawnBox drawn = drawBox(800.0f, 500.0f, "N", style);
  ASSERT_NEAR(drawn.box.direction.x, 1.0f, 1e-4f);
  ASSERT_NEAR(drawn.box.direction.y, 0.0f, 1e-4f);
  EXPECT_NEAR(drawn.rect.left() - 800.0f, style.boxDistance, 0.01f);
}

TEST(SceneRendererBox, GapIndependentOfLabelLength) {
  // The configured distance is a gap to the box's near face, so a label long
  // enough to widen the box past its minimum width must grow the box away
  // from its anchor, leaving the gap untouched.
  const spellcircle::SceneStyle style;
  const SkRect shortRect = drawBox(800.0f, 500.0f, "N", style).rect;
  const SkRect longRect =
      drawBox(800.0f, 500.0f, std::string(120, 'W'), style).rect;

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
  spellcircle::SceneStyle style;
  style.boxDistance = 40.0f;
  const DrawnBox drawn = drawBox(800.0f, 800.0f, "SE", style);
  const spellcircle::Vec2 direction = drawn.box.direction;
  ASSERT_NEAR(direction.x, direction.y, 1e-4f);  // 45 degrees outward

  const SkRect rect = drawn.rect;
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

TEST(RingLabelGeometryCache, ADegenerateRadiusAnswersNothingAndOccupiesNoSlot) {
  spellcircle::RingLabelGeometryCache cache(/*maximumEntries=*/4);
  EXPECT_FALSE(cache.ringForRadius(0.0f).valid());
  EXPECT_FALSE(cache.ringForRadius(0.0f).valid());
  EXPECT_FALSE(cache.ringForRadius(-5.0f).valid());

  const float radii[] = {10.0f, 20.0f, 30.0f, 40.0f};
  sigil::geometry::path::Contour held[4];
  for (int i = 0; i < 4; ++i) {
    held[i] = cache.ringForRadius(radii[i]);
    ASSERT_TRUE(held[i].valid());
  }

  // Every valid radius answers with the SAME measurement it first gave,
  // which is two things at once. Had the degenerate requests been stored
  // they would have counted toward the four-entry capacity and inserting
  // the fourth ring would have flushed the earlier ones, so nothing would
  // still match; and a radius asked for twice is measured once. The held
  // copies keep the originals alive, so a re-measure could not be mistaken
  // for them.
  for (int i = 0; i < 4; ++i) EXPECT_EQ(cache.ringForRadius(radii[i]), held[i]);
  EXPECT_FALSE(cache.ringForRadius(0.0f).valid());
}
