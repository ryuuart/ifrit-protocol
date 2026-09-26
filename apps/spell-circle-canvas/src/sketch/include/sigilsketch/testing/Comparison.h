#pragma once

/** @file
 * @ingroup sketch-testing
 *
 * A still held against another picture on disk, as a value: whether both
 * could be read, their sizes, and SigilMedia's pixel difference between
 * them.
 */

#include <include/core/SkSize.h>
#include <sigilmedia/difference/Difference.h>

#include <filesystem>
#include <string>

namespace sigil::sketch::testing {

/** TWO PICTURES ON DISK HELD AGAINST EACH OTHER. `problem` names a file
 *  that could not be read; otherwise the sizes are both pictures' and
 *  `pixels` the difference over the extent both cover. */
struct Comparison {
  /** Why nothing was compared, naming the file; empty where both were
   *  read. */
  std::string problem;
  SkISize actual = SkISize::MakeEmpty();
  SkISize expected = SkISize::MakeEmpty();
  media::PixelDifference pixels;

  /** Whether the two are one picture: both read, one size, no pixel
   *  apart. */
  [[nodiscard]] bool identical() const {
    return problem.empty() && actual == expected && pixels.identical();
  }
};

/** @p actual held against @p expected, each a PNG, decoded to
 *  premultiplied N32 as a plate is rendered. */
[[nodiscard]] Comparison compare(const std::filesystem::path& actual,
                                 const std::filesystem::path& expected);

}  // namespace sigil::sketch::testing
