#pragma once
// Bench-only helpers for the platform GPU handles (Metal on Apple, null
// elsewhere until the Vulkan/D3D drivers land).

#include <cstddef>

namespace sigil::scry::bench {

/** System default GPU device / a fresh command queue on it, retained for
 *  process lifetime. Null when this platform has no GPU backend yet. */
void *gpuDevice();
void *gpuQueue();

/** Creates a shared-storage BGRA8 texture filled with a solid color, for
 *  updateTexture() benchmarking. Retained for process lifetime. */
void *makeSolidTexture(int width, int height);

} // namespace sigil::scry::bench
