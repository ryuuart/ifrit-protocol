#include "DunhuangStarChart.h"

auto DunhuangStarChart::flash(float t0, float t1, float t2) const
    -> Animatable<float> {
  if (settled) return Animatable<float>(0.0f);
  const float peak = std::min(0.999f, std::max(0.001f, (t1 - t0) / (t2 - t0)));
  return Animatable<float>(bind(&scribe)
                               .window(t0, t2)
                               .map([peak](float v) {
                                 return v < peak ? v / peak
                                                 : (1.0f - v) / (1.0f - peak);
                               })
                               .clamp(0, 1));
}

auto DunhuangStarChart::scrollX(float s, float& x) -> bool {
  const float xr = kOriginR - s * kPxMm;
  if (xr >= kBreakR - 2.0f && xr <= kW + 90.0f) {
    x = xr;
    return true;
  }
  const float xl = kOriginL - s * kPxMm;
  if (xl >= -90.0f && xl <= kBreakL + 2.0f) {
    x = xl;
    return true;
  }
  return false;
}

auto DunhuangStarChart::paperPoint(float ra, float dec, SkPoint& out,
                                   int& region) -> bool {
  if (dec >= 52.0f) {  // map 13 — azimuthal
    const float rmm = (kDiscCenDec - dec) / kPolPerMm;
    const float a = (kAzGain * wrap180(ra - 278.0f)) * kD;
    float xc;
    if (!scrollX(discCentreS(), xc)) return false;
    out = arrange::onEllipse({xc, kBandMid}, {rmm * kPxMm, rmm * kPxMm}, a);
    region = 13;
    return true;
  }
  if (dec > 45.0f || dec < -45.0f) {
    region = 0;
    return false;
  }
  const int k = mapOfRa(ra);
  const float dRa = wrap180(ra - mapCentre(k));
  if (std::abs(dRa) > 24.0f) {
    region = 0;
    return false;
  }
  const float s = mapSlotS(k) + kMapWmm * 0.5f + dRa / kRaPerMm;
  float x;
  if (!scrollX(s, x)) {
    region = k;
    return false;
  }
  const float decTop = mapGcDec(k) + 45.0f;
  out = {x, kFrameTop + (decTop - dec) / kDecPerMm * kPxMm};
  region = k;
  return true;
}

auto DunhuangStarChart::discFrame(SkPoint& c, float& rOuter) -> bool {
  float xc;
  if (!scrollX(discCentreS(), xc)) return false;
  c = {xc, kBandMid};
  rOuter = (kDiscCenDec - 52.0f) / kPolPerMm * kPxMm;
  return true;
}

auto DunhuangStarChart::rebuild(float epoch, float fold) -> void {
  if (std::abs(epoch - lastEpoch) < 1e-3f && std::abs(fold - lastFold) < 1e-4f)
    return;
  lastEpoch = epoch;
  lastFold = fold;
  const path::Rotation M = precession((epoch - 2000.0f) * 0.01f);
  auto pos = pool->positions();
  auto tint = pool->tints();
  for (int i = 0; i < nStars; ++i) {
    Placed& p = placed[(size_t)i];
    precess(M, cat.ra(i), cat.dec(i), p.raNow, p.decNow);
    p.sky = skyPoint(p.raNow, p.decNow);
    int region = 0;
    SkPoint paper;
    p.onPaper = paperPoint(p.raNow, p.decNow, paper, region);
    p.paper = p.onPaper ? paper : p.sky;
    const float f = smooth((fold - p.fold0) / 0.42f);
    pos[(size_t)i] = {p.sky.fX + (p.paper.fX - p.sky.fX) * f,
                      p.sky.fY + (p.paper.fY - p.sky.fY) * f};
    SkColor4f t{1, 1, 1, 1};
    // The sky the chart does NOT carry — the +45..+52 band between the
    // cylindrical maps and the disc, everything south of -45 — leaves the
    // plate entirely rather than lingering as ghosts ON the paper, where it
    // reads as dots. The count is in the feed instead.
    if (!p.onPaper) t.fA = 1.0f - f;
    tint[(size_t)i] = t;
  }
  pool->commit();
}

auto DunhuangStarChart::computeJoins() -> void {
  placed.assign((size_t)nStars, Placed{});
  astSchool.assign((size_t)nAst, ' ');
  astMap.assign((size_t)nAst, 0);
  astRa.assign((size_t)nAst, 0.0f);

  for (const M5Row& r : conc.map5) {
    for (int a = 0; a < nAst; ++a)
      if (cat.ast(a).id == r.cid) {
        astSchool[(size_t)a] = r.school;
        astMap[(size_t)a] = 5;
      }
    m5Sxc += r.sxc;
    m5Map += r.map;
  }
  for (const M13Row& r : conc.map13) {
    for (int a = 0; a < nAst; ++a)
      if (cat.ast(a).id == r.cid) {
        astSchool[(size_t)a] = r.school;
        astMap[(size_t)a] = 13;
      }
  }
  // 伐 Fa, the dagger: "the three hazy red stars ... (no specific label)".
  // Red, NOT encircled, and never named — 'H' is that state, not a school.
  for (int a = 0; a < nAst; ++a)
    if (cat.ast(a).id == "21B") {
      astSchool[(size_t)a] = 'H';
      astMap[(size_t)a] = 5;
    }

  // per-star school: from its asterism where the paper gives one
  std::vector<char> starSchool((size_t)nStars, ' ');
  const path::Rotation M = precession(-13.0f);
  for (int a = 0; a < nAst; ++a) {
    const AstRec& A = cat.ast(a);
    double sx = 0, sy = 0;
    int n = 0;
    for (int w = 0; w < A.words; ++w) {
      const uint16_t v = cat.verts[A.first + w];
      if (v == kVSep) continue;
      if (astSchool[(size_t)a] != ' ')
        starSchool[v] =
            astSchool[(size_t)a] == 'M' ? 'R' : astSchool[(size_t)a];
      float ra, dec;
      precess(M, cat.ra(v), cat.dec(v), ra, dec);
      sx += std::cos(ra * kD);
      sy += std::sin(ra * kD);
      ++n;
    }
    astRa[(size_t)a] = n ? wrap360((float)std::atan2(sy, sx) / kD) : 0.0f;
    if (astMap[(size_t)a] == 5) m5ChenZhuo += astUnique(cat, A);
  }

  for (int i = 0; i < nStars; ++i) {
    Placed& p = placed[(size_t)i];
    float ra, dec;
    precess(M, cat.ra(i), cat.dec(i), ra, dec);
    SkPoint dummy;
    int region = 0;
    const bool on = paperPoint(ra, dec, dummy, region);
    (void)on;
    if (dec >= 52.0f)
      ++nOnDisc;
    else if (dec > 45.0f)
      ++nInGap;
    else if (dec < -45.0f)
      ++nTooSouth;
    else
      ++nOnMaps;
    const char sc = starSchool[(size_t)i];
    if (sc == 'H') {
      p.cell = cellBare;
      ++nSchooled;
    }  // Fa, hazy and bare
    else if (sc == ' ') {
      p.cell = cellOpen;
      ++nUnattested;
    } else {
      ++nSchooled;
      p.cell = sc == 'R' ? cellRed : sc == 'B' ? cellBlack : cellWhite;
    }
    // the fold sweeps right-to-left, in reading order: by RA
    p.fold0 = 0.56f * (wrap360(ra - 296.0f) / 360.0f);
  }

  depMerc = mercatorVsLinear(-27.0f, 43.0f, 1.61f, 5.28f, 0.996f, 15);
  depStereo = stereoVsEquidistant(0.0f, 38.0f, 3.29f, 5.10f, 0.919f, 19);
}
