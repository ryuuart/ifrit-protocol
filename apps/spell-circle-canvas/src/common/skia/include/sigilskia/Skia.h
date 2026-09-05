#pragma once

/** @file
 * Every Qt-free SigilSkia header at once: the Graphite context, the
 * offscreen surface over a texture someone else owns, the image over one
 * that is only sampled, and the pixel reads a device upload takes. The device itself is SigilCoreHardware's
 * — <sigilcore/hardware/GpuDevice.h>. The Qt adapters are included by
 * their own header, <sigilskia/qt/QtInterop.h>.
 */

#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/Pixels.h>
#include <sigilskia/graphite/TextureImage.h>
