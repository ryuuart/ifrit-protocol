#pragma once

/** @file
 * @ingroup sketch-core
 *
 * The device 2D drawing with no frame open lands on in this process: the
 * installed device's context, the window's while a window lends it, or a
 * painter of the process's own stood up when first asked for.
 */

namespace sigil::core::hardware {
class GpuDevice;
}  // namespace sigil::core::hardware

namespace sigil::skia {
class GraphiteContext;
}  // namespace sigil::skia

namespace sigil::draw {
class HostlessDevice;
}  // namespace sigil::draw

namespace sigil::sketch {

/** A Metal or Vulkan device and Graphite standing on it. */
struct Painter {
  core::hardware::GpuDevice* device = nullptr;
  skia::GraphiteContext* graphite = nullptr;
};

/** THE PROCESS'S OWN 2D PAINTER: Graphite on the system's device, stood
 *  up the first time it is asked for and kept for the life of the
 *  process, so a picture formed on it never outlives its device. Its
 *  context's own recorder belongs to the thread that first asked, and
 *  only that thread is answered: a window shown there where no device is
 *  installed renders on this painter's device and queue and draws its
 *  frames through that recorder. Both null on any other thread, where
 *  the machine has no device of its own to make, or where Graphite
 *  declines it. */
[[nodiscard]] Painter ownPainter();
/** The process's own painter where something already stood it up, and
 *  both null otherwise: what a window asks, which never stands one up. */
[[nodiscard]] Painter standingOwnPainter();

/** THE GRAPHITE CONTEXT A PEN WITH NO FRAME DRAWS ON, read at each
 *  ask: the installed device's (`deviceGraphite()`), which is the
 *  window's own while a window lends it that device; else the process's
 *  own painter's; null where there is no device, and drawing with no
 *  frame is raster. Each thread draws on it through a recorder of its
 *  own, never through the context's own, which belongs to the thread
 *  drawing the frames. */
[[nodiscard]] skia::GraphiteContext* hostlessGraphite();

/** THE HOST-LESS DEVICE THIS PROCESS DRAWS ON: SigilDraw's seam over
 *  `hostlessGraphite()`. A buffer formed with no frame open is formed on
 *  that context, an image of it is drawn as it stands by a pen recording
 *  onto that context, and read back to the CPU for any other pen and for
 *  a caller reading pixels. A host installs it with
 *  `draw::useHostlessDevice(&sketch::hostlessPainting())`. */
[[nodiscard]] draw::HostlessDevice& hostlessPainting();

}  // namespace sigil::sketch
