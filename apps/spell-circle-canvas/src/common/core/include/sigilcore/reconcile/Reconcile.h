#pragma once

/** @file
 * @ingroup core-reconcile
 *
 * SigilCoreReconcile in one include: the comparable type erasure, the
 * inherited-value channel, the memo, the tree skeleton, the host contract,
 * the reconciler and its counts, and the phase runner. The animation
 * lanes a patch retargets, and the comparators over the animation values
 * a description carries, are SigilMotion's — <sigilmotion/values/Lanes.h>
 * and the headers of the values themselves.
 */

/** @defgroup core-reconcile The reconciler
 *  Descriptions built fresh every frame, reconciled onto a tree the host
 *  retains, so only what changed is touched. It owns the shape of that
 *  tree — which retained node answers to which description, matched by
 *  key and then by position — the memo that skips a describe whose
 *  inputs did not change, the identity prune that leaves an unchanged
 *  node alone, and the counts of what a pass did. Everything a node
 *  retains beyond its place in the tree is the host's, reached through
 *  named operations the host implements on itself. */

#include "sigilcore/comparable/Erased.h"
#include "sigilcore/reconcile/Environment.h"
#include "sigilcore/reconcile/Host.h"
#include "sigilcore/reconcile/Memo.h"
#include "sigilcore/reconcile/Node.h"
#include "sigilcore/reconcile/Phases.h"
#include "sigilcore/reconcile/Reads.h"
#include "sigilcore/reconcile/Reconciler.h"
#include "sigilcore/reconcile/Stats.h"
