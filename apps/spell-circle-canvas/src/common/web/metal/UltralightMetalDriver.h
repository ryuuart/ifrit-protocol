#pragma once
// Internal to IfritWeb — the Metal implementation of ultralight::GPUDriver.

#include <Ultralight/platform/GPUDriver.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_set>

class SkCanvas;

namespace ifrit::web {

/**
 * Executes Ultralight's GPU command lists on an existing Metal device and
 * command queue (the same pair the host's Graphite context is built on,
 * so all GPU work rides one queue and is implicitly ordered). Views render
 * into driver-owned MTLTextures; the engine publishes them by blit-copying
 * into per-view textures that consumers wrap as Graphite SkImages.
 *
 * All methods must be called on the web thread (Ultralight invokes the
 * GPUDriver callbacks during Renderer::Render()).
 *
 * Metal object parameters/returns are bridged to void* so the interface
 * stays includable from plain C++ TUs.
 */
class UltralightMetalDriver final : public ultralight::GPUDriver {
public:
  /** @p mtlDevice / @p mtlCommandQueue are id<MTLDevice> /
   *  id<MTLCommandQueue> bridged to void*; both are retained. Null when
   *  pipeline-state creation fails (broken shader compile). */
  static std::unique_ptr<UltralightMetalDriver> create(void *mtlDevice,
                                                       void *mtlCommandQueue);

  ~UltralightMetalDriver() override;

  // ultralight::GPUDriver
  void BeginSynchronize() override {}
  void EndSynchronize() override {}
  uint32_t NextTextureId() override;
  void CreateTexture(uint32_t textureId,
                     ultralight::RefPtr<ultralight::Bitmap> bitmap) override;
  void UpdateTexture(uint32_t textureId,
                     ultralight::RefPtr<ultralight::Bitmap> bitmap) override;
  void DestroyTexture(uint32_t textureId) override;
  uint32_t NextRenderBufferId() override;
  void CreateRenderBuffer(uint32_t renderBufferId,
                          const ultralight::RenderBuffer &buffer) override;
  void DestroyRenderBuffer(uint32_t renderBufferId) override;
  uint32_t NextGeometryId() override;
  void CreateGeometry(uint32_t geometryId,
                      const ultralight::VertexBuffer &vertices,
                      const ultralight::IndexBuffer &indices) override;
  void UpdateGeometry(uint32_t geometryId,
                      const ultralight::VertexBuffer &vertices,
                      const ultralight::IndexBuffer &indices) override;
  void DestroyGeometry(uint32_t geometryId) override;
  void UpdateCommandList(const ultralight::CommandList &list) override;

  /** Encodes and commits the pending command list. Returns the ids of the
   *  render buffers that were cleared or drawn to (the views to publish). */
  std::unordered_set<uint32_t> flush();

  /** Creates a private BGRA8 shader-readable texture for publishing.
   *  Returned retained (CFRetain'd id<MTLTexture>); free with
   *  releaseTexture(). */
  void *createPublishTexture(int width, int height);
  static void releaseTexture(void *mtlTexture);

  /** Blit-copies the top-left @p width x @p height region of the texture
   *  registered under @p srcTextureId into @p dstMtlTexture. */
  void copyTexture(uint32_t srcTextureId, void *dstMtlTexture, int width,
                   int height);

  /** Creates a BGRA8 texture usable both as a Graphite render target and
   *  as a sampled page image (shared storage, so CPU uploads work too).
   *  Returned retained; free with releaseTexture(). */
  void *createImageTexture(int width, int height);

  /** Registers @p mtlTexture under a fresh Ultralight texture id so page
   *  draw commands (e.g. ImageSource quads) can bind it. The driver
   *  retains the texture until unregisterExternalTexture(). */
  uint32_t registerExternalTexture(void *mtlTexture);
  void unregisterExternalTexture(uint32_t textureId);

  /** Copies raster pixels into a shared-storage texture (any thread). */
  static void uploadToTexture(void *mtlTexture, const void *pixels,
                              int width, int height, size_t rowBytes);

  /** Runs @p painter with an SkCanvas targeting @p mtlTexture through the
   *  driver's own Graphite context (same device/queue as everything
   *  else), then flushes the GPU work. Web thread only. Returns false if
   *  the wrap or flush fails. */
  bool paintTexture(void *mtlTexture, int width, int height,
                    const std::function<void(SkCanvas &)> &painter);

private:
  struct State;
  explicit UltralightMetalDriver(std::unique_ptr<State> state);

  std::unique_ptr<State> m_state;
};

} // namespace ifrit::web
