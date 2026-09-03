#pragma once

/** @file
 * SigilCompose checks for GENERATED geometry — tilings, subdivisions,
 * lattices, pavings: the constructions whose correctness is a property of a
 * rule rather than of anything you can see at a glance — and for reading
 * back what was actually drawn.
 *
 * Why point-sampled coverage rather than something cheaper: the two obvious
 * cheap checks both PASS on a subdivision that overlaps in one place and
 * gaps in another. Total-area conservation passes because an overlap and a
 * gap of equal area cancel exactly, and containment passes because every
 * child really does lie inside its parent. Sampling the region is what
 * separates them, because it asks each point how many pieces claim it.
 *
 * This header is for tests, sketches and verification passes, not for the
 * paint loop: coverage() costs O(samples × candidate pieces) and the
 * rasterizing helpers allocate a surface per call. It is the one header of
 * a separate target, SigilComposeTesting, so that a shipping paint loop
 * cannot reach it by accident and so that `report()` may speak to a feed
 * without the library itself depending on one. The namespace is `checks`
 * rather than the target's name because GoogleTest owns `::testing`, and
 * a test that brings `sigil::compose` in with a using-directive must be
 * able to spell both without qualifying either.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPath.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <include/pathops/SkPathOps.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Feed.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmeasure/check/Check.h>

#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose::test {

/** What a point-sampled coverage test found. A correct exact cover has
 *  `uncovered == 0 && doubled == 0`. */
struct Coverage {
  int samples = 0;    ///< points tested inside `region`
  int uncovered = 0;  ///< in the region and in NO piece — a gap
  int doubled = 0;    ///< in the region and in MORE THAN ONE — an overlap
  /** Up to `witnesses` example points for each failure, so a caller can
   *  print or draw where it went wrong instead of just how often. */
  std::vector<SkPoint> uncoveredAt;
  std::vector<SkPoint> doubledAt;

  bool exact() const { return uncovered == 0 && doubled == 0; }
  float uncoveredFraction() const {
    return samples > 0 ? (float)uncovered / (float)samples : 0.0f;
  }
  float doubledFraction() const {
    return samples > 0 ? (float)doubled / (float)samples : 0.0f;
  }
};

/** Samples @p region on a `grid × grid` lattice and counts how many
 *  pieces contain each point.
 *
 *  Points are taken at CELL CENTRES, deliberately off the lattice a
 *  tiling is likely to be built on — sampling exactly on shared edges
 *  makes every boundary look doubled and tells you nothing. For the same
 *  reason a piece boundary passing exactly through a sample is a coin
 *  flip; raise `grid` rather than trusting a single small run.
 *
 *  @p grid of 128 is 16384 samples, which resolves a defect about
 *  1/128 of the region across. */
inline Coverage coverage(std::span<const SkPath> pieces, const SkRect& region,
                         int grid = 128, size_t witnesses = 8) {
  Coverage out;
  if (region.isEmpty() || grid < 2) return out;

  // Bounds first: SkPath::contains is not cheap, and most pieces are
  // nowhere near most samples.
  std::vector<SkRect> bounds;
  bounds.reserve(pieces.size());
  for (const SkPath& p : pieces) bounds.push_back(p.getBounds());

  const float dx = region.width() / (float)grid;
  const float dy = region.height() / (float)grid;
  for (int gy = 0; gy < grid; ++gy) {
    const float y = region.top() + ((float)gy + 0.5f) * dy;
    for (int gx = 0; gx < grid; ++gx) {
      const float x = region.left() + ((float)gx + 0.5f) * dx;
      int hits = 0;
      for (size_t i = 0; i < pieces.size() && hits < 2; ++i) {
        if (!bounds[i].contains(x, y)) continue;
        if (pieces[i].contains(x, y)) ++hits;
      }
      ++out.samples;
      if (hits == 0) {
        ++out.uncovered;
        if (out.uncoveredAt.size() < witnesses)
          out.uncoveredAt.push_back({x, y});
      } else if (hits > 1) {
        ++out.doubled;
        if (out.doubledAt.size() < witnesses) out.doubledAt.push_back({x, y});
      }
    }
  }
  return out;
}

/** Coverage over an arbitrary REGION rather than a rect.
 *
 *  An annulus, a sector, a plate — anything whose outline is not a box —
 *  cannot be tested against `region.bounds()` without counting everything
 *  outside it as a gap.
 *
 *  Build the reference region from the SAME vertices as the pieces wherever
 *  you can. A tiling made of polylines tested against a true circle reports
 *  the chord error between them as a ring of phantom gaps, which looks
 *  exactly like a real defect. */
inline Coverage coverage(std::span<const SkPath> pieces, const SkPath& region,
                         int grid = 128, size_t witnesses = 8) {
  Coverage out;
  const SkRect box = region.getBounds();
  if (box.isEmpty() || grid < 2) return out;

  std::vector<SkRect> bounds;
  bounds.reserve(pieces.size());
  for (const SkPath& p : pieces) bounds.push_back(p.getBounds());

  const float dx = box.width() / (float)grid;
  const float dy = box.height() / (float)grid;
  for (int gy = 0; gy < grid; ++gy) {
    const float y = box.top() + ((float)gy + 0.5f) * dy;
    for (int gx = 0; gx < grid; ++gx) {
      const float x = box.left() + ((float)gx + 0.5f) * dx;
      if (!region.contains(x, y))
        continue;  // outside the region entirely — not a gap
      int hits = 0;
      for (size_t i = 0; i < pieces.size() && hits < 2; ++i) {
        if (!bounds[i].contains(x, y)) continue;
        if (pieces[i].contains(x, y)) ++hits;
      }
      ++out.samples;
      if (hits == 0) {
        ++out.uncovered;
        if (out.uncoveredAt.size() < witnesses)
          out.uncoveredAt.push_back({x, y});
      } else if (hits > 1) {
        ++out.doubled;
        if (out.doubledAt.size() < witnesses) out.doubledAt.push_back({x, y});
      }
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Is a band the width it claims?

/** One station of a width audit: where on the spine, what the band
 *  actually measured there, and what the law asked for. */
struct WidthStation {
  float along = 0;     ///< px of arc length from the spine's start
  SkPoint at{0, 0};    ///< the spine point the chord was taken through
  float measured = 0;  ///< the shortest chord of the band through it
  float intended = 0;  ///< the profile's width there
  float error() const { return std::abs(measured - intended); }
};

/** What a width-along audit found. A band that is the width it claims has
 *  `maxError` under whatever the caller's ink can show. */
struct WidthAlong {
  int samples = 0;        ///< stations measured
  float maxError = 0;     ///< the worst |measured − intended|, px
  float rmsError = 0;     ///< the same error over the whole run
  /** The worst stations, most wrong first, so a caller can print or draw
   *  WHERE the band went wrong instead of only how far. */
  std::vector<WidthStation> worst;

  bool within(float tolerance) const {
    return samples > 0 && maxError <= tolerance;
  }
};

/** Measures the width of a drawn @p band along its @p spine and compares
 *  it with what @p profile asked for.
 *
 *  **Why a min-chord raycast and not an area check.** Total ink is the
 *  cheap test and it cannot see a corner defect at all: a band that loses
 *  the inside of a bend and gains an outer chord loses and gains almost
 *  the same area, so the two errors cancel and the area agrees to a
 *  fraction of a percent while the picture is visibly torn. The width is
 *  a LOCAL property and only a local measurement finds it. At each
 *  station the shortest chord of the band through the spine point is
 *  taken, over `directions` evenly spaced headings across a half turn —
 *  shortest, because through any interior point the shortest chord is the
 *  one across the band, whatever the spine's tangent is doing.
 *
 *  A HALF-WIDTH MARGIN AT EACH END IS SKIPPED, and it has to be: within
 *  about half a width of a cap the shortest chord through a point runs
 *  diagonally out through the END of the band rather than across it, so
 *  it reads well under the true width and swamps every real error. The
 *  cap is the one place where the shortest chord through a point is not
 *  the width.
 *
 *  Cost is O(stations × directions × band edges), like `coverage` and for
 *  the same reason: a verification pass, not a paint loop. */
inline WidthAlong widthAlong(const SkPath& band, const SkPath& spine,
                             const geometry::path::Profile& profile,
                             float step = 4.0f, int directions = 90,
                             size_t witnesses = 8) {
  WidthAlong out;
  const float reach = profile.max() * 4.0f + 40.0f;
  if (directions < 2 || !(step > 0) || band.isEmpty()) return out;

  // THE OUTLINE FIRST. A band is usually built as overlapping pieces —
  // one per step, or one per leg — and every shared edge between two of
  // them is a line INSIDE the ink. A raycast that counted those would
  // find its shortest chord at the first interior seam and report a band
  // one sampling step wide, whatever the band actually is. Simplify()
  // resolves the winding into the boundary of the union, which is what
  // the eye sees and what the width means.
  SkPath outline;
  if (!Simplify(band, &outline)) outline = band;

  // Flatten by MEASURING rather than by reading verbs: a band built from
  // a profile carries curves, and a line-only walk would see none of them
  // and report a band it never touched as infinitely wide.
  std::vector<SkPoint> edges;
  {
    SkContourMeasureIter it(outline, false);
    while (sk_sp<SkContourMeasure> contour = it.next()) {
      const float len = contour->length();
      SkPoint prev;
      SkVector tan;
      if (len <= 0 || !contour->getPosTan(0, &prev, &tan)) continue;
      for (float d = 1.0f;; d += 1.0f) {
        const float at = std::min(d, len);
        SkPoint here;
        if (!contour->getPosTan(at, &here, &tan)) break;
        edges.push_back(prev);
        edges.push_back(here);
        prev = here;
        if (at >= len) break;
      }
      if (contour->isClosed()) {
        SkPoint first;
        if (contour->getPosTan(0, &first, &tan)) {
          edges.push_back(prev);
          edges.push_back(first);
        }
      }
    }
  }
  if (edges.empty()) return out;

  // The chord of the flattened band through `p` along `u`, both ways.
  const auto chord = [&](SkPoint p, SkVector u) {
    float fwd = reach, back = reach;
    for (size_t i = 0; i + 1 < edges.size(); i += 2) {
      const SkPoint a = edges[i], b = edges[i + 1];
      const SkVector e{b.x() - a.x(), b.y() - a.y()};
      const float den = u.x() * e.y() - u.y() * e.x();
      if (std::abs(den) < 1e-9f) continue;
      const SkVector w{a.x() - p.x(), a.y() - p.y()};
      const float t = (w.x() * e.y() - w.y() * e.x()) / den;
      const float s = (w.x() * u.y() - w.y() * u.x()) / den;
      if (s < 0.0f || s > 1.0f) continue;
      if (t > 0.0f)
        fwd = std::min(fwd, t);
      else
        back = std::min(back, -t);
    }
    return fwd + back;
  };

  double squared = 0;
  SkContourMeasureIter it(spine, false);
  while (sk_sp<SkContourMeasure> contour = it.next()) {
    const float len = contour->length();
    const float margin = profile.max() * 0.55f + step;
    for (float d = std::max(step, margin); d < len - margin; d += step) {
      SkPoint here;
      SkVector tan;
      if (!contour->getPosTan(d, &here, &tan)) continue;
      float shortest = reach;
      for (int k = 0; k < directions; ++k) {
        const float a = 3.14159265f * (float)k / (float)directions;
        shortest = std::min(shortest, chord(here, {std::cos(a), std::sin(a)}));
      }
      const WidthStation station{d, here, shortest,
                                 profile.acrossAt(len > 0 ? d / len : 0, len)};
      ++out.samples;
      squared += (double)station.error() * station.error();
      out.maxError = std::max(out.maxError, station.error());
      out.worst.push_back(station);
    }
  }
  if (out.samples > 0) out.rmsError = (float)std::sqrt(squared / out.samples);
  std::sort(out.worst.begin(), out.worst.end(),
            [](const WidthStation& a, const WidthStation& b) {
              return a.error() > b.error();
            });
  if (out.worst.size() > witnesses) out.worst.resize(witnesses);
  return out;
}

/** Every distinct endpoint in @p pieces, with how many pieces touch it,
 *  within @p tolerance. The chaining test for decorated tilings: when arcs
 *  drawn on the tiles are meant to link into continuous bands, every
 *  interior endpoint must have degree 2 and any other number is a break. */
struct VertexDegrees {
  std::vector<SkPoint> points;
  std::vector<int> degree;
  /** How many CLOSED contours were seen. A closed contour has no
   *  endpoints and contributes no points at all, which is why this count
   *  is reported separately: if it is nonzero and `points` is empty, the
   *  input is entirely closed and the degree test says nothing about it —
   *  a result that would otherwise read as a clean pass. */
  size_t closedContours = 0;
  /** Which merged point each piece's endpoints landed on, two per
   *  contour in order — the adjacency `components()` needs. */
  std::vector<std::pair<size_t, size_t>> edges;

  /** How many separate CONNECTED pieces the input is; 1 means it is all
   *  one piece.
   *
   *  "Is this a single piece of metal?" is what a decorated tiling, a knot
   *  or a wire graph is usually asking, and the degree list alone cannot
   *  answer it — every endpoint can have the right degree while the figure
   *  falls into two disjoint loops. This unions the `edges` adjacency to
   *  find out. */
  size_t components() const {
    std::vector<size_t> parent(points.size());
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = i;
    auto find = [&](size_t i) {
      while (parent[i] != i) i = parent[i] = parent[parent[i]];
      return i;
    };
    for (const auto& e : edges) {
      const size_t a = find(e.first), b = find(e.second);
      if (a != b) parent[a] = b;
    }
    size_t roots = 0;
    for (size_t i = 0; i < parent.size(); ++i) roots += find(i) == i;
    return roots;
  }

  /** Points with a degree not in [lo, hi] — the ones to look at. */
  std::vector<size_t> outside(int lo, int hi) const {
    std::vector<size_t> bad;
    for (size_t i = 0; i < degree.size(); ++i)
      if (degree[i] < lo || degree[i] > hi) bad.push_back(i);
    return bad;
  }
};

/** Collects the first and last point of every contour of every piece and
 *  merges those within @p tolerance. O(endpoints²) — fine for the
 *  thousands a tiling produces, not for a mesh. */
inline VertexDegrees endpointDegrees(std::span<const SkPath> pieces,
                                     float tolerance = 0.05f) {
  VertexDegrees out;
  auto note = [&](SkPoint p) -> size_t {
    for (size_t i = 0; i < out.points.size(); ++i)
      if (SkPoint::Distance(out.points[i], p) <= tolerance) {
        ++out.degree[i];
        return i;
      }
    out.points.push_back(p);
    out.degree.push_back(1);
    return out.points.size() - 1;
  };
  size_t contourStart = 0;
  bool haveStart = false;
  for (const SkPath& path : pieces) {
    SkPath::Iter iter(path, false);
    SkPoint pts[4];
    SkPoint first{0, 0}, last{0, 0};
    bool open = false;
    for (SkPath::Verb verb = iter.next(pts); verb != SkPath::kDone_Verb;
         verb = iter.next(pts)) {
      switch (verb) {
        case SkPath::kMove_Verb:
          if (open) {
            const size_t end = note(last);
            if (haveStart) out.edges.emplace_back(contourStart, end);
          }
          first = last = pts[0];
          open = true;
          contourStart = note(first);
          haveStart = true;
          break;
        case SkPath::kLine_Verb:
          last = pts[1];
          break;
        case SkPath::kQuad_Verb:
        case SkPath::kConic_Verb:
          last = pts[2];
          break;
        case SkPath::kCubic_Verb:
          last = pts[3];
          break;
        case SkPath::kClose_Verb:
          // A closed contour has NO endpoints. Retract the point noted at
          // its moveTo rather than leaving a phantom degree-1 vertex.
          if (open && haveStart) {
            if (--out.degree[contourStart] == 0 &&
                contourStart + 1 == out.points.size()) {
              out.points.pop_back();
              out.degree.pop_back();
            }
            ++out.closedContours;
          }
          open = false;
          haveStart = false;
          break;
        default:
          break;
      }
    }
    if (open) {
      const size_t end = note(last);
      if (haveStart) out.edges.emplace_back(contourStart, end);
    }
    haveStart = false;
  }
  return out;
}

// ---------------------------------------------------------------------------
// Reading back what you actually drew

/** A rasterised element tree, for checking a claim against the PIXELS
 *  rather than against the description that produced them — the strongest
 *  form a verification here can take, since it tests what was actually
 *  drawn.
 *
 *  **The default colour type is F16, and that is the non-obvious half.**
 *  An 8-bit read-back quantises a faint value to a handful of levels, so a
 *  falloff measured near the dark end returns a confidently wrong number
 *  instead of an obviously broken one. If you are measuring a RATIO,
 *  measure it in float. */
struct Raster {
  SkBitmap bitmap;

  bool valid() const { return !bitmap.isNull(); }
  int width() const { return bitmap.width(); }
  int height() const { return bitmap.height(); }
  /** Unpremultiplied linear-ish read. Out of bounds is transparent. */
  SkColor4f at(int x, int y) const {
    if (x < 0 || y < 0 || x >= bitmap.width() || y >= bitmap.height())
      return {0, 0, 0, 0};
    return bitmap.getColor4f(x, y);
  }
};

inline Raster rasterize(Element root, sigil::weave::FontContext& fonts,
                        SkISize size,
                        SkColorType colorType = kRGBA_F16_SkColorType,
                        SkColor4f background = {0, 0, 0, 0}) {
  Raster out;
  if (size.isEmpty()) return out;
  const SkImageInfo info = SkImageInfo::Make(size.width(), size.height(),
                                             colorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  if (!surface) return out;
  surface->getCanvas()->clear(background);
  // snapshot() sizes by the root's CHILDREN and ignores the root's own
  // dimensions, so the wrapper carries EXPLICIT dims and an explicit
  // canvas size: without them an `absolute().inset(0)` child resolves
  // against nothing and the read-back is of an empty surface.
  if (sk_sp<SkPicture> picture =
          snapshot(box()
                       .width((float)size.width())
                       .height((float)size.height())
                       .child(std::move(root)),
                   fonts, {(float)size.width(), (float)size.height()}))
    surface->getCanvas()->drawPicture(picture);
  out.bitmap.allocPixels(info);
  if (!surface->readPixels(out.bitmap.pixmap(), 0, 0)) out.bitmap.reset();
  return out;
}

// ---------------------------------------------------------------------------
// Saying whether it was right

/** A claim and its verdict are `sigil::measure`'s — `measure::Check`, the
 *  `measure::check()` overloads and `measure::failures()`, from
 *  `<sigilmeasure/check/Check.h>`, spelled under that name. What this
 *  header adds is the one thing the measure library cannot know — how a
 *  check is written into a feed. */

/** THE INKS A TABLE IS REPORTED IN: the style name a feed's row takes, by
 *  the row's standing and its verdict, and the two column widths the
 *  rows are set at.
 *
 *  The names are parameters because the style set a plate reads is the
 *  caller's — nothing here knows what a given study calls its passing
 *  ink. The defaults are the convention the kit's tinted sets are usually
 *  built with; a set that registers none of them sets every row in its
 *  base style, which is legible but says nothing. */
struct ReportStyles {
  std::string pass = "pass";
  std::string fail = "fail";
  /** A finding that did not hold — the subject's failing, in its own ink
   *  on a plate that tells the two apart. */
  std::string finding = "fail";
  std::string reading = "number";
  std::string heading = "heading";
  int labelWidth = 44;
  int valueWidth = 8;
};

/** The style name @p c takes under @p styles. */
inline const std::string& styleOf(const measure::Check& c,
                                  const ReportStyles& styles) {
  switch (c.standing) {
    case measure::Standing::Heading:
      return styles.heading;
    case measure::Standing::Reading:
      return styles.reading;
    case measure::Standing::Finding:
      return c.pass ? styles.pass : styles.finding;
    case measure::Standing::Claim:
      break;
  }
  return c.pass ? styles.pass : styles.fail;
}

/** Append a check to a feed of text rows, the row's style name chosen by
 *  its standing and its verdict. The text is `Check::line()` at the
 *  styles' widths, so what the plate shows is what the table would
 *  print. */
inline void report(feed::TextRing& ring, const measure::Check& c,
                   const ReportStyles& styles) {
  const std::string text = c.line(styles.labelWidth, styles.valueWidth);
  ring.append({std::u8string(text.begin(), text.end()), styleOf(c, styles)});
}

/** The same, with only the two verdict inks named — a claim's pass and
 *  fail — and every other standing in the default ink for it. */
inline void report(feed::TextRing& ring, const measure::Check& c,
                   std::string passStyle = "pass",
                   std::string failStyle = "fail", int labelWidth = 44,
                   int valueWidth = 8) {
  report(ring, c,
         ReportStyles{.pass = std::move(passStyle),
                      .fail = std::move(failStyle),
                      .labelWidth = labelWidth,
                      .valueWidth = valueWidth});
}

/** A whole table into the feed, row by row, in the order it was made —
 *  the verification block of a study, printed as it runs. The summary
 *  line is not written: a plate is not where a run's exit status is read,
 *  and `Table::failures()` is what a build asks. */
inline void report(feed::TextRing& ring, const measure::Table& table,
                   const ReportStyles& styles = {}) {
  for (const measure::Check& c : table.rows) report(ring, c, styles);
}

}  // namespace sigil::compose::test
