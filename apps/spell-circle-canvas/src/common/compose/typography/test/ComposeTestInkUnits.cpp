// An ink that restarts on each unit of its passage: the paint's unit
// square lands on every glyph, cluster, word, line or sentence's own
// text-metric box, where the default lays it once across the passage.
// Each case sets a two-stop ramp and reads back where its two colours
// landed on the letters.

#include <include/utils/SkNoDrawCanvas.h>
#include <src/text/GlyphRun.h>

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

/** The colour at each end of every letter, left to right: a letter is a run
 *  of inked columns, which an H and a full stop both are. */
struct Letter {
  SkColor left, right;
};
std::vector<Letter> lettersAcross(Host& host, int width, int height) {
  std::vector<bool> column(width, false);
  for (int x = 0; x < width; ++x)
    for (int y = 0; y < height && !column[x]; ++y)
      column[x] = inked(host.pixel(x, y));
  std::vector<Letter> letters;
  for (int x = 0; x < width;) {
    if (!column[x]) {
      ++x;
      continue;
    }
    int end = x;
    while (end < width && column[end]) ++end;
    // Two columns in from each edge, inside the stem.
    const int left = std::min(x + 2, end - 1);
    const int right = std::max(end - 3, x);
    letters.push_back({strongest(host, left, 0, left + 1, height),
                       strongest(host, right, 0, right + 1, height)});
    x = end;
  }
  return letters;
}

/** The colour at the top and the bottom of every line, top to bottom. */
struct LineEnds {
  SkColor top, bottom;
};
std::vector<LineEnds> linesDown(Host& host, int width, int height) {
  std::vector<bool> row(height, false);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width && !row[y]; ++x) row[y] = inked(host.pixel(x, y));
  std::vector<LineEnds> lines;
  for (int y = 0; y < height;) {
    if (!row[y]) {
      ++y;
      continue;
    }
    int end = y;
    while (end < height && row[end]) ++end;
    const int top = std::min(y + 2, end - 1);
    const int bottom = std::max(end - 3, y);
    lines.push_back({strongest(host, 0, top, width, top + 1),
                     strongest(host, 0, bottom, width, bottom + 1)});
    y = end;
  }
  return lines;
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

TEST(ComposeInkUnits, AClusterUnitRestartsTheRampOnEachCharacter) {
  Host host(320, 160);
  host.composer.render(box().padding(20).children(
      {text(u8"HH", whiteStyle(96))
           .ink(across(), PaintAnchor::OwnBox, Unit::Cluster)}));
  host.frame();
  const std::vector<Letter> letters = lettersAcross(host, 320, 160);
  ASSERT_EQ(letters.size(), 2u);
  for (const Letter& letter : letters) {
    EXPECT_TRUE(reddish(letter.left));
    EXPECT_TRUE(bluish(letter.right));
  }
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
