#pragma once
// Internal to IfritWeb — the Metal implementation of WebGpuDriver.

#include "../WebGpuDriver.h"

#include <memory>

namespace ifrit::web {

/**
 * Executes Ultralight's GPU command lists on an existing Metal device and
 * command queue (the same pair the host's Graphite context is built on,
 * so all GPU work rides one queue and is implicitly ordered), using the
 * SDK's stock Metal shaders. Views render into driver-owned MTLTextures;
 * publish/slot textures and the Skia interop follow the WebGpuDriver
 * contract, with native handles being id<MTLTexture> bridged to void*.
 */
class UltralightMetalDriver final : public WebGpuDriver {
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

  // WebGpuDriver
  std::unordered_set<uint32_t> flush() override;
  void *createPublishTexture(int width, int height) override;
  void *createImageTexture(int width, int height) override;
  void releaseNativeTexture(void *texture) override;
  void copyTexture(uint32_t srcTextureId, void *dstTexture, int width,
                   int height) override;
  void copyNativeTexture(void *srcTexture, void *dstTexture, int width,
                         int height) override;
  uint32_t registerExternalTexture(void *texture) override;
  void unregisterExternalTexture(uint32_t textureId) override;
  void uploadToTexture(void *texture, const void *pixels, int width,
                       int height, size_t rowBytes) override;
  bool paintTexture(void *texture, int width, int height,
                    const std::function<void(SkCanvas &)> &painter) override;
  sk_sp<SkImage> wrapTexture(skgpu::graphite::Recorder *recorder,
                             void *texture, int width, int height) override;

private:
  struct State;
  explicit UltralightMetalDriver(std::unique_ptr<State> state);

  std::unique_ptr<State> m_state;
};

} // namespace ifrit::web
