#include "DemoSupport.h"

#include <include/core/SkData.h>
#include <include/core/SkPixmap.h>
#include <sigilimage/encode/Encode.h>
#include <sigilloader/source/Sink.h>

void writePng(SkSurface* surface, const std::filesystem::path& path) {
  SkPixmap pixmap;
  if (!surface->peekPixels(&pixmap)) return;
  const sk_sp<SkData> png =
      sigil::image::encodeImage(pixmap, sigil::image::Format::Png);
  if (png) sigil::loader::writeBytes(path, png->data(), png->size());
}
