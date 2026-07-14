#pragma once

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypeface.h>

#include <cstdint>
#include <string>
#include <vector>

namespace textflow {

// One OpenType feature setting, e.g. {"liga", 0} to disable ligatures.
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

// How a span behaves when its paragraph is laid out vertically
// (Paragraph::setWritingMode(WritingMode::kVerticalRL)). Ignored in
// horizontal paragraphs.
enum class VerticalForm : uint8_t {
  kAuto,        // UTR#50 per character: CJK upright, Latin rotated
  kUpright,     // force upright (TTB shaping, 'vert' forms)
  kRotated,     // force rotated 90° clockwise (book-spine Latin)
  kTateChuYoko, // 縦中横: shaped horizontally, set upright in the column —
                // for short runs like two-digit numbers in vertical prose
};

// The subset of style that affects glyph selection and metrics. These fields
// are baked into the shape-cache key, so changing any of them re-shapes the
// words it covers. Everything that only affects how already-positioned glyphs
// are painted belongs in PaintStyle instead and never invalidates shaping.
struct ShapingStyle {
  sk_sp<SkTypeface> typeface; // null → FontContext's default (+ fallback)
  float fontSize = 16.0f;     // pixels in the target canvas coordinate space
  float letterSpacing = 0.0f; // px of tracking added after each cluster
                              // (in vertical text this is JIS "aki")
  std::string languageTag;    // BCP-47 ("ja", "ko", ...); empty → untagged
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

// A blurred, offset copy of the glyphs drawn beneath the main fill.
struct DropShadow {
  SkColor color = 0x66000000;
  SkVector offset = {2, 2};
  float blurSigma = 2.0f;

  /** Compares every shadow rendering property. */
  bool operator==(const DropShadow &other) const {
    return color == other.color && offset == other.offset &&
           blurSigma == other.blurSigma;
  }
};

// Everything here is resolved at draw time from the paragraph's current
// spans: changing any of it never reshapes, never relayouts, and shows up on
// the very next draw() of an existing ParagraphLayout.
struct PaintStyle {
  SkColor color = SK_ColorBLACK;
  sk_sp<SkShader> shader;          // gradient/pattern fill (overrides color's
                                   // RGB; alpha still applies)
  sk_sp<SkMaskFilter> maskFilter;  // e.g. blur on the glyphs themselves
  std::vector<DropShadow> shadows; // drawn back-to-front before the fill
  SkBlendMode blendMode = SkBlendMode::kSrcOver; // e.g. kDstOut punch-outs

  // Identity comparison for the ref-counted effects: enough for span
  // merging, where "same object" is the case that matters.
  /** Compares values and effect-object identities used for span merging. */
  bool operator==(const PaintStyle &other) const {
    return color == other.color && shader.get() == other.shader.get() &&
           maskFilter.get() == other.maskFilter.get() &&
           shadows == other.shadows && blendMode == other.blendMode;
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
