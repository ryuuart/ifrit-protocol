#pragma once

/** @file
 * @ingroup weave-animation
 *
 * Per-glyph choreography utilities — the "letters leave their lines"
 * pattern. An optional layer: nothing in the core pipeline includes it.
 * Lay the paragraph out, walk every placed glyph with forEachPlacedGlyph,
 * dress it however the effect wants, and accumulate into
 * GlyphRSXformBatches rather than drawing letter by letter.
 */

#include "sigilweave/choreograph/GlyphBatches.h"
#include "sigilweave/choreograph/GlyphDress.h"
#include "sigilweave/choreograph/PlacedGlyph.h"
