#pragma once

/** @file
 * @ingroup shaping
 *
 * Named OpenType feature presets — CSS font-variant-* vocabulary as
 * ready-made FontFeature values, so styles read
 * `style.shaping.fontFeatures = {features::tabularNumbers}` instead of
 * hand-spelled four-cc tag lists. Header-only; every constant is a plain
 * FontFeature and combines freely with hand-rolled features.
 */

#include "sigilweave/style/Style.h"

namespace sigil::weave::features {

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

// ── Vertical typesetting (CJK columns) ───────────────────────────────────
// A column asks the face for more than a line does. Shaping a run
// top-to-bottom already applies the face's vertical forms ("vert") and
// reads its vertical metrics; everything below is what a setting asks for
// on top of that, and each is off until a style names it.
//
// A NAMED FEATURE IS NOT GATED ON THE DIRECTION. The shaper runs the
// lookups a style asks for whichever way the run is set, so a style
// carrying these and set along a line takes them there too — substituting
// forms cut for a column, and moving ink off a baseline that was meant to
// move along a column axis. Carry them on the styles a passage sets
// vertically.
/// Suppresses the vertical forms a column takes by default ("vert" 0) —
/// the brackets and long vowel marks stay in their horizontal shapes.
inline constexpr FontFeature verticalFormsOff{"vert", 0};
/// The wider rotation set some faces carry beside their vertical forms
/// ("vrt2"), covering characters "vert" leaves alone.
inline constexpr FontFeature verticalRotatedForms{"vrt2", 1};
/// Alternate vertical positioning ("valt") — the face's own recentring of
/// punctuation on the column axis, which sits off-centre for a line.
inline constexpr FontFeature verticalAlternates{"valt", 1};
/// Proportional vertical metrics ("vpal") — full-width punctuation set to
/// the space its ink needs down the column rather than to a full em.
inline constexpr FontFeature proportionalVerticalMetrics{"vpal", 1};
/// Half-em vertical metrics ("vhal") — the tighter fixed alternative to
/// the proportional set above.
inline constexpr FontFeature halfWidthVerticalMetrics{"vhal", 1};
/// Kana forms cut for a column ("vkna") — small kana sit where a column
/// reads them rather than where a line does.
inline constexpr FontFeature verticalKana{"vkna", 1};
/// Vertical kerning ("vkrn") — the pair adjustments a column needs, which
/// horizontal kerning does not carry.
inline constexpr FontFeature verticalKerning{"vkrn", 1};

/** Returns the stylistic-set feature ss01…ss20 for `index` in [1, 20];
 * indices outside that range clamp into it. */
[[nodiscard]] constexpr FontFeature stylisticSet(int index) {
  const int clampedIndex = index < 1 ? 1 : (index > 20 ? 20 : index);
  FontFeature feature{"ss00", 1};
  feature.tag[2] = static_cast<char>('0' + clampedIndex / 10);
  feature.tag[3] = static_cast<char>('0' + clampedIndex % 10);
  return feature;
}

// The tags are the whole contract of this header, and they are decided at
// compile time — so the compiler is what holds them to it. A wrong four-cc
// or a stylistic set that does not clamp fails here rather than reaching a
// shaper.
static_assert(tabularNumbers == FontFeature{"tnum", 1});
static_assert(standardLigaturesOff == FontFeature{"liga", 0});
static_assert(smallCaps == FontFeature{"smcp", 1});
static_assert(stylisticSet(1) == FontFeature{"ss01", 1});
static_assert(stylisticSet(7) == FontFeature{"ss07", 1});
static_assert(stylisticSet(20) == FontFeature{"ss20", 1});
static_assert(stylisticSet(0) == FontFeature{"ss01", 1});
static_assert(stylisticSet(99) == FontFeature{"ss20", 1});

}  // namespace sigil::weave::features
