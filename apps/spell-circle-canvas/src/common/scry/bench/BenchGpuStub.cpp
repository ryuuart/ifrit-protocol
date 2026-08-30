#include "BenchGpu.h"

// No GPU backend on this platform yet (the Vulkan driver arrives with the
// Windows/Linux ports) — scry_bench runs CPU mode only here.

namespace sigil::scry::bench {

sigil::skia::GpuDevice* gpuDevice() { return nullptr; }
sigil::skia::TextureHandle makeSolidTexture(int, int) { return {}; }

}  // namespace sigil::scry::bench
