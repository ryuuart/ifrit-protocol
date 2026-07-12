#include <textflow/TextFlow.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <gtest/gtest.h>

#include <algorithm>
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

// Exact pen x where a run's word ends. Blob ink bounds are conservative
// (font-bounds based), so line-edge checks use shaped advances instead.
// Assumes single-segment words (true for these test strings).
float runEnd(const Paragraph &para, const PlacedRun &run) {
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
