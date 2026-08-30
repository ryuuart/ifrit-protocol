#pragma once

/** @file
 * @ingroup animation
 *
 * Per-glyph choreography utilities — the "letters leave their lines"
 * pattern (rain, ripples, marquees, staggered reveals) distilled from the
 * demos and the gallery. Optional layer: nothing in the core pipeline
 * includes this.
 *
 * The recipe: lay the paragraph out normally, walk every placed glyph with
 * forEachPlacedGlyph, dress it however the effect wants — displaced,
 * rotated, faded, tinted, drawn through another face, or placed by a full
 * matrix where an RSXform cannot reach — and accumulate into
 * GlyphRSXformBatches, which is one drawGlyphsRSXform call per (font, paint
 * pass) instead of thousands of per-glyph draws.
 *
 * A PlacedGlyph carries the identity an effect selects and staggers on —
 * which glyph of which word, line, style span and sentence it is — beside
 * the geometry it draws with, and the span's complete PaintStyle, so an
 * animated letter keeps the gradients, strokes and glow passes its span was
 * styled with.
 */

#include "sigilweave/choreograph/GlyphBatches.h"
#include "sigilweave/choreograph/GlyphDress.h"
#include "sigilweave/choreograph/PlacedGlyph.h"
