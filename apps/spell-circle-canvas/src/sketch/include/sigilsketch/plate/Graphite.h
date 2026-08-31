#pragma once

/** @file
 * The device a headless run draws on: a process-owned native device and
 * queue, with a Graphite context over them.
 */

#include <memory>

namespace sigil::skia {
class GraphiteContext;
}

namespace sigil::sketch {

/** The headless Graphite context, or null when this build has no
 *  Graphite backend for the platform. An interactive host shares the
 *  device its window system already owns; this exists only for a run
 *  with no window, which therefore owns its own. */
std::unique_ptr<skia::GraphiteContext> headlessGraphite();

}  // namespace sigil::sketch
