#pragma once

/** @file
 * @ingroup weave-paint
 *
 * The paint feature's face: the two draws of a finished layout as free
 * functions over the ParagraphLayout members — draw() one blob per run and
 * pass, drawBatched() one drawGlyphs call per (font, paint) bucket and
 * pass — and the resolver a pass with a SigilMaterial instance is shaded
 * through, registered by whoever links a renderer.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>

#include <functional>

#include "sigilweave/layout/ParagraphLayout.h"
#include "sigilweave/paragraph/Paragraph.h"

namespace sigil::material {
class Material;
}

/** DRAWING A FINISHED LAYOUT: `ParagraphLayout`'s own draw calls as free
 *  functions, so a caller that holds a layout and a canvas needs nothing
 *  else. The batched form is the one a scene with many individually
 *  animated glyphs wants; the plain form resolves each run's ordered
 *  paint layers from the paragraph's current spans. Every draw completes
 *  all underlays, then all foregrounds, then all overlays. A style's layers
 *  keep their declared order within a band. Layout decides where the glyphs
 *  are, and nothing here moves them. */
namespace sigil::weave::paint {

/** Draws every run of @p layout, resolving its ordered paint layers from
 *  the paragraph's current spans; @p overridePaint replaces every span's
 *  paint. Runs keep layout order within each paint band.
 *  `ParagraphLayout::draw` as a free function. */
inline void draw(SkCanvas* canvas, const ParagraphLayout& layout,
                 const Paragraph& paragraph,
                 const PaintStyle* overridePaint = nullptr) {
  layout.draw(canvas, paragraph, overridePaint);
}

/** Draws a layout with horizontal runs merged
 *  into one drawGlyphs per (font, PaintStyle) bucket and pass, transformed
 *  runs from their baked blobs. Within each paint band, buckets and blob
 *  fallbacks draw in first-encounter order; each bucket includes all of its
 *  matching runs. `ParagraphLayout::drawBatched` as a free function. */
inline void drawBatched(
    SkCanvas* canvas, const ParagraphLayout& layout, const Paragraph& paragraph,
    const PaintStyle* overridePaint = nullptr,
    const ParagraphLayout::LiveVariations* liveVariations = nullptr) {
  layout.drawBatched(canvas, paragraph, overridePaint, liveVariations);
}

/** Turns a pass's material into its shader. Bounds cover the run's or
 *  batch's glyph ink before layer offsets, strokes or filters; transformed
 *  and unshaped blobs use their conservative bounds. Whitespace can have
 *  empty bounds. Null draws the pass with its configured paint alone. */
using MaterialResolver = std::function<sk_sp<SkShader>(
    const sigil::material::Material& material, const SkRect& bounds)>;

/** Registers the resolver PaintStyle::foregroundMaterial and
 *  PaintLayer::material are shaded through.
 *  The paint feature links no renderer, so the host that draws installs
 *  one — SigilMaterial's Skia backend, over the bounds of what the pass
 *  covers. Replaces any earlier resolver; an empty function clears. */
void setMaterialResolver(MaterialResolver resolver);

/** Whether a resolver is registered. */
bool hasMaterialResolver();

}  // namespace sigil::weave::paint
