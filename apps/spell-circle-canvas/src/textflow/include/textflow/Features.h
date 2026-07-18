#pragma once

/** @file
 * @ingroup shaping
 *
 * Named OpenType feature presets — CSS font-variant-* vocabulary as
 * ready-made FontFeature values, so styles read
 * `style.shaping.fontFeatures = {Features::tabularNumbers}` instead of
 * hand-spelled four-cc tag lists. Header-only; every constant is a plain
 * FontFeature and combines freely with hand-rolled features.
 */

#include "Style.h"

namespace textflow::Features {

// ── Numerals (CSS font-variant-numeric) ──────────────────────────────────
/// Equal-width figures for tables and timers ("tnum").
inline constexpr FontFeature tabularNumbers{"tnum", 1};
/// Naturally-spaced figures for running text ("pnum").
inline constexpr FontFeature proportionalNumbers{"pnum", 1};
/// Lowercase-style figures with ascenders/descenders ("onum").
inline constexpr FontFeature oldstyleNumbers{"onum", 1};
/// Uppercase-height lining figures ("lnum").
inline constexpr FontFeature liningNumbers{"lnum", 1};
/// Slashed zero to distinguish 0 from O ("zero").
inline constexpr FontFeature slashedZero{"zero", 1};
/// Diagonal fractions from digit/slash sequences ("frac").
inline constexpr FontFeature fractions{"frac", 1};
/// Ordinal markers like 1st → 1ˢᵗ ("ordn").
inline constexpr FontFeature ordinals{"ordn", 1};

// ── Capitals (CSS font-variant-caps) ─────────────────────────────────────
/// Lowercase → small capitals ("smcp").
inline constexpr FontFeature smallCaps{"smcp", 1};
/// Uppercase → small capitals ("c2sc"); combine with smallCaps for
/// all-small-caps.
inline constexpr FontFeature capitalsToSmallCaps{"c2sc", 1};

// ── Ligatures & alternates (CSS font-variant-ligatures / -alternates) ────
/// Disables standard ligatures ("liga" 0) — e.g. for code.
inline constexpr FontFeature standardLigaturesOff{"liga", 0};
/// Disables contextual ligatures ("clig" 0).
inline constexpr FontFeature contextualLigaturesOff{"clig", 0};
/// Enables discretionary ligatures ("dlig").
inline constexpr FontFeature discretionaryLigatures{"dlig", 1};
/// Enables historical ligatures ("hlig").
inline constexpr FontFeature historicalLigatures{"hlig", 1};
/// Disables contextual alternates ("calt" 0).
inline constexpr FontFeature contextualAlternatesOff{"calt", 0};
/// Enables swash forms ("swsh").
inline constexpr FontFeature swashes{"swsh", 1};

/** Returns the stylistic-set feature ss01…ss20 for `index` in [1, 20];
 * indices outside that range clamp into it. */
[[nodiscard]] constexpr FontFeature stylisticSet(int index) {
  const int clampedIndex = index < 1 ? 1 : (index > 20 ? 20 : index);
  FontFeature feature{"ss00", 1};
  feature.tag[2] = static_cast<char>('0' + clampedIndex / 10);
  feature.tag[3] = static_cast<char>('0' + clampedIndex % 10);
  return feature;
}

} // namespace textflow::Features
