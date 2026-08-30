#pragma once

/** @file
 * @ingroup animation
 *
 * How one glyph is dressed for a batched draw — where it lands, what it is
 * faded and tinted by, which face it draws with, and the matrix a shear or
 * a non-uniform scale needs — with the two helpers a dressing reaches for:
 * the rotation snap that keeps tumbling letters from minting a fresh glyph
 * mask per frame, and the memoized colour filter that folds a tint, a flash
 * and a glow into one matrix a batch can key on by pointer.
 */

#include <include/core/SkColor.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

namespace sigil::weave {

/** Quantizes `angle` to `cosine`/`sine` on a 64-step table (≈5.6° per step,
 * visually indistinguishable for tumbling letters): continuous per-letter
 * angles would re-rasterize every glyph mask every frame on the CPU raster
 * backend. The GPU backend doesn't need this, but it doesn't hurt there
 * either.
 */
void quantizeAngle(float angle, float& cosine, float& sine);

/** The same snap on a CALLER-CHOSEN ladder: `steps` directions round the
 * circle, computed rather than tabled because a caller cuts the ladder by
 * rendered glyph size — one step turns an outline by a fixed angle, a fixed
 * angle displaces a glyph's extremity by more pixels the larger the glyph
 * is drawn, so a size-blind ladder that vanishes on a caption ticks
 * visibly on display type. At 64 steps this answers bit for bit what the
 * tabled overload answers. `steps <= 0` is the exact angle — the
 * continuous opt-out spelled as a ladder of none. */
void quantizeAngle(float angle, int steps, float& cosine, float& sine);

/** Returns the colour filter that scales a pass's RED, GREEN and BLUE by
 * `tint`, then adds `add` and screens `screen` over the result — composed
 * over whatever filter the pass already carried (that one runs first, so
 * the modulation applies to the finished colour). Alpha is left alone: a
 * per-glyph fade rides the paint's own alpha instead.
 *
 * ALL THREE TERMS RIDE ONE COLOUR MATRIX, because screening against a
 * constant is affine per channel — c → c(1−s) + s — so multiply, add and
 * screen fold into one scale-and-bias: c·tint·(1−s) + (add·(1−s) + s).
 * The matrix filter clamps its output, which is where "added, clamped"
 * happens on this path. Neutral add and screen (all zero) contribute a
 * zero bias, leaving exactly the scale-only matrix a bare tint builds.
 *
 * MEMOIZED, and that is a correctness requirement rather than a saving: a
 * batch's key is a whole SkPaint, and SkPaint compares its colour filter by
 * POINTER, so a freshly built filter per glyph would mint a bucket per glyph
 * and undo the batching entirely. Callers quantize the tint for the same
 * reason they quantize alpha; the cap is what keeps a caller that does not
 * from growing the table without bound.
 *
 * Past the cap the LEAST RECENTLY USED entry goes, one at a time. What that
 * buys over emptying the table is the case where the cap is reached at all:
 * a caller whose live tints sit just over the cap would, with a wholesale
 * drop, lose every filter it is still using — including the ones it asks for
 * again on the same frame — and rebuild them all, repeatedly. Evicting the
 * coldest entry instead costs a caller only the tints it has stopped using,
 * so a working set at the cap keeps its identities stable and its batching
 * intact.
 *
 * The composed filter holds a reference to `under`, and the table holds the
 * composed filter, so the address `under` contributes to the key cannot be
 * recycled underneath a live entry.
 */
sk_sp<SkColorFilter> tintFilter(const SkColor4f& tint,
                                sk_sp<SkColorFilter> under,
                                const SkColor4f& add = {0, 0, 0, 0},
                                const SkColor4f& screen = {0, 0, 0, 0});

/// How one glyph is DRESSED for a batched draw: where it lands, what it is
/// faded and tinted by, and which face it draws with.
///
/// Everything here is per-GLYPH and nothing here is per-pass: a dressed glyph
/// still draws its span's whole PaintStyle, one bucket per pass.
struct GlyphDress {
  SkPoint center = {0, 0};  ///< where the glyph's advance-centre lands
  float cosine = 1;         ///< rotation with the uniform scale folded in,
  float sine = 0;           ///< the RSXform convention
  /// Multiplies every pass's alpha. Quantize it if an effect drives it
  /// continuously — distinct alphas are distinct buckets.
  float alphaScale = 1.0f;
  /// Multiplies every pass's colour, channel by channel. A pass painting a
  /// flat colour multiplies that colour; a pass painting a shader (or
  /// already carrying a colour filter) gets the equivalent modulating
  /// filter, so a gradient keeps its ramp and takes the tint over it.
  /// Alpha here folds into `alphaScale`. White is no tint.
  SkColor4f colorMul = {1, 1, 1, 1};
  /// Added to every pass's colour after the multiply, clamped at the draw
  /// — the flash a multiplier cannot brighten into. RGB only; the alpha
  /// component is never read, coverage being `alphaScale`'s lane. Zero is
  /// no flash, and keeps the untouched-paint fast path.
  SkColor4f colorAdd = {0, 0, 0, 0};
  /// Screened over every pass's colour after the add — c becomes
  /// 1 − (1 − c)(1 − screen) — the glow that lifts each channel by its
  /// headroom and never clips. RGB only, as `colorAdd`. Zero is no glow.
  /// Screening against a constant is affine per channel, so all three
  /// colour terms ride the one memoized matrix filter together.
  SkColor4f colorScreen = {0, 0, 0, 0};
  /// The face to draw with, or null for the shaped word's own — a varied
  /// clone for a glyph whose effect drives a variable-font axis. It is part
  /// of the bucket key, so two faces are two buckets.
  sk_sp<SkTypeface> face;
  /// Non-null: the glyph-local vector from the glyph's DRAW ORIGIN to the
  /// pose centre `center` names — the point the rotation and the scale turn
  /// about. Null keeps the horizontal convention, (halfAdvance, 0).
  ///
  /// A vertical column needs it: an upright glyph's advance runs down the
  /// page while the glyph itself is drawn from a horizontal origin, so half
  /// its advance to the RIGHT is half a column pitch away from anything the
  /// eye would call its centre. Borrowed for the duration of the call.
  const SkVector* centreOffset = nullptr;
  /// Non-null: draw this glyph under this MATRIX instead of an RSXform,
  /// which is the only way to place a shear or a non-uniform scale. It
  /// carries the whole placement — centre, rotation and scale included — so
  /// `center`, `cosine` and `sine` are unread when it is set. Borrowed for
  /// the duration of the addGlyph call.
  const SkMatrix* matrix = nullptr;
};

}  // namespace sigil::weave
