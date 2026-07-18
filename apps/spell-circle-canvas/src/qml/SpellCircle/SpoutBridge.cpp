// Windows bring-up draft: compiled only when the Spout SDK is available
// (see CMakeLists.txt), untested until the Windows port lands. Verify the
// SpoutDX include layout and target name against the installed vcpkg
// spout2[dx] package at bring-up.

#include "SpoutBridge.h"

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <SpoutDX.h> // vcpkg spout2[dx] (target Spout2::SpoutDX)

#include <d3d11.h>

struct SpoutBridgePrivate {
  spoutDX sender;
  bool open = false;
};

SpoutBridge::SpoutBridge(std::string name)
    : m_name(std::move(name)),
      m_private(std::make_unique<SpoutBridgePrivate>()) {}

SpoutBridge::~SpoutBridge() { stop(); }

void SpoutBridge::start(QRhi *rhi) {
  if (!rhi || rhi->backend() != QRhi::D3D11)
    return;
  stop(); // restarting: never orphan a previous sender

  const auto *nativeHandles =
      static_cast<const QRhiD3D11NativeHandles *>(rhi->nativeHandles());
  auto *device = static_cast<ID3D11Device *>(nativeHandles->dev);
  if (!device)
    return;

  // SpoutDX shares textures created on the application's own device, so no
  // second D3D11 device (and no cross-device copy) is involved.
  m_private->sender.SetSenderName(m_name.c_str());
  m_private->open = m_private->sender.OpenDirectX11(device);
}

void SpoutBridge::publishFrame(QRhiTexture *texture,
                               QRhiCommandBuffer * /*commandBuffer*/,
                               int /*width*/, int /*height*/) {
  if (!m_private->open || !texture)
    return;

  // Qt packs the ID3D11Texture2D pointer into a quint64 on D3D11.
  auto *d3dTexture = reinterpret_cast<ID3D11Texture2D *>(
      static_cast<uintptr_t>(texture->nativeTexture().object));
  if (!d3dTexture)
    return;

  // SendTexture copies into the shared sender texture through the immediate
  // context and handles size changes by recreating the sender.
  m_private->sender.SendTexture(d3dTexture);
}

void SpoutBridge::stop() {
  if (!m_private->open)
    return;
  m_private->sender.ReleaseSender();
  m_private->sender.CloseDirectX11();
  m_private->open = false;
}
