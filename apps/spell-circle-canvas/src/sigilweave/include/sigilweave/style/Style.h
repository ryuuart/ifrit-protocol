#pragma once

/** @file
 * @ingroup weave-shaping
 *
 * The style vocabulary every other SigilWeave header speaks, one include
 * over its subjects. A TextStyle splits into two halves on purpose:
 * ShapingStyle is baked into the shape-cache key, and PaintStyle is
 * resolved at draw time only. The split is about who owns glyph
 * advances, not about what is visible: a change is paint-side only if it
 * cannot move a glyph.
 */

#include "sigilweave/style/Decoration.h"
#include "sigilweave/style/Length.h"
#include "sigilweave/style/PaintLayer.h"
#include "sigilweave/style/PaintStyle.h"
#include "sigilweave/style/ShapingStyle.h"
#include "sigilweave/style/TextStyle.h"
#include "sigilweave/style/Type.h"
#include "sigilweave/style/TypeSheet.h"
