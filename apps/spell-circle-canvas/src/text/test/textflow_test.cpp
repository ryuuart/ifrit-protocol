#include <textflow/TextFlow.h>

#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <set>
#include <cmath>
#include <memory>
#include <numeric>

using namespace textflow;

namespace {

FontContext &sharedContext() {
  // CoreText font-manager construction enumerates system fonts; do it once.
  static auto *ctx = new FontContext(SkFontMgr_New_CoreText(nullptr));
  return *ctx;
}

TextStyle basicStyle(float size = 16.0f) {
  TextStyle style;
  style.shaping.size = size;
  return style;
}

Paragraph makeParagraph(std::string_view utf8, float size = 16.0f) {
  Paragraph para;
  para.appendText(utf8, basicStyle(size));
  return para;
}

// Exact pen x where a run ends. Blob ink bounds are conservative
// (font-bounds based), so line-edge checks use shaped advances instead.
// Each run is one word *segment* (multi-segment words emit several runs,
// each offset by its own xOffset), so use the segment's shaped advance.
float runEnd(const Paragraph &para, const PlacedRun &run) {
  if (run.shaped)
    return run.origin.x() + run.shaped->advance;
  return run.origin.x() + para.words()[run.wordIndex].width;
}

} // namespace

// ── Shaping & caching ─────────────────────────────────────────────────────

TEST(Shaper, ShapesLatinWord) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("Hello");
  para.ensureShaped(ctx);
  ASSERT_EQ(para.words().size(), 1u);
  const Word &word = para.words()[0];
  ASSERT_EQ(word.segments.size(), 1u);
  EXPECT_EQ(word.segments[0].shaped->glyphs.size(), 5u);
  EXPECT_GT(word.width, 0.0f);
  EXPECT_EQ(word.spaceWidth, 0.0f);
}

TEST(Shaper, CacheHitsOnIdenticalWords) {
  FontContext &ctx = sharedContext();
  ctx.purgeShapeCache();
  ctx.resetStats();
  Paragraph para = makeParagraph("tick tock tick tock tick");
  para.ensureShaped(ctx);
  // 3 distinct shape inputs: "tick", "tock", " " (glue). Repeats hit cache.
  EXPECT_EQ(ctx.stats().shapeCalls, 3u);
  EXPECT_GT(ctx.stats().shapeCacheHits, 0u);
}

TEST(Shaper, EditReshapesOnlyTheEditedWord) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "the quick brown fox jumps over the lazy dog again and again");
  para.ensureShaped(ctx);

  ctx.resetStats();
  // "quick" → "swift": positions 4..9.
  para.replaceText(4, 9, "swift");
  para.ensureShaped(ctx);
  // Only the new word content misses the cache ("swift"); everything else
  // (words and glue) must hit.
  EXPECT_LE(ctx.stats().shapeCalls, 1u);
  EXPECT_GT(ctx.stats().shapeCacheHits, 10u);
}

TEST(Shaper, PaintOnlyRestyleNeverReshapes) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("colorful words change their paint only");
  para.ensureShaped(ctx);

  ctx.resetStats();
  para.setPaint(9, 14, PaintStyle{SK_ColorRED});
  para.ensureShaped(ctx);
  EXPECT_EQ(ctx.stats().shapeCalls, 0u);
}

TEST(Shaper, FontSizeRestyleReshapesOnlyCoveredWords) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("alpha beta gamma delta epsilon");
  para.ensureShaped(ctx);

  ctx.resetStats();
  TextStyle big = basicStyle(24.0f);
  para.setStyle(6, 10, big); // "beta"
  para.ensureShaped(ctx);
  // "beta"@24 is new; its neighbors keep their cached 16px shapes. The glue
  // after "beta" also re-shapes at the new size (2 calls max).
  EXPECT_LE(ctx.stats().shapeCalls, 2u);
}

TEST(Shaper, ClustersAreMonotone) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("office"); // 'ffi' may ligate
  para.ensureShaped(ctx);
  const auto &clusters = para.words()[0].segments[0].shaped->clusters;
  EXPECT_TRUE(std::is_sorted(clusters.begin(), clusters.end()));
}

TEST(Shaper, WordBlobIsSharedAcrossLayouts) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("stable");
  para.ensureShaped(ctx);
  const ShapedWordRef &shaped = para.words()[0].segments[0].shaped;
  const SkTextBlob *first = wordBlob(*shaped).get();
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(wordBlob(*shaped).get(), first);
}

// ── Itemization ───────────────────────────────────────────────────────────

TEST(Itemization, MixedLatinCjkSplitsIntoWords) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("Skia は速い and 빠르다 也很快");
  para.ensureShaped(ctx);
  ASSERT_GT(para.words().size(), 4u);

  bool sawIdeographic = false, sawLatin = false;
  for (const Word &word : para.words()) {
    if (word.ideographic)
      sawIdeographic = true;
    else if (word.width > 0)
      sawLatin = true;
  }
  EXPECT_TRUE(sawIdeographic);
  EXPECT_TRUE(sawLatin);
}

TEST(Itemization, CjkGetsPerCharacterBreakOpportunities) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("日本語のテキスト");
  para.ensureShaped(ctx);
  // ICU line breaking splits ideographic text nearly per character; the
  // exact count depends on kinsoku rules, but it must be far more than one.
  EXPECT_GE(para.words().size(), 4u);
  for (const Word &word : para.words())
    EXPECT_TRUE(word.ideographic);
}

TEST(Itemization, FallbackResolvesCjkGlyphs) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("abc漢字xyz");
  para.ensureShaped(ctx);
  for (const Word &word : para.words())
    for (const WordSegment &seg : word.segments) {
      ASSERT_TRUE(seg.shaped->typeface);
      for (uint16_t glyph : seg.shaped->glyphs)
        EXPECT_NE(glyph, 0) << "missing glyph (.notdef) leaked into layout";
    }
}

TEST(Itemization, HardBreakIsMandatory) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("first line\nsecond");
  para.ensureShaped(ctx);
  bool sawMandatory = false;
  for (const Word &word : para.words())
    sawMandatory |= word.mandatoryBreakAfter;
  EXPECT_TRUE(sawMandatory);
}

TEST(Itemization, RtlWordShapesRtl) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("שלום");
  para.ensureShaped(ctx);
  ASSERT_EQ(para.words().size(), 1u);
  const auto &clusters = para.words()[0].segments[0].shaped->clusters;
  ASSERT_GE(clusters.size(), 2u);
  // RTL output is in visual order: cluster values run backwards.
  EXPECT_GT(clusters.front(), clusters.back());
}

TEST(Paragraph, ReplaceTextPreservesSurroundingStyles) {
  FontContext &ctx = sharedContext();
  Paragraph para;
  TextStyle red = basicStyle();
  red.paint.color = SK_ColorRED;
  TextStyle blue = basicStyle();
  blue.paint.color = SK_ColorBLUE;
  para.appendText("red ", red);
  para.appendText("blue", blue);

  para.replaceText(4, 8, "teal"); // swap the blue word's text
  para.ensureShaped(ctx);
  ASSERT_GE(para.spans().size(), 2u);
  EXPECT_EQ(para.spans().front().style.paint.color, SK_ColorRED);
  // Inserted text inherits the style at the edit point (the blue span).
  EXPECT_EQ(para.spans().back().style.paint.color, SK_ColorBLUE);
  EXPECT_EQ(para.text(), u"red teal");
}

// ── Flow geometry ─────────────────────────────────────────────────────────

TEST(Flow, BlockFlowFillsRect) {
  BlockFlow flow(SkRect::MakeXYWH(10, 20, 300, 100));
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(0, 20, 15, out));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_FLOAT_EQ(out[0].origin.x(), 10);
  EXPECT_FLOAT_EQ(out[0].origin.y(), 35); // top + ascent
  EXPECT_FLOAT_EQ(out[0].length, 300);
  ASSERT_TRUE(flow.lineIntervals(4, 20, 15, out)); // last fitting line
  EXPECT_FALSE(flow.lineIntervals(5, 20, 15, out));
}

TEST(Flow, ExclusionSplitsLineAroundCircle) {
  ExclusionFlow flow(SkRect::MakeWH(400, 200));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(150, 50, 100, 100), 0});

  std::vector<LineInterval> out;
  // A band through the circle's center: two intervals around x∈[100, 300].
  ASSERT_TRUE(flow.lineIntervals(4, 20, 15, out)); // band y=[80,100]
  ASSERT_EQ(out.size(), 2u);
  EXPECT_FLOAT_EQ(out[0].origin.x(), 0);
  EXPECT_LE(out[0].length, 150.0f);
  EXPECT_GE(out[1].origin.x(), 250.0f - 1.0f);

  // A band fully above the circle: one full-width interval.
  ASSERT_TRUE(flow.lineIntervals(0, 20, 15, out));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_FLOAT_EQ(out[0].length, 400);
}

TEST(Flow, ExclusionRectBlocksWholeBand) {
  ExclusionFlow flow(SkRect::MakeWH(300, 100));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kRect, SkRect::MakeXYWH(0, 30, 300, 20), 0});
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(1, 25, 18, out)); // band [25,50] overlaps
  EXPECT_TRUE(out.empty());
}

// ── Layout: greedy ────────────────────────────────────────────────────────

namespace {

SkPath pentagramPath(SkPoint center, float r, SkPathFillType fill) {
  SkPathBuilder builder;
  for (int i = 0; i < 5; ++i) {
    // Every second vertex of a pentagon: the classic self-intersecting star.
    const float angle = -1.5707963f + static_cast<float>(i) * 2 * 6.2831853f / 5;
    const SkPoint p = {center.x() + r * std::cos(angle),
                       center.y() + r * std::sin(angle)};
    if (i == 0)
      builder.moveTo(p);
    else
      builder.lineTo(p);
  }
  builder.close();
  builder.setFillType(fill);
  return builder.detach();
}

} // namespace

TEST(Flow, PathExclusionRespectsFillRule) {
  // A pentagram's centre is winding-filled but even-odd-hollow; the band
  // through the centre must block or stay open accordingly.
  const SkPoint c = {200, 150};
  auto centerIntervals = [&](SkPathFillType fill) {
    ExclusionFlow flow(SkRect::MakeWH(400, 300));
    flow.shapes().push_back(
        ExclusionFlow::Shape::Path(pentagramPath(c, 100, fill)));
    std::vector<LineInterval> out;
    // Line band [145, 155] straddles the star's centre.
    EXPECT_TRUE(flow.lineIntervals(29, 5, 4, out));
    return out;
  };

  const std::vector<LineInterval> winding =
      centerIntervals(SkPathFillType::kWinding);
  // Solid star: just the two stretches left and right of it.
  ASSERT_EQ(winding.size(), 2u);
  EXPECT_LT(winding[0].origin.x() + winding[0].length, c.x() - 55);
  EXPECT_GT(winding[1].origin.x(), c.x() + 55);

  const std::vector<LineInterval> evenOdd =
      centerIntervals(SkPathFillType::kEvenOdd);
  // Hollow centre pentagon: a third interval opens up inside the star.
  ASSERT_EQ(evenOdd.size(), 3u);
  EXPECT_GT(evenOdd[1].origin.x(), c.x() - 40);
  EXPECT_LT(evenOdd[1].origin.x() + evenOdd[1].length, c.x() + 40);
}

TEST(Flow, CompoundPathKeepsHoleAvailable) {
  // Donut: two circles, even-odd — text may flow inside the hole.
  SkPathBuilder builder;
  builder.addCircle(200, 150, 100);
  builder.addCircle(200, 150, 50);
  builder.setFillType(SkPathFillType::kEvenOdd);

  ExclusionFlow flow(SkRect::MakeWH(400, 300));
  flow.shapes().push_back(ExclusionFlow::Shape::Path(builder.detach()));
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(14, 10, 8, out)); // band [140, 150]
  ASSERT_EQ(out.size(), 3u);
  // Middle interval sits inside the hole (|x - 200| < 50 at this height).
  EXPECT_GT(out[1].origin.x(), 145.0f);
  EXPECT_LT(out[1].origin.x() + out[1].length, 255.0f);
  EXPECT_GT(out[1].length, 60.0f);
}

TEST(Flow, PathExclusionTipsBetweenScanlinesStillBlock) {
  // A left-pointing wedge whose tip falls strictly between the band's
  // sample scanlines: the conservative edge-extent union must still block
  // the tip's x-range.
  SkPathBuilder builder;
  builder.moveTo(100, 103); // tip at y=103, inside band [100, 110] but off
  builder.lineTo(300, 96);  // the top/mid/bottom sample lines
  builder.lineTo(300, 111);
  builder.close();

  ExclusionFlow flow(SkRect::MakeWH(400, 300));
  flow.shapes().push_back(ExclusionFlow::Shape::Path(builder.detach()));
  std::vector<LineInterval> out;
  ASSERT_TRUE(flow.lineIntervals(10, 10, 8, out)); // band [100, 110]
  ASSERT_FALSE(out.empty());
  // Nothing may be placed across the tip: the first interval ends at or
  // before x=100.
  EXPECT_LE(out[0].origin.x() + out[0].length, 100.0f + 0.5f);
}

TEST(Flow, PathExclusionOffsetMovesWithoutReflatten) {
  SkPath star = pentagramPath({200, 150}, 100, SkPathFillType::kWinding);
  ExclusionFlow flow(SkRect::MakeWH(800, 300));
  flow.shapes().push_back(ExclusionFlow::Shape::Path(star));

  std::vector<LineInterval> at0, at300;
  ASSERT_TRUE(flow.lineIntervals(29, 5, 4, at0));
  flow.shapes()[0].pathOffset = {300, 0};
  ASSERT_TRUE(flow.lineIntervals(29, 5, 4, at300));
  ASSERT_EQ(at0.size(), at300.size());
  for (size_t i = 0; i < at0.size(); ++i) {
    // Interval edges bordering the star shift by exactly the offset; the
    // flow-rect edges stay put.
    const float end0 = at0[i].origin.x() + at0[i].length;
    const float end300 = at300[i].origin.x() + at300[i].length;
    EXPECT_TRUE(at300[i].origin.x() == at0[i].origin.x() ||
                std::abs(at300[i].origin.x() - (at0[i].origin.x() + 300)) <
                    0.01f);
    EXPECT_TRUE(end300 == end0 || std::abs(end300 - (end0 + 300)) < 0.01f);
  }
}

TEST(Flow, RunsNeverEnterExclusionShapes) {
  // The gallery scene, distilled: mixed Latin/CJK justified text flowing
  // around a drifting donut and circle. Every placed run must stay inside
  // one of its line's intervals — text ending up *inside* a shape means
  // the breaker placed an overfull line into the gap beside it.
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "Typography is the craft of arranging type, and glyphs flow around "
      "obstacles the way water flows around stones. 日本語のテキストも同じ"
      "流れに乗って進み、한국어 단어들도 자연스럽게 흐르고, 中文字符同样"
      "围绕形状排布。 Latin and CJK mix freely because every word is shaped "
      "once, cached, and repositioned with pure arithmetic frame after "
      "frame while the donut drifts back and forth across the column.");

  SkPathBuilder donutBuilder;
  donutBuilder.addCircle(430, 260, 150);
  donutBuilder.addCircle(430, 260, 75);
  donutBuilder.setFillType(SkPathFillType::kEvenOdd);
  const SkPath donutPath = donutBuilder.detach();

  const float lineHeight = 26, lineAscent = 20;
  for (int phase = 0; phase < 6; ++phase) {
    ExclusionFlow flow(SkRect::MakeWH(760, 900));
    ExclusionFlow::Shape donut = ExclusionFlow::Shape::Path(donutPath, 8);
    donut.pathOffset = {60.0f * std::sin(static_cast<float>(phase) * 1.1f),
                        70.0f * std::cos(static_cast<float>(phase) * 0.7f)};
    flow.shapes().push_back(donut);
    flow.shapes().push_back(ExclusionFlow::Shape::Circle(
        SkRect::MakeXYWH(80.0f + 20.0f * static_cast<float>(phase), 480, 180,
                         180),
        8));

    for (Breaker breaker : {Breaker::kGreedy, Breaker::kKnuthPlass}) {
      LayoutOptions opts;
      opts.breaker = breaker;
      opts.alignment = Alignment::kJustify;
      opts.lineHeight = lineHeight;
      opts.ascent = lineAscent;
      Layout layout = layoutParagraph(ctx, para, flow, opts);
      EXPECT_FALSE(layout.overflowed());

      std::vector<LineInterval> ivs;
      for (const PlacedRun &run : layout.runs) {
        if (!run.shaped)
          continue;
        ASSERT_TRUE(flow.lineIntervals(run.line, lineHeight, lineAscent, ivs));
        const float x0 = run.origin.x();
        const float x1 = x0 + run.shaped->advance;
        bool inside = false;
        for (const LineInterval &iv : ivs)
          inside = inside || (x0 >= iv.origin.x() - 0.75f &&
                              x1 <= iv.origin.x() + iv.length + 0.75f);
        EXPECT_TRUE(inside)
            << "run on line " << run.line << " spans [" << x0 << ", " << x1
            << "] outside every interval (breaker "
            << (breaker == Breaker::kGreedy ? "greedy" : "knuth-plass")
            << ", phase " << phase << ")";
      }
    }
  }
}

TEST(Layout, GreedyLinesRespectWidth) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "one two three four five six seven eight nine ten eleven twelve "
      "thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty");
  BlockFlow flow(SkRect::MakeWH(200, 600));
  Layout layout = layoutParagraph(ctx, para, flow);

  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 1);
  for (const PlacedRun &run : layout.runs)
    EXPECT_LE(runEnd(para, run), 200.0f + 2.0f) << "run leaks past the right edge";
}

TEST(Layout, MandatoryBreakStartsNewLine) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("alpha\nbeta");
  BlockFlow flow(SkRect::MakeWH(500, 300));
  Layout layout = layoutParagraph(ctx, para, flow);
  ASSERT_EQ(layout.runs.size(), 2u);
  EXPECT_NE(layout.runs[0].line, layout.runs[1].line);
  EXPECT_LT(layout.runs[0].origin.y(), layout.runs[1].origin.y());
}

TEST(Layout, CenterAndEndAlignment) {
  FontContext &ctx = sharedContext();
  LayoutOptions opts;

  Paragraph para = makeParagraph("word");
  BlockFlow flow(SkRect::MakeWH(400, 100));

  opts.alignment = Alignment::kStart;
  const float startX = layoutParagraph(ctx, para, flow, opts).runs[0].origin.x();
  opts.alignment = Alignment::kCenter;
  const float centerX =
      layoutParagraph(ctx, para, flow, opts).runs[0].origin.x();
  opts.alignment = Alignment::kEnd;
  const float endX = layoutParagraph(ctx, para, flow, opts).runs[0].origin.x();

  EXPECT_FLOAT_EQ(startX, 0);
  EXPECT_GT(centerX, startX);
  EXPECT_GT(endX, centerX);
  EXPECT_NEAR(centerX * 2, endX, 1.0f);
}

TEST(Layout, JustifiedLinesFillTheMeasure) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "justification stretches the spaces between words so every full line "
      "extends to the right edge of the measure exactly");
  BlockFlow flow(SkRect::MakeWH(260, 600));
  LayoutOptions opts;
  opts.alignment = Alignment::kJustify;
  Layout layout = layoutParagraph(ctx, para, flow, opts);
  ASSERT_GT(layout.lineCount, 2);

  // Every line except the last must reach (near) the right edge.
  std::vector<float> lineEnds(static_cast<size_t>(layout.lineCount), 0.0f);
  for (const PlacedRun &run : layout.runs)
    lineEnds[static_cast<size_t>(run.line)] =
        std::max(lineEnds[static_cast<size_t>(run.line)], runEnd(para, run));
  for (int line = 0; line + 1 < layout.lineCount; ++line)
    EXPECT_NEAR(lineEnds[static_cast<size_t>(line)], 260.0f, 3.0f)
        << "line " << line << " not justified";
  EXPECT_LT(lineEnds.back(), 260.0f); // ragged last line
}

TEST(Layout, ExclusionShapeSplitsText) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "text flows around the shape and continues on the far side of it, "
      "filling both fragments of every interrupted line with words");
  ExclusionFlow flow(SkRect::MakeWH(400, 300));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(140, 40, 120, 120), 6});
  flow.setMinIntervalWidth(40);
  Layout layout = layoutParagraph(ctx, para, flow);

  // Some line must have runs both left and right of the circle.
  bool split = false;
  for (int line = 0; line < layout.lineCount && !split; ++line) {
    bool left = false, right = false;
    for (const PlacedRun &run : layout.runs) {
      if (run.line != line)
        continue;
      if (run.origin.x() < 140)
        left = true;
      if (run.origin.x() > 260)
        right = true;
    }
    split = left && right;
  }
  EXPECT_TRUE(split);
}

TEST(Layout, LineSetFlowPlacesTextOnArbitrarySegments) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("words on custom lines flow freely");

  LineSetFlow flow;
  flow.lines().push_back({LineInterval{{50, 40}, {1, 0}, 150}});
  flow.lines().push_back({LineInterval{{200, 90}, {1, 0}, 150}});
  Layout layout = layoutParagraph(ctx, para, flow);

  ASSERT_FALSE(layout.runs.empty());
  for (const PlacedRun &run : layout.runs) {
    if (run.line == 0) {
      EXPECT_FLOAT_EQ(run.origin.y(), 40);
      EXPECT_GE(run.origin.x(), 50);
    } else {
      EXPECT_FLOAT_EQ(run.origin.y(), 90);
      EXPECT_GE(run.origin.x(), 200);
    }
  }
}

TEST(Layout, RotatedLineBakesTransformedBlob) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("diagonal");
  const float inv = 1.0f / std::sqrt(2.0f);
  LineSetFlow flow;
  flow.lines().push_back({LineInterval{{0, 0}, {inv, inv}, 400}});
  Layout layout = layoutParagraph(ctx, para, flow);
  ASSERT_EQ(layout.runs.size(), 1u);
  // Transformed runs bake positions into the blob (origin stays at 0,0)
  // and the glyphs march diagonally.
  EXPECT_EQ(layout.runs[0].origin, (SkPoint{0, 0}));
  const SkRect bounds = layout.runs[0].blob->bounds();
  EXPECT_GT(bounds.right(), 40.0f);
  EXPECT_GT(bounds.bottom(), 40.0f);
}

TEST(Layout, PathFlowLaysGlyphsAlongCircle) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("around and around and around it goes");
  SkPath circle = SkPath::Circle(200, 200, 120);
  PathFlow flow(circle);
  Layout layout = layoutParagraph(ctx, para, flow);

  ASSERT_FALSE(layout.runs.empty());
  for (const PlacedRun &run : layout.runs) {
    const SkRect bounds = run.blob->bounds();
    const float dx = bounds.centerX() - 200.0f;
    const float dy = bounds.centerY() - 200.0f;
    const float dist = std::sqrt(dx * dx + dy * dy);
    EXPECT_NEAR(dist, 120.0f, 40.0f) << "glyphs strayed off the circle";
  }
}

TEST(Layout, AdvanceScaleTightensContourSpacing) {
  FontContext &ctx = sharedContext();
  SkContourMeasureIter iter(SkPath::Circle(0, 0, 200), /*forceClosed=*/false);
  const sk_sp<SkContourMeasure> ring = iter.next();
  ASSERT_TRUE(ring);

  // Same text on the same ring, once at natural arc consumption and once at
  // half — the half-scale layout's final word must sit at roughly half the
  // angle around the ring (pen starts at (200, 0) and marches clockwise).
  auto lastRunAngle = [&](float scale) {
    Paragraph para = makeParagraph("curvature compensation", 40.0f);
    LineInterval interval;
    interval.contour = ring;
    interval.length = ring->length() / scale;
    interval.advanceScale = scale;
    LineSetFlow flow;
    flow.lines().push_back({interval});
    Layout layout = layoutParagraph(ctx, para, flow);
    EXPECT_FALSE(layout.runs.empty());
    const SkRect bounds = layout.runs.back().blob->bounds();
    float angle = std::atan2(bounds.centerY(), bounds.centerX());
    if (angle < 0)
      angle += 6.2831853f;
    return angle;
  };

  const float full = lastRunAngle(1.0f);
  const float half = lastRunAngle(0.5f);
  EXPECT_GT(full, half * 1.5f)
      << "advanceScale should compress the arc the text subtends";
  EXPECT_NEAR(full, half * 2.0f, full * 0.25f);
}

TEST(Layout, OverflowReportsFirstUnplacedWord) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "far more text than could ever fit inside such a tiny little box");
  BlockFlow flow(SkRect::MakeWH(120, 40));
  Layout layout = layoutParagraph(ctx, para, flow);
  EXPECT_TRUE(layout.overflowed());
  EXPECT_GT(layout.firstUnplacedWord, 0u);
}

// ── Layout: Knuth-Plass ───────────────────────────────────────────────────

namespace {

// Sum of squared leftover space across all full lines — the raggedness
// measure Knuth-Plass style breaking should not lose to greedy on.
float raggedness(const Paragraph &para, const Layout &layout, float measure) {
  if (layout.lineCount <= 1)
    return 0;
  std::vector<float> lineEnds(static_cast<size_t>(layout.lineCount), 0.0f);
  for (const PlacedRun &run : layout.runs)
    lineEnds[static_cast<size_t>(run.line)] =
        std::max(lineEnds[static_cast<size_t>(run.line)], runEnd(para, run));
  float total = 0;
  for (int line = 0; line + 1 < layout.lineCount; ++line) {
    const float slack = measure - lineEnds[static_cast<size_t>(line)];
    total += slack * slack;
  }
  return total;
}

} // namespace

TEST(KnuthPlass, ProducesValidLines) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "In olden times when wishing still helped one, there lived a king "
      "whose daughters were all beautiful; and the youngest was so beautiful "
      "that the sun itself, which has seen so much, was astonished whenever "
      "it shone in her face.");
  BlockFlow flow(SkRect::MakeWH(300, 900));
  LayoutOptions opts;
  opts.breaker = Breaker::kKnuthPlass;
  opts.alignment = Alignment::kJustify;
  Layout layout = layoutParagraph(ctx, para, flow, opts);

  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 3);
  for (const PlacedRun &run : layout.runs)
    EXPECT_LE(runEnd(para, run), 300.0f + 3.0f);
  // Words appear in order (logical == visual for pure-LTR text).
  std::vector<uint32_t> seen;
  for (const PlacedRun &run : layout.runs)
    seen.push_back(run.wordIndex);
  EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end()));
}

TEST(KnuthPlass, NoWorseRaggednessThanGreedy) {
  FontContext &ctx = sharedContext();
  const char *tale =
      "It was the best of times, it was the worst of times, it was the age "
      "of wisdom, it was the age of foolishness, it was the epoch of belief, "
      "it was the epoch of incredulity, it was the season of Light, it was "
      "the season of Darkness, it was the spring of hope, it was the winter "
      "of despair.";
  const float measure = 320;

  Paragraph para = makeParagraph(tale);
  BlockFlow flowGreedy(SkRect::MakeWH(measure, 2000));
  Layout greedy = layoutParagraph(ctx, para, flowGreedy); // ragged-right

  BlockFlow flowKp(SkRect::MakeWH(measure, 2000));
  LayoutOptions kpOpts;
  kpOpts.breaker = Breaker::kKnuthPlass;
  Layout kp = layoutParagraph(ctx, para, flowKp, kpOpts);

  EXPECT_FALSE(kp.overflowed());
  EXPECT_LE(raggedness(para, kp, measure), raggedness(para, greedy, measure) * 1.05f);
}

TEST(KnuthPlass, JustifiedCjkParagraph) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "吾輩は猫である。名前はまだ無い。どこで生れたかとんと見当がつかぬ。"
      "何でも薄暗いじめじめした所でニャーニャー泣いていた事だけは記憶している。");
  BlockFlow flow(SkRect::MakeWH(280, 600));
  LayoutOptions opts;
  opts.breaker = Breaker::kKnuthPlass;
  opts.alignment = Alignment::kJustify;
  Layout layout = layoutParagraph(ctx, para, flow, opts);

  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 2);
  for (const PlacedRun &run : layout.runs)
    EXPECT_LE(runEnd(para, run), 280.0f + 3.0f);
}

// ── Incremental behavior end-to-end ───────────────────────────────────────

TEST(Incremental, OneWordEditKeepsOtherWordBlobs) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "steady text with one word that will change between frames while all "
      "other words keep their shaped blobs perfectly intact");
  BlockFlow flow(SkRect::MakeWH(300, 600));
  Layout before = layoutParagraph(ctx, para, flow);

  para.replaceText(17, 20, "two"); // "one" → "two"
  Layout after = layoutParagraph(ctx, para, flow);

  // Blobs are shared via the shape cache: unchanged words reuse the very
  // same SkTextBlob instances across the edit.
  size_t sharedBlobs = 0;
  for (const PlacedRun &a : before.runs)
    for (const PlacedRun &b : after.runs)
      if (a.blob.get() == b.blob.get())
        sharedBlobs++;
  EXPECT_GT(sharedBlobs, before.runs.size() / 2);
}

TEST(Incremental, MovingExclusionOnlyRepositions) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "the shape moves through the paragraph and every frame the words "
      "reflow around it without any reshaping at all, just new positions");
  ExclusionFlow flow(SkRect::MakeWH(360, 400));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(50, 30, 90, 90), 4});

  Layout first = layoutParagraph(ctx, para, flow);
  ctx.resetStats();
  flow.shapes()[0].bounds.offset(60, 25);
  Layout second = layoutParagraph(ctx, para, flow);

  EXPECT_EQ(ctx.stats().shapeCalls, 0u) << "moving a shape must not reshape";
  EXPECT_FALSE(second.runs.empty());
}

// ── Typographic options (last-line alignment, hyphenation, effects) ───────

TEST(Typography, LastLineAlignmentEnd) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "a justified paragraph whose final line is pushed to the right edge "
      "instead of hanging on the left like usual short last lines do");
  BlockFlow flow(SkRect::MakeWH(260, 600));
  LayoutOptions opts;
  opts.alignment = Alignment::kJustify;
  opts.lastLineAlignment = Alignment::kEnd;
  Layout layout = layoutParagraph(ctx, para, flow, opts);
  ASSERT_GT(layout.lineCount, 1);

  // The last line's last word must end at the right edge, and its first run
  // must not start at x=0.
  float lastLineEnd = 0, lastLineStart = 1e9f;
  for (const PlacedRun &run : layout.runs) {
    if (run.line != layout.lineCount - 1)
      continue;
    lastLineEnd = std::max(lastLineEnd, runEnd(para, run));
    lastLineStart = std::min(lastLineStart, run.origin.x());
  }
  EXPECT_NEAR(lastLineEnd, 260.0f, 1.0f);
  EXPECT_GT(lastLineStart, 5.0f);
}

TEST(Typography, SoftHyphenRendersHyphenOnBreak) {
  FontContext &ctx = sharedContext();
  // "extra­ordinarily" fits neither whole nor as "extra" without the
  // discretionary break being taken on a narrow measure.
  Paragraph para = makeParagraph("an extra­ordinarily narrow measure");
  BlockFlow flow(SkRect::MakeWH(90, 300));
  Layout layout = layoutParagraph(ctx, para, flow);

  const Word *hyphenWord = nullptr;
  for (const Word &word : para.words())
    if (word.hyphenBreak)
      hyphenWord = &word;
  ASSERT_NE(hyphenWord, nullptr);
  ASSERT_TRUE(hyphenWord->hyphenGlyph);

  // The hyphen glyph's shared blob must appear among the placed runs.
  const SkTextBlob *hyphenBlob = wordBlob(*hyphenWord->hyphenGlyph).get();
  bool found = false;
  for (const PlacedRun &run : layout.runs)
    found |= run.blob.get() == hyphenBlob;
  EXPECT_TRUE(found);
}

TEST(Typography, SoftHyphenInvisibleWhenNotBroken) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("extra­ordinarily");
  BlockFlow flow(SkRect::MakeWH(500, 100)); // plenty of room: no break
  Layout layout = layoutParagraph(ctx, para, flow);

  ASSERT_EQ(para.words().size(), 2u); // "extra·" + "ordinarily"
  const Word &first = para.words()[0];
  EXPECT_TRUE(first.hyphenBreak);
  EXPECT_EQ(first.spaceWidth, 0.0f); // halves join with zero gap
  const SkTextBlob *hyphenBlob = wordBlob(*first.hyphenGlyph).get();
  for (const PlacedRun &run : layout.runs)
    EXPECT_NE(run.blob.get(), hyphenBlob) << "hyphen rendered without break";
  // Both halves sit on one line, adjacent.
  ASSERT_EQ(layout.lineCount, 1);
}

TEST(Typography, KnuthPlassUsesSoftHyphens) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "the as­ton­ish­ing­ly in­com­pre­hen"
      "­si­ble hy­phen­ation ma­chin­ery works");
  BlockFlow flow(SkRect::MakeWH(120, 600));
  LayoutOptions opts;
  opts.breaker = Breaker::kKnuthPlass;
  opts.alignment = Alignment::kJustify;
  Layout layout = layoutParagraph(ctx, para, flow, opts);
  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 2);
}

TEST(Typography, SpanRestyleAcrossLines) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "a long sentence that will certainly wrap across several lines gets "
      "one continuous span of emphasis applied to its middle third and the "
      "styling must follow the words wherever the line breaker puts them");
  BlockFlow flow(SkRect::MakeWH(220, 600));
  Layout before = layoutParagraph(ctx, para, flow);
  ASSERT_GT(before.lineCount, 3);

  ctx.resetStats();
  // Style the middle third, snapped to word boundaries. (A range cutting
  // *inside* a word splits that word into fragments and costs a one-time
  // reshape of the two boundary words; whole-word ranges cost zero.)
  const std::u16string &text = para.text();
  uint32_t from = static_cast<uint32_t>(text.find(u' ', text.size() / 3)) + 1;
  uint32_t to = static_cast<uint32_t>(text.find(u' ', 2 * text.size() / 3));
  para.setPaint(from, to, PaintStyle{SK_ColorRED});
  Layout after = layoutParagraph(ctx, para, flow);
  EXPECT_EQ(ctx.stats().shapeCalls, 0u);

  // Red runs must exist on more than one line.
  const auto &spans = para.spans();
  int firstRedLine = -1, lastRedLine = -1;
  for (const PlacedRun &run : after.runs) {
    if (run.styleIndex < spans.size() &&
        spans[run.styleIndex].style.paint.color == SK_ColorRED) {
      if (firstRedLine < 0)
        firstRedLine = run.line;
      lastRedLine = run.line;
    }
  }
  ASSERT_GE(firstRedLine, 0);
  EXPECT_GT(lastRedLine, firstRedLine);
}

TEST(Typography, ShadowAndShaderDrawWithoutRelayout) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("effects are paint-only");
  BlockFlow flow(SkRect::MakeWH(400, 100));
  Layout layout = layoutParagraph(ctx, para, flow);

  ctx.resetStats();
  PaintStyle fancy;
  fancy.color = SK_ColorWHITE;
  fancy.shadows.push_back({0x80000000, {3, 3}, 2.5f});
  para.setPaint(0, 7, fancy);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 100));
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  layout.draw(surface->getCanvas(), para); // same layout object, new paint
  EXPECT_EQ(ctx.stats().shapeCalls, 0u);

  // The shadow must have put ink outside the pure-white fill: sample any
  // non-white, non-transparent pixel.
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  bool sawShadowInk = false;
  for (int y = 0; y < pixmap.height() && !sawShadowInk; ++y)
    for (int x = 0; x < pixmap.width() && !sawShadowInk; ++x) {
      const SkColor c = pixmap.getColor(x, y);
      if (SkColorGetA(c) > 0 && c != SK_ColorWHITE)
        sawShadowInk = true;
    }
  EXPECT_TRUE(sawShadowInk);
}

// ── Complex scripts & grapheme clusters ───────────────────────────────────

namespace {

// True when every glyph in the paragraph resolved to a real glyph (no
// .notdef): the script actually shaped with a covering font.
bool allGlyphsResolved(const Paragraph &para) {
  for (const Word &word : para.words())
    for (const WordSegment &seg : word.segments)
      for (uint16_t glyph : seg.shaped->glyphs)
        if (glyph == 0)
          return false;
  return true;
}

size_t uniqueClusterCount(const ShapedWord &sw) {
  std::set<uint32_t> unique(sw.clusters.begin(), sw.clusters.end());
  return unique.size();
}

} // namespace


TEST(KnuthPlass, LinesNeverExceedTheMeasure) {
  FontContext &ctx = sharedContext();
  // Hyphen-laden text (the gallery scene) across a breathing measure: no
  // placed line may stick out past the measure — overfull breaks are
  // infeasible unless there is truly no alternative.
  Paragraph para;
  para.appendText(
      "The para­graph breaker con­sid­ers every way to break "
      "this text into lines and picks the one with the least bad­ness, "
      "ex­act­ly like TeX. Greedy breaking com­mits line by "
      "line and leaves rag­ged, in­con­sis­tent "
      "spac­ing be­hind; op­ti­mal breaking spreads the "
      "slack across the whole para­graph in­stead.",
      basicStyle(17.0f));

  LayoutOptions opts;
  opts.alignment = Alignment::kJustify;
  opts.breaker = Breaker::kKnuthPlass;
  opts.lineHeight = 27;

  for (float measure = 150; measure <= 430; measure += 7.0f) {
    BlockFlow flow(SkRect::MakeWH(measure, 3000));
    Layout layout = layoutParagraph(ctx, para, flow, opts);
    EXPECT_FALSE(layout.overflowed());
    for (const PlacedRun &run : layout.runs) {
      if (!run.shaped)
        continue;
      const float end = run.origin.x() + run.shaped->advance;
      EXPECT_LE(end, measure + 0.75f)
          << "line " << run.line << " leaks past the " << measure
          << "px measure";
    }
  }
}

// ── Inline placeholders (pills / images woven into the flow) ─────────────

TEST(Placeholders, ReservesWidthInTheLine) {
  FontContext &ctx = sharedContext();
  Paragraph para;
  para.appendText("before ", basicStyle());
  para.appendPlaceholder({90, 20, 0}, basicStyle());
  para.appendText(" after", basicStyle());

  BlockFlow flow(SkRect::MakeWH(600, 60)); // everything on one line
  Layout layout = layoutParagraph(ctx, para, flow);

  const auto rects = layout.placeholderRects(para);
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_FLOAT_EQ(rects[0].rect.width(), 90);
  EXPECT_FLOAT_EQ(rects[0].rect.height(), 20);

  // "after" starts past the slot's right edge.
  float afterX = -1;
  for (const PlacedRun &run : layout.runs) {
    if (run.placeholder >= 0)
      continue;
    const Word &word = para.words()[run.wordIndex];
    const std::u16string_view text(para.text());
    if (text.substr(word.textBegin, 5) == u"after")
      afterX = run.origin.x();
  }
  ASSERT_GE(afterX, 0);
  EXPECT_GE(afterX, rects[0].rect.right() - 0.25f);
}

TEST(Placeholders, SitOnTheBaselineWithDrop) {
  FontContext &ctx = sharedContext();
  Paragraph para;
  para.appendText("x ", basicStyle());
  para.appendPlaceholder({40, 30, 8}, basicStyle()); // bottom 8px below base
  BlockFlow flow(SkRect::MakeWH(300, 60));
  Layout layout = layoutParagraph(ctx, para, flow);

  float baselineY = -1;
  for (const PlacedRun &run : layout.runs)
    if (run.placeholder < 0)
      baselineY = run.origin.y();
  const auto rects = layout.placeholderRects(para);
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_FLOAT_EQ(rects[0].rect.bottom(), baselineY + 8);
  EXPECT_FLOAT_EQ(rects[0].rect.top(), baselineY + 8 - 30);
}

TEST(Placeholders, WrapAndJustifyLikeWords) {
  FontContext &ctx = sharedContext();
  Paragraph para;
  for (int i = 0; i < 6; ++i) {
    para.appendText("word word word ", basicStyle());
    para.appendPlaceholder({60, 14, 0}, basicStyle());
    para.appendText(" ", basicStyle());
  }
  BlockFlow flow(SkRect::MakeWH(220, 400));
  LayoutOptions opts;
  opts.alignment = Alignment::kJustify;
  Layout layout = layoutParagraph(ctx, para, flow, opts);

  const auto rects = layout.placeholderRects(para);
  ASSERT_EQ(rects.size(), 6u);
  std::set<int> lines;
  for (const auto &placed : rects) {
    lines.insert(placed.line);
    // Slots never overflow the measure.
    EXPECT_GE(placed.rect.left(), -0.25f);
    EXPECT_LE(placed.rect.right(), 220.5f);
  }
  EXPECT_GT(lines.size(), 1u) << "slots must wrap onto later lines";
}

TEST(Placeholders, ResizeRelayoutsLive) {
  FontContext &ctx = sharedContext();
  Paragraph para;
  para.appendText("pill: ", basicStyle());
  para.appendPlaceholder({50, 16, 0}, basicStyle());
  BlockFlow flow(SkRect::MakeWH(400, 60));
  Layout before = layoutParagraph(ctx, para, flow);

  ctx.resetStats();
  para.setPlaceholder(0, {120, 16, 0});
  Layout after = layoutParagraph(ctx, para, flow);
  EXPECT_EQ(ctx.stats().shapeCalls, 0u) << "resizing a slot reshapes nothing";
  EXPECT_FLOAT_EQ(after.placeholderRects(para)[0].rect.width(), 120);
}

// ── Typographic correctness (Blink/HarfBuzzShaperTest-inspired) ──────────
// The invariants Chrome's text stack enforces in its shaper/layout unit
// tests, adapted to TextFlow's word model.

TEST(Correctness, VariableAxesReachHarfBuzz) {
  // A variable instance must shape with the same design position Skia
  // rasterizes: identical advances across weights would mean HarfBuzz
  // silently shaped the default instance.
  FontContext &ctx = sharedContext();
  sk_sp<SkTypeface> base =
      ctx.fontMgr()->matchFamilyStyle("Noto Sans", SkFontStyle::Normal());
  if (!base || base->getVariationDesignPosition({}) <= 0)
    GTEST_SKIP() << "no variable Noto Sans installed";

  const SkFontArguments::VariationPosition::Coordinate heavy{
      SkSetFourByteTag('w', 'g', 'h', 't'), 900.0f};
  SkFontArguments args;
  args.setVariationDesignPosition({&heavy, 1});
  sk_sp<SkTypeface> black = base->makeClone(args);
  ASSERT_TRUE(black);

  constexpr std::string_view kText = "hamburgefonstiv";
  auto shapedAdvance = [&](const sk_sp<SkTypeface> &tf) {
    Paragraph para;
    TextStyle style = basicStyle(32.0f);
    style.shaping.typeface = tf;
    para.appendText(kText, style);
    para.ensureShaped(ctx);
    float total = 0;
    for (const Word &w : para.words())
      total += w.width;
    return total;
  };

  const float regular = shapedAdvance(base);
  const float bold = shapedAdvance(black);
  EXPECT_GT(bold, regular * 1.01f) << "wght 900 should shape wider than 400";

  // HarfBuzz's advances agree with what Skia measures for that instance.
  const SkScalar measured =
      makeFont(black, 32.0f)
          .measureText(kText.data(), kText.size(), SkTextEncoding::kUTF8);
  EXPECT_NEAR(bold, measured, bold * 0.015f);
}

TEST(Correctness, ClusterCoverageIsComplete) {
  FontContext &ctx = sharedContext();
  // Ligating Latin, joining Arabic, conjunct Devanagari, ZWJ emoji.
  const char *samples[] = {"office", "العربية", "नमस्ते", "👨‍👩‍👧"};
  for (const char *sample : samples) {
    Paragraph para = makeParagraph(sample);
    para.ensureShaped(ctx);
    for (const Word &word : para.words())
      for (const WordSegment &seg : word.segments) {
        const auto &clusters = seg.shaped->clusters;
        ASSERT_FALSE(clusters.empty()) << sample;
        const size_t segLen = seg.shaped->glyphs.size();
        // Every cluster index points inside the shaped text, and the run
        // starts at offset 0 from one end (LTR: front, RTL: back).
        const uint32_t first = std::min(clusters.front(), clusters.back());
        EXPECT_EQ(first, 0u) << sample;
        for (uint32_t c : clusters)
          EXPECT_LT(c, word.textEnd - word.textBegin) << sample;
        EXPECT_GT(segLen, 0u);
      }
  }
}

TEST(Correctness, ZwnjBlocksArabicJoining) {
  FontContext &ctx = sharedContext();
  auto glyphsOf = [&](const char *text) {
    Paragraph para = makeParagraph(text);
    para.ensureShaped(ctx);
    std::multiset<uint16_t> ids;
    for (const Word &word : para.words())
      for (const WordSegment &seg : word.segments)
        for (uint16_t g : seg.shaped->glyphs)
          if (g != 0)
            ids.insert(g);
    return ids;
  };
  // "بب" joins (initial+final forms); a ZWNJ between forces isolated forms.
  EXPECT_NE(glyphsOf("بب"), glyphsOf("ب‌ب"));
}

TEST(Correctness, CombiningMarkAttachesToBase) {
  FontContext &ctx = sharedContext();
  Paragraph nfc = makeParagraph("café");         // é precomposed
  Paragraph nfd = makeParagraph("café");        // e + combining acute
  nfc.ensureShaped(ctx);
  nfd.ensureShaped(ctx);
  ASSERT_EQ(nfc.words().size(), 1u);
  ASSERT_EQ(nfd.words().size(), 1u);
  // The decomposed form must not gain width: the mark attaches to the base.
  EXPECT_NEAR(nfc.words()[0].width, nfd.words()[0].width, 0.75f);
  // And the mark forms one grapheme cluster with its base: the NFD segment
  // reports at most as many clusters as it has base characters (4).
  std::set<uint32_t> unique(nfd.words()[0].segments[0].shaped->clusters.begin(),
                            nfd.words()[0].segments[0].shaped->clusters.end());
  EXPECT_LE(unique.size(), 4u);
}

TEST(Correctness, KinsokuProhibitsLineInitialPunctuation) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("これは、禁則処理のテストです。行頭に句読点は来ない。");
  para.ensureShaped(ctx);
  const std::u16string &text = para.text();
  for (const Word &word : para.words()) {
    // A break opportunity never lands *before* a closing punctuation mark:
    // no word (== potential line start) begins with 。、」.
    const char16_t first = text[word.textBegin];
    EXPECT_NE(first, u'。');
    EXPECT_NE(first, u'、');
    EXPECT_NE(first, u'」');
    // …and never *after* an opening bracket: no word's content ends with 「.
    if (word.textEnd > word.textBegin) {
      EXPECT_NE(text[word.textEnd - 1], u'「');
    }
  }
}

TEST(Correctness, NbspNeverBreaks) {
  FontContext &ctx = sharedContext();
  Paragraph spaced = makeParagraph("100 km");
  Paragraph glued = makeParagraph("100 km");
  spaced.ensureShaped(ctx);
  glued.ensureShaped(ctx);
  EXPECT_EQ(spaced.words().size(), 2u);
  EXPECT_EQ(glued.words().size(), 1u) << "NBSP must not be a break point";
}

TEST(Correctness, TabsMeasureAsSpaces) {
  FontContext &ctx = sharedContext();
  Paragraph tab = makeParagraph("a\tb");
  Paragraph space = makeParagraph("a b");
  tab.ensureShaped(ctx);
  space.ensureShaped(ctx);
  ASSERT_EQ(tab.words().size(), 2u);
  EXPECT_FLOAT_EQ(tab.words()[0].spaceWidth, space.words()[0].spaceWidth);
}

TEST(Correctness, JustifiedShrinkNeverCollapsesSpaces) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph(
      "several reasonably long words keep justification honest here", 18.0f);
  para.ensureShaped(ctx);
  // A measure a hair narrower than a natural line forces shrink.
  LayoutOptions opts;
  opts.alignment = Alignment::kJustify;
  BlockFlow flow(SkRect::MakeWH(200, 400));
  Layout layout = layoutParagraph(ctx, para, flow, opts);

  ASSERT_GT(layout.lineCount, 1);
  for (size_t i = 0; i + 1 < layout.runs.size(); ++i) {
    const PlacedRun &a = layout.runs[i];
    const PlacedRun &b = layout.runs[i + 1];
    if (a.line != b.line)
      continue;
    const float gap = b.origin.x() - runEnd(para, a);
    const float glue = para.words()[a.wordIndex].spaceWidth;
    if (glue <= 0)
      continue;
    // Shrink is clamped at the glue's shrink limit (glueShrink = 1/3).
    EXPECT_GT(gap, glue * (1.0f - 0.34f) - 0.25f)
        << "space collapsed past the shrink limit on line " << a.line;
  }
}

TEST(Correctness, BidiVisualOrderForMixedDirections) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("aaa בבב גגג zzz", 16.0f);
  BlockFlow flow(SkRect::MakeWH(600, 60)); // one wide line
  Layout layout = layoutParagraph(ctx, para, flow);

  // Logical order: aaa(0) בבב(1) גגג(2) zzz(3). UAX#9: the two RTL words
  // swap visually — גגג renders left of בבב, both between aaa and zzz.
  float x[4] = {0, 0, 0, 0};
  for (const PlacedRun &run : layout.runs)
    if (run.wordIndex < 4)
      x[run.wordIndex] = run.origin.x();
  EXPECT_LT(x[0], x[2]);
  EXPECT_LT(x[2], x[1]) << "RTL pair must render in reversed visual order";
  EXPECT_LT(x[1], x[3]);
}

TEST(Correctness, StrutMatchesFontMetrics) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("metrics", 32.0f);
  const Paragraph::Strut strut = para.strut(ctx);
  const SkFont font = makeFont(ctx.defaultTypeface(), 32.0f);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  EXPECT_FLOAT_EQ(strut.ascent, -metrics.fAscent);
  EXPECT_FLOAT_EQ(strut.height,
                  -metrics.fAscent + metrics.fDescent + metrics.fLeading);
}

TEST(Correctness, EditAtSurrogateBoundaryIsSafe) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("ab 𝕏𝕐 cd"); // 𝕏/𝕐 are surrogate pairs
  para.ensureShaped(ctx);
  // Cut straight through the middle of the first surrogate pair.
  const size_t at = para.text().find(u"ab");
  ASSERT_NE(at, std::u16string::npos);
  para.replaceText(4, 5, "Z"); // [4,5) is inside a pair for this string
  para.ensureShaped(ctx);      // must not crash or emit garbage words
  for (const Word &word : para.words())
    EXPECT_LE(word.textEnd, para.text().size());
  Layout layout = layoutParagraph(
      ctx, para, *std::make_unique<BlockFlow>(SkRect::MakeWH(400, 100)));
  EXPECT_FALSE(layout.runs.empty());
}

// ── Vertical writing mode ────────────────────────────────────────────────

TEST(Vertical, UprightCjkStacksDownColumns) {
  FontContext &ctx = sharedContext();
  Paragraph para;
  para.appendText("縦書きのテキストは上から下へ流れる", basicStyle(20.0f));
  para.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(200, 220));
  LayoutOptions opts;
  opts.lineHeight = 30; // column pitch
  Layout layout = layoutParagraph(ctx, para, flow, opts);

  ASSERT_FALSE(layout.runs.empty());
  EXPECT_GT(layout.lineCount, 1) << "16 chars at 20px must overflow one column";
  float prevYInLine0 = -1e9f;
  float line0x = 0, line1x = 0;
  for (const PlacedRun &run : layout.runs) {
    EXPECT_FALSE(run.transformed) << "upright CJK is a positioned blob";
    EXPECT_TRUE(run.shaped->vertical);
    if (run.line == 0) {
      line0x = run.origin.x();
      EXPECT_GT(run.origin.y(), prevYInLine0) << "pen must travel downward";
      prevYInLine0 = run.origin.y();
    } else if (run.line == 1) {
      line1x = run.origin.x();
    }
  }
  EXPECT_LT(line1x, line0x) << "columns must advance right to left";
}

TEST(Vertical, VertFeatureSubstitutesForms) {
  FontContext &ctx = sharedContext();
  auto glyphsOf = [&](WritingMode mode) {
    Paragraph para;
    para.appendText("「縦組み」", basicStyle(20.0f));
    para.setWritingMode(mode);
    para.ensureShaped(ctx);
    std::multiset<uint16_t> ids;
    for (const Word &word : para.words())
      for (const WordSegment &seg : word.segments)
        for (uint16_t g : seg.shaped->glyphs)
          ids.insert(g);
    return ids;
  };
  // Vertical shaping must swap in 'vert' forms (rotated brackets at least).
  EXPECT_NE(glyphsOf(WritingMode::kHorizontal),
            glyphsOf(WritingMode::kVerticalRL));
}

TEST(Vertical, AutoRotatesLatinMixedIntoCjk) {
  FontContext &ctx = sharedContext();
  Paragraph para;
  para.appendText("縦書きにHTTPが混ざる", basicStyle(20.0f));
  para.setWritingMode(WritingMode::kVerticalRL);
  VerticalBlockFlow flow(SkRect::MakeWH(200, 400));
  LayoutOptions opts;
  opts.lineHeight = 30;
  Layout layout = layoutParagraph(ctx, para, flow, opts);

  bool sawUpright = false, sawRotated = false;
  for (const PlacedRun &run : layout.runs) {
    if (run.shaped->vertical)
      sawUpright = true;
    else if (run.transformed)
      sawRotated = true; // Latin baked as a rotated RSXform blob
  }
  EXPECT_TRUE(sawUpright);
  EXPECT_TRUE(sawRotated);
}

TEST(Vertical, TateChuYokoSetsRunUprightAcrossColumn) {
  FontContext &ctx = sharedContext();
  TextStyle jp = basicStyle(20.0f);
  TextStyle tcy = basicStyle(20.0f);
  tcy.shaping.verticalForm = VerticalForm::kTateChuYoko;

  Paragraph para;
  para.appendText("平成", jp);
  para.appendText("31", tcy);
  para.appendText("年の縦組み", jp);
  para.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(200, 400));
  LayoutOptions opts;
  opts.lineHeight = 30;
  Layout layout = layoutParagraph(ctx, para, flow, opts);

  const float axis = 200 - 30 * 0.5f; // first column's central axis
  const PlacedRun *tcyRun = nullptr;
  for (const PlacedRun &run : layout.runs)
    if (!run.shaped->vertical && !run.transformed)
      tcyRun = &run;
  ASSERT_NE(tcyRun, nullptr) << "the digit run must be placed upright";
  // Centred across the column: origin shifted left by half its advance.
  EXPECT_NEAR(tcyRun->origin.x(), axis - tcyRun->shaped->advance * 0.5f, 0.5f);
  // And it must not consume more column length than its font height (~23px
  // at 20px), far less than the two digits' horizontal advance would be if
  // they were stacked.
  const Word &word = para.words()[std::min<size_t>(
      tcyRun->wordIndex, para.words().size() - 1)];
  EXPECT_LT(word.width, 30.0f);
}

// ── Query layer (Query.h — optional, built on the Paragraph edit log) ────

TEST(Query, FindAllAndRegex) {
  Paragraph para = makeParagraph("the cat sat on the mat, the end");
  const std::vector<CharRange> the = findAll(para, "the");
  ASSERT_EQ(the.size(), 3u);
  EXPECT_EQ(the[0], (CharRange{0, 3}));
  EXPECT_EQ(the[2], (CharRange{24, 27}));

  const auto ats = findRegex(para, "[cms]at");
  ASSERT_TRUE(ats.has_value());
  ASSERT_EQ(ats->size(), 3u);
  EXPECT_EQ((*ats)[0], (CharRange{4, 7}));   // cat
  EXPECT_EQ((*ats)[2], (CharRange{19, 22})); // mat

  EXPECT_FALSE(findRegex(para, "[unclosed").has_value());
}

TEST(Query, WordRangesMatchSegmentation) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("one two three");
  const std::vector<CharRange> words = wordRanges(para, ctx);
  ASSERT_EQ(words.size(), 3u);
  EXPECT_EQ(words[0], (CharRange{0, 3}));
  EXPECT_EQ(words[1], (CharRange{4, 7}));
  EXPECT_EQ(words[2], (CharRange{8, 13}));
}

TEST(Query, MarkerSetFollowsEdits) {
  Paragraph para = makeParagraph("alpha beta gamma delta");
  MarkerSet marks(para);
  marks.set("greek", findAll(para, "gamma")); // [11, 16)

  // Insert before the marker: it shifts.
  para.replaceText(0, 0, ">>> ");
  ASSERT_TRUE(marks.sync(para));
  ASSERT_EQ(marks.get("greek")->size(), 1u);
  EXPECT_EQ(marks.get("greek")->front(), (CharRange{15, 20}));

  // Replace text overlapping the marker: it absorbs the replacement.
  para.replaceText(17, 22, "MMA plus"); // ">>> alpha beta gaMMA plus delta"
  ASSERT_TRUE(marks.sync(para));
  EXPECT_EQ(marks.get("greek")->front(), (CharRange{15, 25}));

  // Delete the whole marked range: the marker collapses and is dropped.
  para.replaceText(15, 25, "");
  ASSERT_TRUE(marks.sync(para));
  EXPECT_TRUE(marks.get("greek")->empty());
}

TEST(Query, MarkerSetStylesAcrossEdits) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("keep the flame lit forever");
  MarkerSet marks(para);
  marks.set("flame", findAll(para, "flame"));

  para.replaceText(0, 4, "guard"); // shifts the marker by +1
  PaintStyle red;
  red.color = 0xFFFF0000;
  marks.applyPaint(para, "flame", red);
  para.ensureShaped(ctx);

  // The span carrying red must cover exactly "flame" in the new text.
  const std::u16string &text = para.text();
  const size_t at = text.find(u"flame");
  ASSERT_NE(at, std::u16string::npos);
  bool found = false;
  for (const StyleSpan &span : para.spans())
    if (span.style.paint.color == 0xFFFF0000) {
      EXPECT_EQ(span.start, static_cast<uint32_t>(at));
      EXPECT_EQ(span.end, static_cast<uint32_t>(at + 5));
      found = true;
    }
  EXPECT_TRUE(found);
}

TEST(Query, MarkerSetReportsHistoryLoss) {
  Paragraph para = makeParagraph("word");
  MarkerSet marks(para);
  marks.set("w", findAll(para, "word"));
  // Blow past the bounded history (256 ops).
  for (int i = 0; i < 400; ++i)
    para.replaceText(0, 0, "x");
  EXPECT_FALSE(marks.sync(para));
  EXPECT_TRUE(marks.get("w")->empty()); // cleared, caller must re-query
}

TEST(Scripts, ArabicLamAlefLigates) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("لا"); // lam + alef: mandatory ligature
  para.ensureShaped(ctx);
  ASSERT_EQ(para.words().size(), 1u);
  const ShapedWord &sw = *para.words()[0].segments[0].shaped;
  if (!allGlyphsResolved(para))
    GTEST_SKIP() << "no Arabic font on this system";
  EXPECT_EQ(sw.glyphs.size(), 1u) << "lam-alef must fuse into one glyph";
}

TEST(Scripts, ArabicJoinsRtl) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("العربية تكتب من اليمين إلى اليسار");
  para.ensureShaped(ctx);
  if (!allGlyphsResolved(para))
    GTEST_SKIP() << "no Arabic font on this system";
  ASSERT_GE(para.words().size(), 5u);
  for (const Word &word : para.words()) {
    EXPECT_EQ(word.bidiLevel & 1, 1) << "Arabic words must be RTL";
    const auto &clusters = word.segments[0].shaped->clusters;
    if (clusters.size() >= 2) // RTL visual order: clusters run backwards
      EXPECT_GT(clusters.front(), clusters.back());
  }
}

TEST(Scripts, DevanagariFormsConjunctClusters) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("नमस्ते दुनिया");
  para.ensureShaped(ctx);
  if (!allGlyphsResolved(para))
    GTEST_SKIP() << "no Devanagari font on this system";
  // "नमस्ते" is 6 UTF-16 units but the virama fuses स्+ते into one grapheme
  // cluster: distinct clusters must be fewer than code units.
  const Word &namaste = para.words()[0];
  ASSERT_EQ(namaste.segments.size(), 1u);
  const ShapedWord &sw = *namaste.segments[0].shaped;
  EXPECT_LT(uniqueClusterCount(sw), 6u);
  EXPECT_GE(sw.glyphs.size(), 3u);
}

TEST(Scripts, CuneiformSupplementaryPlane) {
  FontContext &ctx = sharedContext();
  // Three codepoints beyond the BMP (U+12000, U+12038, U+1204D): each is a
  // surrogate pair, so correct cluster values step by 2 UTF-16 units.
  Paragraph para = makeParagraph("𒀀𒀸𒁍");
  para.ensureShaped(ctx);
  if (!allGlyphsResolved(para))
    GTEST_SKIP() << "no Cuneiform font on this system";
  std::vector<uint32_t> clusters;
  for (const Word &word : para.words())
    for (const WordSegment &seg : word.segments)
      for (uint32_t c : seg.shaped->clusters)
        clusters.push_back(c + word.textBegin);
  ASSERT_FALSE(clusters.empty());
  for (uint32_t c : clusters)
    EXPECT_EQ(c % 2, 0u) << "clusters must land on surrogate-pair starts";
}

TEST(Scripts, EmojiZwjFamilyIsOneCluster) {
  FontContext &ctx = sharedContext();
  // Family emoji: 4 people joined by ZWJ = 11 UTF-16 units, ONE grapheme.
  Paragraph para = makeParagraph("👨‍👩‍👧‍👦");
  para.ensureShaped(ctx);
  ASSERT_EQ(para.words().size(), 1u);
  ASSERT_EQ(para.words()[0].segments.size(), 1u);
  const ShapedWord &sw = *para.words()[0].segments[0].shaped;
  ASSERT_FALSE(sw.glyphs.empty());
  EXPECT_EQ(uniqueClusterCount(sw), 1u)
      << "a ZWJ family sequence is a single grapheme cluster";
  EXPECT_TRUE(allGlyphsResolved(para));
}

TEST(Scripts, EmojiModifierAndFlagClusters) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("👍🏽 🇺🇸"); // skin tone; regional pair
  para.ensureShaped(ctx);
  ASSERT_EQ(para.words().size(), 2u);
  for (const Word &word : para.words()) {
    const ShapedWord &sw = *word.segments[0].shaped;
    EXPECT_EQ(uniqueClusterCount(sw), 1u)
        << "modifier/flag sequences are single grapheme clusters";
  }
  EXPECT_TRUE(allGlyphsResolved(para));
}

TEST(Scripts, EmojiInsideLatinFallsBackPerSegment) {
  FontContext &ctx = sharedContext();
  Paragraph para = makeParagraph("great👍work");
  para.ensureShaped(ctx);
  std::set<const SkTypeface *> faces;
  for (const Word &word : para.words())
    for (const WordSegment &seg : word.segments)
      faces.insert(seg.shaped->typeface.get());
  EXPECT_GE(faces.size(), 2u) << "emoji must resolve to its own typeface";
  EXPECT_TRUE(allGlyphsResolved(para));
}

// ── OpenType features ─────────────────────────────────────────────────────

TEST(Features, LigatureToggleChangesGlyphCount) {
  FontContext &ctx = sharedContext();
  sk_sp<SkTypeface> hoefler =
      ctx.fontMgr()->matchFamilyStyle("Hoefler Text", SkFontStyle());
  if (!hoefler)
    GTEST_SKIP() << "Hoefler Text not installed";

  TextStyle liga = basicStyle();
  liga.shaping.typeface = hoefler;
  TextStyle noLiga = liga;
  noLiga.shaping.features.push_back({"liga", 0});
  noLiga.shaping.features.push_back({"clig", 0});

  Paragraph a, b;
  a.appendText("official", liga);
  b.appendText("official", noLiga);
  a.ensureShaped(ctx);
  b.ensureShaped(ctx);
  const size_t withLiga = a.words()[0].segments[0].shaped->glyphs.size();
  const size_t withoutLiga = b.words()[0].segments[0].shaped->glyphs.size();
  EXPECT_LT(withLiga, withoutLiga) << "'ffi' must ligate when liga is on";
  // Features are part of the shape-cache key: both variants coexist.
  EXPECT_NE(a.words()[0].segments[0].shaped.get(),
            b.words()[0].segments[0].shaped.get());
}

// ── 2000-token multi-script confetti stress ───────────────────────────────

TEST(Stress, BabelConfetti2000) {
  FontContext &ctx = sharedContext();
  const char *tokens[] = {
      "حرف", "كلمة",  "अक्षर", "शब्द", "אות",  "מילה", "ตัวอักษร",
      "字",   "글",     "λόγος", "буква", "🎉",   "👍🏽",  "文字",
      "ঢাকা", "கடல்",  "ᚱᚢᚾ",   "ainm",  "słowo", "λέξη"};
  std::mt19937 rng(77);
  Paragraph para;
  TextStyle style = basicStyle(18.0f);
  std::string text;
  for (int i = 0; i < 2000; ++i) {
    text += tokens[rng() % 20];
    text += ' ';
  }
  para.appendText(text, style);

  LineSetFlow flow;
  for (int i = 0; i < 2000; ++i) {
    const float angle = static_cast<float>(rng() % 628) * 0.01f;
    flow.lines().push_back({LineInterval{
        {20.0f + static_cast<float>(rng() % 1360),
         20.0f + static_cast<float>(rng() % 860)},
        {std::cos(angle), std::sin(angle)},
        60}});
  }
  Layout layout = layoutParagraph(ctx, para, flow);
  EXPECT_GT(layout.runs.size(), 1500u);
  EXPECT_GT(para.words().size(), 1900u);

  // Nothing may leak a .notdef for scripts macOS covers (all of these).
  size_t notdef = 0, total = 0;
  for (const PlacedRun &run : layout.runs)
    for (uint16_t glyph : run.shaped->glyphs) {
      total++;
      notdef += glyph == 0;
    }
  EXPECT_EQ(notdef, 0u) << notdef << " of " << total << " glyphs unresolved";
}
