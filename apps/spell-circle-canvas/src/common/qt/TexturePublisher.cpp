#include "TexturePublisher.h"

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <cstdint>
#include <utility>

namespace ifrit::qt {

sigil::io::frames::Publisher createPublisher(sigil::io::Hub& hub, QRhi* rhi,
                                             std::string_view name) {
  if (!rhi) return {};
#if defined(Q_OS_MACOS)
  if (rhi->backend() == QRhi::Metal) {
    const auto* handles =
        static_cast<const QRhiMetalNativeHandles*>(rhi->nativeHandles());
    if (!handles || !handles->dev) return {};
    return hub.publish(
        "syphon://" + std::string(name),
        {.device = {.api = sigil::io::frames::GraphicsApi::Metal,
                    .handle = handles->dev}});
  }
#elif defined(Q_OS_WIN)
  if (rhi->backend() == QRhi::D3D11) {
    const auto* handles =
        static_cast<const QRhiD3D11NativeHandles*>(rhi->nativeHandles());
    if (!handles || !handles->dev) return {};
    return hub.publish(
        "spout://" + std::string(name),
        {.device = {.api = sigil::io::frames::GraphicsApi::Direct3D11,
                    .handle = handles->dev}});
  }
#endif
  return {};
}

void publishFrame(sigil::io::frames::Publisher& publisher,
                  QRhiTexture* texture, QRhiCommandBuffer* commandBuffer,
                  QSize size) {
  if (!texture || size.isEmpty()) return;
  void* nativeBuffer = nullptr;
#if defined(Q_OS_MACOS)
  if (!commandBuffer) return;
  const auto* handles = static_cast<const QRhiMetalCommandBufferNativeHandles*>(
      commandBuffer->nativeHandles());
  nativeBuffer = handles ? handles->commandBuffer : nullptr;
#else
  (void)commandBuffer;
#endif
  // QRhi carries the backend's native texture pointer in an integer.
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  void* nativeTexture = reinterpret_cast<void*>(
      static_cast<uintptr_t>(texture->nativeTexture().object));
  publisher.send({.texture = nativeTexture,
                  .commandBuffer = nativeBuffer,
                  .width = size.width(),
                  .height = size.height()});
}

}  // namespace ifrit::qt
