#pragma once
// Internal to SigilScry — the Metal implementation of WebGpuDriver.

#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <memory>

#include "../WebGpuDriver.h"

namespace sigil::scry {

/**
 * Executes Ultralight's GPU command lists on the Metal device and queue
 * behind a GpuDevice (the host's own, so all GPU work rides one queue
 * and is implicitly ordered), using the SDK's stock Metal shaders. Views
 * render into driver-owned MTLTextures; publish/slot textures are
 * imported into the device borrowed and handed out as handles, and the
 * Skia interop records on the web thread's own recorder over the
 * shared Graphite context.
 */
class UltralightMetalDriver final : public WebGpuDriver {
 public:
  /** @p device must be Metal and, with @p graphite, outlive the driver.
   *  Null when pipeline-state creation fails (broken shader compile). */
  static std::unique_ptr<UltralightMetalDriver> create(
      sigil::skia::GpuDevice& device, sigil::skia::GraphiteContext& graphite);

  // Defined where State is complete.
  // NOLINTNEXTLINE(performance-trivially-destructible)
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
                          const ultralight::RenderBuffer& buffer) override;
  void DestroyRenderBuffer(uint32_t renderBufferId) override;
  uint32_t NextGeometryId() override;
  void CreateGeometry(uint32_t geometryId,
                      const ultralight::VertexBuffer& vertices,
                      const ultralight::IndexBuffer& indices) override;
  void UpdateGeometry(uint32_t geometryId,
                      const ultralight::VertexBuffer& vertices,
                      const ultralight::IndexBuffer& indices) override;
  void DestroyGeometry(uint32_t geometryId) override;
  void UpdateCommandList(const ultralight::CommandList& list) override;

  // WebGpuDriver
  std::unordered_set<uint32_t> flush() override;
  sigil::skia::TextureHandle createPublishTexture(int width,
                                                  int height) override;
  sigil::skia::TextureHandle createImageTexture(int width, int height) override;
  void releaseTexture(sigil::skia::TextureHandle texture) override;
  void copyTexture(uint32_t srcTextureId, sigil::skia::TextureHandle dst,
                   int width, int height) override;
  bool copyDeviceTexture(sigil::skia::TextureHandle src,
                         sigil::skia::TextureHandle dst, int width,
                         int height) override;
  uint32_t registerExternalTexture(sigil::skia::TextureHandle texture) override;
  void unregisterExternalTexture(uint32_t textureId) override;
  void uploadToTexture(sigil::skia::TextureHandle texture, const void* pixels,
                       int width, int height, size_t rowBytes) override;
  bool paintTexture(sigil::skia::TextureHandle texture, int width, int height,
                    const std::function<void(SkCanvas&)>& painter) override;
  sk_sp<SkImage> wrapTexture(skgpu::graphite::Recorder* recorder,
                             sigil::skia::TextureHandle texture, int width,
                             int height) override;

 private:
  struct State;
  explicit UltralightMetalDriver(std::unique_ptr<State> state);

  std::unique_ptr<State> m_state;
};

}  // namespace sigil::scry
