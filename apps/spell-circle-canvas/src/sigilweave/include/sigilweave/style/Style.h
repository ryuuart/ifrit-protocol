#pragma once

/** @file
 * @ingroup shaping
 *
 * The style vocabulary every other SigilWeave header speaks, one include
 * over its subjects. A TextStyle splits into two halves on purpose:
 *   - ShapingStyle — typeface, size, letter spacing, language, OpenType
 *     features, vertical form. Baked into the shape-cache key: change any
 *     field and the words it covers are re-shaped. (ShapingStyle.h)
 *   - PaintStyle   — an SkPaint foreground plus ordered glyph-paint passes
 *     behind and above it (PaintLayer.h) and the line decorations
 *     (Decoration.h). Resolved at draw time only: recoloring, animating
 *     a shader, or restyling effects never re-shapes and never relayouts.
 *     (PaintStyle.h)
 *
 * The split is about who owns glyph advances, not about what is visible: a
 * change is paint-side only if it cannot move a glyph. One appearance change
 * escapes PaintStyle without re-shaping — an advance-invariant variable-font
 * axis driven through ParagraphLayout::LiveVariations at draw time
 * (ParagraphLayout.h), which reuses the shaped positions precisely because
 * the axis leaves advances alone.
 *
 * TextStyle.h holds the two halves together; StyleSet.h the named registry
 * of them; Type.h the designated-init aggregate a call site names a
 * style's numbers in. Attach styles to text through Paragraph /
 * ParagraphBuilder (Paragraph.h).
 */

#include "sigilweave/style/Decoration.h"
#include "sigilweave/style/PaintLayer.h"
#include "sigilweave/style/PaintStyle.h"
#include "sigilweave/style/ShapingStyle.h"
#include "sigilweave/style/StyleSet.h"
#include "sigilweave/style/TextStyle.h"
#include "sigilweave/style/Type.h"
