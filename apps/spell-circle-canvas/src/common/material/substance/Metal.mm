/** @file
 * The Metal engine: its library opened once with the device and queue
 * every Metal cook shares, and each result it leaves on that device
 * copied there into a texture this library owns — on the same queue, so
 * the copy runs after the engine's own work — and carried as a frame
 * whose binding wraps it for a recorder, reading it back into host
 * memory only for a caller drawing with none. A grey result is spread
 * to four channels on the way, because a one-channel texture is sampled
 * as red alone.
 */

#import <Metal/Metal.h>

// The concrete texture a Metal engine hands back is declared by the
// SDK's headers for the platform named here; this is the one source that
// reads it.
#define SUBSTANCE_PLATFORM_METAL 1
#include <substance/framework/platform.h>

#include "Metal.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilskia/graphite/TextureImage.h>

#include <dlfcn.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <utility>

namespace sigil::material::sbsar::detail {

namespace {

std::atomic<uint64_t> readbackCount{0};

sk_sp<SkColorSpace> colorSpaceOf(Encoding encoding) {
  switch (encoding) {
    case Encoding::Srgb:
      return SkColorSpace::MakeSRGB();
    case Encoding::Linear:
      return SkColorSpace::MakeSRGBLinear();
    case Encoding::Raw:
      break;
  }
  // Data carries no colour space, so nothing converts it on the way to a
  // surface: a normal or a roughness arrives as the numbers it was cooked.
  return nullptr;
}

/** The host-memory colour type a texture of @p format reads back as;
 *  unknown for a format the cook never asks for. */
SkColorType colorTypeOf(MTLPixelFormat format) {
  switch (format) {
    case MTLPixelFormatRGBA8Unorm:
    case MTLPixelFormatRGBA8Unorm_sRGB:
      return kRGBA_8888_SkColorType;
    case MTLPixelFormatBGRA8Unorm:
    case MTLPixelFormatBGRA8Unorm_sRGB:
      return kBGRA_8888_SkColorType;
    case MTLPixelFormatRGBA16Unorm:
      return kR16G16B16A16_unorm_SkColorType;
    case MTLPixelFormatRGBA16Float:
      return kRGBA_F16_SkColorType;
    case MTLPixelFormatRGBA32Float:
      return kRGBA_F32_SkColorType;
    default:
      return kUnknown_SkColorType;
  }
}

/** The four-channel format a grey result of @p format is spread into;
 *  invalid for a format that is not grey. */
MTLPixelFormat spreadFormatOf(MTLPixelFormat format) {
  switch (format) {
    case MTLPixelFormatR8Unorm:
      return MTLPixelFormatRGBA8Unorm;
    case MTLPixelFormatR16Unorm:
      return MTLPixelFormatRGBA16Unorm;
    case MTLPixelFormatR16Float:
      return MTLPixelFormatRGBA16Float;
    case MTLPixelFormatR32Float:
      return MTLPixelFormatRGBA32Float;
    default:
      return MTLPixelFormatInvalid;
  }
}

/** The kernel that writes a grey texture's one channel into the first
 *  three of a colour texture, opaque. The engine does not honour a
 *  request for four channels from an output authored as grey, so the
 *  spread happens here. */
constexpr const char* kSpreadGrey = R"(
#include <metal_stdlib>
using namespace metal;
kernel void spreadGrey(texture2d<float, access::read> grey [[texture(0)]],
                       texture2d<float, access::write> colour [[texture(1)]],
                       uint2 at [[thread_position_in_grid]]) {
  if (at.x >= colour.get_width() || at.y >= colour.get_height()) return;
  const float value = grey.read(at).r;
  colour.write(float4(value, value, value, 1.0), at);
}
)";

/** The spread kernel built once on @p device; nil when it does not build. */
id<MTLComputePipelineState> spreadPipeline(id<MTLDevice> device) {
  static id<MTLComputePipelineState> built = [&]() -> id<MTLComputePipelineState> {
    NSError* error = nil;
    id<MTLLibrary> library =
        [device newLibraryWithSource:@(kSpreadGrey) options:nil error:&error];
    if (!library) return nil;
    id<MTLFunction> function = [library newFunctionWithName:@"spreadGrey"];
    if (!function) return nil;
    return [device newComputePipelineStateWithFunction:function error:&error];
  }();
  return built;
}

/** ONE COOKED OUTPUT, STANDING ON THE DEVICE: wrapped once per recorder
 *  that asks, read back once for every caller with none. */
class CookedBinding final : public media::DeviceBinding {
 public:
  CookedBinding(id<MTLTexture> texture, id<MTLCommandQueue> queue,
                Encoding encoding)
      : m_texture(texture), m_queue(queue), m_encoding(encoding) {}

  sk_sp<SkImage> image(skgpu::graphite::Recorder* recorder) override {
    std::lock_guard lock(m_mutex);
    if (recorder) {
      if (m_recorder != recorder || !m_wrapped) {
        m_recorder = recorder;
        m_wrapped = skia::wrapImage(*recorder, (__bridge void*)m_texture,
                                    kUnpremul_SkAlphaType,
                                    colorSpaceOf(m_encoding));
      }
      return m_wrapped;
    }
    if (!m_read) m_read = readBack();
    return m_read;
  }

 private:
  /** The texture's pixels in host memory, through a buffer the device
   *  copies into; the calling thread waits for the copy. */
  sk_sp<SkImage> readBack() const {
    const SkColorType type = colorTypeOf(m_texture.pixelFormat);
    if (type == kUnknown_SkColorType) return nullptr;
    const int width = (int)m_texture.width, height = (int)m_texture.height;
    const size_t pixelBytes = SkColorTypeBytesPerPixel(type);
    const size_t rowBytes = (size_t)width * pixelBytes;
    id<MTLBuffer> buffer =
        [m_texture.device newBufferWithLength:rowBytes * (size_t)height
                                      options:MTLResourceStorageModeShared];
    if (!buffer) return nullptr;
    id<MTLCommandBuffer> commands = [m_queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
    [blit copyFromTexture:m_texture
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake((NSUInteger)width,
                                             (NSUInteger)height, 1)
                        toBuffer:buffer
               destinationOffset:0
          destinationBytesPerRow:rowBytes
        destinationBytesPerImage:rowBytes * (size_t)height];
    [blit endEncoding];
    [commands commit];
    [commands waitUntilCompleted];
    if (commands.status != MTLCommandBufferStatusCompleted) return nullptr;
    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(SkImageInfo::Make(width, height, type,
                                                 kUnpremul_SkAlphaType,
                                                 colorSpaceOf(m_encoding))))
      return nullptr;
    for (int row = 0; row < height; ++row)
      std::memcpy(bitmap.getAddr(0, row),
                  static_cast<const uint8_t*>(buffer.contents) +
                      rowBytes * (size_t)row,
                  rowBytes);
    bitmap.setImmutable();
    readbackCount.fetch_add(1);
    return bitmap.asImage();
  }

  std::mutex m_mutex;
  id<MTLTexture> m_texture;
  id<MTLCommandQueue> m_queue;
  Encoding m_encoding;
  skgpu::graphite::Recorder* m_recorder = nullptr;
  sk_sp<SkImage> m_wrapped;
  sk_sp<SkImage> m_read;
};

}  // namespace

const MetalEngine* metalEngine() {
  static const MetalEngine* const opened = []() -> const MetalEngine* {
    void* library = dlopen(SIGIL_SUBSTANCE_METAL_ENGINE, RTLD_NOW | RTLD_LOCAL);
    if (!library) return nullptr;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) return nullptr;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (!queue) return nullptr;
    // Held for the process: every Metal renderer and every cooked texture
    // names this device and queue, and the renderer uses the library for
    // as long as it lives.
    auto* engine = new MetalEngine;
    engine->library = library;
    engine->device = (__bridge_retained void*)device;
    engine->queue = (__bridge_retained void*)queue;
    return engine;
  }();
  return opened;
}

bool isMetalResult(const SubstanceAir::TextureAgnostic& texture) {
  return SubstanceAir::isPlatformMatch(texture);
}

media::DeviceFrame deviceFrameOf(const SubstanceAir::TextureAgnostic& texture,
                                 Encoding encoding) {
  const MetalEngine* engine = metalEngine();
  if (!engine || !SubstanceAir::isPlatformMatch(texture)) return {};
  const SubstanceTexture& cooked = SubstanceAir::castToConcrete(texture);
  id<MTLTexture> source = (__bridge id<MTLTexture>)cooked.texture;
  if (!source) return {};
  id<MTLDevice> device = (__bridge id<MTLDevice>)engine->device;
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)engine->queue;

  const MTLPixelFormat spread = spreadFormatOf(source.pixelFormat);
  const bool grey = spread != MTLPixelFormatInvalid;
  id<MTLComputePipelineState> spreading = grey ? spreadPipeline(device) : nil;
  if (grey && !spreading) return {};
  MTLTextureDescriptor* describe = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:grey ? spread : source.pixelFormat
                                   width:source.width
                                  height:source.height
                               mipmapped:NO];
  describe.storageMode = MTLStorageModePrivate;
  describe.usage = grey ? (MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite)
                        : MTLTextureUsageShaderRead;
  id<MTLTexture> owned = [device newTextureWithDescriptor:describe];
  if (!owned) return {};
  // On the engine's own queue, so the copy runs after the work that wrote
  // the result, and waited for here on the engine's thread so the result
  // can be released the moment this returns.
  id<MTLCommandBuffer> commands = [queue commandBuffer];
  if (grey) {
    id<MTLComputeCommandEncoder> compute = [commands computeCommandEncoder];
    [compute setComputePipelineState:spreading];
    [compute setTexture:source atIndex:0];
    [compute setTexture:owned atIndex:1];
    const MTLSize group = MTLSizeMake(16, 16, 1);
    const MTLSize groups = MTLSizeMake((source.width + 15) / 16,
                                       (source.height + 15) / 16, 1);
    [compute dispatchThreadgroups:groups threadsPerThreadgroup:group];
    [compute endEncoding];
  } else {
    id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
    [blit copyFromTexture:source
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(source.width, source.height, 1)
                toTexture:owned
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
  }
  [commands commit];
  [commands waitUntilCompleted];
  if (commands.status != MTLCommandBufferStatusCompleted) return {};

  media::DeviceFrame frame;
  frame.kind = media::DeviceFrame::Kind::Texture;
  frame.storage = std::shared_ptr<void>((__bridge_retained void*)owned,
                                        [](void* held) { CFRelease(held); });
  frame.device = engine->device;
  frame.pointer = frame.storage.get();
  frame.format = (uint32_t)owned.pixelFormat;
  frame.width = (int)owned.width;
  frame.height = (int)owned.height;
  frame.binding = std::make_shared<CookedBinding>(owned, queue, encoding);
  return frame;
}

namespace {

/** The engine's word for the layout of a texture of @p format; the 8-bit
 *  word, which the engine reads most formats as, for any other. */
unsigned inputFormatOf(MTLPixelFormat format) {
  switch (format) {
    case MTLPixelFormatRGBA16Unorm:
      return Substance_PF_RGBA | Substance_PF_16I;
    case MTLPixelFormatRGBA16Float:
      return Substance_PF_RGBA | Substance_PF_16F;
    case MTLPixelFormatRGBA32Float:
      return Substance_PF_RGBA | Substance_PF_32F;
    default:
      return Substance_PF_RGBA;
  }
}

/** @p image uploaded into a texture of its own on @p device, as 8-bit
 *  RGBA with straight alpha — what the CPU engine's inputs are too. */
id<MTLTexture> uploaded(id<MTLDevice> device, const sk_sp<SkImage>& image) {
  const int width = image->width(), height = image->height();
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(SkImageInfo::Make(width, height,
                                               kRGBA_8888_SkColorType,
                                               kUnpremul_SkAlphaType)))
    return nil;
  if (!image->readPixels(nullptr, bitmap.pixmap(), 0, 0)) return nil;
  MTLTextureDescriptor* describe = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                   width:(NSUInteger)width
                                  height:(NSUInteger)height
                               mipmapped:NO];
  describe.storageMode = MTLStorageModeShared;
  describe.usage = MTLTextureUsageShaderRead;
  id<MTLTexture> texture = [device newTextureWithDescriptor:describe];
  if (!texture) return nil;
  [texture replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)width,
                                         (NSUInteger)height)
             mipmapLevel:0
               withBytes:bitmap.getPixels()
             bytesPerRow:bitmap.rowBytes()];
  return texture;
}

}  // namespace

MetalInput metalInputOf(const media::Frame& frame) {
  const MetalEngine* engine = metalEngine();
  if (!engine) return {};
  id<MTLDevice> device = (__bridge id<MTLDevice>)engine->device;
  MetalInput input;
  id<MTLTexture> texture = nil;
  if (frame.device.kind == media::DeviceFrame::Kind::Texture &&
      frame.device.device == engine->device && frame.device.pointer) {
    // Already on this device: named where it stands, and held by the
    // frame's own storage.
    texture = (__bridge id<MTLTexture>)frame.device.pointer;
    input.texture = frame.device.storage;
  } else {
    const sk_sp<SkImage> image = media::deviceImage(frame, nullptr);
    if (!image) return {};
    texture = uploaded(device, image);
    if (!texture) return {};
    input.texture = std::shared_ptr<void>((__bridge_retained void*)texture,
                                          [](void* held) { CFRelease(held); });
  }
  SubstanceTextureInput described = {};
  described.mTexture.texture = (__bridge void*)texture;
  described.mTexture.format = (unsigned int)texture.pixelFormat;
  described.mTexture.level0Width = (unsigned short)texture.width;
  described.mTexture.level0Height = (unsigned short)texture.height;
  described.mTexture.mipmapCount = 1;
  described.level0Width = (unsigned int)texture.width;
  described.level0Height = (unsigned int)texture.height;
  described.pixelFormat = (unsigned char)inputFormatOf(texture.pixelFormat);
  described.mipmapCount = 1;
  SubstanceAir::TextureInputAgnostic agnostic;
  static_assert(sizeof(described) <= sizeof(agnostic.textureInputAny));
  std::memcpy(agnostic.textureInputAny, &described, sizeof(described));
  agnostic.platform = (SubstanceEngineIDEnum)SUBSTANCE_API_PLATFORM;
  input.image = SubstanceAir::InputImage::create(agnostic);
  if (!input.image) return {};
  return input;
}

uint64_t metalReadbacks() { return readbackCount.load(); }

}  // namespace sigil::material::sbsar::detail
