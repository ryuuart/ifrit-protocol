#pragma once

/** @file
 * @ingroup weave-testing
 *
 * A render held against a committed baseline image, or adopted as the
 * new one. The baseline is a PNG a test commits beside itself; what a
 * comparison found comes back as a value, and the one line that says it
 * names both files.
 */

#include <include/core/SkPixmap.h>
#include <include/core/SkSize.h>
#include <sigilimage/difference/Difference.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace sigil::weave::testing {

/** Whether a comparison judges the render or adopts it as the baseline —
 *  the plate ledger's rebase, one file at a time. */
enum class BaselineAction : uint8_t { kJudge, kAdopt };

/** What holding a render against its baseline found. */
enum class BaselineOutcome : uint8_t {
  kMatched,     ///< every pixel identical
  kDiffered,    ///< same size, some pixel differs
  kResized,     ///< the two sizes differ
  kMissing,     ///< no baseline file to judge against
  kUnreadable,  ///< a file that decodes to no image
  kAdopted,     ///< the render was written as the baseline
  kUnwritable,  ///< adopting it failed to write the file
};

/** ONE COMPARISON AS A VALUE. `rejected` is where the render was written
 *  when it did not match, so the two files can be looked at side by
 *  side; empty when nothing was written. */
struct BaselineComparison {
  BaselineOutcome outcome = BaselineOutcome::kMissing;
  std::filesystem::path baseline;
  std::filesystem::path rejected;
  SkISize renderSize = {0, 0};
  SkISize baselineSize = {0, 0};
  image::PixelDifference difference;
  /** The render stands: it matched, or it is now the baseline. */
  [[nodiscard]] bool passed() const {
    return outcome == BaselineOutcome::kMatched ||
           outcome == BaselineOutcome::kAdopted;
  }
};

/** Holds @p render against the PNG at @p baseline, or under
 *  `BaselineAction::kAdopt` writes it there. A render that does not
 *  match is written to @p rejected when that is not empty, its
 *  directory made as needed. Identity is the bar: the plate is
 *  rasterized on the CPU, so the same layout on the same machine
 *  answers the same bytes, and any difference is a change to explain. */
[[nodiscard]] BaselineComparison compareToBaseline(
    const SkPixmap& render, const std::filesystem::path& baseline,
    BaselineAction action = BaselineAction::kJudge,
    const std::filesystem::path& rejected = {});

/** The comparison as the one line a failing case prints: the outcome,
 *  the sizes or the difference, and the files to open. */
[[nodiscard]] std::string describe(const BaselineComparison& comparison);

}  // namespace sigil::weave::testing
