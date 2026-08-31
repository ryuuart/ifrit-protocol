#pragma once

/** @file
 * Internal to the kernel — the RETAINED runtime shared by the phase
 * translation units and the Composer facade, as one include over its
 * subjects: the Instance node, the slot table, the transform arithmetic
 * and Composer::Impl. Element DESCRIPTIONS live in
 * ComposeInternal.h; this is the resolved, mutable, per-frame side.
 */

#include "ComposerImpl.h"
#include "Instance.h"
#include "SlotSpecs.h"
#include "Transforms.h"
