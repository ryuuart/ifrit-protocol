#pragma once
// Headless GPU sweep support: a process-owned native device/queue pair and
// the Graphite context over them, mirroring bench/ComposeBenchGpu. The
// interactive app shares Qt's device instead (ComposeGalleryView); this is
// only for `--headless --gpu`, which has no QRhi. Returns null when the
// build has no Graphite backend for this platform.

#include <memory>

class SkiaGraphiteContext;

namespace compose_gallery {
std::unique_ptr<SkiaGraphiteContext> makeHeadlessGraphite();
}
