#pragma once
// Bench-only Metal handles for the Graphite re-measure (item 21).
namespace sigil::compose::bench {
void *gpuDevice();
void *gpuQueue();
} // namespace sigil::compose::bench
