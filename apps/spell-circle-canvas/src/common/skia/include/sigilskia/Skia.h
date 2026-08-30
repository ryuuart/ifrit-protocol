#pragma once

/** @file
 * Every Qt-free SigilSkia header at once: the device with its handles
 * and fences, the Graphite context and the offscreen surface over a
 * texture someone else owns. The Qt adapters are included by their own
 * header, <sigilskia/qt/QtInterop.h>.
 */

#include <sigilskia/device/Fence.h>
#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/device/Handle.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
