/** @file
 * VERTICAL CJK TEXT through the compose surface: `Element::writingMode`,
 * the axis swap in measurement, per-span vertical forms, and the rest of
 * the text engine — tracks, span restyling, clamping — asked to behave in
 * columns.
 *
 * Every assertion here reads either the live SigilWeave layout the node
 * produced or the pixels it drew, because "vertical" is a claim about
 * geometry and nothing else can check it.
 */

#include <sigilweave/style/Features.h>

#include <algorithm>
#include <array>

#include "support/TextTestSupport.h"

namespace {

/** Japanese prose long enough to need more than one column. */
constexpr const char8_t* kProse =
    u8"縦組みの文章は上から下へ流れ右から左へと列が進む";

/** Long enough to fill several columns, so an exclusion in the middle of
 *  the block costs the passage columns it can be counted on. */
constexpr const char8_t* kLongProse =
    u8"縦組みの文章が円をよけて流れる様子を見るための本文であり、列は右から"
    u8"左へと進みながら、障害物の上と下に分かれて組まれてゆく。文字は列の心"
    u8"に沿って落ちてゆき、円に出会えば頭と足に分かれ、円を過ぎればまた一本"
    u8"の列に戻ってゆく。";

sigil::weave::TextStyle jp(float size, SkColor color) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.languageTag = "ja";
  s.paint.foreground.setColor(color);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

int inkCount(Host& host, SkIRect region) {
  int hits = 0;
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      if (host.pixel(x, y) != SK_ColorBLACK) ++hits;
  return hits;
}

int countPixels(Host& host, SkIRect region, SkColor color) {
  int hits = 0;
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      if (host.pixel(x, y) == color) ++hits;
  return hits;
}

/** The x of each column's first run, in column order. A column's runs all
 *  sit on one axis, so the first one names it. */
std::vector<float> columnAxes(const sigil::weave::ParagraphLayout& layout) {
  std::vector<float> axes;
  int seen = -1;
  for (const sigil::weave::PositionedRun& run : layout.runs) {
    if (run.lineIndex == seen || run.transformed) continue;
    seen = run.lineIndex;
    axes.push_back(run.origin.x());
  }
  return axes;
}

}  // namespace

TEST(TextVertical, ColumnsRunTopToBottomAndAdvanceRightToLeft) {
  Host host(300, 240);
  host.composer.render(box().padding(10).child(
      text(kProse, jp(22, SK_ColorWHITE))
          .width(200)
          .height(180)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  ASSERT_GT(layout->lineCount, 1) << "the measure must have wrapped a column";

  const std::vector<float> axes = columnAxes(*layout);
  ASSERT_GE(axes.size(), 2u);
  EXPECT_LT(axes[1], axes[0])
      << "vertical-rl advances columns RIGHT TO LEFT: column 1 must stand "
         "left of column 0";

  // Within one column the pen travels DOWN.
  float previous = -1e9f;
  for (const sigil::weave::PositionedRun& run : layout->runs) {
    if (run.lineIndex != 0 || run.transformed) continue;
    EXPECT_GT(run.origin.y(), previous);
    previous = run.origin.y();
  }

  // And the first column is the RIGHTMOST ink on the canvas: text starts at
  // the node's right edge, not its left.
  const int rightHalf = inkCount(host, SkIRect::MakeXYWH(150, 0, 150, 240));
  const int leftHalf = inkCount(host, SkIRect::MakeXYWH(0, 0, 150, 240));
  EXPECT_GT(rightHalf, 0);
  EXPECT_GT(rightHalf, leftHalf);
}

TEST(TextVertical, IntrinsicMeasurementSwapsTheAxes) {
  // The same passage, measured both ways with no explicit box: horizontal
  // text is a wide short line, vertical text is a tall thin column.
  const SkSize wide =
      sigil::compose::measure(text(kProse, jp(20, SK_ColorWHITE)), fonts());
  const SkSize tall = sigil::compose::measure(
      text(kProse, jp(20, SK_ColorWHITE))
          .writingMode(sigil::weave::WritingMode::kVerticalRL),
      fonts());
  EXPECT_GT(wide.width(), wide.height());
  EXPECT_GT(tall.height(), tall.width())
      << "a vertical column's MAIN extent is its height";
  // One unwrapped column is one column pitch wide, which is roughly the
  // line height a horizontal setting of the same style would use.
  EXPECT_LT(tall.width(), wide.width() * 0.25f);
  EXPECT_GT(tall.height(), wide.height() * 2.0f);
}

TEST(TextVertical, TateChuYokoSurvivesMaterialization) {
  // A digit pair asks to be set HORIZONTALLY inside the column. Under
  // UTR#50's default the same digits would be ROTATED — a transformed run —
  // so the two forms are distinguishable in the layout alone.
  Host host(300, 260);
  sigil::weave::TextStyle tcy = jp(24, SK_ColorWHITE);
  tcy.shaping.verticalForm = sigil::weave::VerticalForm::kTateChuYoko;

  const auto describe = [&](const sigil::weave::TextStyle& digitStyle) {
    return box().padding(10).child(
        text(rich(jp(24, SK_ColorWHITE))
                 .add(u8"平成")
                 .add(u8"31", digitStyle)
                 .add(u8"年に対応"))
            .width(200)
            .height(200)
            .writingMode(sigil::weave::WritingMode::kVerticalRL)
            .key("t"));
  };

  host.composer.render(describe(tcy));
  host.frame();
  const auto* withTcy = host.composer.paragraphLayout("t");
  ASSERT_NE(withTcy, nullptr);
  int horizontalRuns = 0;
  for (const sigil::weave::PositionedRun& run : withTcy->runs)
    if (run.shaped && !run.transformed && !run.shaped->vertical)
      ++horizontalRuns;
  EXPECT_EQ(horizontalRuns, 1)
      << "the tate-chu-yoko run must be shaped horizontally and set upright "
         "in the column — not rotated, not stacked";

  // The control: the same digits with no per-span form rotate instead.
  host.composer.render(describe(jp(24, SK_ColorWHITE)));
  host.frame();
  const auto* plain = host.composer.paragraphLayout("t");
  ASSERT_NE(plain, nullptr);
  int stillHorizontal = 0;
  for (const sigil::weave::PositionedRun& run : plain->runs)
    if (run.shaped && !run.transformed && !run.shaped->vertical)
      ++stillHorizontal;
  EXPECT_EQ(stillHorizontal, 0)
      << "without the per-span form the digits take UTR#50's rotation";
}

TEST(TextVertical, MaxLinesClampsColumns) {
  Host host(340, 200);
  host.composer.render(box().padding(10).child(
      text(kProse, jp(20, SK_ColorWHITE))
          .width(300)
          .height(120)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .maxLines(2)
          .key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  EXPECT_EQ(layout->lineCount, 2) << "maxLines clamps COLUMNS in vertical text";
  EXPECT_TRUE(layout->overflowed());
}

TEST(TextVertical, AClampedColumnEndsInItsMarker) {
  // The marker lands at the FOOT of the clamped column, upright like the
  // characters above it, and the cut moves up the column to make room for
  // it — the same trade a clamped line makes at its end.
  const auto clamped = [](Host& host, bool withMarker) {
    Element leaf = text(kProse, jp(20, SK_ColorWHITE))
                       .width(300)
                       .height(120)
                       .writingMode(sigil::weave::WritingMode::kVerticalRL)
                       .maxLines(2)
                       .key("t");
    if (withMarker) leaf.ellipsis(u8"…");
    host.composer.render(box().padding(10).child(std::move(leaf)));
    host.frame();
    return host.composer.paragraphLayout("t");
  };
  Host bareHost(340, 200), markedHost(340, 200);
  const auto* bare = clamped(bareHost, false);
  const auto* marked = clamped(markedHost, true);
  ASSERT_NE(bare, nullptr);
  ASSERT_NE(marked, nullptr);

  EXPECT_TRUE(marked->overflowed());
  ASSERT_TRUE(marked->ellipsized);
  ASSERT_FALSE(bare->ellipsized);
  ASSERT_GE(marked->runs.size(), 2u);

  const sigil::weave::PositionedRun& marker = marked->runs.back();
  ASSERT_TRUE(marker.shaped);
  EXPECT_TRUE(marker.shaped->vertical) << "upright, like the column it ends";
  const sigil::weave::PositionedRun& tail =
      marked->runs[marked->runs.size() - 2];
  EXPECT_EQ(marker.lineIndex, tail.lineIndex) << "on the clamped column";
  EXPECT_FLOAT_EQ(marker.origin.x(), tail.origin.x()) << "on its axis";
  EXPECT_GE(marker.origin.y(), tail.origin.y());
  EXPECT_LT(marked->firstUnplacedWord, bare->firstUnplacedWord)
      << "the cut moved up, and the words it gave up are reported unplaced";

  // And it is DRAWN, below the last of the text: the stretch of column
  // past where the type stops carries the marker's ink.
  constexpr int kPad = 10;  // the box the leaf sits in
  const float textFoot = tail.origin.y() + tail.shaped->advance;
  EXPECT_GE(marker.origin.y(), textFoot - 0.25f);
  const SkIRect foot = SkIRect::MakeXYWH(
      kPad + (int)marker.origin.x() - 12, kPad + (int)textFoot, 24,
      (int)(marker.origin.y() - textFoot + marker.shaped->advance) + 1);
  EXPECT_GT(inkCount(markedHost, foot), 0)
      << "nothing was drawn where the marker was placed";
}

TEST(TextVertical, ALineSelectorAddressesAColumn) {
  // sel::line resolves through the LAYOUT, and in a vertical passage the
  // layout numbers columns. Column 0 is the RIGHTMOST one.
  Host host(300, 240);
  host.composer.render(box().padding(10).child(
      text(kProse, jp(22, SK_ColorWHITE))
          .width(200)
          .height(180)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .spanPaint(sel::line(0), sigil::weave::PaintStyle(SK_ColorRED))
          .key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  ASSERT_GT(layout->lineCount, 1);

  const std::vector<float> axes = columnAxes(*layout);
  ASSERT_GE(axes.size(), 2u);
  const int split = (int)((axes[0] + axes[1]) * 0.5f);
  const SkIRect rightmost = SkIRect::MakeLTRB(split, 0, 300, 240);
  const SkIRect rest = SkIRect::MakeLTRB(0, 0, split, 240);
  EXPECT_GT(countPixels(host, rightmost, SK_ColorRED), 10)
      << "column 0 is the one at the right edge";
  EXPECT_EQ(countPixels(host, rest, SK_ColorRED), 0)
      << "the restyle leaked into a column it does not name";
  EXPECT_GT(countPixels(host, rest, SK_ColorWHITE), 10) << "the rest";
}

TEST(TextVertical, AClusterEntranceStaggersDownTheColumn) {
  // Mid-cascade the top of the column has arrived and the bottom has not:
  // reading order down a column is what a stagger beats over.
  choreograph::Output<float> progress{0.45f};
  Host host(200, 320);
  host.composer.render(box().padding(10).child(
      text(u8"一二三四五六七八九十", jp(24, SK_ColorWHITE))
          .width(60)
          .height(300)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .fx({.effect = fx::rise(30),
               .stagger = stagger(unit::Cluster, {.eachMs = 90}),
               .progress = &progress})
          .key("t")));
  host.frame();
  const int top = inkCount(host, SkIRect::MakeXYWH(0, 10, 200, 120));
  const int bottom = inkCount(host, SkIRect::MakeXYWH(0, 190, 200, 120));
  EXPECT_GT(top, 0) << "the head of the column must have arrived";
  EXPECT_GT(top, bottom * 2)
      << "the tail of the column must still be on its way";

  progress = 1.0f;
  host.frame();
  EXPECT_GT(inkCount(host, SkIRect::MakeXYWH(0, 190, 200, 120)), 0)
      << "at rest the whole column draws";
}

TEST(TextVertical, SpanPaintRecolorsAColumnWithoutReshaping) {
  Host host(300, 260);
  const std::u8string body = u8"赤い文字と白い文字";
  const auto shapesOf = [&] {
    std::vector<const void*> out;
    const auto* layout = host.composer.paragraphLayout("t");
    if (!layout) return out;
    for (const sigil::weave::PositionedRun& run : layout->runs)
      out.push_back(run.shaped.get());
    return out;
  };
  const auto originsOf = [&] {
    std::vector<SkPoint> out;
    const auto* layout = host.composer.paragraphLayout("t");
    if (!layout) return out;
    for (const sigil::weave::PositionedRun& run : layout->runs)
      out.push_back(run.origin);
    return out;
  };

  const auto describe = [&](bool restyled) {
    Element t = text(body, jp(24, SK_ColorWHITE))
                    .width(200)
                    .height(220)
                    .writingMode(sigil::weave::WritingMode::kVerticalRL)
                    .key("t");
    if (restyled)
      t.spanPaint(sel::text(u8"赤い"), sigil::weave::PaintStyle(SK_ColorRED));
    return box().padding(10).child(std::move(t));
  };

  host.composer.render(describe(false));
  host.frame();
  const std::vector<const void*> shapesBefore = shapesOf();
  const std::vector<SkPoint> originsBefore = originsOf();
  ASSERT_FALSE(shapesBefore.empty());

  host.composer.render(describe(true));
  host.frame();
  EXPECT_EQ(shapesOf(), shapesBefore) << "a paint-only restyle re-shaped";
  EXPECT_EQ(originsOf(), originsBefore) << "a paint-only restyle moved a glyph";
  const SkIRect all = SkIRect::MakeXYWH(0, 0, 300, 260);
  EXPECT_GT(countPixels(host, all, SK_ColorRED), 10) << "the named range";
  EXPECT_GT(countPixels(host, all, SK_ColorWHITE), 10) << "the rest";
}

TEST(TextVertical, TheParagraphOverloadKeepsTheFieldMaskContract) {
  Host host(300, 240);
  auto vertical = std::make_shared<sigil::weave::Paragraph>();
  vertical->appendText(kProse, jp(20, SK_ColorWHITE));
  vertical->setWritingMode(sigil::weave::WritingMode::kVerticalRL);

  // No setter names the mode: the paragraph's own mode stands.
  host.composer.render(box().padding(10).child(
      text(vertical, {}).width(200).height(180).key("t")));
  host.frame();
  const auto* asPassed = host.composer.paragraphLayout("t");
  ASSERT_NE(asPassed, nullptr);
  ASSERT_FALSE(asPassed->runs.empty());
  EXPECT_TRUE(asPassed->runs.front().shaped->vertical)
      << "an unset writingMode() must leave a passed paragraph alone";

  // The setter names it: the setter wins, field by field, as every other
  // layout-option setter does on this overload.
  host.composer.render(box().padding(10).child(
      text(vertical, {})
          .width(200)
          .height(180)
          .writingMode(sigil::weave::WritingMode::kHorizontal)
          .key("t2")));
  host.frame();
  const auto* overridden = host.composer.paragraphLayout("t2");
  ASSERT_NE(overridden, nullptr);
  ASSERT_FALSE(overridden->runs.empty());
  EXPECT_FALSE(overridden->runs.front().shaped->vertical)
      << "writingMode() must override the paragraph it was handed";
}

TEST(TextVertical, OnPathIgnoresWritingModeAndSaysSoOnce) {
  Host host(240, 240);
  ::testing::internal::CaptureStderr();
  host.composer.render(
      box().child(text(u8"縦書きは曲線に乗らない", jp(20, SK_ColorWHITE))
                      .width(200)
                      .height(200)
                      .centerAt({120, 120})
                      .writingMode(sigil::weave::WritingMode::kVerticalRL)
                      .onPath({.path = shapes::circle()})
                      .key("t")));
  host.frame();
  const std::string first = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(first.find("onPath"), std::string::npos)
      << "the conflict must be diagnosed, not silently resolved";

  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  ASSERT_FALSE(layout->runs.empty());
  for (const sigil::weave::PositionedRun& run : layout->runs)
    if (run.shaped)
      EXPECT_FALSE(run.shaped->vertical)
          << "the PATH is the baseline — there are no columns to advance";

  // Once, not once per frame.
  ::testing::internal::CaptureStderr();
  host.frame();
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "");
}

TEST(TextVertical, AnUprightGlyphTurnsAboutItsColumnAxis) {
  // The RSXform convention backs a glyph out from its pose centre by half
  // its advance along its own x. In a column half an advance is a step DOWN
  // the page, so a glyph that scales must not also slide half a column
  // pitch sideways: the pivot is the point on the COLUMN AXIS its pen
  // reached. A flat quarter-scale makes the drift, if there is any, a
  // quarter of a line height rather than a rounding error.
  Host host(240, 300);
  const auto describe = [&](bool shrunk) {
    Element t = text(u8"一二三四五", jp(28, SK_ColorWHITE))
                    .width(60)
                    .height(260)
                    .writingMode(sigil::weave::WritingMode::kVerticalRL)
                    .key("t");
    if (shrunk)
      t.fx({.effect = fx::effect(
                "quarter",
                [](const GlyphInfo&, float, Rng&) {
                  GlyphMod m;
                  m.scale = 0.25f;
                  return m;
                },
                40.0f)});
    return box().padding(10).child(std::move(t));
  };
  const auto inkCentroidX = [&] {
    double sum = 0;
    int hits = 0;
    for (int y = 0; y < 300; ++y)
      for (int x = 0; x < 240; ++x)
        if (host.pixel(x, y) != SK_ColorBLACK) {
          sum += x;
          ++hits;
        }
    return hits ? sum / hits : 0.0;
  };

  host.composer.render(describe(false));
  host.frame();
  const double atRest = inkCentroidX();
  ASSERT_GT(atRest, 0.0);

  host.composer.render(describe(true));
  host.frame();
  const double shrunk = inkCentroidX();
  ASSERT_GT(shrunk, 0.0) << "the shrunken glyphs must still draw";
  EXPECT_NEAR(shrunk, atRest, 4.0)
      << "a scaled column drifted off its axis — the pivot took half a "
         "VERTICAL advance sideways";
}

TEST(TextVertical, BeatsOfRunsDownTheColumnAndAcrossToTheNext) {
  // A cascade's read-back has to speak the geometry the column is in. A
  // vertical glyph advances DOWN its column and stands about an em across
  // it, so beats descend inside a column and step LEFT to the next one —
  // the reading order kVerticalRL puts them in, and the exact opposite of
  // what a horizontal rect built from the same numbers would say.
  Host host(220, 300);
  host.composer.render(box().padding(10).child(
      text(kProse, jp(20, SK_ColorWHITE))
          .key("col")
          .width(180)
          .height(240)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .fx({.effect = fx::rise(10),
               .stagger = stagger(unit::Cluster, {.eachMs = 40})})));
  host.frame();

  const std::vector<Beat> beats = host.composer.beatsOf("col", 0);
  ASSERT_GT(beats.size(), 12u);
  const SkRect block =
      host.composer.bounds("col").value_or(SkRect::MakeEmpty());
  ASSERT_FALSE(block.isEmpty()) << "the column block was never laid out";
  for (const Beat& beat : beats) {
    EXPECT_FALSE(beat.rect.isEmpty());
    EXPECT_GT(beat.rect.height(), 4.0f)
        << "a vertical glyph's beat has no extent along its own advance";
  }
  // Down a column: consecutive beats descend until the column breaks.
  int descents = 0, columnBreaks = 0;
  for (size_t i = 1; i < beats.size(); ++i) {
    if (beats[i].rect.centerY() > beats[i - 1].rect.centerY() + 1.0f)
      ++descents;
    else if (beats[i].rect.centerX() < beats[i - 1].rect.centerX() - 1.0f)
      ++columnBreaks;
  }
  EXPECT_GT(descents, (int)beats.size() / 2)
      << "the beats do not run down the column";
  EXPECT_GT(columnBreaks, 0)
      << "the passage never broke to a second column: nothing is proven";
  // …and the whole schedule stays inside the node it belongs to.
  for (const Beat& beat : beats)
    EXPECT_TRUE(block.intersects(beat.rect))
        << "a beat landed outside the column block";
}

TEST(TextVertical, ASubstitutionIsGatedOnTheAxisItsRunAdvancesOn) {
  // A code-point substitution draws its replacement at the ORIGINAL glyph's
  // pen position, so it is sound exactly when the two advance the pen
  // equally along the axis THAT PEN STEPS ON — and a column's pen steps
  // down. The instrument face is authored so the two axes disagree about
  // both pairs: B advances as A does down a column and not along a line, C
  // along a line and not down a column. A gate reading one axis for both
  // orientations therefore reaches the wrong verdict on every case here.
  const sk_sp<SkTypeface> face = fonts().fontManager()->makeFromFile(
      SIGILCOMPOSE_TEST_ASSET_DIR "/VerticalAdvance.ttf");
  ASSERT_TRUE(face) << "test asset VerticalAdvance.ttf failed to load";
  const SkGlyphID a = face->unicharToGlyph(U'A');
  const SkGlyphID b = face->unicharToGlyph(U'B');
  const SkGlyphID c = face->unicharToGlyph(U'C');
  ASSERT_TRUE(a && b && c) << "the instrument face is missing a letter";
  const auto advance = [&](SkGlyphID glyph, bool vertical) {
    return fonts().glyphAdvanceEm(face, glyph, vertical);
  };
  ASSERT_FLOAT_EQ(advance(a, true), advance(b, true));
  ASSERT_NE(advance(a, false), advance(b, false));
  ASSERT_FLOAT_EQ(advance(a, false), advance(c, false));
  ASSERT_NE(advance(a, true), advance(c, true));

  sigil::weave::TextStyle style;
  style.shaping.typeface = face;
  style.shaping.fontSize = 40.0f;
  // Latin stands upright in a column only when told to; left on kAuto it
  // would lie on its side, which is a rotated HORIZONTAL run and would put
  // the horizontal verdict back under the test.
  style.shaping.verticalForm = sigil::weave::VerticalForm::kUpright;
  style.paint.foreground.setColor(SK_ColorWHITE);
  style.paint.foreground.setAntiAlias(true);

  const auto render = [&](Host& host, sigil::weave::WritingMode mode,
                          char32_t point) {
    GlyphMod mod;
    mod.codepoint = point;
    host.composer.render(box().padding(10).child(
        text(u8"AAA", style)
            .key("k")
            .writingMode(mode)
            .fx({.effect = fx::effect(
                     point ? "sub" : "rest",
                     [mod](const GlyphInfo&, float, Rng&) { return mod; },
                     /*reach=*/120.0f)})));
    host.frame();
  };
  const auto drawn = [&](sigil::weave::WritingMode mode, char32_t point) {
    Host rest(220, 260), swapped(220, 260);
    render(rest, mode, 0);
    render(swapped, mode, point);
    return !identicalPixels(rest, swapped, 220, 260);
  };

  const sigil::weave::WritingMode column =
      sigil::weave::WritingMode::kVerticalRL;
  const sigil::weave::WritingMode line = sigil::weave::WritingMode::kHorizontal;
  EXPECT_TRUE(drawn(column, U'B'))
      << "a column refused a replacement that steps exactly as far down it "
         "as the glyph it replaces — the gate measured the advance ACROSS "
         "the column instead of along it";
  EXPECT_FALSE(drawn(column, U'C'))
      << "a column admitted a replacement with a shorter vertical advance, "
         "which moves every glyph below it";
  EXPECT_TRUE(drawn(line, U'C'))
      << "a level run refused an equal-width replacement";
  EXPECT_FALSE(drawn(line, U'B'))
      << "a level run admitted a wider replacement, which moves every glyph "
         "after it";
}

TEST(TextVertical, AMarkAnchorsToTheColumnItsUnitStandsIn) {
  // A mark's rect is the union of the advance boxes of the glyphs it
  // addressed, and in a column those boxes stack DOWN one axis. So a
  // phrase's mark is a tall narrow box standing in that phrase's column —
  // not a wide short one across the page.
  Host host(300, 260);
  host.composer.render(box().padding(10).child(
      text(kProse, jp(22, SK_ColorWHITE))
          .width(200)
          .height(200)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .mark(sel::text(u8"縦組み"),
                box().key("rule").width(Dim(3.0f)).fill(red()))
          .key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  const SkRect rule =
      host.composer.bounds("rule").value_or(SkRect::MakeEmpty());
  ASSERT_FALSE(rule.isEmpty()) << "the mark never resolved";
  EXPECT_GT(rule.height(), rule.width() * 3)
      << "three characters of a column make a TALL mark";

  // And it stands in the column those characters are in: the phrase opens
  // the passage, so that is column 0, the rightmost.
  const std::vector<float> axes = columnAxes(*layout);
  ASSERT_GE(axes.size(), 2u);
  EXPECT_GT(rule.centerX(), (axes[0] + axes[1]) * 0.5f)
      << "the mark drifted out of the column it names";
}

TEST(TextVertical, ASpanStyleReshapesOnlyTheRunItNames) {
  // A span restyle that changes SHAPING re-shapes its own run and leaves
  // its neighbours' shaped words alone — the same contract a line keeps,
  // asked of a column, where the re-shaped run also changes how far the
  // pen steps DOWN.
  Host host(300, 300);
  const auto describe = [&](bool dressed) {
    Element t = text(u8"縦組みの文章", jp(24, SK_ColorWHITE))
                    .width(80)
                    .height(260)
                    .writingMode(sigil::weave::WritingMode::kVerticalRL)
                    .key("t");
    if (dressed) {
      sigil::weave::TextStyle big = jp(40, SK_ColorWHITE);
      t.spanStyle(sel::text(u8"文章"), big);
    }
    return box().padding(10).child(std::move(t));
  };
  host.composer.render(describe(false));
  host.frame();
  const auto* plain = host.composer.paragraphLayout("t");
  ASSERT_NE(plain, nullptr);
  const float plainExtent = plain->runs.back().origin.y();

  host.composer.render(describe(true));
  host.frame();
  const auto* dressed = host.composer.paragraphLayout("t");
  ASSERT_NE(dressed, nullptr);
  bool sawBigger = false;
  for (const sigil::weave::PositionedRun& run : dressed->runs)
    if (run.shaped && run.shaped->fontSize > 30.0f) sawBigger = true;
  EXPECT_TRUE(sawBigger) << "the named run kept the size it was set at";
  EXPECT_GT(dressed->runs.back().origin.y(), plainExtent)
      << "a taller run must push the pen further down the column";
}

TEST(TextVertical, AColumnCarriesItsSidelineBesideTheType) {
  // 傍線 through the compose surface: a decoration on a span of a vertical
  // passage runs BESIDE its column, the length of the run it dresses.
  Host host(240, 300);
  sigil::weave::PaintStyle sidelined(SK_ColorWHITE);
  sigil::weave::Decoration sideline;
  sideline.thickness = 3.0f;
  sideline.color = SK_ColorRED;
  sidelined.addDecoration(sideline);
  host.composer.render(box().padding(10).child(
      text(u8"一二三四五六七八", jp(24, SK_ColorWHITE))
          .width(60)
          .height(260)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .spanPaint(sel::text(u8"三四五六"), sidelined)
          .key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  ASSERT_FALSE(layout->runs.empty());
  const float axis = layout->runs.front().origin.x();

  int top = 1000, bottom = -1, leftOfAxis = 0, redPixels = 0;
  for (int y = 0; y < 300; ++y)
    for (int x = 0; x < 240; ++x) {
      const SkColor c = host.pixel(x, y);
      if (SkColorGetR(c) < 200 || SkColorGetG(c) > 128 || SkColorGetB(c) > 128)
        continue;
      ++redPixels;
      if ((float)x < axis) ++leftOfAxis;
      top = std::min(top, y);
      bottom = std::max(bottom, y);
    }
  ASSERT_GT(redPixels, 0) << "the span drew no band";
  EXPECT_EQ(leftOfAxis, 0) << "the band crossed the column axis";
  EXPECT_GT(bottom - top, 40) << "the band must run DOWN the column";
  // Four of eight characters are dressed, so the band covers about half
  // the column and not all of it.
  EXPECT_LT(bottom - top, 200);
}

TEST(TextVertical, ACascadeOverLinesBeatsColumnByColumn) {
  // `unit::Line` IS a column here. Mid-cascade the first column has
  // arrived whole and the next has not — the two halves of the same
  // passage separated by the geometry, not by the text.
  choreograph::Output<float> progress{0.35f};
  Host host(300, 260);
  host.composer.render(box().padding(10).child(
      text(kProse, jp(22, SK_ColorWHITE))
          .width(200)
          .height(200)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .fx({.effect = fx::typeOn(),
               .stagger = stagger(unit::Line, {.eachMs = 400}),
               .progress = &progress})
          .key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  ASSERT_GT(layout->lineCount, 1);
  const std::vector<float> axes = columnAxes(*layout);
  ASSERT_GE(axes.size(), 2u);
  const int split = (int)((axes[0] + axes[1]) * 0.5f);

  EXPECT_GT(inkCount(host, SkIRect::MakeLTRB(split, 0, 300, 260)), 0)
      << "the first column must have arrived";
  EXPECT_EQ(inkCount(host, SkIRect::MakeLTRB(0, 0, split, 260)), 0)
      << "a later column arrived with the first: the cascade is not "
         "beating over COLUMNS";

  progress = 1.0f;
  host.frame();
  EXPECT_GT(inkCount(host, SkIRect::MakeLTRB(0, 0, split, 260)), 0)
      << "at rest every column draws";
}

TEST(TextVertical, AColumnStyleCanAskTheFaceForItsVerticalMetrics) {
  // The vertical feature presets are ordinary shaping fields, so they
  // reach a column through the same span verbs as any other — and because
  // they are part of shaping identity, asking for one re-shapes the run
  // rather than repainting it.
  Host host(240, 300);
  const auto describe = [&](bool asked) {
    sigil::weave::TextStyle style = jp(24, SK_ColorWHITE);
    if (asked)
      style.shaping.fontFeatures = {
          sigil::weave::Features::proportionalVerticalMetrics,
          sigil::weave::Features::verticalKana};
    return box().padding(10).child(
        text(u8"「縦組み」、ちょっと。", style)
            .width(60)
            .height(260)
            .writingMode(sigil::weave::WritingMode::kVerticalRL)
            .key("t"));
  };
  host.composer.render(describe(false));
  host.frame();
  const auto* plain = host.composer.paragraphLayout("t");
  ASSERT_NE(plain, nullptr);
  ASSERT_FALSE(plain->runs.empty());
  const void* plainShape = plain->runs.front().shaped.get();

  host.composer.render(describe(true));
  host.frame();
  const auto* asked = host.composer.paragraphLayout("t");
  ASSERT_NE(asked, nullptr);
  ASSERT_FALSE(asked->runs.empty());
  EXPECT_NE(asked->runs.front().shaped.get(), plainShape)
      << "a feature list that is part of the shape key reused a shaped word";
}

TEST(TextVertical, ABandStandsAtRestUnderATrack) {
  // A track draws its own glyphs in batched buckets, and a bucket carries
  // glyphs alone — so the band a span asked for is drawn beside them, from
  // the layout the letters left at rest. It therefore does NOT travel with
  // the cascade: the letters rise into place and the sideline stands still
  // the whole way, which is the same stand a mark() takes under a track.
  choreograph::Output<float> progress{0.4f};
  sigil::weave::PaintStyle sidelined(SK_ColorWHITE);
  sigil::weave::Decoration sideline;
  sideline.thickness = 3.0f;
  sideline.color = SK_ColorRED;
  sidelined.addDecoration(sideline);
  const auto describe = [&] {
    return box().padding(10).child(
        text(u8"一二三四五六七八", jp(24, SK_ColorWHITE))
            .width(60)
            .height(220)
            .writingMode(sigil::weave::WritingMode::kVerticalRL)
            .spanPaint(sel::text(u8"三四五六"), sidelined)
            .fx({.effect = fx::rise(24),
                 .stagger = stagger(unit::Cluster, {.eachMs = 90}),
                 .progress = &progress})
            .key("t"));
  };

  Host host(240, 260);
  host.composer.render(describe());
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  ASSERT_FALSE(layout->runs.empty());
  const float axis = layout->runs.front().origin.x();

  // Every pixel the band painted, as a box: red ink, well clear of the
  // white glyphs.
  const auto bandBox = [&] {
    SkIRect extent = SkIRect::MakeLTRB(1000, 1000, -1, -1);
    for (int y = 0; y < 260; ++y)
      for (int x = 0; x < 240; ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetR(c) < 200 || SkColorGetG(c) > 128 ||
            SkColorGetB(c) > 128)
          continue;
        extent.fLeft = std::min(extent.fLeft, x);
        extent.fTop = std::min(extent.fTop, y);
        extent.fRight = std::max(extent.fRight, x);
        extent.fBottom = std::max(extent.fBottom, y);
      }
    return extent;
  };

  const SkIRect midCascade = bandBox();
  ASSERT_LE(midCascade.left(), midCascade.right())
      << "the band a span asked for was not drawn under a track";
  EXPECT_GE((float)midCascade.left(), axis)
      << "the sideline crossed the column axis";
  EXPECT_GT(midCascade.height(), 40)
      << "the band must run DOWN the column it dresses";
  EXPECT_LT(midCascade.height(), 200)
      << "the band covers the dressed phrase, not the whole column";

  // Every white pixel: the glyphs, which the track IS moving.
  const auto glyphInk = [&] {
    int hits = 0;
    for (int y = 0; y < 260; ++y)
      for (int x = 0; x < 240; ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetR(c) > 128 && SkColorGetG(c) > 128 &&
            SkColorGetB(c) > 128)
          ++hits;
      }
    return hits;
  };
  const int midInk = glyphInk();

  progress = 1.0f;
  host.frame();
  EXPECT_NE(glyphInk(), midInk)
      << "the cascade did not move the letters, so this proves nothing "
         "about the band";
  EXPECT_EQ(bandBox(), midCascade)
      << "the band moved with the cascade: it must stand at the rest "
         "placement the layout gives it";
}

TEST(TextVertical, ASidelineCanTakeTheOtherSideOfTheColumn) {
  // A column reads its emphasis line on the RIGHT, which is the default
  // here; `Decoration::Side::kOpposite` is the other placement — the side
  // an automatic vertical underline would take — and it is a plain field
  // on the decoration, so it reaches a span through the same paint verb.
  const auto describe = [&](sigil::weave::Decoration::Side side) {
    sigil::weave::PaintStyle sidelined(SK_ColorWHITE);
    sigil::weave::Decoration sideline;
    sideline.thickness = 3.0f;
    sideline.color = SK_ColorRED;
    sideline.side = side;
    sidelined.addDecoration(sideline);
    return box().padding(10).child(
        text(u8"一二三四五六七八", jp(24, SK_ColorWHITE))
            .width(60)
            .height(220)
            .writingMode(sigil::weave::WritingMode::kVerticalRL)
            .spanPaint(sel::text(u8"三四五六"), sidelined)
            .key("t"));
  };

  Host host(240, 260);
  const auto redSpread = [&] {
    int count = 0, leftOfAxis = 0, rightOfAxis = 0;
    const auto* layout = host.composer.paragraphLayout("t");
    const float axis = layout->runs.front().origin.x();
    for (int y = 0; y < 260; ++y)
      for (int x = 0; x < 240; ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetR(c) < 200 || SkColorGetG(c) > 128 ||
            SkColorGetB(c) > 128)
          continue;
        ++count;
        if ((float)x < axis)
          ++leftOfAxis;
        else
          ++rightOfAxis;
      }
    return std::array<int, 3>{count, leftOfAxis, rightOfAxis};
  };

  host.composer.render(describe(sigil::weave::Decoration::Side::kDefault));
  host.frame();
  const std::array<int, 3> onTheRight = redSpread();
  ASSERT_GT(onTheRight[0], 0) << "the span drew no band";
  EXPECT_EQ(onTheRight[1], 0) << "the default band stands right of the axis";

  host.composer.render(describe(sigil::weave::Decoration::Side::kOpposite));
  host.frame();
  const std::array<int, 3> onTheLeft = redSpread();
  ASSERT_GT(onTheLeft[0], 0) << "the swapped band vanished";
  EXPECT_EQ(onTheLeft[2], 0) << "the swapped band stayed right of the axis";
  EXPECT_NEAR(onTheLeft[1], onTheRight[2], onTheRight[2] * 0.25)
      << "the same band, the other side: it must keep its length";
}

TEST(TextVertical, ColumnsFlowAroundASilhouette) {
  // `flowAround` reads the same in both writing modes: the target's
  // silhouette is subtracted from the text's geometry, and in a column
  // that means the column it crosses is cut in two — a head above the
  // shape and a foot below it — rather than a line shortened beside it.
  const auto scene = [&](bool avoidIt) {
    Element leaf = text(kLongProse, jp(19, SK_ColorWHITE))
                       .width(280)
                       .height(300)
                       .writingMode(sigil::weave::WritingMode::kVerticalRL)
                       .key("body");
    if (avoidIt) leaf.flowAround("obstacle", 5);
    return stack()
        .child(box()
                   .key("obstacle")
                   .width(90)
                   .height(90)
                   .left(90)
                   .top(110)
                   .shape(shapes::circle())
                   .fill(Fill::color({0, 0.4f, 0, 1})))
        .child(box().inset(0).child(std::move(leaf)).zIndex(1));
  };
  Host over(300, 320), around(300, 320);
  over.composer.render(scene(false));
  over.frame();
  around.composer.render(scene(true));
  around.frame();

  // The middle of the disc: type crosses it when nothing is declared, and
  // never when the silhouette is.
  const SkIRect middle = SkIRect::MakeLTRB(118, 138, 152, 172);
  EXPECT_TRUE(anyWhiteIn(over, middle));
  EXPECT_FALSE(anyWhiteIn(around, middle));
  // And the columns the disc crosses are CUT, not stopped: type stands
  // both above it and below it, in the strip the disc sits in.
  EXPECT_TRUE(anyWhiteIn(around, SkIRect::MakeLTRB(95, 20, 185, 100)));
  EXPECT_TRUE(anyWhiteIn(around, SkIRect::MakeLTRB(95, 210, 185, 295)));

  const auto* layout = around.composer.paragraphLayout("body");
  ASSERT_NE(layout, nullptr);
  const auto* clear = over.composer.paragraphLayout("body");
  ASSERT_NE(clear, nullptr);
  EXPECT_GT(layout->lineCount, clear->lineCount)
      << "the exclusion costs the passage columns";
}
