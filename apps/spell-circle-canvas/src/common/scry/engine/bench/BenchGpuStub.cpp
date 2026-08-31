/** @file
 * The bench helpers where no GPU backend exists: scry_engine_bench runs
 * CPU mode only here.
 */

#include "BenchGpu.h"

namespace sigil::scry::bench {

sigil::skia::GpuDevice* gpuDevice() { return nullptr; }
sigil::skia::TextureHandle makeSolidTexture(int, int) { return {}; }

}  // namespace sigil::scry::bench
