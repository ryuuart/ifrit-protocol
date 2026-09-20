#pragma once

/** @file
 * @ingroup weave-shaping
 *
 * Umbrella header for the SigilWeave engine — a cache-first Skia text
 * layout library built directly on HarfBuzz and ICU (no
 * SkParagraph/SkShaper). It pulls in every engine feature and links
 * against the `SigilWeave` interface target, which is every engine
 * archive. A consumer of one feature includes that feature's header,
 * spelled `<sigilweave/<feature>/Name.h>`.
 *
 * The features, dependencies pointing down:
 *   - unicode/      the text analysis leaf: transcoding, scripts, case,
 *                   segmentation, bidi. ICU only.
 *   - style/        `TextStyle` = `ShapingStyle` (shape-cache key) +
 *                   `PaintStyle` (draw-time); paint layers, decorations,
 *                   `Type`, the partial a style is named in, with `Length`
 *                   and the merges that resolve one, the `StyleSheet` of
 *                   named partials, the OpenType feature presets.
 *   - fonts/        `FontContext`, the per-thread service object (caches,
 *                   HarfBuzz, fallback, varied faces), and `shapeWord()`.
 *   - paragraph/    the document: UTF-16 text + style spans + placeholders,
 *                   analysed into `Word`s and shaped lazily; `RichText`,
 *                   the same content as one comparable value; `Unit`, the
 *                   granularity a passage is addressed by.
 *   - layout/       the geometry text flows into (`BlockFlow`,
 *                   `ExclusionFlow`, `VerticalBlockFlow`, `LineSetFlow`,
 *                   `PathFlow`, or your own `FlowGeometry`), the options,
 *                   `layoutParagraph()` and its positioned runs, and
 *                   `Story`, a text and its block styles filled into as
 *                   many frames as it is given; `TextContext`, single-style
 *                   text with configured paragraph reuse and owned results.
 *   - decoration/   underline, strikethrough, overline and highlight bands
 *                   resolved against the placed runs.
 *   - paint/        `ParagraphLayout::draw()` and `drawBatched()`.
 *   - choreograph/  per-glyph animation: `forEachPlacedGlyph`,
 *                   `GlyphDress`, `GlyphRSXformBatches`.
 *   - query/        find / select / annotate ranges, `Selector` and the
 *                   `selectors::` vocabulary, and edit-following `MarkerSet`s.
 *
 * Separate targets, never pulled in here: ports/ (the OS font manager),
 * kit/ (consumer discipline: guards, buckets, labels) and qt/ (the Qt
 * bridge).
 */

/** @defgroup weave-unicode Unicode analysis
 *  Transcoding, scripts, case mapping, segmentation and bidi as plain
 *  values over UTF-16 text (unicode/Unicode.h). */
/** @defgroup weave-shaping Shaping & fonts
 *  Word shaping, the shape cache, font management, fallback, and the style
 *  vocabulary that keys it all (style/Style.h, fonts/FontContext.h,
 *  fonts/Shaper.h). */
/** @defgroup weave-document Document model
 *  Styled UTF-16 text with incremental analysis: Paragraph, spans,
 *  placeholders, words, and the edit history, with the same content as a
 *  comparable value (paragraph/Paragraph.h, paragraph/Word.h,
 *  paragraph/RichText.h). */
/** @defgroup weave-geometry Flow geometry
 *  The shapes text flows into: blocks, exclusions, vertical columns,
 *  explicit line sets, and paths (layout/Flow.h). */
/** @defgroup weave-layout Layout
 *  Line breaking and placement: layoutParagraph, options, positioned runs,
 *  and the label fast path (layout/ParagraphLayout.h,
 *  layout/LayoutOptions.h, layout/PositionedRun.h,
 *  layout/TextContext.h). */
/** @defgroup weave-paint Painting & effects
 *  Draw-time appearance: the two draws of a finished layout, the paint
 *  layers a run is dressed in, and the decoration bands resolved
 *  against the placed runs (paint/Paint.h, the PaintStyle half of
 *  style/Style.h, decoration/Decoration.h). */
/** @defgroup weave-query Query & markers
 *  Range search, selection as a value, and edit-following marker sets
 *  (query/Query.h, query/Selector.h). */
/** @defgroup weave-animation Animation
 *  Per-glyph choreography over finished layouts (choreograph/). */
/** @defgroup weave-kit Consumer kit
 *  The discipline a consumer of the engine needs and the engine does not
 *  impose: layout memoization, glyph bucketing, the hyphenation and
 *  line-edge tables the engine asks for and holds no opinion about, the
 *  OpenType feature presets, the one-call label draw, and deterministic
 *  sample content (kit/). Its own interface target, linked on top of the
 *  engine. */
/** @defgroup weave-ports Platform ports
 *  The system font manager, behind the seam the engine names fonts
 *  through (ports/SystemFontManager.h). Its own target, so the engine
 *  itself binds to no operating system. */
/** @defgroup weave-qt The Qt bridge
 *  QString and Skia types crossing into the engine's own
 *  (qt/SigilWeaveQt.h). Its own target, so the engine stays Qt-free. */

#include "sigilweave/choreograph/Choreograph.h"
#include "sigilweave/decoration/Decoration.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/Beside.h"
#include "sigilweave/layout/Flow.h"
#include "sigilweave/layout/ParagraphLayout.h"
#include "sigilweave/layout/Story.h"
#include "sigilweave/layout/TextContext.h"
#include "sigilweave/paint/Paint.h"
#include "sigilweave/paragraph/Paragraph.h"
#include "sigilweave/paragraph/RichText.h"
#include "sigilweave/paragraph/Unit.h"
#include "sigilweave/query/Query.h"
#include "sigilweave/query/Selector.h"
#include "sigilweave/style/Style.h"
#include "sigilweave/unicode/Unicode.h"
