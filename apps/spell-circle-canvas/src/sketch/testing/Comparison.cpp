/** @file
 * Two pictures on disk, decoded and held against each other.
 */

#include "sigilsketch/testing/Comparison.h"

#include <sigilio/source/Source.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/image/Decode.h>

#include <memory>

namespace sigil::sketch::testing {

namespace {

/** The picture at @p path, decoded; null when it cannot be read or is
 *  not a picture. */
std::shared_ptr<const media::Image> readPicture(
    const std::filesystem::path& path) {
  const std::optional<io::Bytes> bytes = io::readBytes(path);
  if (!bytes) return nullptr;
  auto picture = media::decode<media::Image>(bytes->span(), {}, path);
  if (!picture || picture->frames().empty()) return nullptr;
  return picture;
}

}  // namespace

Comparison compare(const std::filesystem::path& actual,
                   const std::filesystem::path& expected) {
  Comparison comparison;
  const auto left = readPicture(actual);
  if (!left) {
    comparison.problem = "cannot read " + actual.string();
    return comparison;
  }
  const auto right = readPicture(expected);
  if (!right) {
    comparison.problem = "cannot read " + expected.string();
    return comparison;
  }
  comparison.actual = media::toSk(left->size());
  comparison.expected = media::toSk(right->size());
  comparison.pixels = media::difference(*left, *right);
  return comparison;
}

}  // namespace sigil::sketch::testing
