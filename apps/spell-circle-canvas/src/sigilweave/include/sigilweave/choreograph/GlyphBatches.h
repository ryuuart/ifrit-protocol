#pragma once

/** @file
 * @ingroup animation
 *
 * Glyphs grouped by (font, paint pass) so a frame of thousands of animated
 * letters collapses into a handful of drawGlyphsRSXform calls. A glyph is
 * added with its span's whole PaintStyle and lands in one bucket per pass
 * it draws; the buckets draw band by band — underlays, then foregrounds,
 * then overlays — so a per-glyph fade that splits a style across buckets
 * never lifts a halo over a neighbour's stroke.
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

/// Glyphs grouped by (font, paint pass): a frame of thousands of animated
/// letters collapses into a handful of drawGlyphsRSXform calls. Reuse one
/// instance across frames — clear() keeps the allocations.
///
/// A glyph is added with a whole PaintStyle and lands in one batch per pass
/// it draws: each underlay in order, then the foreground, then each overlay.
/// Because a batch's key is a complete SkPaint, a pass keeps its gradient,
/// stroke, blend mode and mask filter — animating letters and styling them
/// are not alternatives. Batches draw band by band — every underlay batch,
/// then every foreground batch, then every overlay batch, each band in
/// creation order — so every underlay lands beneath every foreground even
/// when per-glyph fades split one style into several buckets. Creation order
/// alone cannot promise that: the first glyph at a new fade mints its
/// underlay bucket after every earlier fade's foreground bucket, and a
/// blurred halo reaches past its own glyph onto its neighbours' strokes.
///
/// A GlyphDress carries what varies per glyph rather than per pass — the
/// placement, the fade, the tint, a varied face, and the matrix a shear or a
/// non-uniform scale needs. The face joins the bucket key; the fade and the
/// tint change only the resolved paint.
struct GlyphRSXformBatches {
  /// Which stratum of a PaintStyle a bucket's pass came from. The draw
  /// walks these in declaration order, so a bucket's band — not when it was
  /// minted — decides what it composites over.
  enum class PassBand : uint8_t { Underlay, Foreground, Overlay };

  /// One (font, paint pass) bucket: parallel glyph/transform arrays that feed
  /// a single drawGlyphsRSXform call. The font is held as the identity
  /// makeFont() needs rather than as the shaped word it came from, so words
  /// set in the same face and size share one bucket — and so a bucket kept
  /// across frames never outlives a shaped word the cache has since evicted.
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
    /// The glyphs of this bucket that an RSXform cannot place — a shear, a
    /// non-uniform scale — with the matrix each draws under. They cost one
    /// canvas concat and one draw apiece and are the reason to keep them a
    /// separate lane: a glyph whose deviation IS an RSXform never pays for
    /// a neighbour that is not.
    std::vector<SkGlyphID> matrixGlyphs;  ///< parallel to `matrices`
    std::vector<SkMatrix> matrices;
  };
  /** THE GLYPHS ADDED HERE MOVE BETWEEN FRAMES, so their origins are placed
   *  on Skia's SUBPIXEL PHASE GRID instead of on whole pixels.
   *
   *  A glyph mask is rasterized for a quantized origin. Left on whole
   *  pixels, a run creeping along by a fraction of a pixel per frame does
   *  not creep at all: each letter stands still until its own origin
   *  crosses a pixel boundary and then HOPS a whole pixel, at its own
   *  moment, which is exactly the unsteadiness a turning ring shows. On the
   *  phase grid the same creep advances a quarter pixel at a time, and the
   *  hop is a quarter of what it was.
   *
   *  IT IS OFF BY DEFAULT because the grid is the second factor in a
   *  product. Every mask is a (glyph, rotation, phase) triple: the phases
   *  multiply what a rotation ladder has already multiplied, on both axes
   *  for an off-axis run. A run at REST gains nothing from it — its letters
   *  are not creeping anywhere — and would pay the multiplied population
   *  for a placement no one can see move, which is why settled type keeps
   *  whole-pixel origins.
   *
   *  A MOVING run's arithmetic is the other way round. Its masks were never
   *  going to be reused: the rotation it needs this frame is a different
   *  rotation next frame, so the population it mints is per-frame either
   *  way, and the phase grid only refines a mask it was going to rasterize
   *  regardless. This is the same trade the rotation ladder makes and not a
   *  competing one — the ladder still bounds the ROTATIONS, and dropping it
   *  in exchange costs several times what the grid does. */
  bool subpixel = false;
  std::vector<Batch> batches;  ///< one entry per distinct (font, pass) pair
  /// Where the last pass landed. Neighbouring glyphs repeat a pass, and a
  /// full SkPaint is dearer to compare than a color, so the scan starts
  /// where it last succeeded. Bounds-checked: callers own `batches`.
  size_t recentBatch = 0;

  /** Returns the batch for one shaped word's font and one resolved pass.
   * `face` overrides the shaped word's own typeface when it is non-null —
   * the varied clone a driven variable-font axis asks for — and is part of
   * the key, so the same word set at two axis coordinates is two buckets. */
  [[nodiscard]] Batch& batchForPass(const ShapedWord* font,
                                    const sk_sp<SkTypeface>& face,
                                    const SkPaint& paint, SkVector offset,
                                    PassBand band);

  /** Appends one glyph — once per pass of `style` — anchored at its
   * advance-centre `centerPosition` and rotated by (`cosine`, `sine`), the
   * placement convention the effects use.
   *
   * `alphaScale` multiplies every pass's alpha, which is how a per-glyph
   * fade stays batched: the style itself is untouched and only the resolved
   * paint differs. Quantize it if the effect drives it continuously —
   * distinct alphas are distinct buckets. Passes with nothing to draw are
   * skipped, so a fully faded glyph costs no bucket at all.
   */
  void addGlyph(const ShapedWord* font, const PaintStyle& style,
                SkGlyphID glyph, float halfAdvance, SkPoint centerPosition,
                float cosine = 1, float sine = 0, float alphaScale = 1.0f) {
    addGlyph(font, style, glyph, halfAdvance,
             GlyphDress{.center = centerPosition,
                        .cosine = cosine,
                        .sine = sine,
                        .alphaScale = alphaScale});
  }

  /** Appends one dressed glyph — once per pass of `style` — placed, faded,
   * tinted and faced as `dress` says.
   *
   * The tint and the fade never touch the style itself: only the RESOLVED
   * paint of each pass differs, which is what keeps a coloured, faded letter
   * in the same handful of buckets as its neighbours. Passes with nothing to
   * draw are skipped, so a fully faded glyph costs no bucket at all.
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

  /** Clears glyph data while retaining batch allocations for the next frame.
   *
   * A frame that minted a pathological number of buckets releases them
   * instead, because a retained bucket also retains its paint — and with it
   * every shader, filter and blender that paint holds a reference to.
   */
  void clear();

  /** Draws every batch — underlay buckets, then foreground buckets, then
   * overlay buckets, each band in creation order — and returns the number
   * of glyph draws it issued, one per glyph per pass. The band walk is what
   * keeps a blurred halo beneath a neighbouring letter's stroke when
   * per-glyph fades have split the style across several buckets. */
  int draw(SkCanvas* canvas) const;

 private:
  /** One bucket's draws: the shared RSXform lane, then its matrix lane. */
  static int drawBatch(SkCanvas* canvas, const Batch& batch, bool subpixel);
};

}  // namespace sigil::weave
