/** @file
 * The platform port: one font manager for the process, the fallback
 * chain resolved against it, and the face it holds once per ask.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontArguments.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkSpan.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <string>
#include <thread>
#include <vector>

namespace sigil::weave::ports {
namespace {

/** A NAME NO FONT CARRIES, so a list opening with it always runs out
 *  onto whatever the machine falls back to. */
constexpr const char* kAbsent = "No Family Is Called This 4a7c15";

/** The face's own family, which is what two answers are compared by: a
 *  manager may hand back a fresh SkTypeface for a family it has already
 *  matched, so two resolutions of one family are the same ANSWER without
 *  being the same object. */
std::string familyOf(const sk_sp<SkTypeface>& face) {
  SkString name;
  if (face) face->getFamilyName(&name);
  return name.c_str();
}

/** One face read straight off the tree's instrument directory, through
 *  the port's own manager — the loader every committed fixture is opened
 *  with. */
sk_sp<SkTypeface> instrument(const char* fileName) {
  const std::string path =
      std::string(SIGIL_TEST_INSTRUMENT_DIR "/") + fileName;
  return systemFontManager()->makeFromFile(path.c_str());
}

/** How many variation axes @p face declares. */
int axesOf(const sk_sp<SkTypeface>& face) {
  return face->getVariationDesignPosition(
      SkSpan<SkFontArguments::VariationPosition::Coordinate>());
}

TEST(SystemFontManager, OneManagerStandsForTheWholeProcess) {
  // Constructing one enumerates the installed font set, so a second is
  // that cost paid again — and every FontContext in the process shapes
  // through the strikes of whichever manager it was handed.
  const sk_sp<SkFontMgr> first = systemFontManager();
  const sk_sp<SkFontMgr> second = systemFontManager();
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first.get(), second.get());
}

TEST(SystemFontManager, ReadsAFaceOutOfAFileWithoutMatchingAnyFamily) {
  // The committed instruments are opened this way and no other: a path,
  // not a family, so the machine's font list decides nothing about them.
  // A face that came back matched instead would answer for a character
  // this one deliberately does not carry.
  const sk_sp<SkTypeface> sans = instrument("Sans.ttf");
  ASSERT_NE(sans, nullptr) << "the instrument faces are not on disk";
  EXPECT_NE(sans->unicharToGlyph('A'), 0);
  EXPECT_EQ(sans->unicharToGlyph(0x4E00), 0)
      << "the Latin instrument answered for a Han character";

  // …and a file's own axes come through it, which is what a variable
  // instrument is for.
  const sk_sp<SkTypeface> variable = instrument("Variable.ttf");
  ASSERT_NE(variable, nullptr);
  EXPECT_GT(axesOf(variable), 0) << "the variable instrument declares no axes";
  // The static instrument has no variation table to read, which is what
  // a face reports as no axes at all.
  EXPECT_LE(axesOf(sans), 0);
}

/** What every claim below needs of the machine: a font manager that
 *  answers with SOMETHING for a family it does not have. A port with no
 *  platform behind it hands back an empty manager, and a fallback chain
 *  over it resolves to nothing at all — which is a machine without fonts
 *  rather than a defect in the chain. */
class InstalledFonts : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!pickTypeface({}))
      GTEST_SKIP() << "this machine's font manager offers no default family";
  }
};

TEST_F(InstalledFonts, AListRunsOutOntoTheDefaultFamilyRatherThanOntoNothing) {
  // A caller names the face it wants and then the stand-ins it will
  // accept; a machine that has none of them still draws.
  const sk_sp<SkTypeface> given = pickTypeface({kAbsent, kAbsent});
  ASSERT_NE(given, nullptr);
  EXPECT_EQ(familyOf(given), familyOf(pickTypeface({})));
}

TEST_F(InstalledFonts, TheWeightAndSlantSpellingIsTheStyleSpelling) {
  // The two overloads are one resolution reached two ways, for a caller
  // holding two numbers rather than an SkFontStyle — and the held form
  // is one entry however it was spelled.
  constexpr int kWeight = SkFontStyle::kBold_Weight;
  const SkFontStyle style(kWeight, SkFontStyle::kNormal_Width,
                          SkFontStyle::kItalic_Slant);
  EXPECT_EQ(
      familyOf(pickTypeface({kAbsent}, kWeight, SkFontStyle::kItalic_Slant)),
      familyOf(pickTypeface({kAbsent}, style)));
  EXPECT_EQ(face({kAbsent}, kWeight, SkFontStyle::kItalic_Slant).get(),
            face({kAbsent}, style).get());
}

/** ONE ASK, made either through the holder or by a fresh walk.
 *
 *  The families are spelled inside the row's own function rather than
 *  carried in a field: the two calls take an initializer_list, whose
 *  backing array lives only as long as the expression that wrote it, so
 *  a list cannot be held in a value the way a row is. The rows differ in
 *  each of the things the holder's key is built from. */
struct Ask {
  const char* what;
  sk_sp<SkTypeface> (*resolve)(bool held);
};

class HeldFace : public InstalledFonts,
                 public ::testing::WithParamInterface<Ask> {};

TEST_P(HeldFace, AnswersWhatAFreshWalkWouldAndHandsThatBackForever) {
  const Ask& ask = GetParam();
  const sk_sp<SkTypeface> held = ask.resolve(true);
  ASSERT_NE(held, nullptr);
  // THE ANSWER IS THE WALK'S. Holding it must not change which face a
  // caller gets, only how often the installed list is read for it.
  EXPECT_EQ(familyOf(held), familyOf(ask.resolve(false)));
  // …AND IT IS ONE FACE. A face is compared by pointer wherever a style,
  // a memo key or an inherited value is compared, so a second answer to
  // one ask is a value that never compares equal to the first.
  EXPECT_EQ(ask.resolve(true).get(), held.get());
}

INSTANTIATE_TEST_SUITE_P(
    Asks, HeldFace,
    ::testing::Values(
        Ask{"NoFamiliesAtAll",
            [](bool held) {
              return held ? face({}) : pickTypeface({});
            }},
        Ask{"OneFamily",
            [](bool held) {
              return held ? face({kAbsent}) : pickTypeface({kAbsent});
            }},
        Ask{"AFallbackChain",
            [](bool held) {
              return held ? face({kAbsent, "Menlo", "monospace"})
                          : pickTypeface({kAbsent, "Menlo", "monospace"});
            }},
        Ask{"TheSameChainBold",
            [](bool held) {
              return held ? face({kAbsent, "Menlo", "monospace"},
                                 SkFontStyle::Bold())
                          : pickTypeface({kAbsent, "Menlo", "monospace"},
                                         SkFontStyle::Bold());
            }},
        Ask{"TheSameChainItalic",
            [](bool held) {
              return held ? face({kAbsent, "Menlo", "monospace"},
                                 SkFontStyle::Italic())
                          : pickTypeface({kAbsent, "Menlo", "monospace"},
                                         SkFontStyle::Italic());
            }}),
    [](const ::testing::TestParamInfo<Ask>& row) { return row.param.what; });

TEST_F(InstalledFonts, EveryThreadAskingAtOnceIsAnsweredWithTheOneFace) {
  // The walk happens under the holder's lock, so a family two threads
  // ask for at once is walked once and both are given the same face. A
  // describe runs on whichever thread the host calls on, which is what
  // makes this the ordinary case rather than a stress.
  constexpr int kThreads = 8;
  std::vector<sk_sp<SkTypeface>> answers((size_t)kThreads);
  std::vector<std::thread> askers;
  askers.reserve((size_t)kThreads);
  for (int i = 0; i < kThreads; ++i)
    askers.emplace_back([&answers, i] {
      answers[(size_t)i] =
          face({"No Family Is Called This Either 1ceff", "Menlo", "monospace"});
    });
  for (std::thread& asker : askers) asker.join();
  ASSERT_NE(answers[0], nullptr);
  for (const sk_sp<SkTypeface>& answer : answers)
    EXPECT_EQ(answer.get(), answers[0].get());
}

}  // namespace
}  // namespace sigil::weave::ports
