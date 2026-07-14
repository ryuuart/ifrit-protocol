#pragma once
#include <memory>
#include <string>

// Forward-declared Qt RHI types + pimpl keep ObjC out of this header so it's
// includable from plain C++.
class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;
struct SyphonBridgePrivate;

/**
 * C++ wrapper around SyphonMetalServer that publishes Metal textures to the
 * Syphon inter-application video-sharing protocol on macOS.
 */
class SyphonBridge {
public:
  /** @p name is the Syphon server name visible to client applications. */
  explicit SyphonBridge(std::string name);
  ~SyphonBridge();

  /** Initializes the SyphonMetalServer using the Metal device from @p rhi. */
  void start(QRhi *rhi);

  /**
   * Appends a Syphon blit to the still-open @p commandBuffer for the given
   * texture region. No-op when no Syphon clients are connected.
   */
  void publishFrame(QRhiTexture *texture, QRhiCommandBuffer *commandBuffer,
                    int width, int height);

  /** Stops and releases the SyphonMetalServer. */
  void stop();

private:
  std::string m_name;
  std::unique_ptr<SyphonBridgePrivate> m_private;
};
