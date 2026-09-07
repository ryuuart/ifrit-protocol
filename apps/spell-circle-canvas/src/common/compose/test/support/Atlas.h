#pragma once
// A decoded image asset with two distinguishable cells, for atlas and
// image-leaf tests.

#include <sigilimage/asset/ImageAsset.h>

#include "Host.h"

namespace {

/** A 2-cell atlas: left 16x16 red, right 16x16 green. */
std::shared_ptr<sigil::image::ImageAsset> twoCellAtlas() {
  SkBitmap src;
  src.allocN32Pixels(32, 16);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 16, 16));
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(16, 0, 16, 16));
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(src.asImage()));
}

}  // namespace
