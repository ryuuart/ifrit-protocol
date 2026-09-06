#pragma once

/** @file
 * The graphite feature's headers at once: the Graphite context, the
 * offscreen surface over a texture someone else owns, the image over one
 * that is only sampled, and the pixel reads a device upload takes. The device
 * itself is SigilCoreHardware's — <sigilcore/hardware/GpuDevice.h>. The two
 * SkCanvas ops Graphite leaves unimplemented and the Qt adapters are
 * included by their own headers, <sigilskia/draw/Direct.h> and
 * <sigilskia/qt/QtInterop.h>.
 *
 * THE TWO WRAPS OWN THEIR TEXTURE DIFFERENTLY, though both are handed one
 * as a bare void*. An OffscreenSurface BORROWS it — construct one fresh per
 * use, and keep the texture alive for as long as that surface draws.
 * `wrapImage` RETAINS it until the last image naming it is gone, so an
 * image outliving the frame that owned its texture still samples pixels.
 * `wrapPlanarImage` retains nothing and hands its planes back through the
 * release it was given.
 */

#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/Pixels.h>
#include <sigilskia/graphite/TextureImage.h>
