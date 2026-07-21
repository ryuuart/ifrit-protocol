#pragma once

/** @file
 * @ingroup shaping
 *
 * The style vocabulary every other SigilWeave header speaks. A TextStyle splits
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
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::weave {

/// One OpenType feature setting, e.g. {"liga", 0} to disable ligatures.
struct FontFeature {
  char tag[4] = {' ', ' ', ' ', ' '}; ///< OpenType feature tag, unterminated
  uint32_t value = 1; ///< 0 disables, 1 enables, higher selects alternates

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
  char tag[4] = {' ', ' ', ' ', ' '}; ///< OpenType axis tag, unterminated
  float value = 0;                    ///< design-space coordinate on the axis

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
  std::vector<FontFeature> fontFeatures; ///< passed to HarfBuzz verbatim
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
  VerticalForm verticalForm = VerticalForm::kAuto; ///< ignored in horizontal
                                                   ///< paragraphs

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
 * SigilWeave mirror of selected fields: callers may use colors, animated
 * shaders, strokes, mask/image/color filters, path effects, and custom
 * blenders. `offset` moves only this pass, which makes shadows and displaced
 * highlights cheap without saveLayer().
 */
struct PaintLayer {
  SkPaint paint;            ///< applied as configured — nothing is overridden
  SkVector offset = {0, 0}; ///< px translation of this pass only

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
 * By default a decoration spans the decorated range, not individual words:
 * contiguous same-style runs on a line merge into one continuous band that
 * also covers the glue between words (CSS behavior — an underlined sentence
 * is one line, a highlight reads like one marker stroke). Skip-ink breaks
 * come only from glyph ink, never from word gaps. `span = Span::kPerWord`
 * opts back into one band per word (spell-check squiggles, word chips).
 *
 * kHighlight is the background member of the family: a full-text-height
 * band (ascent to descent by default) drawn *beneath* every glyph pass, so
 * with the default range spanning it renders as a continuous highlighter
 * stroke behind the words and their gaps.
 *
 * A decoration separates two concerns: *band geometry* (kind, span,
 * thickness, offset, skipInk) and *band fill*. The fill has two spellings:
 * `color` is the lightweight one, and `paint` is the full SkPaint
 * vocabulary — shaders (PaintShaders.h presets animate per frame through
 * Paragraph::setPaint() without relayout, exactly like glyph paint), blend
 * modes, mask filters. Glyphs and decorations resolve their fills
 * independently, so a shaded highlight under plain-colored text — or the
 * reverse — needs no coordination between the two. Multi-pass band effects
 * compose the same way glyph passes do: stack several decorations with the
 * same geometry and different fills.
 *
 * Scope: decorations render on straight horizontal runs only — transformed
 * (path/rotated) and vertical runs skip them.
 */
struct Decoration {
  /// Selects which font metric anchors the band by default. kHighlight is
  /// drawn beneath the glyph passes; the others above them.
  enum class Kind : uint8_t {
    kUnderline,
    kStrikethrough,
    kOverline,
    kHighlight,
  };
  /// How far one band extends along the line.
  enum class Span : uint8_t {
    kDecoratedRange, ///< merge contiguous same-style runs, covering gaps
    kPerWord,        ///< one band per word run; breaks at every gap
  };

  Kind kind = Kind::kUnderline; ///< only underlines honor `skipInk`
  Span span = Span::kDecoratedRange; ///< continuous band vs one per word
  /// SK_ColorTRANSPARENT → the resolved foreground paint's color — except
  /// for kHighlight, where an opaque foreground would hide the text, so it
  /// resolves to the foreground color at ~25% alpha instead.
  SkColor color = SK_ColorTRANSPARENT;
  /// 0 → thickness from font metrics (kHighlight: ascent + descent),
  /// floored at 1px.
  float thickness = 0;
  /// 0 → position from font metrics (kHighlight: the ascent line);
  /// otherwise the band's top edge in px relative to the baseline
  /// (positive below, Skia's y-grows-down convention).
  float offset = 0;
  /// Underlines only: interrupt the line where glyph ink (descenders)
  /// crosses the band, via SkTextBlob::getIntercepts.
  bool skipInk = true;
  /// Full-vocabulary band fill. When set it is applied verbatim — nothing
  /// is overridden, exactly like a PaintLayer wrapping a caller-configured
  /// SkPaint — and it takes precedence over `color`, whose resolution rules
  /// (including the translucent kHighlight default) no longer apply: alpha,
  /// anti-aliasing, and everything else are the caller's. Band geometry is
  /// untouched — the paint fills the same rect segments a plain color
  /// would, ink skipping included.
  std::optional<SkPaint> paint;

  /** Compares kind, span, fill (color and paint override), geometry
   * overrides, and ink skipping. */
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
  SkPaint foreground; ///< the main glyph pass, drawn between the layer lists
  std::vector<PaintLayer> underlays; ///< drawn in order beneath `foreground`
  std::vector<PaintLayer> overlays;  ///< drawn in order above `foreground`
  /// Line decorations in vector order — highlights beneath every glyph
  /// pass, the rest above them. See Decoration for band defaults, range
  /// vs per-word spanning, and the straight-horizontal-runs-only scope.
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
    decorations.push_back(std::move(decoration));
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
  ShapingStyle shaping; ///< changes re-shape the covered words
  PaintStyle paint;     ///< changes never re-shape or relayout

  /** Sets or replaces one variable-font axis (fluent sugar over
   *  `shaping.variations`). Replaces in place when the axis is already
   *  present — repeated calls stay order-stable, so styles built by the
   *  same call sequence share one varied-typeface memo entry. */
  TextStyle &variation(const char (&tag)[5], float value) {
    for (FontVariation &v : shaping.variations)
      if (v.tag[0] == tag[0] && v.tag[1] == tag[1] && v.tag[2] == tag[2] &&
          v.tag[3] == tag[3]) {
        v.value = value;
        return *this;
      }
    shaping.variations.emplace_back(tag, value);
    return *this;
  }
  /** The `wght` axis, fluently: `style.weight(650)`. Weight participates
   *  in shaping identity (wght changes advances — it is NOT paint-safe),
   *  so animating it re-shapes; fonts with a `GRAD` axis offer the
   *  advance-invariant alternative: `variation("GRAD", v)`. */
  TextStyle &weight(float wght) { return variation("wght", wght); }
  /** The optical-size axis, fluently: `style.opticalSize(72)`. */
  TextStyle &opticalSize(float opsz) { return variation("opsz", opsz); }

  /** Compares both shaping and paint configuration. */
  bool operator==(const TextStyle &other) const {
    return shaping == other.shaping && paint == other.paint;
  }
};

} // namespace sigil::weave
