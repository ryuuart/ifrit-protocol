#pragma once

/** @file
 * @ingroup shaping
 *
 * Umbrella header for the SigilWeave engine — a cache-first Skia text
 * layout library built directly on HarfBuzz and ICU (no
 * SkParagraph/SkShaper). Transitional: it pulls in every engine feature so
 * a consumer written against one flat header keeps compiling, and it links
 * against the `SigilWeave` interface target, which is every engine
 * archive. New code includes the feature headers it uses, spelled
 * `<sigilweave/<feature>/Name.h>`; README.md in this directory is the map.
 *
 * The features, dependencies pointing down:
 *   - unicode/      the text analysis leaf: transcoding, scripts, case,
 *                   segmentation, bidi. ICU only.
 *   - style/        `TextStyle` = `ShapingStyle` (shape-cache key) +
 *                   `PaintStyle` (draw-time); paint layers, decorations,
 *                   the `StyleSet` registry, the OpenType feature presets.
 *   - fonts/        `FontContext`, the per-thread service object (caches,
 *                   HarfBuzz, fallback, varied faces), and `shapeWord()`.
 *   - paragraph/    the document: UTF-16 text + style spans + placeholders,
 *                   analysed into `Word`s and shaped lazily.
 *   - layout/       the geometry text flows into (`BlockFlow`,
 *                   `ExclusionFlow`, `VerticalBlockFlow`, `LineSetFlow`,
 *                   `PathFlow`, or your own `FlowGeometry`), the options,
 *                   `layoutParagraph()` and its positioned runs.
 *   - decoration/   underline, strikethrough, overline and highlight bands
 *                   resolved against the placed runs.
 *   - paint/        `ParagraphLayout::draw()` and `drawBatched()`.
 *   - choreograph/  per-glyph animation: `forEachPlacedGlyph`,
 *                   `GlyphDress`, `GlyphRSXformBatches`.
 *   - query/        find / select / annotate ranges and edit-following
 *                   `MarkerSet`s.
 *   - cache/        `SingleLineParagraphCache` for high-frequency labels.
 *
 * Separate targets, never pulled in here: shaders/ (animated SkSL presets
 * for PaintStyle layers), ports/ (the OS font manager), kit/ (consumer
 * discipline: guards, buckets, labels) and qt/ (the Qt bridge).
 */

/** @defgroup unicode Unicode analysis
 *  Transcoding, scripts, case mapping, segmentation and bidi as plain
 *  values over UTF-16 text (unicode/Unicode.h). */
/** @defgroup shaping Shaping & fonts
 *  Word shaping, the shape cache, font management, fallback, and the style
 *  vocabulary that keys it all (style/Style.h, fonts/FontContext.h,
 *  fonts/Shaper.h, style/Features.h). */
/** @defgroup document Document model
 *  Styled UTF-16 text with incremental analysis: Paragraph, spans,
 *  placeholders, words, and the edit history (paragraph/Paragraph.h,
 *  paragraph/Word.h). */
/** @defgroup geometry Flow geometry
 *  The shapes text flows into: blocks, exclusions, vertical columns,
 *  explicit line sets, and paths (layout/Flow.h). */
/** @defgroup layout Layout
 *  Line breaking and placement: layoutParagraph, options, positioned runs,
 *  and the label fast path (layout/ParagraphLayout.h,
 *  layout/LayoutOptions.h, layout/PositionedRun.h,
 *  cache/SingleLineParagraphCache.h). */
/** @defgroup paint Painting & effects
 *  Draw-time appearance: paint layers, decoration bands, and the preset
 *  shader library (the PaintStyle half of style/Style.h;
 *  decoration/Decoration.h; shaders/PaintShaders.h). */
/** @defgroup query Query & markers
 *  Range search and edit-following marker sets (query/Query.h). */
/** @defgroup animation Animation
 *  Per-glyph choreography over finished layouts (choreograph/). */

#include "sigilweave/cache/SingleLineParagraphCache.h"
#include "sigilweave/choreograph/Choreograph.h"
#include "sigilweave/decoration/Decoration.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/Flow.h"
#include "sigilweave/layout/ParagraphLayout.h"
#include "sigilweave/paint/Paint.h"
#include "sigilweave/paragraph/Paragraph.h"
#include "sigilweave/query/Query.h"
#include "sigilweave/style/Features.h"
#include "sigilweave/style/Style.h"
#include "sigilweave/unicode/Unicode.h"
