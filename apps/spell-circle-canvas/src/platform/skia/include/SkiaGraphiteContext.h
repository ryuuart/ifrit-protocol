#pragma once
#include <memory>

class QRhi;

namespace skgpu::graphite {
class Context;
class Recorder;
} // namespace skgpu::graphite

/**
 * Owns the Skia Graphite Context + Recorder used to draw into
 * QCanvasOffscreenCanvas textures from SkCanvas. Built from the same native
 * device/queue that Qt's QRhi already uses (Metal on Apple platforms,
 * Vulkan elsewhere — one create() TU per build, see
 * SkiaGraphiteContextMetal.mm / SkiaGraphiteContextVulkan.cpp), so
 * Graphite's GPU work rides the same command queue as Qt Quick's own
 * rendering.
 */
class SkiaGraphiteContext {
public:
  /** Returns null if @p rhi isn't backed by this build's graphics API or
   *  Context creation fails. */
  static std::unique_ptr<SkiaGraphiteContext> create(QRhi *rhi);

  ~SkiaGraphiteContext();

  /** Returns the owned Graphite context. */
  skgpu::graphite::Context *context() const { return m_context.get(); }
  /** Returns the recorder associated with `context()`. */
  skgpu::graphite::Recorder *recorder() const { return m_recorder.get(); }

private:
  SkiaGraphiteContext(std::unique_ptr<skgpu::graphite::Context> context,
                      std::unique_ptr<skgpu::graphite::Recorder> recorder);

  std::unique_ptr<skgpu::graphite::Context> m_context;
  std::unique_ptr<skgpu::graphite::Recorder> m_recorder;
};
