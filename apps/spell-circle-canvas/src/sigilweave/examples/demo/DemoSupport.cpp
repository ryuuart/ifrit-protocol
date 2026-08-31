#include "DemoSupport.h"

#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

void writePng(SkSurface* surface, const std::filesystem::path& path) {
  SkPixmap pixmap;
  if (!surface->peekPixels(&pixmap)) return;
  SkFILEWStream stream(path.string().c_str());
  if (stream.isValid()) SkPngEncoder::Encode(&stream, pixmap, {});
}
