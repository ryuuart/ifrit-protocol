#pragma once

/** @file
 * WebImage::Impl — the slot's image source as Ultralight sees it, the
 * bitmap behind it on CPU engines or the device texture behind it on
 * GPU ones. Internal: it names Ultralight types, and Ultralight is
 * private to the library.
 */

#include <Ultralight/Ultralight.h>
#include <sigilskia/device/Handle.h>

#include <cstdint>
#include <string>

#include "sigilscry/engine/WebEngine.h"
#include "sigilscry/engine/WebImage.h"

namespace sigil::scry {

class WebImage::Impl {
 public:
  WebEngine::Impl* engine = nullptr;
  std::string name;
  int width = 0;
  int height = 0;

  // Web-thread-only Ultralight state.
  ultralight::RefPtr<ultralight::ImageSource> source;
  ultralight::RefPtr<ultralight::Bitmap> bitmap;  // CPU engines

  // GPU engines: immutable after creation, readable from any thread.
  sigil::skia::TextureHandle gpuTexture;  // driver-owned, on the device
  uint32_t gpuTextureId = 0;
};

}  // namespace sigil::scry
