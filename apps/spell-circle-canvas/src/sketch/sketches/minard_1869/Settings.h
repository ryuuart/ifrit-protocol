#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathMeasure.h>
#include <include/core/SkTypeface.h>
#include <include/pathops/SkPathOps.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/table/Table.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Heading.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

namespace skia = sigil::material::skia;
namespace field = sigil::material::field;
namespace measure = sigil::measure;
namespace patterns = sigil::material::pattern;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using sigil::material::skia::Paint;
namespace geometry = sigil::geometry;
namespace path = sigil::geometry::path;
namespace data = sigil::data;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace minard_1869 {}
using namespace minard_1869;
namespace minard_1869 {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

using sigil::compose::hexColor;  // hexColor(0xRRGGBB[, a]) -> SkColor4f, usable
                                 // constexpr

// ---------------------------------------------------------------------------
// palette — sampled by percentile over masked regions of the two scans.
// TWO worlds, deliberately kept apart: the aged artefact, and the audit.

constexpr SkColor4f kDesk = hexColor(0x1b1a18);  // the table the sheet lies on
constexpr SkColor4f kPaperShadow = hexColor(0xb9ad98);  // p10: edges, foxing
constexpr SkColor4f kPaperBody = hexColor(0xcbbfab);  // p50: the sheet's ground
constexpr SkColor4f kPaperLight = hexColor(0xd6cab6);  // p90
// "LE ROUGE", AS THE SHEET PRINTS IT. Minard names the advance zone red
// and the 1869 lithograph lays it as a pale buff ochre — a warm cream a
// shade off the paper, not a rose. A pinker, darker zone competes with
// the black retreat band instead of sitting under it, which inverts the
// sheet's whole reading: the black is the figure and the zone the ground.
constexpr SkColor4f kZoneDark = hexColor(0xcfb890);
constexpr SkColor4f kZone = hexColor(0xd8c39b);
constexpr SkColor4f kZoneLight = hexColor(0xe0cda8);
// The engraved strengths. They are what the band's WIDTH is for, so they
// are set firm and black on the lithograph and have to be readable off
// the plate at plate size; smaller than this and the sheet's own subject
// cannot be checked against the drawing that carries it.
constexpr float kNumSize = 11.4f;

constexpr SkColor4f kInk = hexColor(0x25211d);  // p50 printed black
constexpr SkColor4f kInkDeep = hexColor(0x0a0806);
constexpr SkColor4f kInkThin = hexColor(0x4e4436);     // hairlines, hachures
constexpr SkColor4f kManuscript = hexColor(0x3b3a46);  // iron-gall, colder
constexpr SkColor4f kStampRed = hexColor(0x8e3b34);

constexpr SkColor4f kCard = hexColor(0xf2ece0);  // the audit's cooler paper
constexpr SkColor4f kCardInk = hexColor(0x1c1a17);
constexpr SkColor4f kBlue = hexColor(0x2f6f9c);      // MEASURED
constexpr SkColor4f kClaimRed = hexColor(0x8c2f22);  // WHAT THE LEGEND SAYS
constexpr SkColor4f kPass = hexColor(0x3e6b4a);
constexpr SkColor4f kAmber = hexColor(0xb5761e);
constexpr SkColor4f kGrey = hexColor(0x6d675c);

// ---------------------------------------------------------------------------
// composition — canvas 2560 x 1600

constexpr float kW = 2560.0f, kH = 1600.0f;
constexpr float kSheetX = 48.0f, kSheetY = 128.0f;
constexpr float kSheetW = 1400.0f, kSheetH = 1220.0f;  // aspect 1.1475
// The sheet is drawn at its own aspect, so this number is REAL: every band
// width on screen is a millimetre count of Minard's paper.
constexpr float kPxPerMm = kSheetW / 620.0f;  // 2.2581

// the printed frame, shared by both panels (BnF scan: x 157.5..3842.5)
constexpr float kFrameL = 46.0f;
constexpr float kFrameW = 579.14f * kPxPerMm;  // 1307.7
constexpr float kFrameR = kFrameL + kFrameW;
constexpr float kFrameT = 47.0f;
constexpr float kDivHN = 566.0f;   // Hannibal | Napoleon
constexpr float kDivNT = 1016.0f;  // map | temperature
constexpr float kFrameB = 1160.0f;

// Napoleon panel: x = a + b*lon, y = c − d*lat.
// b from the Commons scan (130.890 px/deg / 3.4482 px/mm * kPxPerMm);
// d/b = 2.142.
constexpr float kLonPx = 85.69f;
constexpr float kLon0 = 22.867f;           // longitude at the frame's left rule
constexpr float kLatPx = kLonPx * 2.142f;  // 183.55
constexpr float kLatRefY = 668.0f;         // y of lat 55.8 (Moscou)

// the audited scale: 1.1258 mm per 10,000 men (four direct BnF spot reads)
constexpr float kMmPer10k = 1.1258f;
constexpr float kStatedMmPer10k = 1.0f;
constexpr float kPxPer10k = kMmPer10k * kPxPerMm;  // 2.5421
constexpr float kLigneHalf = 1.1279f;              // 2.2558 / 2

inline float mapX(float lon) { return kFrameL + (lon - kLon0) * kLonPx; }
inline float mapY(float lat) { return kLatRefY + (55.8f - lat) * kLatPx; }
inline float bandPx(float men) { return men * kPxPer10k / 10000.0f; }

// audit column
constexpr float kAuditX = 1492.0f, kAuditW = 1020.0f;
constexpr float kConsoleY = 1356.0f, kConsoleH = 236.0f;

// ---------------------------------------------------------------------------
// THE DATA
//
// Minard.troops, all 51 rows, regrouped the way MINARD DRAWS IT rather
// than the way Wilkinson encodes it:
// Minard writes 422.000 on ONE band at the Niemen and lets the columns
// peel off; Wilkinson records three parallel bands from x = 0. Both are
// defensible; Minard's is the one that makes the flow identities visible.
//
// Every table stands in a file beside this sketch and is read through the
// hub like any other resource: the march by band, the twenty cities with
// Minard's plotted position and the gazetteer's, the nine engraved
// temperatures, the Hannibal sheet's stations and its place names, the
// three runs the audit measured off the scans, and the legend paragraphs.

struct Station {
  float lon, lat;
  float men;  // the strength the band CARRIES from this station eastward/onward
};

struct City {
  std::string plate;  // Minard's own spelling, kept
  float lon, lat;     // Minard's plotted position
  float rlon, rlat;   // gazetteer
  float dx, dy;       // label offset in sheet px
};

struct Temp {
  std::string label;  // exactly as engraved (8bre / 9bre / Xbre kept)
  float lon;
  float reaumur;
  int daysSincePrev;
};

// Stations READ OFF the BnF scan in sheet coordinates, which the panel's
// own lack of a fittable projection makes the only honest option: a
// least-squares latitude fit on that band returns d/b = 0.048 at
// R² = 0.12, i.e. there is no projection there to fit.
struct HStation {
  float x, y;
  float men;
  std::string at;
};

struct Place {
  std::string name;
  float x, y;
  int kind;  // 0 region caps, 1 town, 2 river, 3 people
};

/** One reading off a scan: a strength, and the band width it was drawn
 *  at in millimetres. */
struct Measured {
  float men, mm;
};

struct Leg {
  std::string name;
  float ratio;
};

/** EVERYTHING THE PLATE IS DRAWN FROM, as one value. */
struct Sheet {
  std::vector<Station> advTrunk, advNorth, advPolotzk;
  std::vector<Station> retEast, retWest, retPolotzk, retNorth;
  std::vector<City> cities;
  std::vector<Temp> temps;
  std::vector<HStation> hannibal;
  std::vector<Place> places;
  std::vector<Measured> treads, floorPts;
  std::vector<Leg> legs;
  std::vector<std::string> legend;
};

inline Sheet readSheet(sketch::Assets& assets) {
  Sheet s;
  const auto file = [&assets](const char* name) {
    return assets.table("data/minard/" + std::string(name));
  };

  if (const auto t = file("march.csv")) {
    const auto group = t->column<std::string>("group");
    const auto lon = t->column<double>("lon");
    const auto lat = t->column<double>("lat");
    const auto men = t->column<double>("men");
    // The seven bands are one file with a column that says which: the
    // retreat is drawn in two pieces so the Bobr junction is an ENDPOINT
    // of both, which is what test::endpointDegrees needs to see the army
    // as one component.
    const std::pair<std::string_view, std::vector<Station>*> band[] = {
        {"advance trunk", &s.advTrunk},     {"advance north", &s.advNorth},
        {"advance polotzk", &s.advPolotzk}, {"retreat east", &s.retEast},
        {"retreat west", &s.retWest},       {"retreat polotzk", &s.retPolotzk},
        {"retreat north", &s.retNorth}};
    for (size_t i = 0; i < group.size(); ++i)
      for (const auto& [name, into] : band)
        if (group[i] == name)
          into->push_back({(float)lon[i], (float)lat[i], (float)men[i]});
  }

  if (const auto t = file("cities.csv")) {
    const auto plate = t->column<std::string>("plate");
    const auto lon = t->column<double>("lon");
    const auto lat = t->column<double>("lat");
    const auto rlon = t->column<double>("rlon");
    const auto rlat = t->column<double>("rlat");
    const auto dx = t->column<double>("dx");
    const auto dy = t->column<double>("dy");
    for (size_t i = 0; i < plate.size(); ++i)
      s.cities.push_back({plate[i], (float)lon[i], (float)lat[i],
                          (float)rlon[i], (float)rlat[i], (float)dx[i],
                          (float)dy[i]});
  }

  if (const auto t = file("temperatures.csv")) {
    const auto label = t->column<std::string>("label");
    const auto lon = t->column<double>("lon");
    const auto reaumur = t->column<double>("reaumur");
    const auto days = t->column<double>("days");
    for (size_t i = 0; i < label.size(); ++i)
      s.temps.push_back(
          {label[i], (float)lon[i], (float)reaumur[i], (int)days[i]});
  }

  if (const auto t = file("hannibal.csv")) {
    const auto x = t->column<double>("x");
    const auto y = t->column<double>("y");
    const auto men = t->column<double>("men");
    const auto at = t->column<std::string>("at");
    for (size_t i = 0; i < x.size(); ++i)
      s.hannibal.push_back({(float)x[i], (float)y[i], (float)men[i], at[i]});
  }

  if (const auto t = file("hannibal_places.csv")) {
    const auto name = t->column<std::string>("name");
    const auto x = t->column<double>("x");
    const auto y = t->column<double>("y");
    const auto kind = t->column<double>("kind");
    for (size_t i = 0; i < name.size(); ++i)
      s.places.push_back({name[i], (float)x[i], (float)y[i], (int)kind[i]});
  }

  const auto readMeasured = [&](const char* name, std::vector<Measured>& into) {
    if (const auto t = file(name)) {
      const auto men = t->column<double>("men");
      const auto mm = t->column<double>("mm");
      for (size_t i = 0; i < men.size(); ++i)
        into.push_back({(float)men[i], (float)mm[i]});
    }
  };
  readMeasured("band_treads.csv", s.treads);
  readMeasured("band_floor.csv", s.floorPts);

  if (const auto t = file("leg_ratios.csv")) {
    const auto leg = t->column<std::string>("leg");
    const auto ratio = t->column<double>("ratio");
    for (size_t i = 0; i < leg.size(); ++i)
      s.legs.push_back({leg[i], (float)ratio[i]});
  }

  if (const auto t = file("legend.csv"))
    for (const std::string& line : t->column<std::string>("text"))
      s.legend.push_back(line);

  return s;
}

// ---------------------------------------------------------------------------
// geometry helpers

inline SkPoint stationPt(const Station& s) {
  return {mapX(s.lon), mapY(s.lat)};
}

inline SkPath polyline(const std::vector<Station>& st) {
  SkPathBuilder p;
  for (size_t i = 0; i < st.size(); ++i) {
    const SkPoint q = stationPt(st[i]);
    if (i == 0)
      p.moveTo(q);
    else
      p.lineTo(q);
  }
  return p.detach();
}

inline SkPath polylineH(const std::vector<HStation>& st) {
  SkPathBuilder p;
  for (size_t i = 0; i < st.size(); ++i) {
    if (i == 0)
      p.moveTo(st[i].x, st[i].y);
    else
      p.lineTo(st[i].x, st[i].y);
  }
  return p.detach();
}

/** Cumulative arc length to each station of a polyline. */
inline std::vector<float> arcAt(const std::vector<SkPoint>& pts) {
  std::vector<float> a(pts.size(), 0.0f);
  for (size_t i = 1; i < pts.size(); ++i)
    a[i] = a[i - 1] + SkPoint::Distance(pts[i], pts[i - 1]);
  return a;
}

/** The one trap every implementer hits: index the width by ARC LENGTH,
 *  not by PathSample::fraction and not by longitude. The stations are not
 *  equally spaced and the retreat's hook below Moscow is not monotone in
 *  x, so a fraction-indexed profile puts every riser in the wrong place.
 *  The check is concrete — each riser must fall at its named city.
 *
 *  A STRENGTH THAT CHANGES AT A PLACE IS A STEPPED LAW, which is what
 *  `profile::Spans` holds: the boundaries are the cumulative arc lengths
 *  of the stations and the widths are the band px each station carries
 *  onward, one more width than there are boundaries. It does not
 *  interpolate, and it must not — Minard engraves a strength that drops
 *  at a city, not a curve through the cities. */
using WidthProfile = path::profile::Spans;

/** THE FLOW BAND'S WIDTH, as a comparable Profile — and the sketch's own
 *  trap (above) is exactly why it is PX-KEYED. `alongIsPx` makes the seam
 *  hand `across` arc-length px measured from the spine's start, which is
 *  the unit `WidthProfile::arc` is already in, so every riser stays at its
 *  named city. Under the `spans::upTo(reveal)` the band is drawn with, a
 *  fraction would be a fraction of the REVEALED route and every riser
 *  would march east as the campaign advances.
 *
 *  `scale` is the LIVE 12.6% morph, read at paint. The profile compares on
 *  the pointer, not on the value behind it — two bands reading the same
 *  Output ARE the same law — and the node stays `Cache::None`, which is how
 *  a node holds a value the reconciler cannot see change. Being comparable
 *  also makes the ribbon equal to ITSELF, so an identical re-describe
 *  prunes instead of re-recording the whole band.
 *
 *  max(): the morph runs mmScale from kMmPer10k DOWN to kStatedMmPer10k
 *  (1.1258 → 1.0), so the ratio never exceeds 1 and the widest the band
 *  ever draws is the stepped law's own widest exactly. A guessed bound is
 *  not needed and would be wrong: this one is derived from the morph's
 *  own endpoints. */
struct FlowWidth {
  WidthProfile prof;
  const ch::Output<float>* scale = nullptr;
  static constexpr bool alongIsPx = true;
  float across(float px) const {
    return prof.across(px) * ((scale ? scale->value() : kMmPer10k) / kMmPer10k);
  }
  float max() const { return prof.max(); }
  bool operator==(const FlowWidth& o) const {
    return prof == o.prof && scale == o.scale;
  }
};

/** The stepped law over a route: the boundaries are the arc lengths of
 *  the stations AFTER the first — a span holds from the boundary before
 *  it to the boundary at it, so the leading width holds from the start —
 *  and the widths are what each station carries onward. */
inline WidthProfile spansOver(const std::vector<SkPoint>& pts,
                              const std::vector<float>& men) {
  const std::vector<float> arc = arcAt(pts);
  WidthProfile w;
  w.upTo.assign(arc.begin() + (arc.empty() ? 0 : 1), arc.end());
  for (float m : men) w.widthsPx.push_back(bandPx(m));
  return w;
}

inline WidthProfile profileOf(const std::vector<Station>& st) {
  std::vector<SkPoint> pts;
  std::vector<float> men;
  pts.reserve(st.size());
  men.reserve(st.size());
  for (const Station& s : st) {
    pts.push_back(stationPt(s));
    men.push_back(s.men);
  }
  return spansOver(pts, men);
}

inline WidthProfile profileOfH(const std::vector<HStation>& st) {
  std::vector<SkPoint> pts;
  std::vector<float> men;
  pts.reserve(st.size());
  men.reserve(st.size());
  for (const HStation& s : st) {
    pts.push_back({s.x, s.y});
    men.push_back(s.men);
  }
  return spansOver(pts, men);
}

/** ∫ w ds over the profile — the ink Minard intended, in px². */
inline float inkIntegral(const SkPath& spine, const WidthProfile& w,
                         float step = 1.0f) {
  float total = 0;
  SkContourMeasureIter iter(spine, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
    for (float d = 0; d < len; d += step)
      total += w.across(d) * std::min(step, len - d);
  }
  return total;
}

// ---------------------------------------------------------------------------
// haversine, for the geography card

inline float haversineKm(float lon1, float lat1, float lon2, float lat2) {
  const float R = 6371.0088f;
  const float p1 = lat1 * kDeg, p2 = lat2 * kDeg;
  const float dp = (lat2 - lat1) * kDeg, dl = (lon2 - lon1) * kDeg;
  const float a =
      std::sin(dp / 2) * std::sin(dp / 2) +
      std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
  return 2 * R * std::asin(std::min(1.0f, std::sqrt(a)));
}
inline float cityKm(const City& c) {
  return haversineKm(c.lon, c.lat, c.rlon, c.rlat);
}

// ---------------------------------------------------------------------------
// path sugar — both spellings are COMPARABLE values, so the nodes wearing
// them prune. A rule between two points is a function of its two points
// and nothing else, so the two points are its key; a path already cooked
// is its own identity, and `heldPath` compares it by the generation the
// copy carries.

inline Shape segFn(SkPoint a, SkPoint b) {
  return keyedShape(std::tuple(a.x(), a.y(), b.x(), b.y()), [a, b](SkSize) {
    SkPathBuilder p;
    p.moveTo(a);
    p.lineTo(b);
    return p.detach();
  });
}
inline Shape pathFn(SkPath path) { return heldPath(std::move(path)); }

/** The coastlines and rivers of both panels: A SMOOTH PATH THE POINTS
 *  STEER — one quadratic per interior point, bounded by the hull they
 *  span and passing through none of them. That is what makes a dozen
 *  placed points read as one coast instead of a chain of chords. */
inline SkPath smooth(const std::vector<SkPoint>& p) {
  std::vector<glm::vec2> pts;
  pts.reserve(p.size());
  for (const SkPoint& q : p) pts.push_back({q.x(), q.y()});
  return path::smoothThrough(pts);
}

inline SkPath rectPath(float l, float t, float r, float bm) {
  SkPathBuilder p;
  p.addRect(SkRect::MakeLTRB(l, t, r, bm));
  return p.detach();
}

inline weave::TextStyle type(sk_sp<SkTypeface> face, float size,
                             SkColor4f color, float tracking = 0) {
  return weave::textStyle({.face = std::move(face),
                           .size = size,
                           .color = color,
                           .track = tracking});
}

/** The French thousands separator the plate actually engraves: 422.000,
 *  never 422,000. */
inline std::string french(float men) {
  const long v = (long)std::lround(men);
  std::string s = std::to_string(v);
  for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ".");
  return s;
}

}  // namespace minard_1869

// ===========================================================================
