#pragma once

#include <include/core/SkColor.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypeface.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace textflow {

// One OpenType feature setting, e.g. {"liga", 0} to disable ligatures.
struct FontFeature {
  char tag[4] = {' ', ' ', ' ', ' '};
  uint32_t value = 1;

  FontFeature() = default;
  FontFeature(const char (&t)[5], uint32_t v) : value(v) {
    std::memcpy(tag, t, 4);
  }
  bool operator==(const FontFeature &o) const {
    return value == o.value && std::memcmp(tag, o.tag, 4) == 0;
  }
};

// The subset of style that affects glyph selection and metrics. These fields
// are baked into the shape-cache key, so changing any of them re-shapes the
// words it covers. Everything that only affects how already-positioned glyphs
// are painted belongs in PaintStyle instead and never invalidates shaping.
struct ShapingStyle {
  sk_sp<SkTypeface> typeface; // null → FontContext's default (+ fallback)
  float size = 16.0f;
  float letterSpacing = 0.0f; // px of tracking added after each cluster
  std::string language;       // BCP-47 tag ("ja", "ko", ...); empty → untagged
  std::vector<FontFeature> features;

  bool operator==(const ShapingStyle &o) const {
    return typeface.get() == o.typeface.get() && size == o.size &&
           letterSpacing == o.letterSpacing && language == o.language &&
           features == o.features;
  }
};

// A blurred, offset copy of the glyphs drawn beneath the main fill.
struct DropShadow {
  SkColor color = 0x66000000;
  SkVector offset = {2, 2};
  float blurSigma = 2.0f;

  bool operator==(const DropShadow &o) const {
    return color == o.color && offset == o.offset &&
           blurSigma == o.blurSigma;
  }
};

// Everything here is resolved at draw time from the paragraph's current
// spans: changing any of it never reshapes, never relayouts, and shows up on
// the very next draw() of an existing Layout.
struct PaintStyle {
  SkColor color = SK_ColorBLACK;
  sk_sp<SkShader> shader;         // gradient/pattern fill (overrides color's
                                  // RGB; alpha still applies)
  sk_sp<SkMaskFilter> maskFilter; // e.g. blur on the glyphs themselves
  std::vector<DropShadow> shadows; // drawn back-to-front before the fill

  // Identity comparison for the ref-counted effects: enough for span
  // merging, where "same object" is the case that matters.
  bool operator==(const PaintStyle &o) const {
    return color == o.color && shader.get() == o.shader.get() &&
           maskFilter.get() == o.maskFilter.get() && shadows == o.shadows;
  }
};

struct TextStyle {
  ShapingStyle shaping;
  PaintStyle paint;

  bool operator==(const TextStyle &o) const {
    return shaping == o.shaping && paint == o.paint;
  }
};

} // namespace textflow
