#pragma once

/** @file
 * The swept operator's header stands with the point operators, in
 * `pop/Sweep.h`, because sweeping a profile along a rail is one: a
 * described value, a host executor, and a device executor beside it
 * dispatching the same kernel. This forwards to it so a consumer
 * spelling the old path still compiles; spell the new one, and link
 * `SigilGeometryMeshPop`, which is where the code is.
 */

#include "sigilgeometry/mesh/pop/Sweep.h"
