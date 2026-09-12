#pragma once

/** @file
 * SigilCoreReconcile in one include: the comparable type erasure, the
 * inherited-value channel, the memo, the tree skeleton, the host contract,
 * the reconciler and its counts, and the phase runner. The animation
 * lanes a patch retargets, and the comparators over the animation values
 * a description carries, are SigilMotion's — <sigilmotion/values/Lanes.h>
 * and the headers of the values themselves.
 */

#include "sigilcore/comparable/Erased.h"
#include "sigilcore/reconcile/Environment.h"
#include "sigilcore/reconcile/Host.h"
#include "sigilcore/reconcile/Memo.h"
#include "sigilcore/reconcile/Node.h"
#include "sigilcore/reconcile/Phases.h"
#include "sigilcore/reconcile/Reads.h"
#include "sigilcore/reconcile/Reconciler.h"
#include "sigilcore/reconcile/Stats.h"
