#include "TexturePublisher.h"

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <cstdint>
#include <utility>

namespace ifrit::qt {

std::unique_ptr<sigil::io::publish::Publisher> createPublisher(
    QRhi* rhi, std::string name) {
  if (!rhi) return nullptr;
#if defined(Q_OS_MACOS)
  if (rhi->backend() == QRhi::Metal) {
    const auto* handles =
        static_cast<const QRhiMetalNativeHandles*>(rhi->nativeHandles());
    return sigil::io::publish::createPublisher(
        std::move(name), sigil::io::publish::Backend::Metal,
        handles ? handles->dev : nullptr);
  }
#elif defined(Q_OS_WIN)
  if (rhi->backend() == QRhi::D3D11) {
    const auto* handles =
        static_cast<const QRhiD3D11NativeHandles*>(rhi->nativeHandles());
    return sigil::io::publish::createPublisher(
        std::move(name), sigil::io::publish::Backend::Direct3D11,
        handles ? handles->dev : nullptr);
  }
#endif
  return nullptr;
}

void publishFrame(sigil::io::publish::Publisher& publisher,
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
  publisher.publishFrame(nativeTexture, nativeBuffer, size.width(),
                         size.height());
}

}  // namespace ifrit::qt
