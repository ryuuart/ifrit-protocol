/** @file
 * The Unicode leaf: transcoding, character properties, scripts, case
 * mapping, segmentation and bidirectional levels, each checked as the
 * plain value it returns, with no fonts involved.
 */

#include <gtest/gtest.h>
#include <sigilweave/unicode/Unicode.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

using namespace sigil::weave::unicode;

// ── Transcoding ────────────────────────────────────────────────────────

TEST(Transcoding, Utf8ToUtf16RoundTripsAstralAndBmp) {
  const std::u8string utf8 = u8"café \U0001F600 中";
  const std::u16string utf16 = toUtf16(utf8);
  EXPECT_EQ(utf16, u"café \U0001F600 中");
  EXPECT_EQ(utf16.size(), 9u) << "the emoji is one surrogate pair";
  EXPECT_TRUE(toUtf8(utf16) == utf8) << "UTF-8 round trip";
}

TEST(Transcoding, EmptyTextTranscodesEmptyAndALoneSurrogateSubstitutes) {
  EXPECT_TRUE(toUtf16(u8"").empty());
  EXPECT_TRUE(toUtf8(u"").empty());
  const char8_t truncated[] = {0xE4, 0xB8};  // the first two bytes of 中
  EXPECT_TRUE(toUtf16(std::u8string_view(truncated, 2)).empty());
  EXPECT_TRUE(toUtf8(u"a\xD800") == u8"a\uFFFD")
      << "a lone surrogate substitutes";
}

TEST(Transcoding, DecodeAtAdvancesByCodePoint) {
  const std::u16string text = u"a\U0001F600b\xD800";
  size_t offset = 0;
  EXPECT_EQ(decodeAt(text, offset), U'a');
  EXPECT_EQ(offset, 1u);
  EXPECT_EQ(decodeAt(text, offset), U'\U0001F600');
  EXPECT_EQ(offset, 3u);
  EXPECT_EQ(decodeAt(text, offset), U'b');
  EXPECT_EQ(decodeAt(text, offset), char32_t{0xD800})
      << "an unpaired surrogate decodes as itself";
  EXPECT_EQ(offset, text.size());
}

// ── Character properties ───────────────────────────────────────────────

TEST(Properties, WhitespaceExcludesNoBreakSpaces) {
  EXPECT_TRUE(isWhitespace(U' '));
  EXPECT_TRUE(isWhitespace(U'\t'));
  EXPECT_TRUE(isWhitespace(U'\n'));
  EXPECT_TRUE(isWhitespace(U'\u3000')) << "ideographic space";
  EXPECT_FALSE(isWhitespace(U'\u00A0')) << "no-break space";
  EXPECT_FALSE(isWhitespace(U'\u202F')) << "narrow no-break space";
  EXPECT_FALSE(isWhitespace(U'a'));
}

TEST(Properties, EveryMandatoryBreakCharacterSaysSoAndNoOtherDoes) {
  EXPECT_TRUE(isHardLineBreak(u'\n'));
  EXPECT_TRUE(isHardLineBreak(u'\r'));
  EXPECT_TRUE(isHardLineBreak(u'\u0085')) << "next line";
  EXPECT_TRUE(isHardLineBreak(u'\u2028')) << "line separator";
  EXPECT_TRUE(isHardLineBreak(u'\u2029')) << "paragraph separator";
  EXPECT_TRUE(isHardLineBreak(u'\u000B')) << "vertical tab";
  EXPECT_TRUE(isHardLineBreak(u'\u000C')) << "form feed";
  EXPECT_FALSE(isHardLineBreak(u' '));
  EXPECT_FALSE(isHardLineBreak(u'\t'));
}

TEST(Properties, AFormatCharacterTakesTheTypefaceOfWhatItFollows) {
  EXPECT_TRUE(inheritsTypeface(U'\u200D')) << "zero width joiner";
  EXPECT_TRUE(inheritsTypeface(U'\uFE0F')) << "variation selector";
  EXPECT_TRUE(inheritsTypeface(U'\u0301')) << "combining acute";
  EXPECT_TRUE(inheritsTypeface(U' '));
  EXPECT_TRUE(inheritsTypeface(U'\u00AD')) << "soft hyphen is a format char";
  EXPECT_FALSE(inheritsTypeface(U'a'));
  EXPECT_FALSE(inheritsTypeface(U'中'));
  EXPECT_FALSE(inheritsTypeface(U'\U0001F600'));
}

TEST(Properties, BidiIsNeededOnlyForACharacterWhoseOwnClassIsRightToLeft) {
  // The question is the character's bidirectional class, so a mark that
  // only ever takes the direction of what it follows cannot force one,
  // however right-to-left the block around it reads.
  EXPECT_FALSE(mayRequireBidi(U'a'));
  EXPECT_FALSE(mayRequireBidi(U'中'));
  EXPECT_FALSE(mayRequireBidi(U'\u0000'));
  EXPECT_TRUE(mayRequireBidi(U'\u05D0')) << "Hebrew alef";
  EXPECT_FALSE(mayRequireBidi(U'\u0591')) << "a Hebrew accent, a mark";
  EXPECT_TRUE(mayRequireBidi(U'\u0627')) << "Arabic alef";
  EXPECT_TRUE(mayRequireBidi(U'\u200F')) << "right-to-left mark";
  EXPECT_TRUE(mayRequireBidi(U'\uFB1D')) << "Hebrew presentation form";
  EXPECT_FALSE(mayRequireBidi(U'\u200E')) << "left-to-right mark";
  EXPECT_TRUE(mayRequireBidi(U'\U0001E900')) << "Adlam, beyond the BMP";
}

TEST(Properties, UpperCaseIsAskedOfTheCodePointItself) {
  EXPECT_TRUE(isUpperCase(U'A'));
  EXPECT_TRUE(isUpperCase(U'Ü'));
  EXPECT_FALSE(isUpperCase(U'a'));
  EXPECT_FALSE(isUpperCase(U'中')) << "a script with no case has none";
}

TEST(Properties, LettersAndTheirLowerCaseFormsAreAskedPerCodePoint) {
  EXPECT_TRUE(isLetter(U'a'));
  EXPECT_TRUE(isLetter(U'ü'));
  EXPECT_TRUE(isLetter(U'中')) << "a script with no case has letters too";
  EXPECT_FALSE(isLetter(U'2'));
  EXPECT_FALSE(isLetter(U'-'));
  EXPECT_FALSE(isLetter(U' '));
  EXPECT_EQ(lowerCased(U'Ü'), U'ü');
  EXPECT_EQ(lowerCased(U'Λ'), U'λ');
  EXPECT_EQ(lowerCased(U'ü'), U'ü');
  EXPECT_EQ(lowerCased(U'中'), U'中') << "no lower-case form, returned as is";
}

TEST(Properties, EachCharacterSaysWhetherAColumnStandsItUprightOrTurnsIt) {
  EXPECT_EQ(verticalOrientation(U'中'), VerticalOrientation::kUpright);
  EXPECT_EQ(verticalOrientation(U'a'), VerticalOrientation::kRotated);
  EXPECT_EQ(verticalOrientation(U'\u3001'),
            VerticalOrientation::kTransformedUpright)
      << "ideographic comma";
  EXPECT_EQ(verticalOrientation(U'\u30FC'),
            VerticalOrientation::kTransformedRotated)
      << "katakana prolonged sound mark";
}

// ── Scripts ────────────────────────────────────────────────────────────

TEST(ScriptItemization, ACodePointNamesItsScriptAndOnlyARealScriptHasAName) {
  const Script latin = scriptOf(U'a');
  const Script han = scriptOf(U'中');
  EXPECT_TRUE(isSpecificScript(latin));
  EXPECT_TRUE(isSpecificScript(han));
  EXPECT_NE(latin, han);
  EXPECT_STREQ(scriptShortName(latin), "Latn");
  EXPECT_STREQ(scriptShortName(han), "Hani");
  EXPECT_EQ(scriptOf(U' '), kCommonScript);
  EXPECT_EQ(scriptOf(U'\u0301'), kInheritedScript);
  EXPECT_FALSE(isSpecificScript(kCommonScript));
  EXPECT_FALSE(isSpecificScript(kInheritedScript));
  EXPECT_GT(scriptLimit(), han);
  EXPECT_EQ(scriptShortName(scriptLimit()), nullptr);
  EXPECT_EQ(scriptShortName(-1), nullptr);
}

TEST(ScriptItemization, TheShaperTagIsTheIsoCodePackedIntoFourBytes) {
  const auto packed = [](char a, char b, char c, char d) {
    return static_cast<ShaperScript>(
        (static_cast<uint32_t>(static_cast<unsigned char>(a)) << 24) |
        (static_cast<uint32_t>(static_cast<unsigned char>(b)) << 16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(c)) << 8) |
        static_cast<uint32_t>(static_cast<unsigned char>(d)));
  };
  EXPECT_EQ(shaperScript(scriptOf(U'a')), packed('L', 'a', 't', 'n'));
  EXPECT_EQ(shaperScript(scriptOf(U'中')), packed('H', 'a', 'n', 'i'));
  EXPECT_EQ(shaperScript(scriptOf(U'א')), packed('H', 'e', 'b', 'r'));
  const ShaperScript defaultTag = shaperScript(kCommonScript);
  EXPECT_EQ(shaperScript(kInheritedScript), defaultTag)
      << "text of no script of its own is shaped under the default rules";
  EXPECT_EQ(shaperScript(scriptLimit()), defaultTag);
  EXPECT_EQ(shaperScript(-1), defaultTag);
}

TEST(Properties, FullWidthIsACharacterPropertyAndNotAScriptOne) {
  EXPECT_TRUE(isFullWidth(U'中'));
  EXPECT_TRUE(isFullWidth(U'あ')) << "hiragana";
  EXPECT_TRUE(isFullWidth(U'한')) << "hangul";
  EXPECT_TRUE(isFullWidth(U'Ａ'))
      << "fullwidth Latin stands in a full-width cell like the kanji it is "
         "set among";
  EXPECT_FALSE(isFullWidth(U'A'));
  EXPECT_FALSE(isFullWidth(U'ก')) << "Thai";
  EXPECT_FALSE(isFullWidth(U' '));
}

TEST(ScriptItemization, ItemizeAttachesCommonToNeighbours) {
  const std::u16string text = u"  abc, 中文! ";
  const std::vector<ScriptRun> runs = itemize(text);
  ASSERT_EQ(runs.size(), 2u);
  EXPECT_EQ(runs[0].script, scriptOf(U'a'))
      << "leading spaces adopt the first real script";
  EXPECT_EQ(runs[0].end, 7u) << "the comma and space stay with Latin";
  EXPECT_EQ(runs[1].script, scriptOf(U'中'));
  EXPECT_EQ(runs[1].end, text.size()) << "trailing punctuation stays with Han";
}

TEST(ScriptItemization, ItemizeHandsBackNothingForATextWithNoCharacters) {
  const std::vector<ScriptRun> empty = itemize(u"");
  ASSERT_EQ(empty.size(), 1u);
  EXPECT_EQ(empty[0].end, 0u);
  EXPECT_EQ(empty[0].script, kCommonScript);

  const std::vector<ScriptRun> punctuation = itemize(u"... 123");
  ASSERT_EQ(punctuation.size(), 1u);
  EXPECT_EQ(punctuation[0].script, kCommonScript);
  EXPECT_EQ(punctuation[0].end, 7u);

  std::vector<ScriptRun> reused = {{99, 42}};
  itemize(u"a\U0001F600b", reused);
  ASSERT_EQ(reused.size(), 1u) << "the emoji is Common and joins Latin";
  EXPECT_EQ(reused[0].end, 4u);
}

// ── Line-break classes ─────────────────────────────────────────────────

TEST(LineBreakClasses, TheProhibitedListingsAreAscendingAndDisjoint) {
  const std::vector<char32_t> start = lineStartProhibited();
  const std::vector<char32_t> end = lineEndProhibited();
  ASSERT_FALSE(start.empty());
  ASSERT_FALSE(end.empty());
  EXPECT_TRUE(std::is_sorted(start.begin(), start.end()));
  EXPECT_TRUE(std::is_sorted(end.begin(), end.end()));
  EXPECT_EQ(std::adjacent_find(start.begin(), start.end()), start.end())
      << "a code point carries one line-break class, so it is listed once";
  std::vector<char32_t> both;
  std::set_intersection(start.begin(), start.end(), end.begin(), end.end(),
                        std::back_inserter(both));
  EXPECT_TRUE(both.empty()) << "nothing may neither open nor close a line";
}

TEST(LineBreakClasses, TheClassesAreTheMarksTheNameSays) {
  const std::vector<char32_t> start = lineStartProhibited();
  const std::vector<char32_t> end = lineEndProhibited();
  const auto holds = [](const std::vector<char32_t>& listing, char32_t code) {
    return std::binary_search(listing.begin(), listing.end(), code);
  };
  EXPECT_TRUE(holds(start, U'、')) << "ideographic comma, closing punctuation";
  EXPECT_TRUE(holds(start, U'」')) << "closing bracket";
  EXPECT_TRUE(holds(start, U')')) << "closing parenthesis";
  EXPECT_TRUE(holds(start, U'ー')) << "prolonged sound mark, a nonstarter";
  EXPECT_TRUE(holds(start, U'ぁ')) << "small kana, a Japanese starter";
  EXPECT_TRUE(holds(start, U'!')) << "exclamation";
  EXPECT_TRUE(holds(start, U':')) << "infix numeric separator";
  EXPECT_TRUE(holds(end, U'(')) << "opening parenthesis";
  EXPECT_TRUE(holds(end, U'「')) << "opening bracket";
  EXPECT_FALSE(holds(start, U'a'));
  EXPECT_FALSE(holds(end, U'a'));
  EXPECT_FALSE(holds(start, U'中')) << "an ideograph opens a line freely";
}

// ── Case mapping ───────────────────────────────────────────────────────

TEST(CaseMapping, AFullCaseMappingMayLengthenTheTextItMaps) {
  EXPECT_EQ(caseMapped(u"hello", Case::kUpper), u"HELLO");
  EXPECT_EQ(caseMapped(u"HeLLo", Case::kLower), u"hello");
  EXPECT_EQ(caseMapped(u"straße", Case::kUpper), u"STRASSE")
      << "full mappings may lengthen the text";
}

TEST(CaseMapping, CapitalizeTouchesOnlyTheFirstCodePoint) {
  EXPECT_EQ(caseMapped(u"hELLO", Case::kCapitalize), u"HELLO");
  EXPECT_EQ(caseMapped(u"ǆap", Case::kCapitalize), u"ǅap")
      << "titlecase differs from uppercase for the dz digraph";
  EXPECT_EQ(caseMapped(u"", Case::kCapitalize), u"");
}

TEST(CaseMapping, LocaleChangesTheMapping) {
  EXPECT_EQ(caseMapped(u"istanbul", Case::kUpper, "tr"), u"İSTANBUL");
  EXPECT_EQ(caseMapped(u"istanbul", Case::kUpper, "en"), u"ISTANBUL");
}

TEST(CaseMapping, OutParameterFormReplacesContents) {
  std::u16string out = u"stale";
  ASSERT_TRUE(caseMap(u"abc", Case::kUpper, {}, out));
  EXPECT_EQ(out, u"ABC");
  ASSERT_TRUE(caseMap(u"", Case::kUpper, {}, out));
  EXPECT_TRUE(out.empty());
}

// ── Segmentation ───────────────────────────────────────────────────────

namespace {

/// The offsets alone, for the cases the flag is not what is under test.
std::vector<uint32_t> breakOffsets(std::u16string_view text,
                                   std::string_view locale = {}) {
  std::vector<uint32_t> offsets;
  for (const LineBreak& entry : lineBreaks(text, locale))
    offsets.push_back(entry.offset);
  return offsets;
}

}  // namespace

TEST(Segmentation, LineBreaksFollowSpacesAndEndAtTheText) {
  const std::u16string text = u"one two\nthree";
  EXPECT_EQ(breakOffsets(text), (std::vector<uint32_t>{4, 8, 13}));
  EXPECT_EQ(breakOffsets(u""), (std::vector<uint32_t>{0}));
  EXPECT_EQ(breakOffsets(u"word"), (std::vector<uint32_t>{4}));
}

TEST(Segmentation, TheSegmentationSaysWhichBreakTheTextDemands) {
  // The flag is the rule that opened the boundary, so a CR LF pair — which
  // no test of the character before the boundary can judge — is one
  // mandatory break, and the vertical tab and form feed no hand-written
  // list remembers are mandatory too.
  const std::vector<LineBreak> breaks = lineBreaks(u"a\r\nb\u000Bc");
  ASSERT_EQ(breaks.size(), 3u);
  EXPECT_EQ(breaks[0], (LineBreak{3, true}));
  EXPECT_EQ(breaks[1], (LineBreak{5, true}));
  EXPECT_EQ(breaks[2], (LineBreak{6, false})) << "the end of the text";
  for (const LineBreak& entry : lineBreaks(u"one two"))
    EXPECT_FALSE(entry.mandatory);
}

TEST(Segmentation, ATailoringIsWhereAScriptsOwnProhibitionsComeFrom) {
  // The strict Japanese rules forbid a break before a small kana; the
  // untailored root rules allow one.
  const std::u16string text = u"\u3068\u3063\u3066";  // と っ て
  const std::vector<uint32_t> loose = breakOffsets(text, "ja@lb=loose");
  const std::vector<uint32_t> strict = breakOffsets(text, "ja@lb=strict");
  EXPECT_LT(strict.size(), loose.size());
  EXPECT_EQ(strict, breakOffsets(text))
      << "the strict rules are what an untailored text already gets";
}

TEST(Segmentation, GraphemeClustersAreWhatAReaderCallsCharacters) {
  // A base and its combining mark, and a regional-indicator pair, are each
  // one cluster however many code units they take.
  EXPECT_EQ(graphemeBoundaries(u"e\u0301x"),
            (std::vector<uint32_t>{0, 2, 3}));
  EXPECT_EQ(graphemeBoundaries(u"\U0001F1EF\U0001F1F5"),
            (std::vector<uint32_t>{0, 4}));
  EXPECT_EQ(graphemeBoundaries(u""), (std::vector<uint32_t>{0}));
}

TEST(Segmentation, LineBreaksBetweenIdeographsAndNotAtNoBreakSpace) {
  EXPECT_EQ(breakOffsets(u"中文字"), (std::vector<uint32_t>{1, 2, 3}));
  EXPECT_EQ(breakOffsets(u"a b c"), (std::vector<uint32_t>{4, 5}));
  EXPECT_EQ(breakOffsets(u"co­op"), (std::vector<uint32_t>{3, 5}))
      << "a soft hyphen opens a break right after itself";
  std::vector<LineBreak> reused = {{7, false}, {7, false}};
  lineBreaks(u"ab", reused);
  EXPECT_EQ(reused, (std::vector<LineBreak>{{2, false}}));
}

TEST(Segmentation, WordBoundariesBracketEveryWord) {
  EXPECT_EQ(wordBoundaries(u"one two, three"),
            (std::vector<uint32_t>{0, 3, 4, 7, 8, 9, 14}));
  EXPECT_EQ(wordBoundaries(u""), (std::vector<uint32_t>{0}));
  EXPECT_EQ(wordBoundaries(u"中文 abc"), (std::vector<uint32_t>{0, 2, 3, 6}));
}

TEST(Segmentation, SentenceStartsAfterTerminatorsOnly) {
  EXPECT_EQ(sentenceStarts(u"One. Two! Three? Four"),
            (std::vector<uint32_t>{0, 5, 10, 17}));
  EXPECT_EQ(sentenceStarts(u"e.g. this is it. Yes"),
            (std::vector<uint32_t>{0, 17}))
      << "a period before a lowercase letter ends no sentence";
  EXPECT_EQ(sentenceStarts(u"no terminator"), (std::vector<uint32_t>{0}));
  EXPECT_TRUE(sentenceStarts(u"").empty());
}

// ── Bidirectional analysis ─────────────────────────────────────────────

TEST(Bidi, LeftToRightTextIsOneLevelZeroRun) {
  const std::vector<BidiRun> runs = bidi(u"plain latin text");
  ASSERT_EQ(runs.size(), 1u);
  EXPECT_EQ(runs[0].start, 0u);
  EXPECT_EQ(runs[0].end, 16u);
  EXPECT_EQ(runs[0].level, 0);
  EXPECT_TRUE(bidi(u"").empty());
}

TEST(Bidi, RightToLeftTextIsOneLevelOneRun) {
  const std::u16string hebrew = u"שלום";
  const std::vector<BidiRun> runs = bidi(hebrew);
  ASSERT_EQ(runs.size(), 1u);
  EXPECT_EQ(runs[0].end, hebrew.size());
  EXPECT_EQ(runs[0].level, 1);
}

TEST(Bidi, MixedTextSplitsAtLevelChanges) {
  // "abc " + Hebrew + " def": the Hebrew word sits at level 1 between two
  // level-0 stretches; the spaces around it resolve to the paragraph level.
  const std::u16string text = u"abc שלום def";
  const std::vector<BidiRun> runs = bidi(text);
  ASSERT_EQ(runs.size(), 3u);
  EXPECT_EQ(runs[0].start, 0u);
  EXPECT_EQ(runs[0].end, 4u);
  EXPECT_EQ(runs[0].level, 0);
  EXPECT_EQ(runs[1].start, 4u);
  EXPECT_EQ(runs[1].end, 8u);
  EXPECT_EQ(runs[1].level, 1);
  EXPECT_EQ(runs[2].start, 8u);
  EXPECT_EQ(runs[2].end, text.size());
  EXPECT_EQ(runs[2].level, 0);
}

TEST(Bidi, BaseDirectionDecidesTheNeutralLevel) {
  const std::u16string text = u"abc שלום";
  const std::vector<BidiRun> forcedRtl =
      bidi(text, BaseDirection::kRightToLeft);
  ASSERT_GE(forcedRtl.size(), 2u);
  EXPECT_EQ(forcedRtl.front().level, 2)
      << "Latin embedded in a right-to-left paragraph sits at level 2";
  EXPECT_EQ(forcedRtl.back().level, 1);

  const std::vector<BidiRun> autoRtl =
      bidi(u"...", BaseDirection::kAutoRightToLeft);
  ASSERT_EQ(autoRtl.size(), 1u);
  EXPECT_EQ(autoRtl[0].level, 1)
      << "no strong character: the auto direction's fallback applies";
  const std::vector<BidiRun> autoLtr =
      bidi(u"...", BaseDirection::kAutoLeftToRight);
  ASSERT_EQ(autoLtr.size(), 1u);
  EXPECT_EQ(autoLtr[0].level, 0);
}
