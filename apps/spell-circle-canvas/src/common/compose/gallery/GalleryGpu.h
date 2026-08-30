#pragma once
// Headless GPU sweep support: a process-owned native device/queue pair and
// the Graphite context over them, mirroring bench/ComposeBenchGpu. The
// interactive app shares Qt's device instead (ComposeGalleryView); this is
// only for `--headless --gpu`, which has no QRhi. Returns null when the
// build has no Graphite backend for this platform.

#include <memory>

namespace sigil::skia {
class GraphiteContext;
}

namespace compose_gallery {
std::unique_ptr<sigil::skia::GraphiteContext> makeHeadlessGraphite();
}
