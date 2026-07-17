#pragma once
#include <memory>
#include <string>

class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;

/**
 * Publishes rendered frames to an external inter-application video sink.
 *
 * Implementations are graphics-backend-specific: Syphon serves Metal on
 * macOS today, and a Spout implementation is the Direct3D analogue when a
 * Windows port lands. Obtain one through createTexturePublisher(), which
 * returns null on backends without a publisher — callers must treat the
 * publisher as optional and skip publishing when it is absent.
 */
class TexturePublisher {
public:
  virtual ~TexturePublisher() = default;

  /**
   * Appends the publish work for @p texture to the still-open
   * @p commandBuffer. No-op when no clients are connected.
   */
  virtual void publishFrame(QRhiTexture *texture,
                            QRhiCommandBuffer *commandBuffer, int width,
                            int height) = 0;
};

/**
 * Creates the publisher matching @p rhi's active backend: a Syphon server
 * named @p name when the backend is Metal, null otherwise. Never assumes a
 * backend — this factory is the only place allowed to inspect
 * QRhi::backend() on behalf of publishing.
 */
std::unique_ptr<TexturePublisher> createTexturePublisher(QRhi *rhi,
                                                         std::string name);
