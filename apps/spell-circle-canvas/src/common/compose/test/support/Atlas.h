#pragma once
// A decoded image asset with two distinguishable cells, for atlas and
// image-leaf tests.

#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/Image.h>

#include "Host.h"

namespace {

/** A 2-cell atlas: left 16x16 red, right 16x16 green. */
std::shared_ptr<const sigil::media::Image> twoCellAtlas() {
  SkBitmap src;
  src.allocN32Pixels(32, 16);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 16, 16));
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(16, 0, 16, 16));
  return sigil::media::Image::of(src.asImage());
}

}  // namespace
