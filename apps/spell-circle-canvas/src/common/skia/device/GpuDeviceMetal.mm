// The Metal backend of GpuDevice: textures as id<MTLTexture>, fences as
// MTLSharedEvent, both signalled and waited on through command buffers
// on the device's one queue. Compiles without ARC so the references the
// device holds are explicit CFRetain/CFRelease pairs, the same way the
// Graphite bring-up holds its device and queue.

#import <Metal/Metal.h>

#include <chrono>
#include <thread>

#include "DeviceBackend.h"

namespace sigil::skia {

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

class MetalBackend final : public GpuDevice::Backend_ {
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

  NativeTexture createTexture(const TextureDesc &desc) override {
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

 private:
  id<MTLDevice> device() const { return (id<MTLDevice>)m_native.mtlDevice; }
  id<MTLCommandQueue> queue() const { return (id<MTLCommandQueue>)m_native.mtlCommandQueue; }
  static id<MTLSharedEvent> event(void *fence) { return (id<MTLSharedEvent>)fence; }

  NativeDevice m_native;
  bool m_owned;
};

}  // namespace

std::unique_ptr<GpuDevice::Backend_> createMetalBackend(const NativeDevice &native, bool owned) {
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

}  // namespace sigil::skia
