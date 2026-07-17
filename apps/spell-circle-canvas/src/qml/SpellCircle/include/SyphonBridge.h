#pragma once
#include "TexturePublisher.h"

#include <memory>
#include <string>

// Forward-declared Qt RHI types + pimpl keep ObjC out of this header so it's
// includable from plain C++.
struct SyphonBridgePrivate;

/**
 * The Metal TexturePublisher: wraps SyphonMetalServer to publish Metal
 * textures over the Syphon inter-application video-sharing protocol on
 * macOS. Construct through createTexturePublisher(), which only selects
 * this implementation when the QRhi backend is Metal.
 */
class SyphonBridge final : public TexturePublisher {
public:
  /** @p name is the Syphon server name visible to client applications. */
  explicit SyphonBridge(std::string name);
  ~SyphonBridge() override;

  /**
   * Initializes the SyphonMetalServer using the Metal device from @p rhi.
   * No-op unless @p rhi's backend is Metal. Restarting an already-running
   * bridge stops (and releases) the previous server first.
   */
  void start(QRhi *rhi);

  /**
   * Appends a Syphon blit to the still-open @p commandBuffer for the given
   * texture region. No-op when stopped or no Syphon clients are connected.
   */
  void publishFrame(QRhiTexture *texture, QRhiCommandBuffer *commandBuffer,
                    int width, int height) override;

  /** Stops and releases the SyphonMetalServer. Safe to call repeatedly. */
  void stop();

private:
  std::string m_name;
  std::unique_ptr<SyphonBridgePrivate> m_private;
};
