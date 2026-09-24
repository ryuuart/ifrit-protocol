#pragma once

/** @file
 * What every plate case shares: the showcase palette's colours, the
 * single-span style a panel sets its text in, the process's font context,
 * and the one assertion a plate ends on — its render held against the
 * baseline committed under the testing feature's test/plates/.
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
#include "Palette.h"

namespace sigil::weave::test {

inline constexpr SkColor kInk = examples::palette::kInk;
inline constexpr SkColor kAccent = examples::palette::kAccent;
inline constexpr SkColor kBlue = examples::palette::kBlue;
inline constexpr SkColor kShape = examples::palette::kShape;
inline constexpr SkColor kPaper = examples::palette::kPaper;

/// One span's style for a panel: a size, an ink and a language, in
/// whatever face the context resolves.
inline TextStyle plateStyle(float fontSize, SkColor color = kInk,
                            const char* languageTag = "") {
  return kit::makeStyle(fontSize, color, languageTag);
}

/// Whether this run adopts every render as its baseline rather than
/// judging it: SIGIL_PLATES_REBASE set to anything but empty or `0`.
inline bool rebasingPlates() {
  const char* value = std::getenv("SIGIL_PLATES_REBASE");
  return value && *value && std::string_view(value) != "0";
}

/// Holds @p plate against the baseline named @p name, or adopts it when
/// the run is rebasing. A render that does not stand is written under
/// the build tree, and the failure names both files.
inline void expectPlate(const testing::Plate& plate, const std::string& name) {
  const std::string file = name + ".png";
  const testing::BaselineComparison comparison = testing::compareToBaseline(
      plate.pixels(), std::filesystem::path(SIGIL_WEAVE_PLATE_BASELINES) / file,
      rebasingPlates() ? testing::BaselineAction::kAdopt
                       : testing::BaselineAction::kJudge,
      std::filesystem::path(SIGIL_WEAVE_PLATE_RENDERS) / file);
  EXPECT_TRUE(comparison.passed())
      << name << ": " << testing::describe(comparison)
      << " — SIGIL_PLATES_REBASE=1 adopts the render when the move is meant";
}

}  // namespace sigil::weave::test
