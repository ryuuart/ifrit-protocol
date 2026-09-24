#pragma once

/** @file
 * @ingroup weave-testing
 *
 * A render held against a committed baseline image, or adopted as the
 * new one. The baseline is a PNG a test commits beside itself, with the
 * faces a plate was drawn in listed beside it, so a render that moved
 * because the machine's font set did says so rather than reading as a
 * moved layout; what a comparison found comes back as a value, and the
 * one line that says it names both files.
 */

#include <include/core/SkPixmap.h>
#include <include/core/SkSize.h>
#include <sigilimage/difference/Difference.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "sigilweave/testing/Plate.h"

namespace sigil::weave::testing {

/** Whether a comparison judges the render or adopts it as the baseline —
 *  the plate ledger's rebase, one file at a time. */
enum class BaselineAction : uint8_t { kJudge, kAdopt };

/** What holding a render against its baseline found. */
enum class BaselineOutcome : uint8_t {
  kMatched,   ///< every pixel identical
  kDiffered,  ///< same size, some pixel differs
  /// Some pixel differs, or the size does, and the faces the plate was
  /// drawn in are not the ones its baseline was adopted on: the font set
  /// moved, which may be all that did.
  kFacesChanged,
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
  /// The faces the baseline was adopted on that the plate was not drawn
  /// in, and the ones it was drawn in that the baseline was not adopted
  /// on. Both empty when they agree, and when no faces were listed
  /// beside the baseline or the render was not a plate.
  std::vector<std::string> facesGone;
  std::vector<std::string> facesNew;
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

/** Holds @p plate against the PNG at @p baseline as the pixmap form
 *  does, and the faces it was drawn in against the list beside it — the
 *  baseline's path with the extension `.faces`, one `Plate::faces` line
 *  each. Adopting writes both files. A render that does not stand while
 *  the two lists disagree reads `BaselineOutcome::kFacesChanged`.
 *  @silent no list stands beside the baseline: the faces are not judged,
 *  and the comparison is the pixmap form's. */
[[nodiscard]] BaselineComparison compareToBaseline(
    const Plate& plate, const std::filesystem::path& baseline,
    BaselineAction action = BaselineAction::kJudge,
    const std::filesystem::path& rejected = {});

/** Where the faces a plate's baseline was adopted on are listed: beside
 *  @p baseline, with the extension `.faces`. */
[[nodiscard]] std::filesystem::path facesBeside(
    const std::filesystem::path& baseline);

/** The comparison as the one line a failing case prints: the outcome,
 *  the sizes or the difference, the faces that changed, and the files to
 *  open. */
[[nodiscard]] std::string describe(const BaselineComparison& comparison);

}  // namespace sigil::weave::testing
