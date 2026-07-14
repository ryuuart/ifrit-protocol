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
  /// BCP-47 language used both for language-sensitive font fallback and by
  /// HarfBuzz to select OpenType language systems / localized (`locl`)
  /// substitutions. It is deliberately part of the shape key: even when the
  /// resolved typeface is unchanged, language can change its emitted glyphs.
  /// Bidi direction is analyzed separately and does not come from this tag.
  std::string languageTag; ///< e.g. "ja", "sr", "zh-Hant"; empty → default
  std::vector<FontFeature> fontFeatures;
  VerticalForm verticalForm = VerticalForm::kAuto;

  /** Compares every input that participates in the shape-cache key. */
  bool operator==(const ShapingStyle &other) const {
    return typeface.get() == other.typeface.get() &&
           fontSize == other.fontSize && letterSpacing == other.letterSpacing &&
           languageTag == other.languageTag &&
           fontFeatures == other.fontFeatures &&
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

  /** Compares complete paints, layer order, and layer offsets. */
  bool operator==(const PaintStyle &other) const {
    return foreground == other.foreground && underlays == other.underlays &&
           overlays == other.overlays;
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
