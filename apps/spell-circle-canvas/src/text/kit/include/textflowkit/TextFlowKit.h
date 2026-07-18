#pragma once

/** @file
 * Umbrella header for TextFlowKit — companion utilities distilled from
 * TextFlow's gallery, demo, and application consumers. See kit/README.md
 * for the philosophy: the caching and batching that keep animated text
 * cheap are invisible when done right and silently expensive when not, so
 * this layer makes their invalidation keys and bucket structure explicit,
 * with the per-situation variation expressed as callables.
 */

#include "GlyphBuckets.h"
#include "Labels.h"
#include "LayoutGuard.h"
#include "RebuildGuard.h"
#include "SampleText.h"
#include "Timing.h"
