#pragma once
// Bench-only helpers for the platform GPU device (Metal on Apple, null
// elsewhere until the Vulkan driver lands).

#include <sigilskia/device/Handle.h>

namespace sigil::skia {
class GpuDevice;
}

namespace sigil::scry::bench {

/** A device this process owns — the system default and a fresh queue —
 *  for process lifetime. Null when this platform has no GPU backend yet. */
sigil::skia::GpuDevice* gpuDevice();

/** A CPU-accessible BGRA8 texture on gpuDevice() filled with a solid
 *  colour, for updateTexture() benchmarking. Lives for the process. */
sigil::skia::TextureHandle makeSolidTexture(int width, int height);

}  // namespace sigil::scry::bench
