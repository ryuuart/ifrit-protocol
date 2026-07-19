#pragma once
// Bench-only Metal handles for the Graphite re-measure (item 21).
namespace ifrit::compose::bench {
void *gpuDevice();
void *gpuQueue();
} // namespace ifrit::compose::bench
