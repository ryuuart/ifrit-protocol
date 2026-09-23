#pragma once

/** @file
 * @ingroup weave-animation
 *
 * How one glyph is dressed for a batched draw — where it lands, what it
 * is faded and tinted by, which face it draws with — with the rotation
 * snap and the memoized colour filter a dressing reaches for.
 */

#include <include/core/SkColor.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilmaterial/color/Color.h>

namespace sigil::weave {

/** Quantizes @p angle to a cosine and sine on a 64-step table, about
 * 5.6 degrees per step: continuous per-letter angles would re-rasterize
 * every glyph mask every frame on the CPU raster backend.
 */
void quantizeAngle(float angle, float& cosine, float& sine);

/** The same snap on a CALLER-CHOSEN ladder of @p steps directions round
 * the circle, computed rather than tabled because a caller cuts the
 * ladder by rendered glyph size: a ladder that vanishes on a caption
 * ticks visibly on display type. At 64 steps it answers bit for bit what
 * the tabled overload answers, and 0 or less is the exact angle. */
void quantizeAngle(float angle, int steps, float& cosine, float& sine);

/** Returns the colour filter that scales a pass's RED, GREEN and BLUE by
 * @p tint, adds @p add and screens @p screen, composed over @p under,
 * which runs first; alpha is left alone. MEMOIZED, the least recently
 * used entry evicted past the cap.
 * @trap The memo is a correctness requirement: a batch's key compares its
 * colour filter by POINTER, so quantize the tint or mint a bucket a glyph. */
sk_sp<SkColorFilter> tintFilter(const material::Color& tint,
                                sk_sp<SkColorFilter> under,
                                const material::Color& add = {0, 0, 0, 0},
                                const material::Color& screen = {0, 0, 0, 0});

/// How one glyph is DRESSED for a batched draw: where it lands, what it
/// is faded and tinted by, and which face it draws with. Everything here
/// is per-GLYPH and nothing here is per-pass — a dressed glyph still
/// draws its span's whole PaintStyle, one bucket per pass.
struct GlyphDress {
  SkPoint center = {0, 0};  ///< where the glyph's advance-centre lands
  float cosine = 1;         ///< rotation with the uniform scale folded in,
  float sine = 0;           ///< the RSXform convention
  /// Multiplies every pass's alpha. Quantize it if an effect drives it
  /// continuously — distinct alphas are distinct buckets.
  float alphaScale = 1.0f;
  /// Multiplies every pass's colour, channel by channel: a flat colour
  /// directly, and a shader through the equivalent modulating filter, so
  /// a gradient keeps its ramp and takes the tint over it. Alpha folds
  /// into `alphaScale`, and white is no tint.
  material::Color colorMultiplier = {1, 1, 1, 1};
  /// Added to every pass's colour after the multiply, clamped at the draw
  /// — the flash a multiplier cannot brighten into. RGB only; the alpha
  /// component is never read, coverage being `alphaScale`'s lane. Zero is
  /// no flash, and keeps the untouched-paint fast path.
  material::Color colorAdd = {0, 0, 0, 0};
  /// Screened over every pass's colour after the add — c becomes
  /// 1 − (1 − c)(1 − screen) — the glow that lifts each channel by its
  /// headroom and never clips. RGB only, as `colorAdd`. Zero is no glow.
  /// Screening against a constant is affine per channel, so all three
  /// colour terms ride the one memoized matrix filter together.
  material::Color colorScreen = {0, 0, 0, 0};
  /// The face to draw with, or null for the shaped word's own — a varied
  /// clone for a glyph whose effect drives a variable-font axis. It is part
  /// of the bucket key, so two faces are two buckets.
  sk_sp<SkTypeface> face;
  /// Non-null: the glyph-local vector from the glyph's DRAW ORIGIN to
  /// the pose centre — the point the rotation and the scale turn about.
  /// Null keeps the horizontal convention, half the advance to the right,
  /// which a vertical column cannot use. Borrowed for the call.
  const SkVector* centreOffset = nullptr;
  /// Non-null: draw this glyph under this MATRIX instead of an RSXform,
  /// which is the only way to place a shear or a non-uniform scale. It
  /// carries the whole placement, so `center`, `cosine` and `sine` are
  /// unread when it is set. Borrowed for the call.
  const SkMatrix* matrix = nullptr;
};

}  // namespace sigil::weave
