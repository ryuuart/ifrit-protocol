#pragma once

/** @file
 * The Metal device every Graphite case on Apple stands on: the system's
 * default device and one queue on it, the Graphite context made over the
 * two, a hardware device adopted over the very same objects, and the reads
 * and submits the cases share. Each is inline, so every file of the binary
 * reaches the one context and the one adopted device.
 */

#import <Metal/Metal.h>

#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <sigilgpu/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include <gtest/gtest.h>

namespace sigil::skia::test {

inline id<MTLDevice> device() {
  static id<MTLDevice> d = MTLCreateSystemDefaultDevice();
  return d;
}

inline id<MTLCommandQueue> queue() {
  static id<MTLCommandQueue> q = [device() newCommandQueue];
  return q;
}

inline GraphiteContext *graphite() {
  static std::unique_ptr<GraphiteContext> ctx =
      GraphiteContext::createMetal((__bridge void *)device(), (__bridge void *)queue());
  return ctx.get();
}

}  // namespace sigil::skia::test

/** A machine with no Metal device cannot answer anything these cases
 *  ask, so a case there reports that it was not run rather than
 *  reporting that the library is broken. The binary carries the `gpu`
 *  label for the same reason. */
#define SKIP_WITHOUT_METAL()                                                           \
  do {                                                                                 \
    if (::sigil::skia::test::graphite() == nullptr) GTEST_SKIP() << "no Metal device"; \
  } while (0)

namespace sigil::skia::test {

/** A GpuDevice over the very device and queue the context above was
 *  stood up on, so a texture it names is drawn into by that context and
 *  ordered by that one queue. Adopted, so it frees neither. */
inline gpu::GpuDevice *adoptedDevice() {
  static std::unique_ptr<gpu::GpuDevice> d = [] {
    gpu::NativeDevice native;
    native.backend = gpu::Backend::Metal;
    native.mtlDevice = (__bridge void *)device();
    native.mtlCommandQueue = (__bridge void *)queue();
    return gpu::GpuDevice::adopt(native).value_or(nullptr);
  }();
  return d.get();
}

/** An 8x8 BGRA render target the device owns, readable by the CPU so the
 *  Metal arms can check the bytes without a copy. */
inline gpu::TextureDescription smallTarget() {
  gpu::TextureDescription desc;
  desc.width = 8;
  desc.height = 8;
  desc.format = gpu::TextureFormat::BGRA8Unorm;
  desc.cpuAccessible = true;
  return desc;
}

/** The bytes of a Metal texture the device names, after a command buffer
 *  committed behind Graphite's own work has completed — the queue
 *  ordering an asynchronous submit relies on, and nothing more. */
inline std::vector<uint8_t> readMetalBytes(gpu::GpuDevice &dev, gpu::TextureHandle handle,
                                           int size) {
  id<MTLCommandBuffer> barrier = [queue() commandBuffer];
  [barrier commit];
  [barrier waitUntilCompleted];
  std::vector<uint8_t> bytes(size_t(size) * size * 4);
  id<MTLTexture> texture = (__bridge id<MTLTexture>)dev.exportNative(handle).mtlTexture;
  if (!texture) return bytes;
  [texture getBytes:bytes.data()
        bytesPerRow:size_t(size) * 4
         fromRegion:MTLRegionMake2D(0, 0, size, size)
        mipmapLevel:0];
  return bytes;
}

/** Snaps what @p recorder holds, inserts it and submits, waiting for the
 *  device to finish. False when nothing was recorded or the insert or
 *  submit was refused. */
inline bool submitRecorder(GraphiteContext &ctx, skgpu::graphite::Recorder &recorder) {
  std::unique_ptr<skgpu::graphite::Recording> recording = recorder.snap();
  if (!recording) return false;
  const std::unique_lock<std::mutex> lock = ctx.lockContext();
  skgpu::graphite::InsertRecordingInfo insert;
  insert.fRecording = recording.get();
  return ctx.context()->insertRecording(insert) &&
         ctx.context()->submit(skgpu::graphite::SyncToCpu::kYes);
}

}  // namespace sigil::skia::test
