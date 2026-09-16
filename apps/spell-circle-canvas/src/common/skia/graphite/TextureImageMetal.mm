// Metal arm of the image wrap: an id<MTLTexture> as an SkImage on a
// Metal recorder, so a draw samples the texture where it stands — and,
// beside it, the read that copies one into host memory for a drawing
// that stands somewhere else.

#import <Metal/Metal.h>

#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSize.h>
#include <include/gpu/GpuTypes.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Image.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/YUVABackendTextures.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>
#include <sigilskia/graphite/TextureImage.h>

#include <array>
#include <mutex>
#include <optional>
#include <utility>

namespace sigil::skia {

namespace {

/** WHAT THE TEXTURE'S BYTES MEAN, in the vocabulary an image is made in.
 *  The sRGB spellings are the same bytes as their plain twins — the
 *  difference is how a sampler reads them, and the colour space the
 *  caller names is what says that here — so both arrive as the same
 *  colour type. Nothing for a format that is not four eight-bit
 *  channels. */
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

/** THE QUEUE A READ RUNS ON: one for @p device, made on the first read
 *  from it, because a queue made per read would be a queue allocated per
 *  read.
 *
 *  IT IS KEPT FOR THE PROCESS AND NEVER RELEASED, the way the device it
 *  stands on is: a read on another thread may still be submitting to the
 *  queue a device change would replace, and a machine has as many
 *  devices as it has — which is not a count that grows. Only the pair is
 *  behind the lock; the queue orders the work submitted to it itself. */
id<MTLCommandQueue> readQueue(id<MTLDevice> device) {
  static std::mutex mutex;
  static id<MTLDevice> lastDevice = nil;
  static id<MTLCommandQueue> queue = nil;
  const std::lock_guard<std::mutex> lock(mutex);
  if (device != lastDevice) {
    lastDevice = [device retain];
    queue = [device newCommandQueue];
  }
  return queue;
}

}  // namespace

sk_sp<SkImage> wrapImage(skgpu::graphite::Recorder &recorder, void *mtlTexture, int width,
                         int height, SkAlphaType alphaType, sk_sp<SkColorSpace> colorSpace) {
  if (recorder.backend() != skgpu::BackendApi::kMetal || !mtlTexture || width <= 0 || height <= 0)
    return nullptr;

  // RETAINED FOR THE IMAGE'S LIFE, and released by the wrap's own release
  // proc: an image is sampled at draw time and submitted later, and the
  // view or the frame that owned the texture may have resized or gone by
  // then. Without the retain the draw would reach whatever now holds the
  // slot.
  CFTypeRef retained = CFRetain(static_cast<CFTypeRef>(mtlTexture));
  const skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeMetal(SkISize::Make(width, height), retained);
  sk_sp<SkImage> image = SkImages::WrapTexture(
      &recorder, backendTexture, alphaType, std::move(colorSpace),
      [](void *context) { CFRelease(static_cast<CFTypeRef>(context)); },
      const_cast<void *>(static_cast<const void *>(retained)));
  // THE RELEASE PROC IS THE ONLY RELEASE once the wrap has the context: it
  // is bound before the wrap validates anything, and every failing return
  // out of the wrap runs it. A release here as well would free the texture
  // twice.
  return image;
}

sk_sp<SkImage> wrapImage(skgpu::graphite::Recorder &recorder, void *mtlTexture,
                         SkAlphaType alphaType, sk_sp<SkColorSpace> colorSpace) {
  id<MTLTexture> texture = (__bridge id<MTLTexture>)mtlTexture;
  return wrapImage(recorder, mtlTexture, (int)texture.width, (int)texture.height, alphaType,
                   std::move(colorSpace));
}

sk_sp<SkImage> readImage(void *mtlTexture, SkAlphaType alphaType, sk_sp<SkColorSpace> colorSpace) {
  if (!mtlTexture) return nullptr;
  id<MTLTexture> texture = (__bridge id<MTLTexture>)mtlTexture;
  const std::optional<SkColorType> type = colorType(texture.pixelFormat);
  if (!type) return nullptr;
  id<MTLCommandQueue> queue = readQueue(texture.device);
  if (!queue) return nullptr;

  const NSUInteger width = texture.width;
  const NSUInteger height = texture.height;
  const NSUInteger rowBytes = width * 4;
  if (width == 0 || height == 0) return nullptr;

  // A BUFFER, NOT A SECOND TEXTURE: a copy into shared storage is
  // readable on every Mac without asking which side of the bus the
  // texture's own memory is on.
  id<MTLBuffer> pixels = [queue.device newBufferWithLength:rowBytes * height
                                                   options:MTLResourceStorageModeShared];
  if (!pixels) return nullptr;
  id<MTLCommandBuffer> commands = [queue commandBuffer];
  id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
  [blit copyFromTexture:texture
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(width, height, 1)
                      toBuffer:pixels
             destinationOffset:0
        destinationBytesPerRow:rowBytes
      destinationBytesPerImage:rowBytes * height];
  [blit endEncoding];
  [commands commit];
  // The bytes are read on this thread the moment the copy is done, so the
  // wait is the point rather than a stall to be hidden.
  [commands waitUntilCompleted];
  if (commands.status != MTLCommandBufferStatusCompleted) {
    [pixels release];
    return nullptr;
  }

  // THE IMAGE HOLDS THE BUFFER rather than copying out of it: the bytes
  // are already this process's, and a second copy of them would buy
  // nothing. The one reference this call owns is handed to the release
  // proc, which is what lets the buffer go once the last image naming it
  // has.
  sk_sp<SkData> bytes = SkData::MakeWithProc(
      pixels.contents, rowBytes * height,
      [](const void *, void *context) { [(id<MTLBuffer>)context release]; }, (void *)pixels);
  const SkImageInfo info =
      SkImageInfo::Make((int)width, (int)height, *type, alphaType, std::move(colorSpace));
  return SkImages::RasterFromData(info, std::move(bytes), rowBytes);
}

sk_sp<SkImage> wrapPlanarImage(skgpu::graphite::Recorder &recorder,
                               std::span<const TexturePlane> planes, const SkYUVAInfo &info,
                               sk_sp<SkColorSpace> colorSpace, TextureRelease release,
                               void *releaseContext) {
  // THE RELEASE RUNS ON EVERY PATH OUT: the caller handed its planes over
  // once, and a wrap that never happened must not leave them held. Only a
  // refusal made before the wrap is called may run it here — the wrap binds
  // the release to its own context before it validates, and runs it itself
  // on every failing return.
  const auto refuse = [&]() -> sk_sp<SkImage> {
    if (release) release(releaseContext);
    return nullptr;
  };
  if (recorder.backend() != skgpu::BackendApi::kMetal || planes.empty() ||
      planes.size() > SkYUVAInfo::kMaxPlanes)
    return refuse();

  std::array<skgpu::graphite::BackendTexture, SkYUVAInfo::kMaxPlanes> textures;
  for (size_t i = 0; i < planes.size(); ++i) {
    const TexturePlane &plane = planes[i];
    if (!plane.texture || plane.width <= 0 || plane.height <= 0) return refuse();
    textures[i] = skgpu::graphite::BackendTextures::MakeMetal(
        SkISize::Make(plane.width, plane.height), static_cast<CFTypeRef>(plane.texture));
  }

  const skgpu::graphite::YUVABackendTextures yuva(
      info, SkSpan<const skgpu::graphite::BackendTexture>(textures.data(), planes.size()));
  if (!yuva.isValid()) return refuse();

  sk_sp<SkImage> image = SkImages::TextureFromYUVATextures(&recorder, yuva, std::move(colorSpace),
                                                           release, releaseContext);
  return image;
}

}  // namespace sigil::skia
