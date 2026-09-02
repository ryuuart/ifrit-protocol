/** @file
 * The bench helpers where no GPU backend exists: scry_engine_bench runs
 * CPU mode only here.
 */

#include "BenchGpu.h"

namespace sigil::scry::bench {

sigil::core::hardware::GpuDevice* gpuDevice() { return nullptr; }
sigil::core::hardware::TextureHandle makeSolidTexture(int, int) { return {}; }

}  // namespace sigil::scry::bench
