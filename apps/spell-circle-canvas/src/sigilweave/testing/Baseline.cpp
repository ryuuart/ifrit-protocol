#include "sigilweave/testing/Baseline.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/source/Sink.h>
#include <sigilio/source/Source.h>

#include <optional>
#include <system_error>

namespace sigil::weave::testing {

namespace {

/// Writes @p pixels to @p path as a PNG, making its directory first.
bool writePng(const SkPixmap& pixels, const std::filesystem::path& path) {
  const sk_sp<SkData> png = image::encodeImage(pixels, image::Format::Png);
  if (!png) return false;
  std::error_code ignored;
  if (path.has_parent_path())
    std::filesystem::create_directories(path.parent_path(), ignored);
  return io::writeBytes(path, png->data(), png->size());
}

/// The baseline's first frame read back as premultiplied N32, the format
/// a plate is rendered in.
std::optional<SkBitmap> readPng(const std::filesystem::path& path) {
  const std::optional<io::Bytes> bytes = io::readBytes(path);
  if (!bytes) return std::nullopt;
  std::optional<image::ImageAsset> asset = image::ImageAsset::decode(
      SkData::MakeWithCopy(bytes->bytes.data(), bytes->bytes.size()));
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

const char* outcomeName(BaselineOutcome outcome) {
  switch (outcome) {
    case BaselineOutcome::kMatched:
      return "matched";
    case BaselineOutcome::kDiffered:
      return "differed";
    case BaselineOutcome::kResized:
      return "resized";
    case BaselineOutcome::kMissing:
      return "missing";
    case BaselineOutcome::kUnreadable:
      return "unreadable";
    case BaselineOutcome::kAdopted:
      return "adopted";
    case BaselineOutcome::kUnwritable:
      return "unwritable";
  }
  return "unknown";
}

std::string sizeText(SkISize size) {
  return std::to_string(size.width()) + "x" + std::to_string(size.height());
}

}  // namespace

BaselineComparison compareToBaseline(const SkPixmap& render,
                                     const std::filesystem::path& baseline,
                                     BaselineAction action,
                                     const std::filesystem::path& rejected) {
  BaselineComparison comparison;
  comparison.baseline = baseline;
  comparison.renderSize = {render.width(), render.height()};

  if (action == BaselineAction::kAdopt) {
    comparison.outcome = writePng(render, baseline)
                             ? BaselineOutcome::kAdopted
                             : BaselineOutcome::kUnwritable;
    comparison.baselineSize = comparison.renderSize;
    return comparison;
  }

  std::error_code missing;
  if (!std::filesystem::exists(baseline, missing)) {
    comparison.outcome = BaselineOutcome::kMissing;
  } else if (const std::optional<SkBitmap> expected = readPng(baseline);
             !expected) {
    comparison.outcome = BaselineOutcome::kUnreadable;
  } else {
    comparison.baselineSize = {expected->width(), expected->height()};
    if (comparison.baselineSize != comparison.renderSize) {
      comparison.outcome = BaselineOutcome::kResized;
    } else {
      comparison.difference = difference(render, expected->pixmap());
      comparison.outcome = comparison.difference.identical()
                               ? BaselineOutcome::kMatched
                               : BaselineOutcome::kDiffered;
    }
  }
  if (comparison.outcome != BaselineOutcome::kMatched && !rejected.empty() &&
      writePng(render, rejected))
    comparison.rejected = rejected;
  return comparison;
}

std::string describe(const BaselineComparison& comparison) {
  std::string line = outcomeName(comparison.outcome);
  switch (comparison.outcome) {
    case BaselineOutcome::kDiffered:
      line += ": " + std::to_string(comparison.difference.differingPixels) +
              " pixels, widest channel " +
              std::to_string(comparison.difference.worst) + " at (" +
              std::to_string(comparison.difference.x) + ", " +
              std::to_string(comparison.difference.y) + ")";
      break;
    case BaselineOutcome::kResized:
      line += ": rendered " + sizeText(comparison.renderSize) + ", baseline " +
              sizeText(comparison.baselineSize);
      break;
    default:
      break;
  }
  line += "; baseline " + comparison.baseline.string();
  if (!comparison.rejected.empty())
    line += "; render " + comparison.rejected.string();
  return line;
}

}  // namespace sigil::weave::testing
