// kit/Typeset.h — a nested style over the opening of a block: how far
// into the copy it reaches, where it ends when a delimiter rather than a
// count decides, how it rides an initial letter, and the room a hanging
// list's marker keeps.

#include <sigilcompose/kit/Typeset.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "support/ParagraphTestSupport.h"

TEST(ComposeTypeset, ANestedStyleCoversTheWordsItCountsAndStops) {
  // The mechanism is a selector and a span restyle, so the run stops where
  // the TEXT says rather than where a pixel count says: three words in,
  // whatever those words are and wherever they break.
  Host host(400, 300);
  const kit::NestedStyle opening{.until = kit::NestedStyle::Until::Words,
                                 .count = 3,
                                 .style = colouredType(16, SK_ColorGREEN)};
  host.composer.render(
      box().children({text(u8"alpha beta gamma delta epsilon", whiteStyle(16))
                          .key("t")
                          .absolute()
                          .left(20.0f)
                          .top(40.0f)
                          .width(360.0f)
                          .span(kit::nestedRun(opening),
                                SpanDeclarations().font(opening.style))}));
  host.frame();
  const std::vector<TextUnit> words = host.composer.units(
      "t", sigil::weave::selectors::each(sigil::weave::Unit::Word),
      sigil::weave::Unit::Word);
  ASSERT_EQ(words.size(), 5u);
  for (size_t index = 0; index < words.size(); ++index) {
    const SkRect& word = words[index].rect;
    const SkIRect box =
        SkIRect::MakeLTRB((int)word.left(), (int)word.top(),
                          (int)word.right() + 1, (int)word.bottom() + 1);
    EXPECT_EQ(anyGreenIn(host, box), index < 3u)
        << "word " << index << " is on the wrong side of the nested run";
  }
}

TEST(ComposeTypeset, ANestedRunEndsOnItsDelimiterAndIncludesIt) {
  // The spelling that counts nothing: a lead-in that ends at a mark. The
  // mark is quoted into the pattern rather than pasted into it, so a
  // delimiter that is also a regular-expression operator means itself.
  Host host(400, 300);
  const kit::NestedStyle lead{.until = kit::NestedStyle::Until::Delimiter,
                              .delimiter = u8".",
                              .style = colouredType(16, SK_ColorGREEN)};
  host.composer.render(box().children(
      {text(u8"alpha beta. gamma delta", whiteStyle(16))
           .key("t")
           .absolute()
           .left(20.0f)
           .top(40.0f)
           .width(360.0f)
           .span(kit::nestedRun(lead), SpanDeclarations().font(lead.style))}));
  host.frame();
  const std::vector<TextUnit> words = host.composer.units(
      "t", sigil::weave::selectors::each(sigil::weave::Unit::Word),
      sigil::weave::Unit::Word);
  ASSERT_GE(words.size(), 4u);
  const auto greenAt = [&](size_t index) {
    const SkRect& word = words[index].rect;
    return anyGreenIn(
        host, SkIRect::MakeLTRB((int)word.left(), (int)word.top(),
                                (int)word.right() + 1, (int)word.bottom() + 1));
  };
  EXPECT_TRUE(greenAt(0));
  EXPECT_TRUE(greenAt(1))
      << "the word carrying the delimiter is inside the run";
  EXPECT_FALSE(greenAt(2)) << "the run ends AT the mark, not after it";
  // A delimiter that never occurs covers nothing, rather than everything.
  Host missing(400, 300);
  const kit::NestedStyle absent{.until = kit::NestedStyle::Until::Delimiter,
                                .delimiter = u8"§",
                                .style = colouredType(16, SK_ColorGREEN)};
  missing.composer.render(
      box().children({text(u8"alpha beta. gamma delta", whiteStyle(16))
                          .key("t")
                          .absolute()
                          .left(20.0f)
                          .top(40.0f)
                          .width(360.0f)
                          .span(kit::nestedRun(absent),
                                SpanDeclarations().font(absent.style))}));
  missing.frame();
  EXPECT_FALSE(anyGreenIn(missing, SkIRect::MakeXYWH(0, 0, 400, 300)));
}

TEST(ComposeTypeset, AnInitialLetterCarriesANestedOpeningIntoItsBlock) {
  // The case the two pieces exist for: an initial the layout sizes and
  // seats, and the words after it set in a style of their own. Both are
  // properties of ONE text leaf — the initial is not a second element — so
  // the nested run is stated over the same block the initial opens.
  Host host(400, 300);
  const kit::NestedStyle opening{.until = kit::NestedStyle::Until::Words,
                                 .count = 2,
                                 .style = colouredType(16, SK_ColorGREEN)};
  host.composer.render(box().children(
      {box()
           .absolute()
           .left(20.0f)
           .top(40.0f)
           .width(340.0f)
           .height(200.0f)
           .children({text(u8"Whale alpha beta gamma", whiteStyle(16))
                          .key("body")
                          .width(240.0f)
                          .initialLetter({.lines = 3, .margin = 6.0f})
                          .span(kit::nestedRun(opening),
                                SpanDeclarations().font(opening.style))})}));
  host.frame();
  const std::vector<TextUnit> words = host.composer.units(
      "body", sigil::weave::selectors::each(sigil::weave::Unit::Word),
      sigil::weave::Unit::Word);
  ASSERT_GE(words.size(), 4u);
  const auto greenAt = [&](size_t index) {
    const SkRect& word = words[index].rect;
    return anyGreenIn(
        host, SkIRect::MakeLTRB((int)word.left(), (int)word.top(),
                                (int)word.right() + 1, (int)word.bottom() + 1));
  };
  EXPECT_TRUE(greenAt(0));
  EXPECT_TRUE(greenAt(1));
  EXPECT_FALSE(greenAt(2)) << "the nested run counted two words, not three";
}

TEST(ComposeTypeset, ANestedOpeningDoesNotCloseTheSpaceAfterTheInitial) {
  // The initial takes the first grapheme of the opening word and the rest
  // of that word sets beside it; the space after the word is the word's
  // own and has to reach the band the following words are set in. A
  // nested run over the opening restyles those words and re-breaks
  // nothing, so the gap is the one the same passage sets with no initial
  // at all — counted in words or ended at a delimiter alike.
  const std::u8string opening = u8"When the first light, the hall was still.";
  const auto gapAfterTheOpeningWord =
      [&](bool initial, std::optional<kit::NestedStyle> nested) {
        Host host(400, 300);
        auto leaf = text(opening, whiteStyle(14));
        leaf.key("body").absolute().left(20.0f).top(40.0f).width(260.0f);
        if (initial) leaf.initialLetter({.lines = 3, .margin = 8.0f});
        if (nested)
          leaf.span(kit::nestedRun(*nested),
                    SpanDeclarations().font(nested->style));
        host.composer.render(box().children({leaf}));
        host.frame();
        // The opening word is the CAP and the remainder together, so its
        // right edge is the rightmost of the pieces it was placed as.
        float rightOfOpening = 0.0f;
        for (const TextUnit& piece :
             host.composer.units("body", sigil::weave::selectors::words(0, 1),
                                 sigil::weave::Unit::Word))
          rightOfOpening = std::max(rightOfOpening, piece.rect.right());
        const std::vector<TextUnit> second =
            host.composer.units("body", sigil::weave::selectors::words(1, 2),
                                sigil::weave::Unit::Word);
        EXPECT_FALSE(second.empty()) << "the second word was never placed";
        return second.empty() ? 0.0f
                              : second.front().rect.left() - rightOfOpening;
      };

  const float ordinary = gapAfterTheOpeningWord(false, std::nullopt);
  EXPECT_GT(ordinary, 1.0f)
      << "an ordinary paragraph of this passage sets no gap between its "
         "first two words, so nothing below can prove anything";
  EXPECT_NEAR(gapAfterTheOpeningWord(true, std::nullopt), ordinary, 0.5f);
  EXPECT_NEAR(
      gapAfterTheOpeningWord(
          true, kit::NestedStyle{.until = kit::NestedStyle::Until::Words,
                                 .count = 5,
                                 .style = colouredType(14, SK_ColorGREEN)}),
      ordinary, 0.5f);
  EXPECT_NEAR(
      gapAfterTheOpeningWord(
          true, kit::NestedStyle{.until = kit::NestedStyle::Until::Delimiter,
                                 .delimiter = u8",",
                                 .style = colouredType(14, SK_ColorGREEN)}),
      ordinary, 0.5f);
}

TEST(KitBullets, TheMarkerKeepsTheRoomTheIndentOpened) {
  // The marker is placed beside the item's text, at the block's own start,
  // and the item is indented by the hang on EVERY line. A first line
  // pulled back out of the indent would start where the marker already
  // stands and print through it.
  //
  // The marker is left empty here so the only ink on the sheet is the
  // item's own: its leftmost column IS where the first line begins.
  constexpr float kHang = 24.0f;
  const std::array<std::u8string, 1> items = {
      u8"First line long enough that this item wraps, and a second that "
      u8"carries on under it."};
  const std::array<std::u8string, 1> markers = {std::u8string()};
  Host host(300, 160);
  host.composer.render(box().padding(0).children({kit::bullets(
      items, markers, colouredType(13, SK_ColorWHITE), kHang, 240.0f)}));
  host.frame();
  int leftmost = 300;
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < leftmost; ++x)
      if (host.pixel(x, y) != SK_ColorBLACK) {
        leftmost = x;
        break;
      }
  ASSERT_LT(leftmost, 300) << "the list drew nothing";
  EXPECT_GE((float)leftmost, kHang - 1.0f)
      << "the first line was pulled back onto the marker's room";
}
