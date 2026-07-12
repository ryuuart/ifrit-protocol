#include "textflow/Labels.h"

#include <include/core/SkPath.h>

#include <unicode/ustring.h>

namespace textflow {

namespace {

void appendUtf8Key(std::string &key, std::string_view utf8) { key += utf8; }

void appendUtf8Key(std::string &key, std::u16string_view utf16) {
  // Key by UTF-8 so both entry points address the same cache slot.
  std::string utf8(utf16.size() * 3, '\0');
  UErrorCode status = U_ZERO_ERROR;
  int32_t written = 0;
  u_strToUTF8(utf8.data(), static_cast<int32_t>(utf8.size()), &written,
              reinterpret_cast<const UChar *>(utf16.data()),
              static_cast<int32_t>(utf16.size()), &status);
  if (U_SUCCESS(status))
    key.append(utf8.data(), static_cast<size_t>(written));
}

} // namespace

template <typename View>
Paragraph &LabelCache::getImpl(View text, const sk_sp<SkTypeface> &typeface,
                               float size) {
  std::string key;
  appendUtf8Key(key, text);
  key += '\x1f';
  key += std::to_string(typeface ? typeface->uniqueID() : 0);
  key += '\x1f';
  key += std::to_string(static_cast<int>(size * 16.0f));
  auto it = m_labels.find(key);
  if (it == m_labels.end()) {
    if (m_labels.size() > m_maxEntries)
      m_labels.clear(); // scenes cycle labels; don't grow without bound
    TextStyle style;
    style.shaping.typeface = typeface;
    style.shaping.size = size;
    Paragraph para;
    para.appendText(text, style);
    it = m_labels.emplace(std::move(key), std::move(para)).first;
  }
  return it->second;
}

Paragraph &LabelCache::get(std::string_view utf8,
                           const sk_sp<SkTypeface> &typeface, float size) {
  return getImpl(utf8, typeface, size);
}

Paragraph &LabelCache::get(std::u16string_view utf16,
                           const sk_sp<SkTypeface> &typeface, float size) {
  return getImpl(utf16, typeface, size);
}

const sk_sp<SkContourMeasure> &RingCache::ring(float radius) {
  const int key = static_cast<int>(radius * 4.0f);
  auto it = m_rings.find(key);
  if (it == m_rings.end()) {
    if (m_rings.size() > m_maxEntries)
      m_rings.clear();
    SkContourMeasureIter iter(SkPath::Circle(0, 0, radius),
                              /*forceClosed=*/false);
    it = m_rings.emplace(key, iter.next()).first;
  }
  return it->second;
}

LineInterval makeRingLabel(RingCache &rings, const SkFontMetrics &metrics,
                           float rMid, float anchorFraction) {
  LineInterval interval;
  // Baselines sit inward of the optical-middle ring; without compensation
  // the same advances eat more angle at the smaller radius and the label
  // reads too loose — increasingly so on small circles.
  const float rArc = rMid - middleBaselineOffset(metrics);
  if (rArc <= 1.0f || rMid <= 1.0f)
    return interval; // degenerate: contour stays null
  interval.contour = rings.ring(rArc);
  if (!interval.contour)
    return interval;
  const float arcLen = interval.contour->length();
  interval.advanceScale = rArc / rMid;
  interval.length = arcLen / interval.advanceScale;
  // Centre-anchor: closed contours wrap, so a negative start is fine.
  interval.contourStart = anchorFraction * arcLen - arcLen * 0.5f;
  return interval;
}

Layout layoutLabel(FontContext &ctx, Paragraph &label,
                   SkPoint baselineOrigin) {
  LineSetFlow flow;
  flow.lines().push_back({LineInterval{
      baselineOrigin, {1, 0}, label.naturalWidth(ctx) + 1}});
  return layoutParagraph(ctx, label, flow);
}

} // namespace textflow
