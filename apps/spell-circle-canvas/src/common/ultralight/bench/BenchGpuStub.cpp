#include "BenchGpu.h"

// No GPU backend on this platform yet (the Vulkan/D3D drivers arrive
// with the Windows/Linux ports) — ultralight_bench runs CPU mode only here.

namespace sigil::web::bench {

void *gpuDevice() { return nullptr; }
void *gpuQueue() { return nullptr; }
void *makeSolidTexture(int, int) { return nullptr; }

} // namespace sigil::web::bench
