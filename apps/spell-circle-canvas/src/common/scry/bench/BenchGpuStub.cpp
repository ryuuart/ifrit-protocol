#include "BenchGpu.h"

// No GPU backend on this platform yet (the Vulkan/D3D drivers arrive
// with the Windows/Linux ports) — scry_bench runs CPU mode only here.

namespace sigil::scry::bench {

void *gpuDevice() { return nullptr; }
void *gpuQueue() { return nullptr; }
void *makeSolidTexture(int, int) { return nullptr; }

} // namespace sigil::scry::bench
