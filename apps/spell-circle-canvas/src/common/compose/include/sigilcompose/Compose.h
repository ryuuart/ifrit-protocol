#pragma once

/** @file
 * @ingroup compose-core
 *
 * SigilCompose — the root umbrella over the kernel, exactly
 * `core/Core.h`. Each feature has an umbrella of its own
 * under its directory, and each header under a feature stands on its own.
 */

/** @defgroup compose-core The kernel
 *  The values a scene description is made of: the node (`Element`), the
 *  factories that start one, the paint, the shape, the mask, the stroke,
 *  the layout units, the cascade, the grid and table schemes, the derive
 *  phase, the instancing, the feed, and the `Composer` that draws them
 *  (core/). A reader who knows these headers has a complete model. */
/** @defgroup compose-brush Brushes and layer styles
 *  Marks made along a boundary and treatments laid over a surface:
 *  brushes and their stamps, ribbons, lines and rails, hatches,
 *  decorations, and the Photoshop-shaped layer and pixel styles
 *  (brush/). What a node's `stroke`, `background`, `foreground` and
 *  `layerStyle` verbs take. */
/** @defgroup compose-typography Type
 *  Everything a text leaf says beyond its words: the per-glyph textFx tracks
 *  and their beats, the text effects, the selector vocabulary compose
 *  adds to SigilWeave's, the unit a passage is addressed by, readings
 *  beside the type, and type set along a path (typography/). */
/** @defgroup compose-kit The component kit
 *  Stock components built out of the kernel's own verbs — frames,
 *  boards, panels, rows, plates, specimens, chrome, ornament, sprites,
 *  typesetting, instruments and placers (kit/). Each takes content and
 *  reads the theme; none of them decides anything the kernel could
 *  decide. */
/** @defgroup compose-draw The immediate-mode bridge
 *  A SigilDraw pen's marks as a node of a compose tree, so an imperative
 *  drawing stands in a declarative description (draw/). */
/** @defgroup compose-texture Scenes as textures
 *  A compose tree rendered into an offscreen texture or image rather
 *  than onto the canvas, and the tiling that reads one back
 *  (texture/). */
/** @defgroup compose-video Video leaves
 *  A decoded video stream as a leaf of a description, and how its frames
 *  are fitted into the node's box (video/). */
/** @defgroup compose-web Web leaves
 *  An HTML and CSS page rendered to an image and mounted as a leaf
 *  (web/). */
/** @defgroup compose-testing The consumer test harness
 *  What a consumer's own tests reach for: the index of a described tree
 *  and the checks made against it (testing/). Its own include root and
 *  its own target, linked only by a test binary. */

#include "sigilcompose/core/Core.h"
