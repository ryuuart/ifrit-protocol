#pragma once

/** @file
 * MetalDriver::State — the device and queue, the pipeline states, the
 * tables Ultralight's ids index (textures, render buffers, geometry),
 * the pending command list, and the web thread's Graphite recorder.
 * Objective-C++ only; shared by the driver's two translation units.
 */

#import <Metal/Metal.h>
#include <include/gpu/graphite/Recorder.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <memory>
#include <vector>

#include "MetalDriver.h"

namespace sigil::scry {

struct MetalDriver::State {
  id<MTLDevice> device = nil;
  id<MTLCommandQueue> queue = nil;
  // Pipeline states indexed [shader type][blending enabled].
  id<MTLRenderPipelineState> pipelines[2][2] = {{nil, nil}, {nil, nil}};

  boost::unordered_flat_map<uint32_t, id<MTLTexture>> textures;

  struct RenderBufferEntry {
    uint32_t textureId = 0;
  };
  boost::unordered_flat_map<uint32_t, RenderBufferEntry> renderBuffers;

  struct GeometryEntry {
    id<MTLBuffer> vertices = nil;
    id<MTLBuffer> indices = nil;
  };
  boost::unordered_flat_map<uint32_t, GeometryEntry> geometry;

  std::vector<ultralight::Command> commands;
  boost::unordered_flat_set<uint32_t> pendingClear;

  // The host's device and the Graphite context shared with it. The
  // recorder is the web thread's own over that context — a recorder
  // belongs to one thread, and paintTexture() runs here — carrying the
  // context's image provider (raster draws in a paint callback upload and
  // land) and ordered replay (every recording it snaps is inserted, in
  // order, or the recorder is dead). Inserting and submitting touch the
  // context, which the host uses from its own thread, so both happen under
  // the context's lock; the one queue underneath orders this work against
  // the driver's own render passes and the host's frames alike.
  sigil::core::hardware::GpuDevice* gpuDevice = nullptr;
  sigil::skia::GraphiteContext* graphite = nullptr;
  std::unique_ptr<skgpu::graphite::Recorder> webRecorder;

  /** The MTLTexture behind a live handle, or nil for a stale one. */
  id<MTLTexture> texture(sigil::core::hardware::TextureHandle handle) const {
    return (__bridge id<MTLTexture>)gpuDevice->exportNative(handle).mtlTexture;
  }

  /** Imports a driver-created (+1) texture borrowed: the driver keeps the
   *  reference and drops it in releaseTexture(). */
  sigil::core::hardware::TextureHandle import(id<MTLTexture> mtlTexture,
                                              int width, int height) {
    if (!mtlTexture) return {};
    sigil::core::hardware::NativeTexture native;
    native.backend = sigil::core::hardware::Backend::Metal;
    native.mtlTexture = (__bridge_retained void*)mtlTexture;
    native.width = width;
    native.height = height;
    return gpuDevice->importNative(native, /*takeOwnership=*/false);
  }

  uint32_t nextTextureId = 1;
  uint32_t nextRenderBufferId = 1;
  uint32_t nextGeometryId = 1;
};

}  // namespace sigil::scry
