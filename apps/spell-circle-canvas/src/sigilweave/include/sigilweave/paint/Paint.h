#pragma once

/** @file
 * @ingroup shaping
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

namespace sigil::weave::paint {

/** Draws every run of @p layout, resolving its ordered paint layers from
 *  the paragraph's current spans; @p overridePaint replaces every span's
 *  paint. `ParagraphLayout::draw` as a free function. */
inline void draw(SkCanvas* canvas, const ParagraphLayout& layout,
                 const Paragraph& paragraph,
                 const PaintStyle* overridePaint = nullptr) {
  layout.draw(canvas, paragraph, overridePaint);
}

/** Draws the same output with minimal draw calls: horizontal runs merged
 *  into one drawGlyphs per (font, PaintStyle) bucket and pass, transformed
 *  runs from their baked blobs. `ParagraphLayout::drawBatched` as a free
 *  function. */
inline void drawBatched(
    SkCanvas* canvas, const ParagraphLayout& layout, const Paragraph& paragraph,
    const PaintStyle* overridePaint = nullptr,
    const ParagraphLayout::LiveVariations* liveVariations = nullptr) {
  layout.drawBatched(canvas, paragraph, overridePaint, liveVariations);
}

/** Turns a pass's material into the shader its paint draws with, given
 *  the bounds of what the pass covers. Null draws the pass with its paint
 *  alone. */
using MaterialResolver = std::function<sk_sp<SkShader>(
    const sigil::material::Material& material, const SkRect& bounds)>;

/** Registers the resolver every PaintLayer::material is shaded through.
 *  The paint feature links no renderer, so a program that shades passes
 *  with materials installs one — the shaders feature installs SigilMaterial's
 *  Skia backend. Replaces any earlier resolver; an empty function clears. */
void setMaterialResolver(MaterialResolver resolver);

/** Whether a resolver is registered. */
bool hasMaterialResolver();

}  // namespace sigil::weave::paint
