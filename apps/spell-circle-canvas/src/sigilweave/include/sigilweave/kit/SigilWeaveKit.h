#pragma once

/** @file
 * Umbrella header for SigilWeaveKit — companion utilities distilled from
 * SigilWeave's gallery, demo, and application consumers. See kit/README.md
 * for the philosophy: the caching and batching that keep animated text
 * cheap are invisible when done right and silently expensive when not, so
 * this layer makes their invalidation keys and bucket structure explicit,
 * with the per-situation variation expressed as callables.
 */

#include "sigilweave/kit/CachedValue.h"
#include "sigilweave/kit/GlyphBuckets.h"
#include "sigilweave/kit/Labels.h"
#include "sigilweave/kit/LayoutGuard.h"
#include "sigilweave/kit/Palette.h"
#include "sigilweave/kit/Quantize.h"
#include "sigilweave/kit/RebuildGuard.h"
#include "sigilweave/kit/SampleText.h"
#include "sigilweave/kit/Timing.h"
