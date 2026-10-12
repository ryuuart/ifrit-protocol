// The Metal backend of GpuDevice: textures as id<MTLTexture>, fences as
// MTLSharedEvent, both signalled and waited on through command buffers
// on the device's one queue. Compiles without ARC so the references the
// device holds are explicit CFRetain/CFRelease pairs, the same way the
// Graphite bring-up holds its device and queue.

#import <Metal/Metal.h>

#include <chrono>
#include <thread>

#include "DeviceBackend.h"

namespace sigil::core::hardware {

namespace {

MTLPixelFormat toMetal(TextureFormat format) {
  switch (format) {
    case TextureFormat::RGBA8Unorm:
      return MTLPixelFormatRGBA8Unorm;
    case TextureFormat::BGRA8Unorm:
      return MTLPixelFormatBGRA8Unorm;
    case TextureFormat::RGBA16Float:
      return MTLPixelFormatRGBA16Float;
  }
  return MTLPixelFormatRGBA8Unorm;
}

MTLTextureUsage toMetal(TextureUsage usage) {
  MTLTextureUsage out = 0;
  if (has(usage, TextureUsage::ShaderRead)) out |= MTLTextureUsageShaderRead;
  if (has(usage, TextureUsage::ShaderWrite)) out |= MTLTextureUsageShaderWrite;
  if (has(usage, TextureUsage::RenderTarget)) out |= MTLTextureUsageRenderTarget;
  return out;
}

class MetalBackend final : public GpuDevice::DeviceBackend {
 public:
  MetalBackend(const NativeDevice &native, bool owned) : m_native(native), m_owned(owned) {
    if (m_owned) {
      // Both objects came out of a `new`/`MTLCreate…` call at +1 and are
      // released in the destructor; adopted ones stay the host's.
    }
  }

  ~MetalBackend() override {
    if (m_owned) {
      CFRelease(static_cast<CFTypeRef>(m_native.mtlCommandQueue));
      CFRelease(static_cast<CFTypeRef>(m_native.mtlDevice));
    }
  }

  const NativeDevice &native() const override { return m_native; }

  NativeTexture createTexture(const TextureDescription &desc) override {
    const int levels = clampedMipLevels(desc);
    MTLTextureDescriptor *descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:toMetal(desc.format)
                                                           width:(NSUInteger)desc.width
                                                          height:(NSUInteger)desc.height
                                                       mipmapped:levels > 1];
    descriptor.mipmapLevelCount = (NSUInteger)levels;
    descriptor.usage = toMetal(desc.usage);
    descriptor.storageMode = desc.cpuAccessible ? MTLStorageModeShared : MTLStorageModePrivate;
    id<MTLTexture> texture = [device() newTextureWithDescriptor:descriptor];
    if (!texture) return {};
    if (desc.label) texture.label = [NSString stringWithUTF8String:desc.label];
    NativeTexture out;
    out.backend = Backend::Metal;
    out.mtlTexture = (void *)texture;  // +1 from newTexture…, owned by the device
    out.width = desc.width;
    out.height = desc.height;
    out.mipLevels = levels;
    return out;
  }

  // A Metal device names a Metal texture as it stands; it has no way to
  // reach a Vulkan image.
  NativeTexture importTexture(const NativeTexture &texture, bool *made) override {
    *made = false;
    if (texture.backend != Backend::Metal || !texture.mtlTexture) return {};
    return texture;
  }

  void retainTexture(const NativeTexture &texture) override {
    CFRetain(static_cast<CFTypeRef>(texture.mtlTexture));
  }

  void releaseTexture(const NativeTexture &texture) override {
    CFRelease(static_cast<CFTypeRef>(texture.mtlTexture));
  }

  void *createFence() override {
    id<MTLSharedEvent> event = [device() newSharedEvent];
    if (!event) return nullptr;
    event.signaledValue = kFenceInitialValue;
    return (void *)event;  // +1, released in destroyFence
  }

  // …and a Metal event as it stands, borrowed: the handle forgets it and
  // its maker releases it.
  void *importFence(const NativeFence &fence, bool *made) override {
    *made = false;
    if (fence.backend != Backend::Metal) return nullptr;
    return fence.mtlSharedEvent;
  }

  NativeFence exportFence(void *fence) override {
    NativeFence out;
    out.backend = Backend::Metal;
    out.mtlSharedEvent = fence;
    return out;
  }

  void destroyFence(void *fence) override { CFRelease(static_cast<CFTypeRef>(fence)); }

  void signalFence(void *fence, FenceValue value) override {
    id<MTLCommandBuffer> commands = [queue() commandBuffer];
    [commands encodeSignalEvent:event(fence) value:value];
    [commands commit];
  }

  void waitFenceGpu(void *fence, FenceValue value) override {
    id<MTLCommandBuffer> commands = [queue() commandBuffer];
    [commands encodeWaitForEvent:event(fence) value:value];
    [commands commit];
  }

  bool waitFenceCpu(void *fence, FenceValue value, std::chrono::milliseconds timeout) override {
    // Polled rather than listened for: a signal lands within microseconds
    // of the queue reaching it, and a poll has no listener queue to keep
    // alive across the device's lifetime.
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (event(fence).signaledValue < value) {
      if (std::chrono::steady_clock::now() >= deadline) return false;
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    return true;
  }

  FenceValue completedValue(void *fence) override { return event(fence).signaledValue; }

  // A queue runs its command buffers in order, so one empty buffer
  // finishing is everything before it finished.
  void waitIdle() override {
    id<MTLCommandBuffer> commands = [queue() commandBuffer];
    [commands commit];
    [commands waitUntilCompleted];
  }

 private:
  id<MTLDevice> device() const { return (id<MTLDevice>)m_native.mtlDevice; }
  id<MTLCommandQueue> queue() const { return (id<MTLCommandQueue>)m_native.mtlCommandQueue; }
  static id<MTLSharedEvent> event(void *fence) { return (id<MTLSharedEvent>)fence; }

  NativeDevice m_native;
  bool m_owned;
};

}  // namespace

bool readMetalTexture(void *mtlTexture, MetalTextureFacts *facts) {
  id<MTLTexture> texture = (id<MTLTexture>)mtlTexture;
  if (!texture || texture.textureType != MTLTextureType2D || texture.sampleCount != 1 ||
      texture.arrayLength != 1)
    return false;
  switch (texture.pixelFormat) {
    case MTLPixelFormatRGBA8Unorm:
      facts->format = TextureFormat::RGBA8Unorm;
      break;
    case MTLPixelFormatBGRA8Unorm:
      facts->format = TextureFormat::BGRA8Unorm;
      break;
    case MTLPixelFormatRGBA16Float:
      facts->format = TextureFormat::RGBA16Float;
      break;
    default:
      return false;
  }
  facts->mtlDevice = (void *)texture.device;
  facts->width = (int)texture.width;
  facts->height = (int)texture.height;
  facts->mipLevels = (int)texture.mipmapLevelCount;
  const MTLTextureUsage usage = texture.usage;
  // A usage of Unknown is a texture that may be used every way.
  const bool any = usage == MTLTextureUsageUnknown;
  TextureUsage out{};
  if (any || (usage & MTLTextureUsageShaderRead) != 0) out = out | TextureUsage::ShaderRead;
  if (any || (usage & MTLTextureUsageShaderWrite) != 0) out = out | TextureUsage::ShaderWrite;
  if (any || (usage & MTLTextureUsageRenderTarget) != 0) out = out | TextureUsage::RenderTarget;
  facts->usage = out;
  return true;
}

std::unique_ptr<GpuDevice::DeviceBackend> createMetalBackend(const NativeDevice &native,
                                                             bool owned) {
  NativeDevice resolved = native;
  resolved.backend = Backend::Metal;
  if (owned) {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) return nullptr;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (!queue) {
      CFRelease((CFTypeRef)device);
      return nullptr;
    }
    resolved.mtlDevice = (void *)device;
    resolved.mtlCommandQueue = (void *)queue;
  }
  return std::make_unique<MetalBackend>(resolved, owned);
}

}  // namespace sigil::core::hardware
