// A SPAN RESTYLE CARRIED AS AXIS TRACKS — the fold that redraws an
// advance-invariant axis without reshaping, and reshapes instead when the
// axis moves advances — and the block controls a text leaf sets by verb.
//
// The text binary's share of the content suites, one file per subject.

#include <include/core/SkFont.h>

#include <memory>

#include "DressedTypeProbes.h"

namespace {

/** The instrument face carrying the advance-invariant GRAD axis, and that
 *  axis's own design range read off the face rather than written here.
 *  Null only when the face fails the two things a case about the axis needs
 *  of it — advances that hold, and ink that moves — which is a broken
 *  instrument and not a machine without a font. */
sk_sp<SkTypeface> gradFace(float& lo, float& hi) {
  lo = hi = 0;
  sk_sp<SkTypeface> face = sigil::test::instrument::variable();
  if (!face || !fonts().axisIsAdvanceInvariant(face, "GRAD")) return nullptr;
  const int count = face->getVariationDesignParameters({});
  if (count <= 0) return nullptr;
  std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
  face->getVariationDesignParameters({axes.data(), axes.size()});
  for (const auto& axis : axes)
    if (axis.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) {
      lo = axis.min;
      hi = axis.max;
    }
  if (hi <= lo) return nullptr;

  // The advance probe proves advances HOLD; it cannot prove the clone
  // RESPONDS. Rasterize one glyph at both ends and insist it does.
  const sigil::weave::FontVariation vLo("GRAD", lo), vHi("GRAD", hi);
  const auto ink = [&](const sigil::weave::FontVariation& v) {
    SkFont font(fonts().variedTypeface(face, {&v, 1}), 48);
    const SkGlyphID glyph = font.unicharToGlyph('W');
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 80));
    surface->getCanvas()->clear(SK_ColorBLACK);
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    paint.setAntiAlias(true);
    const SkPoint at{10, 60};
    surface->getCanvas()->drawGlyphs(SkSpan(&glyph, 1), SkSpan(&at, 1), {0, 0},
                                     font, paint);
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    surface->readPixels(bitmap.pixmap(), 0, 0);
    return bitmap;
  };
  const SkBitmap light = ink(vLo), heavy = ink(vHi);
  for (int y = 0; y < 80; ++y)
    for (int x = 0; x < 100; ++x)
      if (light.getColor(x, y) != heavy.getColor(x, y)) return face;
  return nullptr;
}

/** The whole surface, for a byte comparison against another frame. */
SkBitmap grab(Host& host, int w, int h) {
  SkBitmap out;
  out.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(out.pixmap(), 0, 0);
  return out;
}

int pixelsDiffering(const SkBitmap& a, const SkBitmap& b, int w, int h) {
  int changed = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) changed += a.getColor(x, y) != b.getColor(x, y);
  return changed;
}

}  // namespace

TEST(TextSpanAxis, AnInvariantAxisRedrawsWithoutReshaping) {
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  ASSERT_TRUE(face) << "the instrument's GRAD axis must hold advances "
                       "while moving ink";

  Host host(400, 120);
  sigil::weave::TextStyle base = coloredStyle(40, SK_ColorWHITE);
  base.shaping.typeface = face;
  const std::u8string body = u8"Count 1234 now";
  host.composer.render(box().padding(10).child(text(body, base).key("t")));
  host.frame();
  const std::vector<const void*> shapesBefore = runShapes(host, "t");
  const std::vector<SkPoint> originsBefore = runOrigins(host, "t");
  ASSERT_FALSE(shapesBefore.empty());
  const SkBitmap plain = grab(host, 400, 120);

  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanStyle(sigil::weave::sel::regex(u8"[0-9]+"),
                     withAxis(base, "GRAD", hi))
          .key("t")));
  host.frame();
  EXPECT_EQ(runShapes(host, "t"), shapesBefore)
      << "an axis-only spanStyle re-shaped a word — the axis went into the "
         "shaping style instead of onto the glyphs";
  EXPECT_EQ(runOrigins(host, "t"), originsBefore)
      << "an axis-only spanStyle moved a glyph";
  EXPECT_GT(pixelsDiffering(plain, grab(host, 400, 120), 400, 120), 20)
      << "the graded numerals are drawing exactly as the ungraded ones did";
}

TEST(TextSpanAxis, AnAxisRestyleKeepsAnEarlierSpanPaintAndFoldsAnyway) {
  // The two verbs address different dimensions, so declaring them in
  // either order over one selection must produce one picture. What made
  // that untrue is invisible from the authoring side: a `TextStyle`
  // carries a paint whether or not its author was thinking about paint,
  // so a `spanStyle` after a `spanPaint` must not read the colour standing
  // under it as a difference to shape away — it would then overwrite it
  // with the style's own, leaving the author's colour nowhere and saying
  // nothing.
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  ASSERT_TRUE(face) << "the instrument's GRAD axis must hold advances "
                       "while moving ink";

  sigil::weave::TextStyle base = coloredStyle(40, SK_ColorWHITE);
  base.shaping.typeface = face;
  const std::u8string body = u8"Count 1234 now";
  const SkIRect all = SkIRect::MakeWH(400, 120);

  // The order an author reaches for when the colour is the point.
  Host paintFirst(400, 120);
  paintFirst.composer.render(box().padding(10).child(
      text(body, base)
          .spanPaint(sigil::weave::sel::regex(u8"[0-9]+"),
                     sigil::weave::PaintStyle(SK_ColorRED))
          .spanStyle(sigil::weave::sel::regex(u8"[0-9]+"),
                     withAxis(base, "GRAD", hi))
          .key("t")));
  paintFirst.frame();
  EXPECT_GT(countColor(paintFirst, all, SK_ColorRED), 20)
      << "the spanStyle painted over the earlier spanPaint's colour";
  const std::vector<const void*> shapes = runShapes(paintFirst, "t");

  // …and the order that works around it, which must now be the same
  // picture rather than the only one that keeps both declarations.
  Host styleFirst(400, 120);
  styleFirst.composer.render(box().padding(10).child(
      text(body, base)
          .spanStyle(sigil::weave::sel::regex(u8"[0-9]+"),
                     withAxis(base, "GRAD", hi))
          .spanPaint(sigil::weave::sel::regex(u8"[0-9]+"),
                     sigil::weave::PaintStyle(SK_ColorRED))
          .key("t")));
  styleFirst.frame();
  EXPECT_EQ(pixelsDiffering(grab(paintFirst, 400, 120),
                            grab(styleFirst, 400, 120), 400, 120),
            0)
      << "the two declaration orders draw different pictures";
  EXPECT_EQ(shapes, runShapes(styleFirst, "t"))
      << "the axis restyle re-shaped the numerals under the earlier "
         "spanPaint instead of folding onto the glyphs it already had";
}

TEST(TextSpanAxis, AnAdvanceVariantAxisReshapesInstead) {
  // The same instrument, read on its other axis: wght genuinely
  // interpolates advances there, so the fold has something to decline.
  const sk_sp<SkTypeface> face = sigil::test::instrument::variable();
  ASSERT_TRUE(face);
  ASSERT_GT(face->getVariationDesignParameters({}), 0);
  ASSERT_FALSE(fonts().axisIsAdvanceInvariant(face, "wght"))
      << "the instrument face's wght must move advances";

  Host host(400, 120);
  sigil::weave::TextStyle base = coloredStyle(44, SK_ColorWHITE);
  base.shaping.typeface = face;
  const std::u8string body = u8"WEIGHT";
  // An axis the face moves advances on cannot be held on the glyphs, so the
  // restyle takes the other road: it re-shapes the words it covers, which is
  // what the verb promises anyway — and a routing decision the author never
  // asked about is nothing to warn about.
  ::testing::internal::CaptureStderr();
  const auto at = [&](float weight) {
    host.composer.render(box().padding(10).child(
        text(body, base)
            .spanStyle(sigil::weave::Selector{}, withAxis(base, "wght", weight))
            .key("t")));
    host.frame();
    return grab(host, 400, 120);
  };
  const SkBitmap light = at(400.0f);
  const SkBitmap heavy = at(900.0f);
  const std::string log = ::testing::internal::GetCapturedStderr();
  int inked = 0;
  for (int y = 0; y < 120; y += 2)
    for (int x = 0; x < 400; x += 2)
      inked += light.getColor(x, y) != SK_ColorBLACK;
  ASSERT_GT(inked, 20) << "the text never drew";
  EXPECT_GT(pixelsDiffering(light, heavy, 400, 120), 20)
      << "an advance-variant axis did not re-shape — the glyphs kept the "
         "outline and pen positions the lighter weight gave them";
  EXPECT_EQ(log.find("moves advances"), std::string::npos)
      << "a restyle that re-shapes warned as if it had been refused: " << log;
}

TEST(TextSpanAxis, TheCoordinateTakesTheSizeScaledLadder) {
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  ASSERT_TRUE(face) << "the instrument's GRAD axis must hold advances "
                       "while moving ink";

  // A FIXED WINDOW of design space, swept at a fixed number of samples, at
  // two rendered sizes. The ladder is cut per rendered size, so the same
  // window holds more rungs on the larger type — and a rung is a retained
  // varied clone, which is countable where a pixel difference of one rung
  // is not. Both sizes sit inside the ladder's proportional band, away from
  // its floor and its ceiling, so this reads the proportion and not a clamp.
  constexpr float kSmallPx = 24.0f, kLargePx = 112.0f;
  constexpr int kSamples = 61;
  const float window = (hi - lo) / 32.0f;
  const auto clonesAcrossTheWindow = [&](float pixelSize) {
    sigil::weave::FontContext local(sigil::weave::ports::systemFontManager());
    sigil::motion::Ticker ticker;
    Composer composer(ticker, local);
    composer.setSize({200, 200});
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 200));
    sigil::weave::TextStyle style = coloredStyle(pixelSize, SK_ColorWHITE);
    style.shaping.typeface = face;
    for (int i = 0; i < kSamples; ++i) {
      const float value =
          lo + (hi - lo) * 0.5f + window * (float)i / (float)(kSamples - 1);
      composer.render(box().padding(4).child(
          // A default-constructed selector addresses every glyph.
          text(u8"888", style)
              .key("t")
              .spanStyle(sigil::weave::Selector{},
                         withAxis(style, "GRAD", value))));
      ticker.tick(1.0 / 60.0);
      surface->getCanvas()->clear(SK_ColorBLACK);
      composer.draw(*surface->getCanvas());
    }
    return local.variedTypefaceCount();
  };

  const size_t coarse = clonesAcrossTheWindow(kSmallPx);
  const size_t fine = clonesAcrossTheWindow(kLargePx);
  EXPECT_GT(coarse, 0u) << "the coordinate never reached a face at all";
  EXPECT_LT(coarse, (size_t)kSamples)
      << "every sample minted its own clone — the value is reaching the memo "
         "unsnapped";
  EXPECT_LT(coarse, fine)
      << "the same window of design space resolved to no more rungs at "
      << kLargePx << " px than at " << kSmallPx
      << " px, so the ladder is not cut by the rendered size";
}

TEST(TextSpanAxis, ALaterDeclarationWinsOnOverlap) {
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  ASSERT_TRUE(face) << "the instrument's GRAD axis must hold advances "
                       "while moving ink";

  Host host(400, 120);
  sigil::weave::TextStyle base = coloredStyle(46, SK_ColorWHITE);
  base.shaping.typeface = face;
  const auto drawn = [&](const std::function<Element(Element)>& dress) {
    host.composer.render(
        box().padding(10).child(dress(text(u8"GRADE", base).key("t"))));
    host.frame();
    return grab(host, 400, 120);
  };
  const SkBitmap light = drawn([&](Element t) {
    return t.spanStyle(sigil::weave::Selector{}, withAxis(base, "GRAD", lo));
  });
  const SkBitmap heavy = drawn([&](Element t) {
    return t.spanStyle(sigil::weave::Selector{}, withAxis(base, "GRAD", hi));
  });
  ASSERT_GT(pixelsDiffering(light, heavy, 400, 120), 20)
      << "the two ends of the axis draw the same, so nothing below is a test";

  const SkBitmap both = drawn([&](Element t) {
    return t.spanStyle(sigil::weave::Selector{}, withAxis(base, "GRAD", lo))
        .spanStyle(sigil::weave::Selector{}, withAxis(base, "GRAD", hi));
  });
  EXPECT_EQ(pixelsDiffering(both, heavy, 400, 120), 0)
      << "the earlier declaration survived the later one — an axis is a "
         "substitution, and overlapping substitutions are last-one-wins";
}

TEST(TextOptionSetters, MaxLinesAndEllipsisClampTheText) {
  Host host(240, 200);
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  const std::u8string body =
      u8"one two three four five six seven eight nine ten eleven twelve";
  host.composer.render(box().padding(10).child(
      text(body, base).width(180).maxLines(2).ellipsis(u8"...").key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  EXPECT_EQ(layout->lineCount, 2);
  EXPECT_TRUE(layout->overflowed());
  EXPECT_TRUE(layout->ellipsized) << "the marker never landed";
}

TEST(TextOptionSetters, HyphenationRendersTheHyphenAtASoftBreak) {
  Host host(260, 220);
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  // A short word, then a long one carrying a discretionary break, in a
  // measure that holds the short word and the long one's first half with
  // its hyphen (at 20 px in the instrument face: 60 + 6 + 108 + 8 = 182 px)
  // and not the two words whole (246 px). Without hyphenation the long word
  // goes down whole and the first line is one run; with it the first line
  // takes the half and the drawn hyphen as well.
  const std::u8string body = u8"short extraordi\u00adnarily";
  const auto runsOnFirstLineWith = [&](bool enabled) {
    host.composer.render(
        box().padding(4).child(text(body, base)
                                   .width(200)
                                   .hyphenation({.enabled = enabled})
                                   .key("t")));
    host.frame();
    const auto* layout = host.composer.paragraphLayout("t");
    if (!layout) return 0;
    int runs = 0;
    for (const sigil::weave::PositionedRun& run : layout->runs)
      if (run.lineIndex == 0) ++runs;
    return runs;
  };
  const int without = runsOnFirstLineWith(false);
  const int with = runsOnFirstLineWith(true);
  EXPECT_GT(with, without) << "the hyphenation setting never reached layout";
}

TEST(TextOptionSetters, SettersOverrideAPassedOptionsValueFieldByField) {
  // The contract for the full-control overload: a setter names one field and
  // leaves every other field of the passed options standing.
  Host host(300, 220);
  auto para = std::make_shared<sigil::weave::Paragraph>();
  para->appendText(u8"one two three four five six seven eight",
                   coloredStyle(20, SK_ColorWHITE));
  sigil::weave::ParagraphLayoutOptions passed;
  passed.alignment = sigil::weave::TextAlignment::kCenter;
  passed.overflow.maxLines = 5;
  // At 20 px in the instrument face the first line is "one two three four"
  // at 198 px, so a centred line starts 11 px in.
  host.composer.render(
      box().child(text(para, passed).width(220).maxLines(2).key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  EXPECT_EQ(layout->lineCount, 2) << "the setter did not override maxLines";
  ASSERT_FALSE(layout->runs.empty());
  EXPECT_GT(layout->runs.front().origin.fX, 1.0f)
      << "the passed alignment was clobbered rather than left alone";
}

TEST(TextOptionSetters, KnuthPlassBreaksARaggedParagraphDifferently) {
  Host host(320, 260);
  const sigil::weave::TextStyle base = coloredStyle(18, SK_ColorWHITE);
  // Deliberately uneven word lengths: the greedy breaker fills each line as
  // far as it goes, and the optimal one trades an early line short to keep
  // the whole paragraph even.
  const std::u8string body =
      u8"a longer word then tiny bits of text and an extraordinarily "
      u8"lengthy one to finish the measure";
  const auto lineStartsUnder = [&](sigil::weave::LineBreakStrategy strategy) {
    host.composer.render(box().padding(4).child(
        text(body, base).width(240).lineBreak(strategy).key("t")));
    host.frame();
    std::vector<uint32_t> starts;
    const auto* layout = host.composer.paragraphLayout("t");
    if (!layout) return starts;
    int previous = -1;
    for (const sigil::weave::PositionedRun& run : layout->runs)
      if (run.lineIndex != previous) {
        starts.push_back(run.wordIndex);
        previous = run.lineIndex;
      }
    return starts;
  };
  const std::vector<uint32_t> greedy =
      lineStartsUnder(sigil::weave::LineBreakStrategy::kGreedy);
  const std::vector<uint32_t> optimal =
      lineStartsUnder(sigil::weave::LineBreakStrategy::kKnuthPlass);
  ASSERT_GT(greedy.size(), 1u);
  EXPECT_NE(greedy, optimal) << "the break strategy setter changed nothing";
}
