/** @file
 * A story as a value: content plus the block styles it is set under, and
 * nothing else — no layout, no cursor, no frame.
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "sigilweave/layout/Story.h"

using namespace sigil::weave;

namespace {

TextStyle body() {
  TextStyle style;
  style.shaping.fontSize = 18;
  return style;
}

}  // namespace

TEST(Story, HoldsContentAndItsBlockStyles) {
  ParagraphStyle heading;
  heading.spaceAfter = 12;
  ParagraphStyle para;
  para.spaceBefore = 6;

  Story article(rich(body()).add(u8"A heading").add(u8"\nand its body"));
  article.paragraphs({heading, para});
  EXPECT_FALSE(article.empty());
  ASSERT_EQ(article.blocks().size(), 2u);
  EXPECT_EQ(article.blocks()[0].spaceAfter, 12);
  EXPECT_EQ(article.content().runs().size(), 2u);
  EXPECT_TRUE(Story().empty());
}

TEST(Story, OneStyledStringIsOneRun) {
  const Story plain(std::u8string(u8"a single passage"), body());
  ASSERT_EQ(plain.content().runs().size(), 1u);
  EXPECT_TRUE(plain.content().runs()[0].utf8 ==
              std::u8string(u8"a single passage"));
}

TEST(Story, EqualityIsTheContentAndTheBlocks) {
  const RichText content = rich(body()).add(u8"same words");
  EXPECT_TRUE(Story(content) == Story(content));
  EXPECT_FALSE(Story(content) == Story(rich(body()).add(u8"other words")));

  ParagraphStyle wide;
  wide.spaceBefore = 24;
  Story spaced(content);
  spaced.paragraphs({wide});
  EXPECT_FALSE(Story(content) == spaced)
      << "how the blocks are set is part of what the story is";
}
