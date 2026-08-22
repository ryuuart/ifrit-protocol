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

#include "ComposeTestSupport.h"

namespace {

/** Japanese prose long enough to need more than one column. */
constexpr const char8_t* kProse =
    u8"縦組みの文章は上から下へ流れ右から左へと列が進む";

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

TEST(TextVertical, AnEllipsisReportsOverflowWithoutDrawingAMarker) {
  // An overflow marker needs a straight horizontal final line to land on. A
  // clamped column reports its overflow all the same, so a caller can see
  // it; what it must not do is silently claim to have drawn one.
  Host host(340, 200);
  host.composer.render(box().padding(10).child(
      text(kProse, jp(20, SK_ColorWHITE))
          .width(300)
          .height(120)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .maxLines(2)
          .ellipsis(u8"...")
          .key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  EXPECT_TRUE(layout->overflowed());
  EXPECT_FALSE(layout->ellipsized) << "a column has no line to trim a marker "
                                      "onto — and must not report one";
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
