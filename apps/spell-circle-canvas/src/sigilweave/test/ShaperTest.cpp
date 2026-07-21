/** @file
 * Word shaping, the shape cache, itemization (script/word
 * segmentation, fallback resolution, bidi), and complex-script
 * correctness (Arabic, Devanagari, SMP, emoji clusters).
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <include/core/SkFontMgr.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <absl/container/flat_hash_set.h>

#include <algorithm>
#include <set>
#include <string>
using namespace sigil::weave;
using namespace sigil::weave::test;

// ── Shaping & caching ─────────────────────────────────────────────────────

TEST(Shaper, ShapesLatinWord) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"Hello");
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  const Word &word = paragraph.words()[0];
  ASSERT_EQ(word.segments.size(), 1u);
  EXPECT_EQ(word.segments[0].shaped->glyphs.size(), 5u);
  EXPECT_GT(word.width, 0.0f);
  EXPECT_EQ(word.spaceWidth, 0.0f);
}

TEST(Shaper, CacheHitsOnIdenticalWords) {
  FontContext &fontContext = sharedContext();
  fontContext.purgeShapeCache();
  fontContext.resetStats();
  Paragraph paragraph = makeParagraph(u8"tick tock tick tock tick");
  paragraph.ensureShaped(fontContext);
  // 3 distinct shape inputs: "tick", "tock", " " (glue). Repeats hit cache.
  EXPECT_EQ(fontContext.stats().shapeCalls, 3u);
  EXPECT_GT(fontContext.stats().shapeCacheHits, 0u);
}

TEST(Shaper, EditReshapesOnlyTheEditedWord) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"the quick brown fox jumps over the lazy dog again and again");
  paragraph.ensureShaped(fontContext);

  fontContext.resetStats();
  // "quick" → "swift": positions 4..9.
  paragraph.replaceText(4, 9, u8"swift");
  paragraph.ensureShaped(fontContext);
  // Only the new word content misses the cache ("swift"); everything else
  // (words and glue) must hit.
  EXPECT_LE(fontContext.stats().shapeCalls, 1u);
  EXPECT_GT(fontContext.stats().shapeCacheHits, 10u);
}

TEST(Shaper, PaintOnlyRestyleNeverReshapes) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph =
      makeParagraph(u8"colorful words change their paint only");
  paragraph.ensureShaped(fontContext);

  fontContext.resetStats();
  paragraph.setPaint(9, 14, PaintStyle{SK_ColorRED});
  paragraph.ensureShaped(fontContext);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u);
}

TEST(Shaper, FontSizeRestyleReshapesOnlyCoveredWords) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"alpha beta gamma delta epsilon");
  paragraph.ensureShaped(fontContext);

  fontContext.resetStats();
  TextStyle big = basicStyle(24.0f);
  paragraph.setStyle(6, 10, big); // "beta"
  paragraph.ensureShaped(fontContext);
  // "beta"@24 is new; its neighbors keep their cached 16px shapes. The glue
  // after "beta" also re-shapes at the new size (2 calls max).
  EXPECT_LE(fontContext.stats().shapeCalls, 2u);
}

TEST(Shaper, ClustersAreMonotone) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"office"); // 'ffi' may ligate
  paragraph.ensureShaped(fontContext);
  const auto &clusters = paragraph.words()[0].segments[0].shaped->clusters;
  EXPECT_TRUE(std::is_sorted(clusters.begin(), clusters.end()));
}

TEST(Shaper, WordBlobIsSharedAcrossLayouts) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"stable");
  paragraph.ensureShaped(fontContext);
  const ShapedWordRef &shaped = paragraph.words()[0].segments[0].shaped;
  const SkTextBlob *first = wordBlob(*shaped).get();
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(wordBlob(*shaped).get(), first);
}
// ── Itemization ───────────────────────────────────────────────────────────

TEST(Itemization, MixedLatinCjkSplitsIntoWords) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"Skia は速い and 빠르다 也很快");
  paragraph.ensureShaped(fontContext);
  ASSERT_GT(paragraph.words().size(), 4u);

  bool sawIdeographic = false, sawLatin = false;
  for (const Word &word : paragraph.words()) {
    if (word.ideographic)
      sawIdeographic = true;
    else if (word.width > 0)
      sawLatin = true;
  }
  EXPECT_TRUE(sawIdeographic);
  EXPECT_TRUE(sawLatin);
}

TEST(Itemization, CjkGetsPerCharacterBreakOpportunities) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"日本語のテキスト");
  paragraph.ensureShaped(fontContext);
  // ICU line breaking splits ideographic text nearly per character; the
  // exact count depends on kinsoku rules, but it must be far more than one.
  EXPECT_GE(paragraph.words().size(), 4u);
  for (const Word &word : paragraph.words())
    EXPECT_TRUE(word.ideographic);
}

TEST(Itemization, FallbackResolvesCjkGlyphs) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"abc漢字xyz");
  paragraph.ensureShaped(fontContext);
  for (const Word &word : paragraph.words())
    for (const WordSegment &seg : word.segments) {
      ASSERT_TRUE(seg.shaped->typeface);
      for (uint16_t glyph : seg.shaped->glyphs)
        EXPECT_NE(glyph, 0) << "missing glyph (.notdef) leaked into layout";
    }
}

TEST(Itemization, CustomFallbackResolverControlsSelection) {
  sk_sp<SkFontMgr> fontManager = ports::systemFontManager();
  sk_sp<SkTypeface> primary =
      fontManager->matchFamilyStyle("Noto Sans", SkFontStyle());
  sk_sp<SkTypeface> preferred =
      fontManager->matchFamilyStyle("Noto Serif JP", SkFontStyle());
  constexpr SkUnichar kJapaneseHiragana = 0x3042; // あ
  if (!primary || !preferred || primary->unicharToGlyph(kJapaneseHiragana) ||
      !preferred->unicharToGlyph(kJapaneseHiragana))
    GTEST_SKIP() << "Noto Sans / Noto Serif JP fallback fixture unavailable";

  int resolverCalls = 0;
  std::string observedLanguage;
  FontContext fontContext(std::move(fontManager), nullptr,
                          [&](SkFontMgr &, const SkTypeface &,
                              int32_t codePoint, std::string_view languageTag) {
                            resolverCalls++;
                            observedLanguage = languageTag;
                            return codePoint == kJapaneseHiragana ? preferred
                                                                  : nullptr;
                          });

  TextStyle textStyle;
  textStyle.shaping.typeface = primary;
  textStyle.shaping.languageTag = "ja";
  Paragraph paragraph;
  paragraph.appendText(u8"あ", textStyle);
  paragraph.ensureShaped(fontContext);

  ASSERT_FALSE(paragraph.words().empty());
  const sk_sp<SkTypeface> &resolved =
      paragraph.words().front().segments.front().shaped->typeface;
  ASSERT_TRUE(resolved);
  EXPECT_EQ(resolved->uniqueID(), preferred->uniqueID());
  EXPECT_EQ(observedLanguage, "ja");
  EXPECT_EQ(resolverCalls, 1);
}

TEST(Itemization, FallbackCacheIncludesLanguage) {
  sk_sp<SkFontMgr> fontManager = ports::systemFontManager();
  sk_sp<SkTypeface> primary =
      fontManager->matchFamilyStyle("Noto Sans", SkFontStyle());
  sk_sp<SkTypeface> simplified =
      fontManager->matchFamilyStyle("Noto Sans SC", SkFontStyle());
  sk_sp<SkTypeface> traditional =
      fontManager->matchFamilyStyle("Noto Sans TC", SkFontStyle());
  constexpr SkUnichar kSharedHanCharacter = 0x4E2D; // 中
  if (!primary || !simplified || !traditional ||
      primary->unicharToGlyph(kSharedHanCharacter) ||
      !simplified->unicharToGlyph(kSharedHanCharacter) ||
      !traditional->unicharToGlyph(kSharedHanCharacter) ||
      simplified->uniqueID() == traditional->uniqueID())
    GTEST_SKIP() << "regional Noto CJK fallback fixtures unavailable";

  int resolverCalls = 0;
  FontContext fontContext(std::move(fontManager), nullptr,
                          [&](SkFontMgr &, const SkTypeface &, int32_t,
                              std::string_view languageTag) {
                            resolverCalls++;
                            return languageTag == "zh-Hant" ? traditional
                                                            : simplified;
                          });

  sk_sp<SkTypeface> hans =
      fontContext.resolveTypeface(primary, kSharedHanCharacter, "zh-Hans");
  sk_sp<SkTypeface> hant =
      fontContext.resolveTypeface(primary, kSharedHanCharacter, "zh-Hant");
  ASSERT_TRUE(hans);
  ASSERT_TRUE(hant);
  EXPECT_EQ(hans->uniqueID(), simplified->uniqueID());
  EXPECT_EQ(hant->uniqueID(), traditional->uniqueID());
  EXPECT_EQ(resolverCalls, 2);

  // Both exact language keys are now warm.
  sk_sp<SkTypeface> warmHans =
      fontContext.resolveTypeface(primary, kSharedHanCharacter, "zh-Hans");
  sk_sp<SkTypeface> warmHant =
      fontContext.resolveTypeface(primary, kSharedHanCharacter, "zh-Hant");
  EXPECT_EQ(warmHans->uniqueID(), simplified->uniqueID());
  EXPECT_EQ(warmHant->uniqueID(), traditional->uniqueID());
  EXPECT_EQ(resolverCalls, 2);
}

TEST(Itemization, HardBreakIsMandatory) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"first line\nsecond");
  paragraph.ensureShaped(fontContext);
  bool sawMandatory = false;
  for (const Word &word : paragraph.words())
    sawMandatory |= word.mandatoryBreakAfter;
  EXPECT_TRUE(sawMandatory);
}

TEST(Itemization, RtlWordShapesRtl) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"שלום");
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  const auto &clusters = paragraph.words()[0].segments[0].shaped->clusters;
  ASSERT_GE(clusters.size(), 2u);
  // RTL output is in visual order: cluster values run backwards.
  EXPECT_GT(clusters.front(), clusters.back());
}
TEST(Scripts, ArabicLamAlefLigates) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"لا"); // lam + alef: mandatory ligature
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  const ShapedWord &shapedWord = *paragraph.words()[0].segments[0].shaped;
  if (!allGlyphsResolved(paragraph))
    GTEST_SKIP() << "no Arabic font on this system";
  EXPECT_EQ(shapedWord.glyphs.size(), 1u)
      << "lam-alef must fuse into one glyph";
}

TEST(Scripts, ArabicJoinsRtl) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"العربية تكتب من اليمين إلى اليسار");
  paragraph.ensureShaped(fontContext);
  if (!allGlyphsResolved(paragraph))
    GTEST_SKIP() << "no Arabic font on this system";
  ASSERT_GE(paragraph.words().size(), 5u);
  for (const Word &word : paragraph.words()) {
    EXPECT_EQ(word.bidiLevel & 1, 1) << "Arabic words must be RTL";
    const auto &clusters = word.segments[0].shaped->clusters;
    if (clusters.size() >= 2) // RTL visual order: clusters run backwards
      EXPECT_GT(clusters.front(), clusters.back());
  }
}

TEST(Scripts, DevanagariFormsConjunctClusters) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"नमस्ते दुनिया");
  paragraph.ensureShaped(fontContext);
  if (!allGlyphsResolved(paragraph))
    GTEST_SKIP() << "no Devanagari font on this system";
  // "नमस्ते" is 6 UTF-16 units but the virama fuses स्+ते into one grapheme
  // cluster: distinct clusters must be fewer than code units.
  const Word &namaste = paragraph.words()[0];
  ASSERT_EQ(namaste.segments.size(), 1u);
  const ShapedWord &shapedWord = *namaste.segments[0].shaped;
  EXPECT_LT(uniqueClusterCount(shapedWord), 6u);
  EXPECT_GE(shapedWord.glyphs.size(), 3u);
}

TEST(Scripts, CuneiformSupplementaryPlane) {
  FontContext &fontContext = sharedContext();
  // Four codepoints beyond the BMP (U+12000, U+12031, U+12038, U+1204D):
  // each is a surrogate pair, so correct cluster values step by 2 UTF-16
  // units. U+12031 is also featured by the hyper-scripts demo.
  Paragraph paragraph = makeParagraph(u8"𒀀𒀱𒀸𒁍");
  paragraph.ensureShaped(fontContext);
  if (!allGlyphsResolved(paragraph))
    GTEST_SKIP() << "no Cuneiform font on this system";
  std::vector<uint32_t> clusters;
  for (const Word &word : paragraph.words())
    for (const WordSegment &segment : word.segments)
      for (uint32_t cluster : segment.shaped->clusters)
        clusters.push_back(cluster + word.textBegin);
  ASSERT_FALSE(clusters.empty());
  for (uint32_t cluster : clusters)
    EXPECT_EQ(cluster % 2, 0u) << "clusters must land on surrogate-pair starts";
}

TEST(Scripts, EmojiZwjFamilyIsOneCluster) {
  FontContext &fontContext = sharedContext();
  // Family emoji: 4 people joined by ZWJ = 11 UTF-16 units, ONE grapheme.
  Paragraph paragraph = makeParagraph(u8"👨‍👩‍👧‍👦");
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 1u);
  ASSERT_EQ(paragraph.words()[0].segments.size(), 1u);
  const ShapedWord &shapedWord = *paragraph.words()[0].segments[0].shaped;
  ASSERT_FALSE(shapedWord.glyphs.empty());
  EXPECT_EQ(uniqueClusterCount(shapedWord), 1u)
      << "a ZWJ family sequence is a single grapheme cluster";
  EXPECT_TRUE(allGlyphsResolved(paragraph));
}

TEST(Scripts, EmojiModifierAndFlagClusters) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"👍🏽 🇺🇸"); // skin tone; regional pair
  paragraph.ensureShaped(fontContext);
  ASSERT_EQ(paragraph.words().size(), 2u);
  for (const Word &word : paragraph.words()) {
    const ShapedWord &shapedWord = *word.segments[0].shaped;
    EXPECT_EQ(uniqueClusterCount(shapedWord), 1u)
        << "modifier/flag sequences are single grapheme clusters";
  }
  EXPECT_TRUE(allGlyphsResolved(paragraph));
}

TEST(Scripts, EmojiInsideLatinFallsBackPerSegment) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"great👍work");
  paragraph.ensureShaped(fontContext);
  absl::flat_hash_set<const SkTypeface *> faces;
  for (const Word &word : paragraph.words())
    for (const WordSegment &segment : word.segments)
      faces.insert(segment.shaped->typeface.get());
  EXPECT_GE(faces.size(), 2u) << "emoji must resolve to its own typeface";
  EXPECT_TRUE(allGlyphsResolved(paragraph));
}

// ── Cache purge (FontContext::purgeAllCaches) ────────────────────────────

TEST(Shaper, PurgeAllCachesRefillsIdentically) {
  // A dedicated context: purging the shared one would slow sibling tests.
  FontContext fontContext(ports::systemFontManager());
  const char8_t *text = u8"warm caches shape 漢字 and ascii alike";

  auto shapeFresh = [&] {
    Paragraph paragraph;
    paragraph.appendText(text, basicStyle());
    paragraph.ensureShaped(fontContext);
    std::vector<float> widths;
    for (const Word &word : paragraph.words())
      widths.push_back(word.width);
    return widths;
  };

  const std::vector<float> before = shapeFresh();
  fontContext.resetStats();
  fontContext.purgeAllCaches();

  // Everything must re-shape from cold (the purge really emptied the shape
  // cache and HarfBuzz records) and land on identical metrics.
  const std::vector<float> after = shapeFresh();
  EXPECT_GT(fontContext.stats().shapeCalls, 0u);
  EXPECT_EQ(fontContext.stats().shapeCacheHits, 0u);
  ASSERT_EQ(after.size(), before.size());
  for (size_t index = 0; index < before.size(); ++index)
    EXPECT_FLOAT_EQ(after[index], before[index]);
}

TEST(Shaper, PurgeAllCachesResetsBorrowedMemos) {
  FontContext fontContext(ports::systemFontManager());
  // Warm the ASCII fast path (per-primary direct-mapped table + last-table
  // memo) and the language-id intern memo.
  Paragraph warm;
  warm.appendText(u8"ascii text", basicStyle());
  warm.ensureShaped(fontContext);
  (void)fontContext.resolveTypeface(nullptr, U'a', "zh-Hans");

  fontContext.purgeAllCaches();

  // The memoized AsciiTable pointer and interned language id both borrowed
  // from maps the purge cleared; resolving again must rebuild them instead
  // of dereferencing stale entries.
  sk_sp<SkTypeface> ascii = fontContext.resolveTypeface(nullptr, U'a', nullptr);
  ASSERT_TRUE(ascii);
  sk_sp<SkTypeface> han =
      fontContext.resolveTypeface(nullptr, 0x4E2D, "zh-Hans");
  ASSERT_TRUE(han);
  EXPECT_NE(han->unicharToGlyph(0x4E2D), 0);

  // Purging twice in a row must also be safe.
  fontContext.purgeAllCaches();
  EXPECT_TRUE(fontContext.resolveTypeface(nullptr, U'b', nullptr));
}

// ── Variable-font axis ergonomics (ShapingStyle::variations) ─────────────

TEST(Shaper, VariationsChangeShapingViaStyle) {
  FontContext &fontContext = sharedContext();
  sk_sp<SkTypeface> base = fontContext.fontManager()->matchFamilyStyle(
      "Noto Sans", SkFontStyle::Normal());
  if (!base || base->getVariationDesignPosition({}) < 1)
    GTEST_SKIP() << "no variable Noto Sans installed";

  auto totalWidth = [&](std::vector<FontVariation> variations) {
    TextStyle style = basicStyle(32.0f);
    style.shaping.typeface = base;
    style.shaping.variations = std::move(variations);
    Paragraph paragraph;
    paragraph.appendText(u8"hamburgefonstiv", style);
    paragraph.ensureShaped(fontContext);
    float width = 0;
    for (const Word &word : paragraph.words())
      width += word.width;
    return width;
  };

  const float regular = totalWidth({});
  const float heavy = totalWidth({{"wght", 900.0f}});
  EXPECT_GT(std::abs(heavy - regular), regular * 0.01f)
      << "wght must reach shaping through ShapingStyle::variations";
  // An unknown axis is inert (clone clamps or ignores it), never an error.
  const float bogus = totalWidth({{"zzzz", 42.0f}});
  EXPECT_GT(bogus, 0.0f);
}

TEST(Shaper, VariedTypefaceIsMemoizedForCacheStability) {
  FontContext fontContext(ports::systemFontManager());
  sk_sp<SkTypeface> base = fontContext.fontManager()->matchFamilyStyle(
      "Noto Sans", SkFontStyle::Normal());
  if (!base || base->getVariationDesignPosition({}) < 1)
    GTEST_SKIP() << "no variable Noto Sans installed";

  const std::vector<FontVariation> axes = {{"wght", 700.0f}};
  sk_sp<SkTypeface> first = fontContext.variedTypeface(base, axes);
  sk_sp<SkTypeface> second = fontContext.variedTypeface(base, axes);
  ASSERT_TRUE(first);
  EXPECT_EQ(first.get(), second.get())
      << "identical variations must return the same clone instance";
  EXPECT_NE(first.get(), base.get());

  // Empty variations pass the base straight through.
  EXPECT_EQ(fontContext.variedTypeface(base, {}).get(), base.get());

  // The memo survives a purge as a *contract*, not as storage: after
  // purgeAllCaches the next request re-resolves (possibly to the same
  // object — Skia's own typeface cache may satisfy the clone) and repeats
  // are stable again.
  fontContext.purgeAllCaches();
  sk_sp<SkTypeface> afterPurge = fontContext.variedTypeface(base, axes);
  ASSERT_TRUE(afterPurge);
  EXPECT_EQ(afterPurge.get(), fontContext.variedTypeface(base, axes).get());

  // Two paragraphs styled with the same variations share shape-cache
  // entries (the memoized clone's uniqueID keys them identically).
  auto shapeOnce = [&] {
    TextStyle style;
    style.shaping.typeface = base;
    style.shaping.variations = axes;
    Paragraph paragraph;
    paragraph.appendText(u8"stable", style);
    paragraph.ensureShaped(fontContext);
  };
  shapeOnce();
  fontContext.resetStats();
  shapeOnce();
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u)
      << "repeated variations must hit the shape cache";
}

TEST(ShaperVariations, TextStyleFluentSugarStaysOrderStable) {
  // weight()/opticalSize()/variation() replace in place when the axis is
  // already present — repeated fluent chains keep one order (one memoized
  // varied-typeface identity), never accumulate duplicates.
  TextStyle style;
  style.weight(500).opticalSize(36).weight(700);
  ASSERT_EQ(style.shaping.variations.size(), 2u);
  EXPECT_EQ(style.shaping.variations[0], FontVariation("wght", 700));
  EXPECT_EQ(style.shaping.variations[1], FontVariation("opsz", 36));
  style.variation("GRAD", 80);
  ASSERT_EQ(style.shaping.variations.size(), 3u);
  EXPECT_EQ(style.shaping.variations[2], FontVariation("GRAD", 80));

  // A chain that ends at the same design position compares EQUAL — the
  // in-place replace keeps first-mention order, so both styles carry
  // [wght, opsz, GRAD] and share one shape-cache identity.
  TextStyle same;
  same.weight(700).opticalSize(36).variation("GRAD", 80);
  EXPECT_TRUE(style == same);
}
