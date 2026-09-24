#pragma once

/** @file
 * What every plate case shares: the colours the panels are painted in,
 * the single-span style a panel sets its text in, the process's font
 * context, and the one assertion a plate ends on — its render and the
 * faces it was drawn in held against the baseline committed under the
 * testing feature's test/plates/.
 */

#include <gtest/gtest.h>
#include <include/core/SkColor.h>
#include <sigilweave/kit/Labels.h>
#include <sigilweave/style/TextStyle.h>
#include <sigilweave/testing/Baseline.h>
#include <sigilweave/testing/Plate.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

#include "Faces.h"

namespace sigil::weave::test {

/// The panels' colours: warm paper, near-black ink, and the accents that
/// read on it. A baseline holds them as pixels, so changing one is a move
/// every plate that paints it has to adopt.
inline constexpr SkColor kInk = 0xFF23252B;
inline constexpr SkColor kAccent = 0xFFC63D2F;
inline constexpr SkColor kBlue = 0xFF2B5AA7;
inline constexpr SkColor kShape = 0x33808A99;
inline constexpr SkColor kPaper = 0xFFFAF7F0;

/// One span's style for a panel: a size, an ink and a language, in
/// whatever face the context resolves.
inline TextStyle plateStyle(float fontSize, SkColor color = kInk,
                            const char* languageTag = "") {
  return kit::makeStyle(fontSize, color, languageTag);
}

/// Whether this run adopts every render as its baseline rather than
/// judging it: SIGIL_PLATES_REBASE set to anything but empty or `0`,
/// which `sigil.py plates --rebase` sets for the library plate cases it
/// runs.
inline bool rebasingPlates() {
  const char* value = std::getenv("SIGIL_PLATES_REBASE");
  return value && *value && std::string_view(value) != "0";
}

/// Holds @p plate and the faces it was drawn in against the baseline
/// named @p name, or adopts both when the run is rebasing. A render that
/// does not stand is written under the build tree, and the failure names
/// both files and any face that changed.
inline void expectPlate(const testing::Plate& plate, const std::string& name) {
  const std::string file = name + ".png";
  const testing::BaselineComparison comparison = testing::compareToBaseline(
      plate, std::filesystem::path(SIGIL_WEAVE_PLATE_BASELINES) / file,
      rebasingPlates() ? testing::BaselineAction::kAdopt
                       : testing::BaselineAction::kJudge,
      std::filesystem::path(SIGIL_WEAVE_PLATE_RENDERS) / file);
  EXPECT_TRUE(comparison.passed())
      << name << ": " << testing::describe(comparison)
      << " — `sigil.py plates --rebase` adopts the render when the move is "
         "meant";
}

}  // namespace sigil::weave::test
