#pragma once

/** @file
 * The style vocabulary every other TextFlow header speaks. A TextStyle splits
 * into two halves on purpose:
 *   - ShapingStyle — typeface, size, letter spacing, language, OpenType
 *     features, vertical form. Baked into the shape-cache key: change any
 *     field and the words it covers are re-shaped.
 *   - PaintStyle   — an SkPaint foreground plus ordered glyph-paint passes
 *     behind and above it. Resolved at draw time only: recoloring, animating
 *     a shader, or restyling effects never re-shapes and never relayouts.
 *
 * Attach styles to text through Paragraph / ParagraphBuilder (Paragraph.h).
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkBlurTypes.h>
#include <include/core/SkColor.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace textflow {

/// One OpenType feature setting, e.g. {"liga", 0} to disable ligatures.
struct FontFeature {
  char tag[4] = {' ', ' ', ' ', ' '};
  uint32_t value = 1;

  /** Creates the default enabled feature with a blank tag. */
  constexpr FontFeature() = default;
  /** Creates a feature from a four-character OpenType tag and value. */
  constexpr FontFeature(const char (&featureTag)[5], uint32_t featureValue)
      : tag{featureTag[0], featureTag[1], featureTag[2], featureTag[3]},
        value(featureValue) {}
  /** Compares the tag and configured value. */
  constexpr bool operator==(const FontFeature &) const = default;
};

/// One variable-font axis override, e.g. {"wght", 650}. Applied to a
/// style's typeface through FontContext::variedTypeface(), which memoizes
/// the varied SkTypeface clone so identical (typeface, variations) pairs
/// share one instance — and therefore one shape-cache identity.
struct FontVariation {
  char tag[4] = {' ', ' ', ' ', ' '};
  float value = 0;

  /** Creates a blank axis setting. */
  constexpr FontVariation() = default;
  /** Creates an axis setting from a four-character OpenType axis tag. */
  constexpr FontVariation(const char (&axisTag)[5], float axisValue)
      : tag{axisTag[0], axisTag[1], axisTag[2], axisTag[3]}, value(axisValue) {}
  /** Compares the axis tag and design-space value. */
  constexpr bool operator==(const FontVariation &) const = default;
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
  kAuto,        ///< UTR#50 per character: CJK upright, Latin rotated
  kUpright,     ///< force upright (TTB shaping, 'vert' forms)
  kRotated,     ///< force rotated 90° clockwise (book-spine Latin)
  kTateChuYoko, ///< 縦中横: shaped horizontally, set upright in the column —
                ///< for short runs like two-digit numbers in vertical prose
};

/// The subset of style that affects glyph selection and metrics. These
/// fields are baked into the shape-cache key, so changing any of them
/// re-shapes the words it covers. Everything that only affects how
/// already-positioned glyphs are painted belongs in PaintStyle instead and
/// never invalidates shaping.
struct ShapingStyle {
  sk_sp<SkTypeface> typeface; ///< null → FontContext's default (+ fallback)
  float fontSize = 16.0f;     ///< pixels in the target canvas coordinate space
  float letterSpacing = 0.0f; ///< px of tracking added after each cluster
                              ///< (in vertical text this is JIS "aki")
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
  std::string languageTag; ///< e.g. "ja", "sr", "zh-Hant"; empty → default
  std::vector<FontFeature> fontFeatures;
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
  VerticalForm verticalForm = VerticalForm::kAuto;

  /** Compares every input that participates in shaping identity. */
  bool operator==(const ShapingStyle &other) const {
    return typeface.get() == other.typeface.get() &&
           fontSize == other.fontSize && letterSpacing == other.letterSpacing &&
           wordSpacing == other.wordSpacing &&
           languageTag == other.languageTag &&
           fontFeatures == other.fontFeatures &&
           variations == other.variations &&
           textTransform == other.textTransform &&
           verticalForm == other.verticalForm;
  }
};

/** One additional rendering of the positioned glyphs.
 *
 * `paint` is intentionally the complete SkPaint vocabulary rather than a
 * TextFlow mirror of selected fields: callers may use colors, animated
 * shaders, strokes, mask/image/color filters, path effects, and custom
 * blenders. `offset` moves only this pass, which makes shadows and displaced
 * highlights cheap without saveLayer().
 */
struct PaintLayer {
  SkPaint paint;
  SkVector offset = {0, 0};

  /** Constructs an anti-aliased black fill pass. */
  PaintLayer() { paint.setAntiAlias(true); }

  /** Constructs an anti-aliased solid-color fill pass. */
  explicit PaintLayer(SkColor color, SkVector layerOffset = {0, 0})
      : offset(layerOffset) {
    paint.setAntiAlias(true);
    paint.setColor(color);
  }

  /** Wraps a caller-configured SkPaint without changing any of its settings. */
  explicit PaintLayer(SkPaint layerPaint, SkVector layerOffset = {0, 0})
      : paint(std::move(layerPaint)), offset(layerOffset) {}

  /** Returns an arbitrary paint with a normal blur mask attached. */
  static PaintLayer blurred(SkPaint paint, float sigma,
                            SkVector offset = {0, 0}) {
    if (sigma > 0)
      paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma));
    return PaintLayer(std::move(paint), offset);
  }

  /** Preset: a blurred, offset solid copy, normally used as an underlay.
   *
   * `spread` dilates the source shape (stroke-and-fill) before the blur
   * mask is applied, so a wide blur keeps a solid core instead of thinning
   * a hairline glyph outline down to near-transparency. `intensity` scales
   * `color`'s alpha, letting callers push a pass brighter without picking a
   * new hex value; values above 1 are clamped to fully opaque.
   */
  static PaintLayer dropShadow(SkColor color = 0x66000000,
                               SkVector offset = {2, 2},
                               float blurSigma = 2.0f, float spread = 0.0f,
                               float intensity = 1.0f) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(color);
    if (intensity != 1.0f) {
      const float alpha =
          std::clamp(SkColorGetA(color) * intensity, 0.0f, 255.0f);
      paint.setAlphaf(alpha / 255.0f);
    }
    if (spread > 0) {
      paint.setStyle(SkPaint::kStrokeAndFill_Style);
      paint.setStrokeWidth(spread);
    }
    return blurred(std::move(paint), blurSigma, offset);
  }

  /** Preset: a zero-offset blurred copy, normally used as an underlay. See
   *  dropShadow() for what `spread` and `intensity` control. */
  static PaintLayer glow(SkColor color, float blurSigma, float spread = 0.0f,
                        float intensity = 1.0f) {
    return dropShadow(color, {0, 0}, blurSigma, spread, intensity);
  }

  /** Preset: a stroked glyph copy, normally placed beneath the foreground. */
  static PaintLayer outline(SkColor color, float width,
                            SkPaint::Join join = SkPaint::kRound_Join) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(color);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    paint.setStrokeJoin(join);
    return PaintLayer(std::move(paint));
  }

  /** SkPaint compares every scalar and effect-object identity. */
  bool operator==(const PaintLayer &other) const {
    return paint == other.paint && offset == other.offset;
  }
};

/** One line decoration (underline / strikethrough / overline) drawn with a
 * run's resolved paint at draw time.
 *
 * Decorations live on the paint side on purpose: adding, removing, or
 * recoloring one never re-shapes and never relayouts, exactly like paint
 * layers. Thickness and position default to the font's own metrics
 * (SkFontMetrics underline/strikeout values, with sensible fallbacks when a
 * face reports none), so the zero-argument spelling
 * `PaintStyle{...}.addDecoration({})` is a correct underline.
 *
 * Scope (v1): decorations render on straight horizontal runs only —
 * transformed (path/rotated) and vertical runs skip them. Each word draws
 * its own span of decoration, so at word gaps the line is per-word (visible
 * only with skipInk = false).
 */
struct Decoration {
  enum class Kind : uint8_t { kUnderline, kStrikethrough, kOverline };
  Kind kind = Kind::kUnderline;
  /// SK_ColorTRANSPARENT → use the resolved foreground paint's color.
  SkColor color = SK_ColorTRANSPARENT;
  /// 0 → thickness from font metrics, floored at 1px.
  float thickness = 0;
  /// 0 → position from font metrics; otherwise the decoration band's top
  /// edge in px relative to the baseline (positive below, Skia's
  /// y-grows-down convention).
  float offset = 0;
  /// Underlines only: interrupt the line where glyph ink (descenders)
  /// crosses the band, via SkTextBlob::getIntercepts.
  bool skipInk = true;

  /** Compares kind, color, geometry overrides, and ink skipping. */
  bool operator==(const Decoration &) const = default;
};

/** Draw-time glyph appearance with explicit composition order.
 *
 * Underlays are drawn in vector order (back-to-front), followed by
 * `foreground`, then overlays in vector order. The default style owns no
 * vectors and remains exactly one glyph draw. Every added layer costs one
 * additional draw for its style/font bucket; blur and image filters may add
 * backend-specific work beyond that. Updating any paint or shader through
 * Paragraph::setPaint() is visible to an existing ParagraphLayout.
 */
struct PaintStyle {
  SkPaint foreground;
  std::vector<PaintLayer> underlays;
  std::vector<PaintLayer> overlays;
  /// Drawn after this style's glyph passes, in vector order. See Decoration
  /// for defaults and the straight-horizontal-runs-only scope.
  std::vector<Decoration> decorations;

  /** Constructs a single anti-aliased black foreground. */
  PaintStyle() { foreground.setAntiAlias(true); }

  /** Preserves the convenient `PaintStyle{SK_ColorRED}` spelling. */
  PaintStyle(SkColor color) : PaintStyle() { foreground.setColor(color); }

  /** Uses a complete caller-configured SkPaint as the foreground. */
  explicit PaintStyle(SkPaint paint) : foreground(std::move(paint)) {}

  /** Appends a pass behind the foreground and returns this style. */
  PaintStyle &addUnderlay(PaintLayer layer) {
    underlays.push_back(std::move(layer));
    return *this;
  }

  /** Appends a pass above the foreground and returns this style. */
  PaintStyle &addOverlay(PaintLayer layer) {
    overlays.push_back(std::move(layer));
    return *this;
  }

  /** Appends a line decoration and returns this style. */
  PaintStyle &addDecoration(Decoration decoration) {
    decorations.push_back(decoration);
    return *this;
  }

  /** Compares complete paints, layer order, offsets, and decorations. */
  bool operator==(const PaintStyle &other) const {
    return foreground == other.foreground && underlays == other.underlays &&
           overlays == other.overlays && decorations == other.decorations;
  }
};

/** Combines cache-affecting shaping state with draw-time paint state. */
struct TextStyle {
  ShapingStyle shaping;
  PaintStyle paint;

  /** Compares both shaping and paint configuration. */
  bool operator==(const TextStyle &other) const {
    return shaping == other.shaping && paint == other.paint;
  }
};

} // namespace textflow
