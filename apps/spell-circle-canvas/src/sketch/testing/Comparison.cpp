/** @file
 * Two pictures on disk, decoded and held against each other.
 */

#include "sigilsketch/testing/Comparison.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilio/source/Source.h>

#include <optional>

namespace sigil::sketch::testing {

namespace {

/** The first frame of the picture at @p path, as premultiplied N32. */
std::optional<SkBitmap> readPicture(const std::filesystem::path& path) {
  const std::optional<io::Bytes> bytes = io::readBytes(path);
  if (!bytes) return std::nullopt;
  std::optional<image::ImageAsset> asset = image::ImageAsset::decode(
      SkData::MakeWithCopy(bytes->data(), bytes->size()));
  if (!asset || asset->frames().empty() || !asset->frames().front().image)
    return std::nullopt;
  const sk_sp<SkImage>& frame = asset->frames().front().image;
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(
          SkImageInfo::MakeN32Premul(frame->width(), frame->height())) ||
      !frame->readPixels(nullptr, bitmap.pixmap(), 0, 0))
    return std::nullopt;
  return bitmap;
}

}  // namespace

Comparison compare(const std::filesystem::path& actual,
                   const std::filesystem::path& expected) {
  Comparison comparison;
  const std::optional<SkBitmap> left = readPicture(actual);
  if (!left) {
    comparison.problem = "cannot read " + actual.string();
    return comparison;
  }
  const std::optional<SkBitmap> right = readPicture(expected);
  if (!right) {
    comparison.problem = "cannot read " + expected.string();
    return comparison;
  }
  comparison.actual = left->dimensions();
  comparison.expected = right->dimensions();
  comparison.pixels = image::difference(left->pixmap(), right->pixmap());
  return comparison;
}

}  // namespace sigil::sketch::testing
