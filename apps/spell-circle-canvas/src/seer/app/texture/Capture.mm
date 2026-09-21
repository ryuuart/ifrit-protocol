// The one readback in the receiver: a shared frame copied into memory the
// CPU can address, and encoded from there.

#include "Capture.h"

#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/source/Sink.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <optional>
#include <vector>

namespace seer::texture {

namespace {

/** WHAT THE TEXTURE'S BYTES MEAN, in the vocabulary the encoder reads
 *  pixels in. The sRGB spellings are the same bytes as their plain twins
 *  — the difference is how a sampler reads them, and nothing here
 *  samples — so both arrive as the same colour type. Nothing when the
 *  format is not four eight-bit channels, which is the only shape a
 *  frame comes over this protocol in. */
std::optional<SkColorType> colorType(MTLPixelFormat format) {
  switch (format) {
    case MTLPixelFormatBGRA8Unorm:
    case MTLPixelFormatBGRA8Unorm_sRGB:
      return kBGRA_8888_SkColorType;
    case MTLPixelFormatRGBA8Unorm:
    case MTLPixelFormatRGBA8Unorm_sRGB:
      return kRGBA_8888_SkColorType;
    default:
      return std::nullopt;
  }
}

}  // namespace

bool writeTexturePng(id<MTLTexture> texture, id<MTLCommandQueue> queue,
                     const std::filesystem::path &path, Rows held) {
  if (!texture || !queue) return false;
  const std::optional<SkColorType> type = colorType(texture.pixelFormat);
  if (!type) {
    std::fprintf(stderr,
                 "the frame is in Metal pixel format %lu, which is not four "
                 "eight-bit channels and cannot be written as a PNG\n",
                 (unsigned long)texture.pixelFormat);
    return false;
  }
  const NSUInteger width = texture.width;
  const NSUInteger height = texture.height;
  const NSUInteger rowBytes = width * 4;

  // A BUFFER, NOT A SECOND TEXTURE: a blit into shared storage is
  // readable on every Mac without asking which side of the bus the
  // texture's own memory is on.
  id<MTLBuffer> readback = [queue.device newBufferWithLength:rowBytes * height
                                                     options:MTLResourceStorageModeShared];
  if (!readback) return false;
  id<MTLCommandBuffer> commands = [queue commandBuffer];
  id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
  [blit copyFromTexture:texture
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(width, height, 1)
                      toBuffer:readback
             destinationOffset:0
        destinationBytesPerRow:rowBytes
      destinationBytesPerImage:rowBytes * height];
  [blit endEncoding];
  [commands commit];
  // The pixels are read on this thread the moment the copy is done, so the
  // wait is the point rather than a stall to be hidden.
  [commands waitUntilCompleted];
  if (commands.status != MTLCommandBufferStatusCompleted) {
    std::fprintf(stderr, "the frame could not be copied off the GPU\n");
    return false;
  }

  // A PNG IS WRITTEN TOP FIRST, so a frame that arrived the other way up
  // is walked backwards into a buffer of its own. It is the whole turn:
  // a row is the same bytes wherever it stands, so nothing is resampled
  // and no channel moves.
  std::vector<std::byte> turned;
  const void *rows = readback.contents;
  if (held == Rows::BottomFirst) {
    turned.resize(rowBytes * height);
    const auto *source = static_cast<const std::byte *>(readback.contents);
    for (NSUInteger row = 0; row < height; ++row)
      std::memcpy(turned.data() + row * rowBytes,
                  source + (height - 1 - row) * rowBytes, rowBytes);
    rows = turned.data();
  }

  // THE ALPHA IS PREMULTIPLIED, because that is how a canvas draws and
  // nothing between there and here has divided it out. The PNG encoder
  // takes it from here.
  const SkImageInfo info = SkImageInfo::Make((int)width, (int)height, *type, kPremul_SkAlphaType);
  const SkPixmap pixels(info, rows, rowBytes);
  const sk_sp<SkData> png = sigil::image::encodeImage(pixels, sigil::image::Format::Png);
  if (!png) {
    std::fprintf(stderr, "the frame could not be encoded as a PNG\n");
    return false;
  }
  if (!sigil::io::writeBytes(path, png->data(), png->size())) {
    std::fprintf(stderr, "%s could not be written\n", path.c_str());
    return false;
  }
  return true;
}

}  // namespace seer::texture
