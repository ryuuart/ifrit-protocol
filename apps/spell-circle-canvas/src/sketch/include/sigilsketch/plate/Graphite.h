#pragma once

/** @file
 * The Graphite context a plate is drawn through: the one the device this
 * process brought up already carries.
 */

namespace sigil::skia {
class GraphiteContext;
}

namespace sigil::sketch {

/** THE PROCESS'S ONE GRAPHITE CONTEXT, or null when it holds no device.
 *
 *  It is the device's own — 2D drawing through it lands in a texture a
 *  3D pass on that same device samples, with no copy and one handle
 *  table naming both — so a canvas photographed here and a set rendered
 *  by the device runtime stand on ONE device. Nothing is created here,
 *  and that is the point: a second device would be a second set of
 *  textures, and a plate drawn on one of them cannot read what the
 *  other painted. A host says which device this is by installing it,
 *  through `sketch::useDevice()`. */
skia::GraphiteContext* deviceGraphite();

}  // namespace sigil::sketch
