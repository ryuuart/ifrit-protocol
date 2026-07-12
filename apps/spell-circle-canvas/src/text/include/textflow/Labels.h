#pragma once

// Label conveniences — the boilerplate a renderer needs to draw lots of
// short, cached, possibly-curved labels every frame at full performance,
// distilled from the SpellCircle scene backend. Optional layer: nothing in
// the core pipeline includes this.
//
//   LabelCache   — shaped label paragraphs keyed by (text, typeface, size):
//                  itemization + shaping run once per unique label, so a
//                  frame's label cost is placement arithmetic only.
//   RingCache    — arc-length-measured origin-centred circles per quantized
//                  radius (SkContourMeasure of a large circle is not
//                  per-frame cheap).
//   makeRingLabel — a curvature-compensated, centre-anchored interval for
//                  text riding a circle (see LineInterval::advanceScale).
//   layoutLabel  — single-line layout at a baseline origin.

#include "Flow.h"
#include "FontContext.h"
#include "Layout.h"
#include "Paragraph.h"

#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>

#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMetrics.h>

#include <string>
#include <string_view>

namespace textflow {

// Vertical offset from a target y to the baseline y that centres a glyph's
// [ascent, descent] span on that target (canvas "middle" baseline).
inline float middleBaselineOffset(const SkFontMetrics &metrics) {
  return -(metrics.fAscent + metrics.fDescent) * 0.5f;
}

// Shaped single-style label paragraphs, cached by (utf8 text, typeface,
// quantized size). Returned references stay valid while other labels are
// inserted (node map) but not past clear().
class LabelCache {
public:
  explicit LabelCache(size_t maxEntries = 1024) : m_maxEntries(maxEntries) {}

  Paragraph &get(std::string_view utf8, const sk_sp<SkTypeface> &typeface,
                 float size);
  Paragraph &get(std::u16string_view utf16, const sk_sp<SkTypeface> &typeface,
                 float size);
  void clear() { m_labels.clear(); }

private:
  template <typename View>
  Paragraph &getImpl(View text, const sk_sp<SkTypeface> &typeface, float size);

  absl::node_hash_map<std::string, Paragraph> m_labels;
  size_t m_maxEntries;
};

// Origin-centred measured rings per quantized radius; translate the canvas
// to the circle centre when drawing.
class RingCache {
public:
  explicit RingCache(size_t maxEntries = 256) : m_maxEntries(maxEntries) {}

  const sk_sp<SkContourMeasure> &ring(float radius);
  void clear() { m_rings.clear(); }

private:
  absl::flat_hash_map<int, sk_sp<SkContourMeasure>> m_rings;
  size_t m_maxEntries;
};

// A label interval around an origin-centred ring: glyph optical middles
// ride radius `rMid`, the text is centre-anchored at `anchorFraction`
// (0..1 around the circle, wrapping), and pen advances are curvature-
// compensated so spacing reads correctly at the middles even on small
// rings. Returns a zero-length interval (contour == null) when the ring is
// degenerate. Feed the result to a LineSetFlow with Alignment::kCenter and
// draw with the canvas translated to the circle centre.
LineInterval makeRingLabel(RingCache &rings, const SkFontMetrics &metrics,
                           float rMid, float anchorFraction);

// Lays `label` out as one unconstrained line whose baseline starts at
// `baselineOrigin` (natural width; shapes on demand, cache-hot).
Layout layoutLabel(FontContext &ctx, Paragraph &label, SkPoint baselineOrigin);

} // namespace textflow
