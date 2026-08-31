#pragma once

/** @file
 * @ingroup shaping
 *
 * The shaping half of a text style: typeface, size, tracking, language,
 * OpenType features, variable-font axes, case transform and vertical
 * form. Every field is baked into the shape-cache key, so changing one
 * re-shapes the words it covers; anything that only changes how the
 * positioned glyphs are painted belongs in PaintStyle instead.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sigil::weave {

/// One OpenType feature setting, e.g. {"liga", 0} to disable ligatures.
struct FontFeature {
  char tag[4] = {' ', ' ', ' ', ' '};  ///< OpenType feature tag, unterminated
  uint32_t value = 1;  ///< 0 disables, 1 enables, higher selects alternates

  /** Creates the default enabled feature with a blank tag. */
  constexpr FontFeature() = default;
  /** Creates a feature from a four-character OpenType tag and value. */
  constexpr FontFeature(const char (&featureTag)[5], uint32_t featureValue)
      : tag{featureTag[0], featureTag[1], featureTag[2], featureTag[3]},
        value(featureValue) {}
  /** Compares the tag and configured value. */
  constexpr bool operator==(const FontFeature&) const = default;
};

/// One variable-font axis override, e.g. {"wght", 650}. Applied to a
/// style's typeface through FontContext::variedTypeface(), which memoizes
/// the varied SkTypeface clone so identical (typeface, variations) pairs
/// share one instance — and therefore one shape-cache identity.
struct FontVariation {
  char tag[4] = {' ', ' ', ' ', ' '};  ///< OpenType axis tag, unterminated
  float value = 0;                     ///< design-space coordinate on the axis

  /** Creates a blank axis setting. */
  constexpr FontVariation() = default;
  /** Creates an axis setting from a four-character OpenType axis tag. */
  constexpr FontVariation(const char (&axisTag)[5], float axisValue)
      : tag{axisTag[0], axisTag[1], axisTag[2], axisTag[3]}, value(axisValue) {}
  /** Compares the axis tag and design-space value. */
  constexpr bool operator==(const FontVariation&) const = default;
};

/// Case transformation applied to a span's text just before shaping (CSS
/// text-transform). The shaped glyphs come from the transformed text while
/// the Paragraph's stored text, edit ranges, and query results all remain
/// untransformed — matching how browser engines treat the property as a
/// rendering effect, not an edit. Because the transformed text is itself
/// the shape-cache key text, "HELLO" typed directly and "hello" with
/// kUppercase share one cache entry. Case mapping is locale-sensitive via
/// ShapingStyle::languageTag (Turkish dotless-i works unprompted).
///
/// Caveat: length-changing mappings (German ß → SS) make per-character
/// cluster indices within such a word approximate for hit-testing; line
/// breaking runs on the untransformed text.
enum class TextTransform : uint8_t {
  kNone,
  kUppercase,
  kLowercase,
  /// Titlecases only the first letter of each word; the rest of the word is
  /// left untouched (CSS semantics — not ICU's lowercase-the-remainder
  /// title mapping).
  kCapitalize,
};

/// How a span behaves when its paragraph is laid out vertically
/// (Paragraph::setWritingMode(WritingMode::kVerticalRL)). Ignored in
/// horizontal paragraphs.
enum class VerticalForm : uint8_t {
  kAuto,         ///< UTR#50 per character: CJK upright, Latin rotated
  kUpright,      ///< force upright (TTB shaping, 'vert' forms)
  kRotated,      ///< force rotated 90° clockwise (book-spine Latin)
  kTateChuYoko,  ///< 縦中横: shaped horizontally, set upright in the column —
                 ///< for short runs like two-digit numbers in vertical prose
};

/// The subset of style that affects glyph selection and metrics. These
/// fields are baked into the shape-cache key, so changing any of them
/// re-shapes the words it covers. Everything that only affects how
/// already-positioned glyphs are painted belongs in PaintStyle instead and
/// never invalidates shaping.
struct ShapingStyle {
  sk_sp<SkTypeface> typeface;  ///< null → FontContext's default (+ fallback)
  float fontSize = 16.0f;      ///< pixels in the target canvas coordinate space
  float letterSpacing = 0.0f;  ///< px of tracking added after each cluster
                               ///< (in vertical text this is JIS "aki")
  /// Horizontal glyph condensation (CSS font-stretch by transform): glyph
  /// shapes AND advances scale by this on the x axis. It is how a face with
  /// no `wdth` axis is condensed or extended. Letter-spacing is NOT scaled
  /// (matches CSS). Part of the shape-cache key; vertical text condenses
  /// glyph width only, never column advance.
  float scaleX = 1.0f;
  /// Extra px added to each word's trailing-whitespace glue (CSS
  /// word-spacing). Applied after the whitespace is measured, so changing
  /// it re-derives words at pure shape-cache-hit cost — it is compared for
  /// restyle detection but is NOT part of the shape-cache key. Negative
  /// values shrink gaps; the glue is floored at zero.
  float wordSpacing = 0.0f;
  /// BCP-47 language used both for language-sensitive font fallback and by
  /// HarfBuzz to select OpenType language systems / localized (`locl`)
  /// substitutions. It is deliberately part of the shape key: even when the
  /// resolved typeface is unchanged, language can change its emitted glyphs.
  /// Bidi direction is analyzed separately and does not come from this tag.
  std::string languageTag;  ///< e.g. "ja", "sr", "zh-Hant"; empty → default
  std::vector<FontFeature> fontFeatures;  ///< passed to HarfBuzz verbatim
  /// Design-space overrides applied to `typeface` (or the context default)
  /// before shaping — the ergonomic alternative to pre-building a varied
  /// SkTypeface via SkFontArguments yourself. Resolution goes through
  /// FontContext's memoized clone cache, so the varied face's uniqueID is a
  /// stable shape-cache identity and HarfBuzz mirrors the same design
  /// position Skia rasterizes. Order-sensitive: [{"wght",700},{"wdth",80}]
  /// and its permutation resolve to equivalent faces but occupy two memo
  /// entries — keep a consistent order at call sites.
  std::vector<FontVariation> variations;
  /// Case transformation applied before shaping; see TextTransform for the
  /// cache and hit-testing story.
  TextTransform textTransform = TextTransform::kNone;
  VerticalForm verticalForm = VerticalForm::kAuto;  ///< ignored in horizontal
                                                    ///< paragraphs

  /** Draw glyphs with HARD edges — no antialiasing.
   *
   *  This lives on the style because Skia takes glyph edging from the
   *  `SkFont`, never from the paint: `paint.foreground.setAntiAlias(false)`
   *  is silently ignored on text, so a caller has no other way to ask for
   *  aliased glyphs while still going through shaping, bidi, fallback and
   *  flow geometry.
   *
   *  It selects a rasterisation, not a face. The outlines are unchanged;
   *  only their coverage is thresholded, which is what small bitmap-era UI
   *  type looks like. Part of the shape-cache key, since the shaped run
   *  carries the flag through to the SkFont used at draw time. */
  bool aliased = false;

  /** Compares every input that participates in shaping identity. */
  bool operator==(const ShapingStyle& other) const {
    return typeface.get() == other.typeface.get() &&
           fontSize == other.fontSize && letterSpacing == other.letterSpacing &&
           scaleX == other.scaleX && aliased == other.aliased &&
           wordSpacing == other.wordSpacing &&
           languageTag == other.languageTag &&
           fontFeatures == other.fontFeatures &&
           variations == other.variations &&
           textTransform == other.textTransform &&
           verticalForm == other.verticalForm;
  }
};

}  // namespace sigil::weave
