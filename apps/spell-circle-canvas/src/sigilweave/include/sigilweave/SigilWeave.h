#pragma once

/** @file
 * @ingroup shaping
 *
 * Umbrella header for the SigilWeave layout engine — a cache-first Skia text
 * layout library built directly on HarfBuzz and ICU (no
 * SkParagraph/SkShaper). Include this to pull in everything, or the
 * individual headers below. See README.md in this directory for the
 * architecture, the performance story, and a fuller API tour; this map is
 * just the "which header does what" index.
 *
 * The core pipeline — the four headers a typical caller touches:
 *   - Style.h            `TextStyle` = `ShapingStyle` (shape-cache key) +
 *                        `PaintStyle` (draw-time). The style vocabulary
 *                        every header speaks.
 *   - FontContext.h      Per-thread service object (caches, HarfBuzz, ICU).
 *                        Make one; hand it to every layout call.
 *   - Paragraph.h        The document: UTF-16 text + style spans +
 *                        placeholders. Build with `ParagraphBuilder` or in
 *                        place; edit live.
 *   - Flow.h             The geometry text flows into: `BlockFlow`
 *                        (rectangle), `ExclusionFlow` (rectangle minus
 *                        shapes), `VerticalBlockFlow`, `LineSetFlow`,
 *                        `PathFlow` — or your own `FlowGeometry`.
 *   - ParagraphLayout.h  `layoutParagraph(context, paragraph, flow,
 *                        options)` -> positioned runs;
 *                        `draw()`/`drawBatched()` them to a canvas.
 *
 * Lower level, rarely included directly:
 *   - Shaper.h           `ShapedWord` and the per-word shaping types the
 *                        layout is built on; reach for it only to inspect
 *                        glyph runs.
 *
 * Optional layers (nothing in the core pipeline depends on them):
 *   - Query.h                    find / select / annotate ranges
 *                                (substring, word, ICU regex) and
 *                                edit-following `MarkerSet`s.
 *   - Choreograph.h              per-glyph animation (rain / ripple /
 *                                marquee).
 *   - SingleLineParagraphCache.h fast path for high-frequency labels &
 *                                captions.
 *
 * Separate library (link SigilWeaveShaders and include it directly):
 *   - PaintShaders.h             animated water, mesh-gradient, and star
 *                                SkShader presets for PaintStyle layers.
 *                                Lives outside this umbrella so the core
 *                                library carries no SkRuntimeEffect/SkSL
 *                                code — presets are content, not engine.
 */

/** @defgroup shaping Shaping & fonts
 *  Word shaping, the shape cache, font management, fallback, and the style
 *  vocabulary that keys it all (Style.h, FontContext.h, Shaper.h,
 *  Features.h). */
/** @defgroup document Document model
 *  Styled UTF-16 text with incremental analysis: Paragraph, spans,
 *  placeholders, and the edit history (Paragraph.h). */
/** @defgroup geometry Flow geometry
 *  The shapes text flows into: blocks, exclusions, vertical columns,
 *  explicit line sets, and paths (Flow.h). */
/** @defgroup layout Layout
 *  Line breaking and placement: layoutParagraph, options, positioned runs,
 *  and the label fast path (ParagraphLayout.h,
 *  SingleLineParagraphCache.h). */
/** @defgroup paint Painting & effects
 *  Draw-time appearance: paint layers, decorations, and the preset shader
 *  library (the PaintStyle half of Style.h; PaintShaders.h). */
/** @defgroup query Query & markers
 *  Range search and edit-following marker sets (Query.h). */
/** @defgroup animation Animation
 *  Per-glyph choreography over finished layouts (Choreograph.h). */

#include "Choreograph.h"
#include "Features.h"
#include "Flow.h"
#include "FontContext.h"
#include "Paragraph.h"
#include "ParagraphLayout.h"
#include "Query.h"
#include "Shaper.h"
#include "SingleLineParagraphCache.h"
#include "Style.h"
