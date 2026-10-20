/** @file
 * A face a font file holds, adopted by a font context under its own
 * family: asked for by name as an installed family is, its nearest face
 * found for a style, and replaced when its file is read again.
 */

#include <gtest/gtest.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/Resource.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "support/Faces.h"

using namespace sigil::weave;

namespace {

/** THE SAME FONT AT ANOTHER WEIGHT: @p bytes with the weight class its
 *  OS/2 table states rewritten to @p weight, which is a second face of the
 *  one family, as a family's bold is. */
std::vector<std::byte> atWeight(std::vector<std::byte> bytes, uint16_t weight) {
  const auto read16 = [&bytes](size_t at) {
    return static_cast<uint16_t>((std::to_integer<int>(bytes[at]) << 8) |
                                 std::to_integer<int>(bytes[at + 1]));
  };
  const auto read32 = [&read16](size_t at) {
    return (uint32_t{read16(at)} << 16) | read16(at + 2);
  };
  const uint16_t tables = read16(4);
  for (uint16_t index = 0; index < tables; ++index) {
    const size_t record = 12 + size_t{index} * 16;
    if (std::to_integer<char>(bytes[record]) != 'O' ||
        std::to_integer<char>(bytes[record + 1]) != 'S' ||
        std::to_integer<char>(bytes[record + 2]) != '/' ||
        std::to_integer<char>(bytes[record + 3]) != '2')
      continue;
    const size_t weightClass = read32(record + 8) + 4;
    bytes[weightClass] = std::byte(weight >> 8);
    bytes[weightClass + 1] = std::byte(weight & 0xFF);
  }
  // The style's names, in both encodings the naming table holds, so the
  // new face is a face of its own to the platform and not the cached one.
  const auto rename = [&bytes](const std::string& from, const std::string& to) {
    for (size_t at = 0; at + from.size() <= bytes.size(); ++at)
      if (std::equal(from.begin(), from.end(), bytes.begin() + at,
                     [](char letter, std::byte held) {
                       return std::byte(letter) == held;
                     }))
        for (size_t index = 0; index < to.size(); ++index)
          bytes[at + index] = std::byte(to[index]);
  };
  const auto wide = [](const std::string& narrow) {
    std::string doubled;
    for (char character : narrow) doubled += {'\0', character};
    return doubled;
  };
  rename("Regular", "Boldest");
  rename(wide("Regular"), wide("Boldest"));
  return bytes;
}

std::string upper(std::string text) {
  std::ranges::transform(text, text.begin(), [](unsigned char character) {
    return static_cast<char>(std::toupper(character));
  });
  return text;
}

}  // namespace

TEST(AdoptedFace, AFontFilesFaceAnswersUnderItsFamily) {
  FontContext context(ports::systemFontManager());
  const Face face = ports::faceFromBytes(test::instrumentBytes("Sans.ttf"));
  ASSERT_TRUE(face);
  const std::string family = face.familyName();
  ASSERT_FALSE(family.empty());
  // The instrument family is installed nowhere, so until the face is
  // adopted the family names nothing.
  EXPECT_FALSE(context.familyTypeface(family));
  context.adopt(face, "res://Sans.ttf");
  EXPECT_EQ(context.familyTypeface(family), face);
  // A family name matches as CSS's does, case aside, and a family of one
  // face answers it for every style.
  EXPECT_EQ(context.familyTypeface(upper(family)), face);
  EXPECT_EQ(context.familyTypeface(
                family, FaceStyle{.weight = 900, .slant = FaceSlant::Italic}),
            face);
  ASSERT_EQ(context.adoptedFaces().size(), 1u);
  EXPECT_EQ(context.adoptedFaces().front().origin, "res://Sans.ttf");
  // Adopted twice, a face is still one adoption; no face is none.
  context.adopt(face, "res://Sans.ttf");
  context.adopt(face);
  context.adopt(Face{});
  EXPECT_EQ(context.adoptedFaces().size(), 1u);
}

TEST(AdoptedFace, AStyleFindsTheFamilysNearestAdoptedFace) {
  FontContext context(ports::systemFontManager());
  const std::vector<std::byte> regularBytes = test::instrumentBytes("Sans.ttf");
  const Face regular = ports::faceFromBytes(regularBytes);
  const Face bold = ports::faceFromBytes(atWeight(regularBytes, 700));
  ASSERT_TRUE(regular && bold);
  ASSERT_EQ(regular.familyName(), bold.familyName());
  ASSERT_GT(bold.style().weight, regular.style().weight)
      << "the rewritten weight class is not the face's weight";
  context.adopt(regular, "res://Sans-Regular.ttf");
  context.adopt(bold, "res://Sans-Bold.ttf");
  const std::string family = regular.familyName();
  EXPECT_EQ(context.familyTypeface(family, {.weight = 400}), regular);
  EXPECT_EQ(context.familyTypeface(family, {.weight = 700}), bold);
  // CSS's matching: above the middle weights the heavier face is nearer,
  // below them the lighter, and an italic the family lacks is its upright.
  EXPECT_EQ(context.familyTypeface(family, {.weight = 600}), bold);
  EXPECT_EQ(context.familyTypeface(family, {.weight = 300}), regular);
  EXPECT_EQ(context.familyTypeface(family,
                                   {.weight = 400, .slant = FaceSlant::Italic}),
            regular);
}

TEST(AdoptedFace, AFaceFromTheSameOriginReplacesTheFamilyItHeld) {
  FontContext context(ports::systemFontManager());
  const Face first = ports::faceFromBytes(test::instrumentBytes("Sans.ttf"));
  const Face second =
      ports::faceFromBytes(test::instrumentBytes("Variable.ttf"));
  ASSERT_TRUE(first && second);
  ASSERT_NE(first.familyName(), second.familyName());
  context.adopt(first, "sketch://type/data/Face.ttf");
  // Asked once, so the answer is held; the replacement must still reach it.
  ASSERT_EQ(context.familyTypeface(first.familyName()), first);
  context.adopt(second, "sketch://type/data/Face.ttf");
  EXPECT_FALSE(context.familyTypeface(first.familyName()));
  EXPECT_EQ(context.familyTypeface(second.familyName()), second);
  ASSERT_EQ(context.adoptedFaces().size(), 1u);
  EXPECT_EQ(context.adoptedFaces().front().face, second);
}

TEST(AdoptedFace, PurgingTheCachesKeepsEveryAdoption) {
  FontContext context(ports::systemFontManager());
  const Face face = ports::faceFromBytes(test::instrumentBytes("Sans.ttf"));
  ASSERT_TRUE(face);
  context.adopt(face);
  context.purgeAllCaches();
  EXPECT_EQ(context.familyTypeface(face.familyName()), face);
}
