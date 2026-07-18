// The one place allowed to pick a TexturePublisher implementation from
// QRhi::backend(): Syphon on Metal (macOS), Spout on D3D11 (Windows, when
// the SDK is present), null everywhere else — callers already treat the
// publisher as optional.
//
// Windows note: publishing currently needs QRhi on D3D11 (Spout) while the
// Skia Graphite text backend needs Vulkan, so a single process gets one or
// the other — pick per run with QSG_RHI_BACKEND — until a D3D11↔Vulkan
// shared-texture path lands.

#include "TexturePublisher.h"

#include <rhi/qrhi.h>

#if defined(Q_OS_MACOS)
#include "SyphonBridge.h"
#elif defined(SPELLCIRCLE_HAVE_SPOUT)
#include "SpoutBridge.h"
#endif

std::unique_ptr<TexturePublisher> createTexturePublisher(QRhi *rhi,
                                                         std::string name) {
  if (!rhi)
    return nullptr;
#if defined(Q_OS_MACOS)
  if (rhi->backend() == QRhi::Metal) {
    auto bridge = std::make_unique<SyphonBridge>(std::move(name));
    bridge->start(rhi);
    return bridge;
  }
#elif defined(SPELLCIRCLE_HAVE_SPOUT)
  if (rhi->backend() == QRhi::D3D11) {
    auto bridge = std::make_unique<SpoutBridge>(std::move(name));
    bridge->start(rhi);
    return bridge;
  }
#endif
  static_cast<void>(name);
  return nullptr;
}
