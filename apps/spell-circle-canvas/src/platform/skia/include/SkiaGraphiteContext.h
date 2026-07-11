#pragma once
#include <memory>

class QRhi;

namespace skgpu::graphite {
class Context;
class Recorder;
} // namespace skgpu::graphite

/**
 * Owns the Skia Graphite Metal Context + Recorder used to draw into
 * QCanvasOffscreenCanvas textures from SkCanvas. Built from the same
 * MTLDevice/MTLCommandQueue that Qt's QRhi already uses (see SyphonBridge for
 * the equivalent pattern), so Graphite's GPU work rides the same Metal
 * command queue as Qt Quick's own rendering.
 */
class SkiaGraphiteContext {
public:
  /** Returns null if @p rhi isn't backed by Metal or Context creation fails. */
  static std::unique_ptr<SkiaGraphiteContext> create(QRhi *rhi);

  ~SkiaGraphiteContext();

  skgpu::graphite::Context *context() const { return m_context.get(); }
  skgpu::graphite::Recorder *recorder() const { return m_recorder.get(); }

private:
  SkiaGraphiteContext(std::unique_ptr<skgpu::graphite::Context> context,
                      std::unique_ptr<skgpu::graphite::Recorder> recorder);

  std::unique_ptr<skgpu::graphite::Context> m_context;
  std::unique_ptr<skgpu::graphite::Recorder> m_recorder;
};
