// An ink that restarts on each unit of its passage: the paint's unit
// square lands on every glyph, cluster, word, line or sentence's own
// text-metric box, where the default lays it once across the passage.
// Each case sets a two-stop ramp and reads back where its two colours
// landed on the letters.

#include <include/utils/SkNoDrawCanvas.h>
#include <src/text/GlyphRun.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "support/TextTestSupport.h"

namespace {

using sigil::weave::Unit;

/** Red on the left of the unit square, blue on the right. */
material::skia::Paint across() {
  return material::skia::Paint::linearUnit(
      {0, 0}, {1, 0}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}});
}

/** Red at the top of the unit square, blue at the bottom. */
material::skia::Paint down() {
  return material::skia::Paint::linearUnit(
      {0, 0}, {0, 1}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}});
}

bool inked(SkColor c) {
  return SkColorGetR(c) + SkColorGetG(c) + SkColorGetB(c) > 90;
}

/** The brightest pixel of one column (or row) of a blob, which is the
 *  inside of a stroke rather than its antialiased rim. */
SkColor strongest(Host& host, int x0, int y0, int x1, int y1) {
  SkColor best = SK_ColorBLACK;
  int bestSum = -1;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      const SkColor c = host.pixel(x, y);
      const int sum = SkColorGetR(c) + SkColorGetB(c);
      if (sum > bestSum) {
        bestSum = sum;
        best = c;
      }
    }
  return best;
}

bool reddish(SkColor c) { return SkColorGetR(c) > 170 && SkColorGetB(c) < 85; }
bool bluish(SkColor c) { return SkColorGetB(c) > 170 && SkColorGetR(c) < 85; }

/** The runs of inked columns between rows `top` and `bottom`, left to
 *  right, each as its first column and one past its last. */
std::vector<std::pair<int, int>> inkedColumns(Host& host, int width, int top,
                                              int bottom) {
  std::vector<bool> column(width, false);
  for (int x = 0; x < width; ++x)
    for (int y = top; y < bottom && !column[x]; ++y)
      column[x] = inked(host.pixel(x, y));
  std::vector<std::pair<int, int>> runs;
  for (int x = 0; x < width;) {
    if (!column[x]) {
      ++x;
      continue;
    }
    int end = x;
    while (end < width && column[end]) ++end;
    runs.emplace_back(x, end);
    x = end;
  }
  return runs;
}

/** The runs of inked rows, top to bottom, each as its first row and one
 *  past its last. */
std::vector<std::pair<int, int>> inkedRows(Host& host, int width, int height) {
  std::vector<bool> row(height, false);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width && !row[y]; ++x) row[y] = inked(host.pixel(x, y));
  std::vector<std::pair<int, int>> runs;
  for (int y = 0; y < height;) {
    if (!row[y]) {
      ++y;
      continue;
    }
    int end = y;
    while (end < height && row[end]) ++end;
    runs.emplace_back(y, end);
    y = end;
  }
  return runs;
}

/** The colour at each end of every letter, left to right: a letter is a run
 *  of inked columns, which an H and a full stop both are. */
struct Letter {
  SkColor left, right;
};
std::vector<Letter> lettersAcross(Host& host, int width, int height) {
  std::vector<Letter> letters;
  for (const auto& [start, end] : inkedColumns(host, width, 0, height)) {
    // Two columns in from each edge, inside the stem.
    const int left = std::min(start + 2, end - 1);
    const int right = std::max(end - 3, start);
    letters.push_back({strongest(host, left, 0, left + 1, height),
                       strongest(host, right, 0, right + 1, height)});
  }
  return letters;
}

/** The colour at the top and the bottom of every line, top to bottom. */
struct LineEnds {
  SkColor top, bottom;
};
std::vector<LineEnds> linesDown(Host& host, int width, int height) {
  std::vector<LineEnds> lines;
  for (const auto& [start, end] : inkedRows(host, width, height)) {
    const int top = std::min(start + 2, end - 1);
    const int bottom = std::max(end - 3, start);
    lines.push_back({strongest(host, 0, top, width, top + 1),
                     strongest(host, 0, bottom, width, bottom + 1)});
  }
  return lines;
}

/** Whether two colours are the same to within antialiasing. */
bool alike(SkColor a, SkColor b) {
  const auto near = [](unsigned x, unsigned y) {
    return (x > y ? x - y : y - x) < 48;
  };
  return near(SkColorGetR(a), SkColorGetR(b)) &&
         near(SkColorGetG(a), SkColorGetG(b)) &&
         near(SkColorGetB(a), SkColorGetB(b));
}

/** How many glyph draws reach the canvas, pictures played through it. */
class GlyphDrawCounter final : public SkNoDrawCanvas {
 public:
  using SkNoDrawCanvas::SkNoDrawCanvas;
  int draws = 0;

 protected:
  void onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                      const SkPaint& paint) override {
    SkCanvas::onDrawTextBlob(blob, x, y, paint);
  }
  void onDrawPicture(const SkPicture* picture, const SkMatrix* matrix,
                     const SkPaint* paint) override {
    SkCanvas::onDrawPicture(picture, matrix, paint);
  }
  void onDrawGlyphRunList(const sktext::GlyphRunList&,
                          const SkPaint&) override {
    ++draws;
  }
};

}  // namespace

TEST(ComposeInkUnits, APassageTakesTheRampOnceAcrossItsLetters) {
  // The default: one unit square over the passage's text-metric box, so
  // the first letter starts red, the last ends blue, and where the two
  // letters meet the ramp is half way.
  Host host(320, 160);
  host.composer.render(
      box().padding(20).children({text(u8"HH", whiteStyle(96)).ink(across())}));
  host.frame();
  const std::vector<Letter> letters = lettersAcross(host, 320, 160);
  ASSERT_EQ(letters.size(), 2u);
  EXPECT_TRUE(reddish(letters[0].left));
  EXPECT_TRUE(bluish(letters[1].right));
  EXPECT_FALSE(bluish(letters[0].right));
  EXPECT_FALSE(reddish(letters[1].left));
}

TEST(ComposeInkUnits, AGlyphUnitRestartsTheRampOnEachLetter) {
  Host host(320, 160);
  host.composer.render(box().padding(20).children(
      {text(u8"HH", whiteStyle(96))
           .ink(across(), PaintAnchor::OwnBox, Unit::Glyph)}));
  host.frame();
  const std::vector<Letter> letters = lettersAcross(host, 320, 160);
  ASSERT_EQ(letters.size(), 2u);
  for (const Letter& letter : letters) {
    EXPECT_TRUE(reddish(letter.left));
    EXPECT_TRUE(bluish(letter.right));
  }
}

TEST(ComposeInkUnits, AClusterUnitPaintsAMarkWithItsBase) {
  // A base and its combining mark are two glyphs and one cluster, in a
  // face that keeps the acute a glyph of its own with no advance. Under
  // Cluster the acute stands in its base letter's ramp, so at any one
  // column it is the colour the H is there; under Glyph its own box has
  // no width and leaves it at an end of the ramp. The draws count the
  // units: a cluster is one draw, a glyph another.
  const auto render = [](Host& host, Unit unit) {
    host.composer.render(box().padding(20).children(
        {text(u8"H́H́", markStyle(96))
             .ink(across(), PaintAnchor::OwnBox, unit)}));
    host.frame();
  };
  // Each acute beside the base under it, at the acute's middle column.
  const auto markAndBase = [](Host& host) {
    std::vector<std::pair<SkColor, SkColor>> pairs;
    const std::vector<std::pair<int, int>> rows = inkedRows(host, 360, 220);
    if (rows.size() != 2) return pairs;  // the acutes, then the letters
    for (const auto& [start, end] :
         inkedColumns(host, 360, rows[0].first, rows[0].second)) {
      const int middle = (start + end) / 2;
      pairs.emplace_back(
          strongest(host, middle, rows[0].first, middle + 1, rows[0].second),
          strongest(host, middle, rows[1].first, middle + 1, rows[1].second));
    }
    return pairs;
  };
  Host clusters(360, 220);
  render(clusters, Unit::Cluster);
  const auto perCluster = markAndBase(clusters);
  ASSERT_EQ(perCluster.size(), 2u);
  for (const auto& [mark, base] : perCluster) EXPECT_TRUE(alike(mark, base));

  Host glyphs(360, 220);
  render(glyphs, Unit::Glyph);
  const auto perGlyph = markAndBase(glyphs);
  ASSERT_EQ(perGlyph.size(), 2u);
  for (const auto& [mark, base] : perGlyph) EXPECT_FALSE(alike(mark, base));

  const auto draws = [&](Unit unit) {
    Host host(360, 220);
    render(host, unit);
    GlyphDrawCounter canvas(360, 220);
    host.composer.draw(canvas);
    return canvas.draws;
  };
  EXPECT_EQ(draws(Unit::Cluster), 2);
  EXPECT_EQ(draws(Unit::Glyph), 4);
}

TEST(ComposeInkUnits, AWordUnitRestartsTheRampOnEachWord) {
  // Two words of two letters: each word starts red and ends blue, and the
  // letters in the middle of a word are half way along its ramp.
  Host host(480, 120);
  host.composer.render(box().padding(10).children(
      {text(u8"HH HH", whiteStyle(64))
           .ink(across(), PaintAnchor::OwnBox, Unit::Word)}));
  host.frame();
  const std::vector<Letter> letters = lettersAcross(host, 480, 120);
  ASSERT_EQ(letters.size(), 4u);
  EXPECT_TRUE(reddish(letters[0].left));
  EXPECT_TRUE(bluish(letters[1].right));
  EXPECT_TRUE(reddish(letters[2].left));
  EXPECT_TRUE(bluish(letters[3].right));
  EXPECT_FALSE(bluish(letters[0].right));
  EXPECT_FALSE(reddish(letters[1].left));
}

TEST(ComposeInkUnits, ALineUnitRestartsTheRampOnEachLine) {
  // A top-to-bottom ramp over two lines: each line runs red to blue from
  // its cap top to its baseline, where the passage's ramp would reach its
  // second line already half blue.
  const auto lines = [](std::optional<Unit> unit) {
    Host host(240, 260);
    host.composer.render(box().padding(10).children(
        {text(u8"HH HH", whiteStyle(64))
             .width(150)
             .ink(down(), PaintAnchor::OwnBox, unit)}));
    host.frame();
    return linesDown(host, 240, 260);
  };
  const std::vector<LineEnds> perLine = lines(Unit::Line);
  ASSERT_EQ(perLine.size(), 2u);
  for (const LineEnds& line : perLine) {
    EXPECT_TRUE(reddish(line.top));
    EXPECT_TRUE(bluish(line.bottom));
  }
  const std::vector<LineEnds> passage = lines(std::nullopt);
  ASSERT_EQ(passage.size(), 2u);
  EXPECT_TRUE(reddish(passage[0].top));
  EXPECT_FALSE(reddish(passage[1].top));
}

TEST(ComposeInkUnits, ASentenceUnitRestartsTheRampOnEachSentence) {
  // Two sentences of two words: the ramp restarts at the second sentence
  // and not at the second word of the first.
  Host host(640, 100);
  host.composer.render(box().padding(10).children(
      {text(u8"HH HH. HH HH.", whiteStyle(40))
           .ink(across(), PaintAnchor::OwnBox, Unit::Sentence)}));
  host.frame();
  const std::vector<Letter> letters = lettersAcross(host, 640, 100);
  // H H H H . H H H H .
  ASSERT_EQ(letters.size(), 10u);
  EXPECT_TRUE(reddish(letters[0].left));
  EXPECT_FALSE(reddish(letters[2].left));
  EXPECT_TRUE(reddish(letters[5].left));
  EXPECT_TRUE(bluish(letters[9].right));
}

TEST(ComposeInkUnits, ARuleStatesAUnitAnchor) {
  Host host(320, 160);
  host.composer.render(
      box()
          .padding(20)
          .applyStyleSheet(StyleSheet{
              rule(".chrome").ink(across(), PaintAnchor::OwnBox, Unit::Glyph)})
          .children({text(u8"HH", whiteStyle(96)).styleClass("chrome")}));
  host.frame();
  const std::vector<Letter> letters = lettersAcross(host, 320, 160);
  ASSERT_EQ(letters.size(), 2u);
  for (const Letter& letter : letters) {
    EXPECT_TRUE(reddish(letter.left));
    EXPECT_TRUE(bluish(letter.right));
  }
}

TEST(ComposeInkUnits, ASpanRestartsItsInkOnEachUnitOfTheRangeItFinds) {
  // The first word keeps the passage's white; the second takes the ramp
  // afresh on each of its letters.
  Host host(480, 120);
  host.composer.render(box().padding(10).children(
      {text(u8"HH HH", whiteStyle(64))
           .span(sigil::weave::selectors::word(1),
                 SpanDeclarations().ink(across(), PaintAnchor::OwnBox,
                                        Unit::Glyph))}));
  host.frame();
  const std::vector<Letter> letters = lettersAcross(host, 480, 120);
  ASSERT_EQ(letters.size(), 4u);
  EXPECT_EQ(letters[0].left, SK_ColorWHITE);
  EXPECT_EQ(letters[1].right, SK_ColorWHITE);
  for (size_t index = 2; index < 4; ++index) {
    EXPECT_TRUE(reddish(letters[index].left));
    EXPECT_TRUE(bluish(letters[index].right));
  }
}

TEST(ComposeInkUnits, APassageWithNoUnitDrawsItsGlyphsInOneDraw) {
  // The cost claim: a passage whose ink names no unit takes the one
  // batched draw it always took, and a unit costs one draw per unit.
  const auto draws = [](std::optional<Unit> unit) {
    Host host(480, 120);
    host.composer.render(box().padding(10).children(
        {text(u8"HH HH", whiteStyle(64))
             .ink(across(), PaintAnchor::OwnBox, unit)}));
    GlyphDrawCounter canvas(480, 120);
    host.composer.draw(canvas);
    return canvas.draws;
  };
  EXPECT_EQ(draws(std::nullopt), 1);
  EXPECT_EQ(draws(Unit::Word), 2);
  EXPECT_EQ(draws(Unit::Glyph), 4);
}

TEST(ComposeInkUnits, ALetterUnderATextFxTrackDrawsWithItsUnitsPaint) {
  // A track hands the letters to the dressed painter, which lays each
  // unit's paint where the unit rests: every letter lowered 30 px still
  // runs red to blue across itself, which a horizontal ramp does not see
  // the lowering for.
  const auto render = [](Host& host, bool lowered) {
    GlyphModifier lower;
    lower.dy = lowered ? 30.0f : 0.0f;
    Text leaf = text(u8"HH", whiteStyle(96))
                    .ink(across(), PaintAnchor::OwnBox, Unit::Glyph);
    if (lowered)
      leaf.textFx({.effect = textFx::effect(
                       "lowered",
                       [lower](const GlyphInfo&, float,
                               sigil::core::noise::Mix64Stream&) {
                         return lower;
                       },
                       /*reach=*/60.0f)});
    host.composer.render(box().padding(20).children({std::move(leaf)}));
    host.frame();
  };
  Host resting(320, 220);
  render(resting, false);
  Host lowered(320, 220);
  render(lowered, true);
  const auto restRows = inkedRows(resting, 320, 220);
  const auto lowRows = inkedRows(lowered, 320, 220);
  ASSERT_EQ(restRows.size(), 1u);
  ASSERT_EQ(lowRows.size(), 1u);
  EXPECT_NEAR(lowRows[0].first - restRows[0].first, 30, 2)
      << "the track never moved the letters";
  const std::vector<Letter> letters = lettersAcross(lowered, 320, 220);
  ASSERT_EQ(letters.size(), 2u);
  for (const Letter& letter : letters) {
    EXPECT_TRUE(reddish(letter.left));
    EXPECT_TRUE(bluish(letter.right));
  }
}

TEST(ComposeInkUnits, ALetterOnAPathTakesTheBoxItStandsInThere) {
  // On a path baseline the unit's box is where the path set the letter,
  // not where the paragraph would have: two letters centred on a line
  // across the lower half of the leaf each restart the ramp.
  const Shape baseline = [](SkSize size) {
    return SkPath::Line({0, size.height() * 0.8f},
                        {size.width(), size.height() * 0.8f});
  };
  Host host(360, 200);
  host.composer.render(box().children(
      {text(u8"HH", whiteStyle(80))
           .width(360)
           .height(200)
           .ink(across(), PaintAnchor::OwnBox, Unit::Glyph)
           .textOnPath({.path = baseline,
                        .at = 0.5f,
                        .align = TextPath::Align::Center})}));
  host.frame();
  const auto rows = inkedRows(host, 360, 200);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_GT(rows[0].first, 60) << "the letters never took the path";
  const std::vector<Letter> letters = lettersAcross(host, 360, 200);
  ASSERT_EQ(letters.size(), 2u);
  for (const Letter& letter : letters) {
    EXPECT_TRUE(reddish(letter.left));
    EXPECT_TRUE(bluish(letter.right));
  }
}

TEST(ComposeInkUnits, AnUprightLetterInAColumnRestartsDownItsAdvance) {
  // An upright glyph in a vertical column has no cap band across it: its
  // box is its advance down the column. A top-to-bottom ramp restarts at
  // each letter's top under Glyph, where the passage's ramp laid down the
  // column block reaches the second letter already past red.
  const auto letters = [](std::optional<Unit> unit) {
    sigil::weave::TextStyle upright = whiteStyle(64);
    upright.shaping.verticalForm = sigil::weave::VerticalForm::kUpright;
    Host host(200, 260);
    host.composer.render(box().padding(20).children(
        {text(u8"HH", upright)
             .paragraph({.writingMode = sigil::weave::WritingMode::kVerticalRL})
             .ink(down(), PaintAnchor::OwnBox, unit)}));
    host.frame();
    return linesDown(host, 200, 260);
  };
  const std::vector<LineEnds> perGlyph = letters(Unit::Glyph);
  ASSERT_EQ(perGlyph.size(), 2u);
  for (const LineEnds& letter : perGlyph) {
    EXPECT_TRUE(reddish(letter.top));
    EXPECT_GT(SkColorGetB(letter.bottom), SkColorGetB(letter.top) + 80);
  }
  const std::vector<LineEnds> passage = letters(std::nullopt);
  ASSERT_EQ(passage.size(), 2u);
  EXPECT_TRUE(reddish(passage[0].top));
  EXPECT_FALSE(reddish(passage[1].top));
}

TEST(ComposeInkUnits, AUnitKeepsEveryOutlineUnderEveryFill) {
  // textStroke's outline is an underlay, and a unit keeps it: under a
  // unit the passes draw band by band across the passage — every outline,
  // then every fill — so an outline wide enough to reach the neighbouring
  // letter's stem still lies under that stem, as it does with no unit.
  Host bare(360, 180);
  bare.composer.render(
      box().padding(40).children({text(u8"HH", whiteStyle(96))}));
  bare.frame();
  const auto plain = inkedColumns(bare, 360, 0, 180);
  ASSERT_EQ(plain.size(), 2u);
  const int gap = plain[1].first - plain[0].second;
  // Wide enough that the second letter's outline covers the first
  // letter's right stem.
  const float width = 2.0f * (float)(gap + 8);
  const auto render = [&](Host& host, std::optional<Unit> unit) {
    host.composer.render(box().padding(40).children(
        {text(u8"HH", whiteStyle(96))
             .textStroke(width, Fill::color({0, 1, 0, 1}))
             .ink(across(), PaintAnchor::OwnBox, unit)}));
    host.frame();
  };
  for (const std::optional<Unit> unit :
       {std::optional<Unit>{}, std::optional<Unit>{Unit::Glyph}}) {
    Host host(360, 180);
    render(host, unit);
    int outline = 0;
    for (int y = 0; y < 180; ++y)
      for (int x = 0; x < 360; ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetG(c) > 170 && SkColorGetR(c) < 60 &&
            SkColorGetB(c) < 60)
          ++outline;
      }
    EXPECT_GT(outline, 200) << "the outline was lost";
    const int stem = plain[0].second - 3;
    const SkColor fill = strongest(host, stem, 0, stem + 1, 180);
    EXPECT_LT((int)SkColorGetG(fill), 60)
        << "a neighbour's outline landed on the first letter's fill";
    EXPECT_GT((int)SkColorGetR(fill) + (int)SkColorGetB(fill), 170);
  }
}

TEST(ComposeInkUnits, AUnitUnderAnotherAnchorIsDroppedAndSaysSo) {
  // A unit restarts the paint on the text's own units, which only the own
  // box reads; under an anchor that spreads one field across the tree it
  // is dropped, once with a warning.
  ::testing::internal::CaptureStderr();
  Rule kept = rule(".kept").ink(across(), PaintAnchor::OwnBox, Unit::Glyph);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a unit under the own box must not warn";
  EXPECT_EQ(kept.inkUnit(), Unit::Glyph);
  ::testing::internal::CaptureStderr();
  Rule dropped =
      rule(".dropped").ink(across(), PaintAnchor::CanvasBox, Unit::Glyph);
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("CanvasBox"), std::string::npos) << log;
  EXPECT_NE(log.find("OwnBox"), std::string::npos) << log;
  EXPECT_FALSE(dropped.inkUnit().has_value());
  EXPECT_EQ(dropped.inkAnchor(), PaintAnchor::CanvasBox);
}

TEST(ComposeInkUnits, SelectionIsNoUnitAndSaysSo) {
  // Selection names the extent a selector found, not a unit a passage is
  // cut into: the unit is dropped with a warning, once, and the ramp is
  // laid across the passage.
  ::testing::internal::CaptureStderr();
  Host host(320, 160);
  host.composer.render(box().padding(20).children(
      {text(u8"HH", whiteStyle(96))
           .ink(across(), PaintAnchor::OwnBox, Unit::Selection)}));
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("Selection"), std::string::npos) << log;
  host.frame();
  const std::vector<Letter> letters = lettersAcross(host, 320, 160);
  ASSERT_EQ(letters.size(), 2u);
  EXPECT_TRUE(reddish(letters[0].left));
  EXPECT_FALSE(bluish(letters[0].right));
  EXPECT_TRUE(bluish(letters[1].right));
}
