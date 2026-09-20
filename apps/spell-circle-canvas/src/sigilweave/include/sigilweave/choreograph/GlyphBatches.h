#pragma once

/** @file
 * @ingroup weave-animation
 *
 * Glyphs grouped by font and paint pass so a frame of thousands of
 * animated letters collapses into a handful of RSXform draw calls, and
 * the buckets draw band by band so a per-glyph fade never lifts a halo
 * over a neighbour's stroke.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sigilweave/choreograph/GlyphDress.h"
#include "sigilweave/choreograph/PlacedGlyph.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/style/Style.h"

namespace sigil::weave {

/// Glyphs grouped by font and paint pass, so a frame of thousands of
/// animated letters collapses into a handful of RSXform draw calls. Reuse
/// one instance across frames — `clear` keeps the allocations. A glyph is
/// added with a whole PaintStyle and lands in one batch per pass it
/// draws, and because a batch's key is a complete SkPaint a pass keeps
/// its gradient, stroke, blend mode and mask filter. Batches draw BAND BY
/// BAND rather than in creation order, which is what keeps every underlay
/// beneath every foreground when per-glyph fades split one style.
struct GlyphRSXformBatches {
  /// Which stratum of a PaintStyle a bucket's pass came from. The draw
  /// walks these in declaration order, so a bucket's band — not when it was
  /// minted — decides what it composites over.
  enum class PassBand : uint8_t { Underlay, Foreground, Overlay };

  /// One font-and-pass bucket: parallel glyph and transform arrays that
  /// feed a single RSXform draw. The font is held as the identity
  /// `makeFont` needs rather than as the shaped word it came from, so
  /// words set in one face and size share a bucket and no bucket kept
  /// across frames outlives a shaped word the cache has evicted.
  struct Batch {
    sk_sp<SkTypeface> typeface;  ///< bucket key: the face to draw with
    float fontSize = 0;          ///< bucket key: px size
    float scaleX = 1.0f;         ///< bucket key: horizontal
                                 ///< condensation baked into shaping
    bool aliased = false;        ///< bucket key: hard-edged raster
    SkPaint paint;               ///< bucket key: the complete pass
    SkVector offset = {0, 0};    ///< bucket key: the pass's own
                                 ///< translation (shadows, echoes)
    /// Bucket key: the stratum this pass draws in. The same paint used as
    /// one style's underlay and another's foreground is two buckets,
    /// because the two composite differently.
    PassBand band = PassBand::Foreground;
    std::vector<SkGlyphID> glyphs;      ///< parallel to `transforms`
    std::vector<SkRSXform> transforms;  ///< per-glyph scale/rotate/translate
    /// The glyphs of this bucket that an RSXform cannot place — a shear,
    /// a non-uniform scale — with the matrix each draws under. They cost
    /// one canvas concat and one draw apiece, and are a separate lane so
    /// that a glyph an RSXform can place never pays for one it cannot.
    std::vector<SkGlyphID> matrixGlyphs;  ///< parallel to `matrices`
    std::vector<SkMatrix> matrices;
  };
  /** THE GLYPHS ADDED HERE MOVE BETWEEN FRAMES, so their origins go on
   *  Skia's SUBPIXEL PHASE GRID instead of on whole pixels: a run creeping
   *  by a fraction of a pixel advances a quarter pixel at a time rather
   *  than standing still and hopping a whole one.
   *  @trap Off by default: every mask is a glyph, rotation and phase
   *  triple, and a run AT REST pays that strike population unseen. */
  bool subpixel = false;
  std::vector<Batch> batches;  ///< one entry per distinct (font, pass) pair
  /// Where the last pass landed: neighbouring glyphs repeat a pass and a
  /// full SkPaint is dear to compare, so the scan starts where it last
  /// succeeded. Bounds-checked, callers owning `batches`.
  size_t recentBatch = 0;

  /** Returns the batch for one shaped word's font and one resolved pass.
   * `face` overrides the shaped word's own typeface when it is non-null —
   * the varied clone a driven variable-font axis asks for — and is part of
   * the key, so the same word set at two axis coordinates is two buckets. */
  [[nodiscard]] Batch& batchForPass(const ShapedWord* font,
                                    const sk_sp<SkTypeface>& face,
                                    const SkPaint& paint, SkVector offset,
                                    PassBand band);

  /** Appends one glyph, once per pass of @p style, anchored at its
   * advance-centre and rotated by the cosine and sine — the placement
   * convention the effects use. @p alphaScale multiplies every pass's
   * alpha, leaving the style untouched so a fade stays batched.
   * @trap Quantize a continuously driven alpha: distinct alphas are
   * distinct buckets. */
  void addGlyph(const ShapedWord* font, const PaintStyle& style,
                SkGlyphID glyph, float halfAdvance, SkPoint centerPosition,
                float cosine = 1, float sine = 0, float alphaScale = 1.0f) {
    addGlyph(font, style, glyph, halfAdvance,
             GlyphDress{.center = centerPosition,
                        .cosine = cosine,
                        .sine = sine,
                        .alphaScale = alphaScale});
  }

  /** Appends one dressed glyph, once per pass of @p style, placed,
   * faded, tinted and faced as @p dress says. The tint and the fade never
   * touch the style itself, only the RESOLVED paint of each pass.
   * @silent a pass has nothing to draw, so a fully faded glyph costs no
   * bucket at all.
   */
  void addGlyph(const ShapedWord* font, const PaintStyle& style,
                SkGlyphID glyph, float halfAdvance, const GlyphDress& dress);

  /** Appends a visited glyph at `centerPosition`, taking its font, advance
   * and span paint from the walk. The ergonomic pairing with
   * forEachPlacedGlyph.
   */
  void addGlyph(const PlacedGlyph& placed, SkPoint centerPosition,
                float cosine = 1, float sine = 0, float alphaScale = 1.0f) {
    addGlyph(placed.shaped, *placed.paint, placed.glyph, placed.advance * 0.5f,
             centerPosition, cosine, sine, alphaScale);
  }

  /** Appends a visited glyph dressed as `dress` says, taking its font,
   * advance and span paint from the walk. `glyph` overrides the walk's own
   * glyph ID, which is how a code-point substitution draws a different
   * letter at the original's pen position. */
  void addGlyph(const PlacedGlyph& placed, const GlyphDress& dress,
                SkGlyphID glyph) {
    addGlyph(placed.shaped, *placed.paint, glyph, placed.advance * 0.5f, dress);
  }
  void addGlyph(const PlacedGlyph& placed, const GlyphDress& dress) {
    addGlyph(placed, dress, placed.glyph);
  }

  /** Clears glyph data while retaining batch allocations for the next
   * frame — except after a frame that minted a pathological number of
   * buckets, which releases them, a retained bucket also retaining its
   * paint and every shader and filter that paint references.
   */
  void clear();

  /** Draws every batch — underlay buckets, then foreground buckets, then
   * overlay buckets, each band in creation order — and returns the number
   * of glyph draws it issued, one per glyph per pass. */
  int draw(SkCanvas* canvas) const;

 private:
  /** One bucket's draws: the shared RSXform lane, then its matrix lane. */
  static int drawBatch(SkCanvas* canvas, const Batch& batch, bool subpixel);
};

}  // namespace sigil::weave
