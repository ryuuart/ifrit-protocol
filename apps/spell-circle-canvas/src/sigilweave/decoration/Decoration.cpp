/** @file
 * Decoration bands resolved against a run: the memoized skip-ink
 * intercepts, the paint a run resolves to, the band a decoration occupies
 * once the font's metrics fill in what the style left at zero, the paint
 * that band draws with, and the spans a run's band covers around its ink.
 */

#include "sigilweave/decoration/Decoration.h"

#include <include/core/SkFontMetrics.h>
#include <include/core/SkPaint.h>
#include <include/core/SkTextBlob.h>

#include <algorithm>
#include <boost/container_hash/hash.hpp>
#include <boost/unordered/unordered_node_map.hpp>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "sigilweave/decoration/DecorationRects.h"

namespace sigil::weave {
namespace detail {

/** Skip-ink intercepts, memoized.
 *
 *  SkTextBlob::getIntercepts resolves a glyph strike and walks glyph
 *  outlines, and an underline redraws it for every run on every frame. Its
 *  inputs, though, only change when the layout does: the blob and the band
 *  window (the two y values the band spans). So the result is cached on
 *  exactly those.
 *
 *  The key is the blob's uniqueID, which Skia never reuses — a blob that is
 *  freed and its memory reallocated therefore cannot collide with a stale
 *  entry. The cache is thread-local, so entries never outlive the thread
 *  that draws with them, and at the entry cap it is cleared wholesale
 *  rather than evicted from: an underline redrawn each frame re-populates
 *  it immediately, and nothing here needs recency ordering. */
const std::vector<SkScalar>& cachedIntercepts(const SkTextBlob& blob,
                                              const SkScalar bounds[2]) {
  struct Key {
    uint32_t blobId;
    SkScalar lo, hi;
    bool operator==(const Key&) const = default;
  };
  struct KeyHash {
    size_t operator()(const Key& key) const {
      // The two band bounds are folded as their BIT PATTERNS: they are
      // window coordinates, and two that differ in the last bit are two
      // different windows. Boost's fold rather than the pinned one: this
      // table lives inside one run and nothing outside the process ever
      // sees a bucket of it.
      uint32_t lo, hi;
      memcpy(&lo, &key.lo, sizeof lo);
      memcpy(&hi, &key.hi, sizeof hi);
      size_t h = 0;
      boost::hash_combine(h, key.blobId);
      boost::hash_combine(h, lo);
      boost::hash_combine(h, hi);
      return h;
    }
  };
  static thread_local boost::unordered_node_map<Key, std::vector<SkScalar>,
                                                KeyHash>
      cache;
  constexpr size_t kMaxInterceptEntries = size_t{1} << 12u;
  const Key key{blob.uniqueID(), bounds[0], bounds[1]};
  if (auto it = cache.find(key); it != cache.end()) return it->second;
  if (cache.size() >= kMaxInterceptEntries) cache.clear();
  std::vector<SkScalar> intercepts;
  const int count = blob.getIntercepts(bounds, nullptr);
  if (count >= 2) {
    intercepts.resize(static_cast<size_t>(count));
    blob.getIntercepts(bounds, intercepts.data());
  }
  return cache.emplace(key, std::move(intercepts)).first->second;
}

const PaintStyle& resolvePaint(const std::vector<StyleSpan>& spans,
                               uint32_t styleIndex,
                               const PaintStyle* overridePaint) {
  static const PaintStyle kFallback;
  if (overridePaint) return *overridePaint;
  return styleIndex < spans.size() ? spans[styleIndex].style.paint : kFallback;
}

ResolvedDecorationBand resolveDecorationBand(const Decoration& decoration,
                                             const SkFontMetrics& metrics,
                                             SkColor foregroundColor,
                                             bool alongColumn) {
  ResolvedDecorationBand band;
  if (decoration.color != SK_ColorTRANSPARENT) {
    band.color = decoration.color;
  } else if (decoration.kind == Decoration::Kind::kHighlight) {
    // An opaque foreground-colored highlight would hide the text it sits
    // behind; default to a quarter-alpha tint of the foreground instead.
    band.color = SkColorSetA(foregroundColor, 0x40);
  } else {
    band.color = foregroundColor;
  }

  band.thickness = decoration.thickness;
  if (band.thickness <= 0) {
    if (decoration.kind == Decoration::Kind::kHighlight) {
      band.thickness = -metrics.fAscent + metrics.fDescent;  // full text height
    } else {
      SkScalar metricThickness = 0;
      const bool hasMetric =
          decoration.kind == Decoration::Kind::kStrikethrough
              ? metrics.hasStrikeoutThickness(&metricThickness)
              : metrics.hasUnderlineThickness(&metricThickness);
      band.thickness =
          hasMetric && metricThickness > 0 ? metricThickness : 1.0f;
    }
  }
  band.thickness = std::max(band.thickness, 1.0f);

  if (decoration.offset != 0) {
    band.position = decoration.offset;
    return band;
  }

  // WHICH SIDE OF THE AXIS the band anchors on. An underline and an
  // overline are one band on opposite sides of the same axis — below the
  // line and above it, right of the column and left of it — so the
  // opposite side is exactly the other one's anchor, and taking it is
  // reading the other one's metric. A strikethrough and a highlight are
  // anchored across the type rather than beside it and have no second side
  // to take. Only the position is borrowed: thickness and color stay the
  // decoration's own, so a highlight keeps its full-box depth and an
  // underline its hairline whichever side it stands on.
  Decoration::Kind anchor = decoration.kind;
  if (decoration.side == Decoration::Side::kOpposite) {
    if (anchor == Decoration::Kind::kUnderline)
      anchor = Decoration::Kind::kOverline;
    else if (anchor == Decoration::Kind::kOverline)
      anchor = Decoration::Kind::kUnderline;
  }

  if (alongColumn) {
    // No baseline to measure from: an upright glyph is centred across the
    // column axis, so the em box's half-depth is the whole geometry. The
    // underline stands clear of the box on the right by default — the side
    // a vertical setting reads its emphasis line on — and the overline on
    // the left.
    const float halfEm = (-metrics.fAscent + metrics.fDescent) * 0.5f;
    switch (anchor) {
      case Decoration::Kind::kUnderline:
        band.position = halfEm;
        break;
      case Decoration::Kind::kOverline:
        band.position = -halfEm - band.thickness;
        break;
      case Decoration::Kind::kStrikethrough:
        band.position = -band.thickness * 0.5f;  // down the axis itself
        break;
      case Decoration::Kind::kHighlight:
        band.position = -halfEm;  // thickness is already the whole em box
        break;
    }
    return band;
  }
  switch (anchor) {
    case Decoration::Kind::kUnderline: {
      SkScalar underlinePosition = 0;  // metric = distance baseline → band top
      band.position = metrics.hasUnderlinePosition(&underlinePosition)
                          ? underlinePosition
                          : band.thickness;
      break;
    }
    case Decoration::Kind::kStrikethrough: {
      SkScalar strikeoutPosition = 0;  // metric = baseline → band bottom (< 0)
      if (metrics.hasStrikeoutPosition(&strikeoutPosition))
        band.position = strikeoutPosition;
      else
        band.position = -metrics.fXHeight * 0.5f - band.thickness * 0.5f;
      break;
    }
    case Decoration::Kind::kOverline:
    case Decoration::Kind::kHighlight:
      band.position = metrics.fAscent;  // negative: the ascent line
      break;
  }
  return band;
}

SkPaint decorationBandPaint(const Decoration& decoration,
                            const ResolvedDecorationBand& band) {
  if (decoration.paint) return *decoration.paint;
  SkPaint fill;
  fill.setAntiAlias(true);
  fill.setColor(band.color);
  return fill;
}

std::vector<std::pair<float, float>> decorationSegments(
    const PositionedRun& run, const Decoration& decoration,
    const ResolvedDecorationBand& band) {
  std::vector<std::pair<float, float>> segments;
  if (run.transformed || !run.shaped || run.placeholderIndex >= 0)
    return segments;
  const bool alongColumn = run.shaped->vertical;
  const float startX = alongColumn ? run.origin.y() : run.origin.x();
  const float endX = startX + run.advance;
  if (endX <= startX) return segments;

  // Intercepts come out of a HORIZONTAL band window, which a column's band
  // is not: down a column the line draws through the ink.
  const bool skipInk = decoration.skipInk && !alongColumn &&
                       decoration.kind == Decoration::Kind::kUnderline &&
                       run.blob;
  if (!skipInk) {
    segments.emplace_back(startX, endX);
    return segments;
  }

  // Blob-local intercepts of glyph ink with the band; grown by one
  // thickness so the line stands off the descender instead of touching it.
  // Memoized on (blob, band window) — see cachedIntercepts.
  const SkScalar bounds[2] = {band.position, band.position + band.thickness};
  const std::vector<SkScalar>& intercepts = cachedIntercepts(*run.blob, bounds);
  const int interceptCount = static_cast<int>(intercepts.size());
  if (interceptCount < 2) {
    segments.emplace_back(startX, endX);
    return segments;
  }

  const float standoff = band.thickness;
  float cursor = startX;
  for (int interceptIndex = 0; interceptIndex + 1 < interceptCount;
       interceptIndex += 2) {
    const float inkStart =
        startX + intercepts[static_cast<size_t>(interceptIndex)] - standoff;
    const float inkEnd =
        startX + intercepts[static_cast<size_t>(interceptIndex) + 1] + standoff;
    if (inkStart > cursor)
      segments.emplace_back(cursor, std::min(inkStart, endX));
    cursor = std::max(cursor, inkEnd);
  }
  if (cursor < endX) segments.emplace_back(cursor, endX);
  return segments;
}

}  // namespace detail
}  // namespace sigil::weave
