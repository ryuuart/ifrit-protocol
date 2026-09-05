/** @file
 * The single-line paragraph cache: what its key discriminates, how nearby
 * sizes fall into one entry, and the two promises the node-based storage
 * behind its pimpl exists to make — a returned reference stays valid while
 * other entries are inserted, and stops being valid when the cache empties
 * itself to stay inside its bound.
 */

#include <gtest/gtest.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/cache/SingleLineParagraphCache.h>

#include <string>
#include <vector>

#include "support/Faces.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// The colour a paragraph's first span carries, which is how a case tells
/// an entry it has already touched from one built fresh.
SkColor firstSpanColor(const Paragraph& paragraph) {
  return paragraph.spans().empty()
             ? SK_ColorTRANSPARENT
             : paragraph.spans().front().style.paint.foreground.getColor();
}

/// Marks `paragraph` so a later lookup can say whether it is the same
/// entry or a replacement built from scratch.
void mark(Paragraph& paragraph) {
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     PaintStyle{SK_ColorRED});
}

}  // namespace

TEST(SingleLineParagraphCache, AnEntryIsBuiltOnceAndHandedBackAfterwards) {
  SingleLineParagraphCache cache;
  Paragraph& first = cache.paragraphFor(u8"label", nullptr, 16.0f);
  EXPECT_EQ(first.text(), u"label");
  mark(first);
  Paragraph& again = cache.paragraphFor(u8"label", nullptr, 16.0f);
  EXPECT_EQ(&again, &first) << "a second lookup must not build a second entry";
  EXPECT_EQ(firstSpanColor(again), SK_ColorRED);
}

TEST(SingleLineParagraphCache, TheSameTextReachesOneEntryThroughEitherView) {
  SingleLineParagraphCache cache;
  Paragraph& fromUtf8 = cache.paragraphFor(u8"label", nullptr, 16.0f);
  Paragraph& fromUtf16 = cache.paragraphFor(u"label", nullptr, 16.0f);
  EXPECT_EQ(&fromUtf16, &fromUtf8)
      << "the two entry points address one cache slot";
}

TEST(SingleLineParagraphCache, AReferenceStaysValidAcrossLaterInsertions) {
  // This is what the node-based storage behind the pimpl is for: a caller
  // holds the paragraph it asked for across a frame in which other labels
  // are asked for, and the rehash those insertions cause must not move it.
  SingleLineParagraphCache cache(/*maximumEntries=*/4096);
  Paragraph& held = cache.paragraphFor(u8"held", nullptr, 16.0f);
  mark(held);
  const Paragraph* address = &held;

  for (int index = 0; index < 512; ++index)
    cache.paragraphFor(u8"filler" + std::u8string(1, (char8_t)('a' + index % 26)) +
                           std::u8string(1, (char8_t)('a' + index / 26)),
                       nullptr, 16.0f);

  EXPECT_EQ(held.text(), u"held") << "the held reference no longer names it";
  EXPECT_EQ(firstSpanColor(held), SK_ColorRED);
  EXPECT_EQ(&cache.paragraphFor(u8"held", nullptr, 16.0f), address)
      << "an insertion moved an entry a caller was holding";
}

TEST(SingleLineParagraphCache, SizesInsideOneQuantisationStepShareAnEntry) {
  // Sizes are quantised into sixteenths of a point, so two sizes a
  // renderer could not tell apart are one entry and a size across the step
  // boundary is its own. Both sizes here are exact in binary, so the
  // boundary is where the quantisation puts it and not where rounding does.
  SingleLineParagraphCache cache;
  Paragraph& base = cache.paragraphFor(u8"caption", nullptr, 16.0f);
  mark(base);

  Paragraph& nearby = cache.paragraphFor(u8"caption", nullptr, 16.03125f);
  EXPECT_EQ(&nearby, &base) << "half a step apart is the same entry";
  EXPECT_EQ(firstSpanColor(nearby), SK_ColorRED);

  Paragraph& across = cache.paragraphFor(u8"caption", nullptr, 16.0625f);
  EXPECT_NE(&across, &base) << "a whole step apart is a different entry";
  EXPECT_NE(firstSpanColor(across), SK_ColorRED)
      << "the entry across the boundary was built fresh";
}

TEST(SingleLineParagraphCache, TheKeyDiscriminatesTextAndTypeface) {
  SingleLineParagraphCache cache;
  Paragraph& plain = cache.paragraphFor(u8"caption", nullptr, 16.0f);
  EXPECT_NE(&cache.paragraphFor(u8"CAPTION", nullptr, 16.0f), &plain)
      << "different text is a different entry";

  ASSERT_TRUE(instrumentFace()) << "the committed instrument face did not load";
  Paragraph& faced = cache.paragraphFor(u8"caption", instrumentFace(), 16.0f);
  EXPECT_NE(&faced, &plain) << "the typeface is part of the key";
  EXPECT_EQ(&cache.paragraphFor(u8"caption", instrumentFace(), 16.0f), &faced);
}

TEST(SingleLineParagraphCache, AFullCacheRetiresWhatItHeldBeforeInsertingMore) {
  // The bound is kept by emptying, which the header states outright: the
  // references handed out before it are gone, and the next lookup of an
  // earlier text builds a new entry rather than returning the old one.
  SingleLineParagraphCache cache(/*maximumEntries=*/4);
  Paragraph& first = cache.paragraphFor(u8"one", nullptr, 16.0f);
  mark(first);
  for (const char8_t* text : {u8"two", u8"three", u8"four"})
    cache.paragraphFor(text, nullptr, 16.0f);

  // The fifth distinct key is the one that cannot fit.
  cache.paragraphFor(u8"five", nullptr, 16.0f);
  EXPECT_NE(firstSpanColor(cache.paragraphFor(u8"one", nullptr, 16.0f)),
            SK_ColorRED)
      << "an entry the cache said it retired was handed back";
}

TEST(SingleLineParagraphCache, ClearRetiresEveryEntry) {
  SingleLineParagraphCache cache;
  mark(cache.paragraphFor(u8"label", nullptr, 16.0f));
  cache.clear();
  EXPECT_NE(firstSpanColor(cache.paragraphFor(u8"label", nullptr, 16.0f)),
            SK_ColorRED);
}
