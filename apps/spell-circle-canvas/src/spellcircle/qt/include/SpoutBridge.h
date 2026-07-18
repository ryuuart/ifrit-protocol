#pragma once
#include "TexturePublisher.h"

#include <memory>
#include <string>

// Pimpl keeps the Spout SDK (and <d3d11.h>) out of this header so it's
// includable from plain C++ on any platform.
struct SpoutBridgePrivate;

/**
 * The Direct3D 11 TexturePublisher: wraps SpoutDX to publish D3D11 textures
 * over the Spout inter-application video-sharing protocol on Windows — the
 * SyphonBridge analogue. Construct through createTexturePublisher(), which
 * only selects this implementation when the QRhi backend is D3D11.
 *
 * Windows bring-up draft: only compiled when the Spout SDK is found (see
 * CMakeLists.txt), untested until the Windows port lands. Note the current
 * backend split there: Spout publishing needs QRhi on D3D11 while the Skia
 * Graphite text backend needs Vulkan, so one process gets one or the other
 * until a D3D11↔Vulkan shared-texture path lands.
 */
class SpoutBridge final : public TexturePublisher {
public:
  /** @p name is the Spout sender name visible to receiver applications. */
  explicit SpoutBridge(std::string name);
  ~SpoutBridge() override;

  /**
   * Initializes the SpoutDX sender on the D3D11 device from @p rhi. No-op
   * unless @p rhi's backend is D3D11. Restarting an already-running bridge
   * releases the previous sender first.
   */
  void start(QRhi *rhi);

  /**
   * Shares @p texture through the Spout sender. D3D11 has no caller-visible
   * command buffer — SpoutDX copies through the immediate context, ordered
   * after the work QRhi already recorded — so @p commandBuffer is unused.
   * No-op when stopped.
   */
  void publishFrame(QRhiTexture *texture, QRhiCommandBuffer *commandBuffer,
                    int width, int height) override;

  /** Releases the Spout sender. Safe to call repeatedly. */
  void stop();

private:
  std::string m_name;
  std::unique_ptr<SpoutBridgePrivate> m_private;
};
