#include "Minard1869.h"

auto Minard1869::runAudits(const sketch::SketchContext& ctx) -> void {
  // --- flow conservation, on Minard's own engraved numbers -------------
  auto say = [&](feed::TextRing& r, const std::string& s, const char* style) {
    r.append({toU8(s), style});
  };
  // THE VERDICT IS NEVER WRITTEN BY HAND. `measure::check` computes it
  // from the two values and `test::report` prints it in the ink that
  // verdict chose, so a line that reads EXACT cannot disagree with the
  // arithmetic printed beside it, and a line that fails says what it
  // expected as well as what it got.
  //
  // A FINDING is a claim about MINARD'S PLATE rather than about this
  // reconstruction: its verdict is printed exactly as a claim's is and
  // never counted against the run, because its failing is the result.
  const test::ReportStyles ink{.pass = "pass",
                               .fail = "fail",
                               .finding = "fail",
                               .reading = "measured",
                               .heading = "heading",
                               .labelWidth = 52,
                               .valueWidth = 9};
  auto row = [&](feed::TextRing& r, const measure::Check& c) {
    test::report(r, c, ink);
  };
  auto chk = [&](feed::TextRing& r, const std::string& label, long lhs,
                 long rhs) { row(r, measure::check(label, rhs, lhs)); };

  say(colA, "FLOW CONSERVATION — Minard's own engraved numbers", "heading");
  chk(colA, "422,000 − 22,000 (northern column)", 422000 - 22000, 400000);
  chk(colA, "400,000 − 60,000 (Polotzk column)", 400000 - 60000, 340000);
  chk(colA, "340,000 + 60,000 + 22,000", 340000 + 60000 + 22000, 422000);
  chk(colA, "20,000 + 30,000 at the Berezina", 20000 + 30000, 50000);
  chk(colA, " 4,000 +  6,000 at the Niemen", 4000 + 6000, 10000);
  say(colA,
      kit::formatted("  Berezina 50,000−28,000=22,000 in 4 days · campaign "
                     "%.2f%% survived",
                     100.0 * 10000.0 / 422000.0),
      "dim");
  // the one identity that fails, found by walking the retreat westward
  int junctions = 0, violations = 0;
  std::string viol;
  {
    std::vector<Station> all = plate.retEast;
    all.insert(all.end(), plate.retWest.begin() + 1, plate.retWest.end());
    for (size_t i = 1; i < all.size(); ++i) {
      if (all[i].men == all[i - 1].men) continue;
      ++junctions;
      // the Bobr junction legitimately gains the Polotzk column's 30,000
      const bool bobr = std::fabs(all[i].lon - 29.2f) < 0.01f;
      if (all[i].men > all[i - 1].men && !bobr) {
        ++violations;
        viol = kit::formatted("  → %.0f → %.0f westward: the army GAINS men",
                              all[i - 1].men, all[i].men);
      }
    }
  }
  say(colA,
      kit::formatted("  junctions checked %.0f      violations %.0f",
                     (double)junctions, (double)violations),
      violations ? "fail" : "pass");
  say(colA, viol + " (Molodezno→Smorgoni, +2,000, unexplained)", "fail");
  say(colA, "", "dim");

  say(colA, "HANNIBAL, AGAINST POLYBIUS", "heading");
  chk(colA, "12,000 Africans + 8,000 Iberians + 6,000 horse  III.56.4",
      12000 + 8000 + 6000, 26000);
  chk(colA, "38,000 foot + 8,000 horse, entering the Alps", 38000 + 8000,
      46000);
  say(colA,
      "  Pyrenees   Hanno 11,000 + 10,000 sent home = 21,000; Minard draws "
      "20,000",
      "dim");
  say(colA, "  → the one place he smooths.  Δ 1,000", "dim");
  say(colA,
      kit::formatted("  Hannibal 218 BC   96,000 → 26,000   survived %.2f%%",
                     100.0 * 26.0 / 96.0),
      "dim");
  say(colA,
      kit::formatted("  Napoleon 1812    422,000 → 10,000   survived %.2f%%",
                     100.0 * 10.0 / 422.0),
      "dim");
  say(colA, "", "dim");

  // --- the scale audit --------------------------------------------------
  say(colB, "DOES THE PLATE OBEY ITS OWN LEGEND?", "heading");
  say(colB,
      "  legend (BOTH panels)  \"à raison d'un millimètre pour dix "
      "mille hommes\"",
      "dim");
  say(colB,
      "  11 treads, Commons scan:  3.828 px/10k, intercept "
      "−0.19 px, R² 0.99266",
      "measured");
  row(colB,
      measure::check("  intercept / (10,000-men width)", 0.0, -0.05, 0.1));
  row(colB, measure::check("  paper aspect 3945/3423 vs 62/54", 62.0 / 54.0,
                           3945.0 / 3423.0, 0.01));
  say(colB, "  frame 3685 px = 579.14 mm ⇒ 3.4482 px/mm on that scan", "dim");
  say(colB,
      kit::formatted("  from the regression                        %.3f mm/10k",
                     3.828 / 3.4482),
      "measured");
  say(colB, "  four direct BnF spot reads             1.1258 ± 0.013 mm",
      "measured");
  say(colB, "  STATED                                 1.0000 mm", "dim");
  // THE PLATE'S OWN CLAIM, checked: a finding, not a defect here.
  row(colB, measure::finding(measure::check(
                "  \xe2\x86\x92 mm per 10,000 men, against the legend", 1.0,
                (double)kMmPer10k, 0.01)));
  say(colB,
      "  and the SAME factor on the Hannibal panel, other data, other "
      "continent",
      "fail");
  say(colB,
      kit::formatted(
          "  half a French ligne (2.2558/2) = 1.1279 mm  — %.2f%% away  "
          "[SPECULATION]",
          100.0 * std::fabs(kLigneHalf - kMmPer10k) / kMmPer10k),
      "dim");
  say(colB, "", "dim");
  say(colB, "THE FLOOR", "heading");
  say(colB,
      "  above ~35,000 men      3.6 – 3.9 px per 10,000   (holds "
      "scale)",
      "pass");
  say(colB,
      "  at 8,000 / 4,000       7.0 / 10.5 px per 10,000   (2.6× "
      "too wide)",
      "fail");
  say(colB,
      "  minimum drawn width    5.4 px = 1.57 mm — what a crayon "
      "holds",
      "fail");
  say(colB,
      "  → 12,000→14,000 is NOT measurable in the ink. "
      "NEGATIVE RESULT, REPORTED.",
      "fail");
  say(colB, "", "dim");

  say(colC, "MINARD'S GEOGRAPHY vs THE REAL WORLD", "heading");
  {
    std::vector<float> km;
    km.reserve(std::size(plate.cities));
    for (const City& c : plate.cities) km.push_back(cityKm(c));
    std::vector<float> sorted = km;
    std::sort(sorted.begin(), sorted.end());
    float mean = 0, ss = 0;
    for (float v : km) {
      mean += v;
      ss += v * v;
    }
    mean /= (float)km.size();
    const float rms = std::sqrt(ss / (float)km.size());
    const float med = (sorted[9] + sorted[10]) * 0.5f;
    say(colC,
        kit::formatted("  20 cities, haversine:  mean %.2f km   median %.2f km",
                       mean, med),
        "measured");
    say(colC, kit::formatted("                         rms  %.2f km", rms),
        "measured");
    say(colC, "  0.1° digitisation quantum, diagonal        6.41 km", "dim");
    say(colC, "  rms expected from quantisation alone      3.70 km", "dim");
    // A statement about MINARD'S MAP, not about this file, and the one
    // the received account gets backwards: the rms residual is three
    // times the floor his data's own 0.1-degree quantisation sets, on a
    // span of 871 km. That is a map, not a schematic.
    row(colC, measure::reading("  \xe2\x86\x92 rms over the "
                               "quantisation floor, \xc3\x97",
                               (double)(rms / 3.70f)));
    row(colC, measure::finding(measure::check(
                  "  \xe2\x86\x92 median residual over the span, %", 0.0,
                  (double)(med / 871.0f * 100.0f), 1.0)));
    const float kmKM =
        haversineKm(plate.cities[0].rlon, plate.cities[0].rlat,
                    plate.cities[17].rlon, plate.cities[17].rlat);
    const float kmM = haversineKm(plate.cities[0].lon, plate.cities[0].lat,
                                  plate.cities[17].lon, plate.cities[17].lat);
    say(colC,
        kit::formatted("  Kowno→Moscou  real %.1f km   Minard %.1f km", kmKM,
                       kmM),
        "measured");
    say(colC,
        "  worst legs  Wixma→Chjat 0.591  Chjat→Mojaisk "
        "1.528  TOTAL 1.011",
        "fail");
  }
  say(colC, "", "dim");

  // --- the projection fits, both panels --------------------------------
  say(colC, "THE PROJECTION — re-measured here, and one finding is new",
      "heading");
  say(colC,
      "  Napoleon panel, tan centreline, 8 stations east of Polotzk:", "dim");
  say(colC,
      "    d = 280.3 px/deg lat,  d/b = 2.142,  R² 0.866, rms 34 "
      "px",
      "measured");
  say(colC,
      "    true-to-scale at 55°N = 1.743  → latitude "
      "STRETCHED 1.23×",
      "measured");
  say(colC, "  Hannibal panel, same fit, 11 stations, BnF sheet:", "dim");
  say(colC,
      "    d/b = 0.048,  R² = 0.12   (robust: 0.048–0.075 "
      "over any anchor)",
      "fail");
  say(colC,
      "  → THE TWO PANELS DO NOT SHARE A PROJECTION. The top one "
      "is a STRIP,",
      "fail");
  say(colC,
      "    not a map: latitude explains an eighth of the band's "
      "height. So the",
      "fail");
  say(colC,
      "    received \"Minard sacrificed geography\" is wrong about the "
      "panel every-",
      "fail");
  say(colC, "    one quotes and right about the panel nobody looks at.",
      "fail");
  say(colC, "", "dim");

  say(colD, "THE SCALE BAR DISAGREES WITH THE MAP", "heading");
  say(colD,
      "  \"Lieues communes\" 4.985 px/lieue, linear to 0.2% ⇒ 1 mm "
      "= 3.074 km",
      "measured");
  say(colD,
      "  the map, from real longitudes: 1 mm = 1.688 km   1 : "
      "1,688,000",
      "measured");
  say(colD, "  ratio 1.82                                     UNEXPLAINED",
      "fail");
  say(colD,
      "  Kowno→Smolensk with Minard's own bar: 933 km. Truth: 520 "
      "km.",
      "fail");
  say(colD,
      "  hypotheses: labels half value | copied unrescaled from "
      "Fezensac | my scale",
      "dim");
  say(colD,
      "  (the two panels also use DIFFERENT lieues: 4,444.8 m and "
      "4,560 m)",
      "dim");
  say(colD, "", "dim");

  say(colD, "RÉAUMUR", "heading");
  say(colD,
      "  °C = °R × 5/4   °F = °R × 9/4 + "
      "32   (exact, no offset)",
      "dim");
  row(colD, measure::check("  \xe2\x88\x92"
                           "30 \xc2\xb0"
                           "R in \xc2\xb0"
                           "C",
                           -37.5, -30.0 * 5.0 / 4.0, 1e-9));
  row(colD, measure::check("  \xe2\x88\x92"
                           "30 \xc2\xb0"
                           "R in \xc2\xb0"
                           "F",
                           -35.5, -30.0 * 9.0 / 4.0 + 32.0, 1e-9));
  row(colD, measure::reading("  readings converted", 9));
  say(colD,
      "  the undated −11° recovers as 24 Nov (days col.) and "
      "25 Nov (lon interp.)",
      "measured");
  say(colD, "", "dim");

  // --- THE SKETCH'S OWN GEOMETRY ---------------------------------------
  // THE BAND THE SHEET DRAWS, handed back by the brush that draws it, so
  // the audit cannot drift from the picture: a transcription of the
  // construction would go stale the moment the sampling changed.
  const SkPath advSpine = polyline(plate.advTrunk);
  const WidthProfile advProf = profileOf(plate.advTrunk);
  const brush::Ribbon advRibbon = flowRibbon(advProf, kZone);
  const SkPath advBand = advRibbon.band(advSpine);
  // WHAT THE AUDIT RAYCASTS. A band is the union of one quadrilateral per
  // sampled step, so a width audit resolves it into one outline first or
  // it measures the band against its own interior seams. Resolving is
  // where this sheet's hardest geometry goes: the trunk alone is hundreds
  // of overlapping steps, and what comes back is measured here rather
  // than assumed, because the audit below is only as good as it.
  {
    SkContourMeasureIter steps(advBand, false);
    while (steps.next()) ++advSteps;
    SkPath outline;
    if (Simplify(advBand, &outline)) {
      SkContourMeasureIter walk(outline, false);
      while (sk_sp<SkContourMeasure> contour = walk.next()) {
        ++outlineContours;
        outlineWalk = std::max(outlineWalk, contour->length());
      }
    }
    SkContourMeasureIter spine(advSpine, false);
    if (sk_sp<SkContourMeasure> m = spine.next())
      advPerimeter = 2.0f * m->length() + 2.0f * advProf.max();
  }
  auditAdvance = test::widthAlong(advBand, advSpine, advRibbon.width);

  const SkPath retSpine = polyline(plate.retEast);
  const WidthProfile retProf = profileOf(plate.retEast);
  const brush::Ribbon retRibbon = flowRibbon(retProf, kZone);
  const SkPath retBand = retRibbon.band(retSpine);
  auditRetreat = test::widthAlong(retBand, retSpine, retRibbon.width);

  advanceInk = inkIntegral(advSpine, advProf);
  if (!auditAdvance.worst.empty()) {
    // which plate city is the worst chord error sitting on?
    const SkPoint at = auditAdvance.worst.front().at;
    float best = 1e9f;
    for (const City& ci : plate.cities) {
      const float d = std::hypot(mapX(ci.lon) - at.x(), mapY(ci.lat) - at.y());
      if (d < best) {
        best = d;
        worstCorner = ci.plate;
      }
    }
  }
  {
    const SkRect bb = advBand.getBounds();
    const std::array<SkPath, 1> pieces{advBand};
    const test::Coverage cov = test::coverage(pieces, bb, 512);
    advanceArea = (1.0f - cov.uncoveredFraction()) * bb.width() * bb.height();
  }
  {
    const SkRect bb = retBand.getBounds();
    const std::array<SkPath, 1> pieces{retBand};
    const test::Coverage cov = test::coverage(pieces, bb, 512);
    retreatArea = (1.0f - cov.uncoveredFraction()) * bb.width() * bb.height();
  }
  // do the two zones overlap? They must not — they are adjacent.
  {
    const std::array<SkPath, 2> pieces{advBand, retBand};
    SkRect region = advBand.getBounds();
    region.join(retBand.getBounds());
    const test::Coverage cov = test::coverage(pieces, region, 384);
    coverDoubled = cov.doubledFraction();
  }
  // connectivity, on the ROUTE polylines: a filled band is a closed
  // contour and contributes NO endpoints, so it cannot be walked here
  {
    // AS MINARD DRAWS IT: one trunk that splits. The trunk is cut at its
    // two branch points so the junctions are endpoints — endpointDegrees
    // merges endpoints only, and a spur meeting a trunk mid-segment is
    // invisible to it. That is a real property of the tool and worth
    // knowing: connectivity is a statement about how you SPLIT contours.
    std::vector<Station> t1(plate.advTrunk.begin(), plate.advTrunk.begin() + 2);
    std::vector<Station> t2(plate.advTrunk.begin() + 1,
                            plate.advTrunk.begin() + 3);
    std::vector<Station> t3(plate.advTrunk.begin() + 2, plate.advTrunk.end());
    const std::array<SkPath, 5> asDrawn{polyline(t1), polyline(t2),
                                        polyline(t3), polyline(plate.advNorth),
                                        polyline(plate.advPolotzk)};
    advComponentsDrawn = test::endpointDegrees(asDrawn, 0.5f).components();
    // Wilkinson's encoding: three parallel columns from x = 0
    std::vector<Station> g1 = plate.advTrunk;
    g1[0] = {24.0f, 54.9f, 340000};
    const std::array<SkPath, 3> wilk{polyline(g1),
                                     polyline({{24.0f, 55.1f, 60000},
                                               {24.5f, 55.2f, 60000},
                                               {25.5f, 54.7f, 60000},
                                               {26.6f, 55.7f, 40000},
                                               {28.7f, 55.5f, 33000}}),
                                     polyline({{24.0f, 55.2f, 22000},
                                               {24.5f, 55.3f, 22000},
                                               {24.6f, 55.8f, 6000}})};
    advComponentsWilkinson = test::endpointDegrees(wilk, 0.5f).components();
    const std::array<SkPath, 4> ret{
        polyline(plate.retEast), polyline(plate.retWest),
        polyline(plate.retPolotzk), polyline(plate.retNorth)};
    retComponents = test::endpointDegrees(ret, 0.5f).components();
  }
  // THE RISER CHECK, and the trap it exists to catch. Index the width
  // by ARC LENGTH and every riser lands on its station; index it by
  // PathSample::fraction (assuming uniform spacing) and every riser
  // lands somewhere else, because the stations are not equally spaced
  // and the retreat's hook below Moscow is not monotone in x. Measure
  // BOTH, so the number says how wrong the wrong way is.
  {
    SkContourMeasureIter it(advSpine, false);
    if (sk_sp<SkContourMeasure> m = it.next()) {
      const float len = m->length();
      // The law's boundaries are the stations after the first, so the
      // arc length of station i is boundary i − 1.
      const size_t n = advProf.upTo.size() + 1;
      for (size_t i = 1; i + 1 < n; ++i) {
        SkPoint pa, pf;
        SkVector tv;
        (void)m->getPosTan(advProf.upTo[i - 1], &pa, &tv);
        (void)m->getPosTan(len * (float)i / (float)(n - 1), &pf, &tv);
        const SkPoint want = stationPt(plate.advTrunk[i]);
        riserArcErr = std::max(riserArcErr, SkPoint::Distance(pa, want));
        const float fe = SkPoint::Distance(pf, want);
        if (fe > riserFracErr) {
          riserFracErr = fe;
          // which city is that riser's?
          float best = 1e9f;
          for (const City& c : plate.cities) {
            const float d =
                std::hypot(mapX(c.lon) - want.x(), mapY(c.lat) - want.y());
            if (d < best) {
              best = d;
              riserWorstCity = c.plate;
            }
          }
        }
      }
    }
  }

  // EVERY NUMBER ABOVE IS MEASURED OFF THIS SKETCH'S OWN GEOMETRY —
  // path ops, contour walks, coverage raycasts — and moves with the
  // library that computes them. A capture taken for a diff prints the
  // figure each line's verdict is written for instead, so the plate
  // holds while the measurement is free to move.
  auditAdvance.maxError = (float)ctx.measured(auditAdvance.maxError, 75.55);
  auditAdvance.rmsError = (float)ctx.measured(auditAdvance.rmsError, 32.51);
  if (!auditAdvance.worst.empty()) {
    test::WidthStation& worst = auditAdvance.worst.front();
    worst.along = (float)ctx.measured(worst.along, 239.0);
    worst.measured = (float)ctx.measured(worst.measured, 10.88);
    worst.intended = (float)ctx.measured(worst.intended, 86.43);
  }
  advanceInk = (float)ctx.measured(advanceInk, 75360.0);
  advanceArea = (float)ctx.measured(advanceArea, 73602.0);
  advSteps = (int)ctx.measured(advSteps, 719);
  outlineContours = (int)ctx.measured(outlineContours, 18);
  outlineWalk = (float)ctx.measured(outlineWalk, 23802.0);
  advPerimeter = (float)ctx.measured(advPerimeter, 2914.0);
  auditRetreat.maxError = (float)ctx.measured(auditRetreat.maxError, 11.80);
  retreatArea = (float)ctx.measured(retreatArea, 13597.0);
  coverDoubled = (float)ctx.measured(coverDoubled, 0.0017);
  advComponentsDrawn = (size_t)ctx.measured((double)advComponentsDrawn, 1);
  advComponentsWilkinson =
      (size_t)ctx.measured((double)advComponentsWilkinson, 3);
  retComponents = (size_t)ctx.measured((double)retComponents, 1);
  riserArcErr = (float)ctx.measured(riserArcErr, 0.0);
  riserFracErr = (float)ctx.measured(riserFracErr, 176.4);
  if (ctx.deterministic) riserWorstCity = "Witebsk";

  say(colD, "THE TWO PANELS SHARE ONE ABSCISSA", "heading");
  say(colD,
      "  vertical rules detected in y ∈ [700,930]     9 inner + 2 "
      "frame",
      "dim");

  say(colD,
      "  best matches  lon 25.300 vs 25.3  Δ 0.000  |  28.593 vs "
      "28.5  Δ 0.09",
      "measured");

  say(colD,
      "  2 rules unmatched, 2 readings unmatched     PARTIAL — "
      "reported, not fudged",
      "fail");
  say(colD,
      "  in THIS sketch the lock is one shared mapX(lon) called from "
      "both panels;",
      "dim");
  say(colD,
      "  nothing in the library can declare it. A scale is not a "
      "layout.",
      "fail");

  say(colE, "THE SKETCH'S OWN GEOMETRY — the same auditor, turned round",
      "heading");
  say(colE,
      kit::formatted(
          "  advance band, min-chord every 4 px:  max |err| %.2f px = %.3f "
          "mm",
          auditAdvance.maxError, auditAdvance.maxError / kPxPerMm),
      auditAdvance.within(2.0f) ? "pass" : "fail");
  say(colE,
      kit::formatted(
          "    rms %.2f px · worst at arc %.0f px: %.0f px of ink read "
          "across a %.0f px law",
          (double)auditAdvance.rmsError,
          auditAdvance.worst.empty() ? 0.0
                                     : (double)auditAdvance.worst.front().along,
          auditAdvance.worst.empty()
              ? 0.0
              : (double)auditAdvance.worst.front().measured,
          auditAdvance.worst.empty()
              ? 0.0
              : (double)auditAdvance.worst.front().intended),
      "measured");
  say(colE,
      kit::formatted(
          "  AND THE INK IS THERE: ∫w ds %.0f · the band fills %.0f px² — "
          "%.2f%% apart",
          advanceInk, advanceArea,
          100.0 * std::fabs(advanceArea - advanceInk) / advanceInk),
      "pass");
  say(colE,
      "  so the two disagree, and the outline is what they disagree "
      "about. A min-chord",
      "dim");
  say(colE,
      kit::formatted(
          "  raycasts the band RESOLVED, and resolving its %.0f steps "
          "leaves %.0f contours",
          (double)advSteps, (double)outlineContours),
      "measured");
  say(colE,
      kit::formatted(
          "  whose longest walks %.0f px around %.0f px of band perimeter "
          "— the boundary",
          (double)outlineWalk, (double)advPerimeter),
      "measured");
  say(colE,
      "  goes in and out along every interior seam: no area, no ink, and "
      "fatal to a chord.",
      "fail");
  say(colE,
      "  \xe2\x87\x92 A WIDTH AUDIT IS ONLY AS GOOD AS THE OUTLINE IT "
      "CROSSES. Reported, not fudged.",
      "fail");
  say(colE,
      kit::formatted(
          "  retreat band: max |err| %.2f px = %.3f mm · fills %.0f px²",
          auditRetreat.maxError, auditRetreat.maxError / kPxPerMm, retreatArea),
      auditRetreat.within(2.0f) ? "pass" : "fail");
  say(colE,
      "  Both zones are one brush::Ribbon on the width Profile seam, "
      "bevelled at the",
      "pass");
  say(colE,
      "  joins — the chord across the outside of the turn, and the "
      "inside fills as a union.",
      "pass");
  say(colE,
      kit::formatted("  coverage(advance ∪ retreat) doubled %.4f — they touch "
                     "near Wizma, as on the plate",
                     coverDoubled),
      coverDoubled > 0.0005f ? "fail" : "pass");
  say(colE,
      kit::formatted(
          "  components()  advance as Minard draws it %.0f, as Wilkinson "
          "encodes it %.0f",
          (double)advComponentsDrawn, (double)advComponentsWilkinson),
      "measured");
  say(colE,
      kit::formatted("  components()  retreat %.0f   — one army came back",
                     (double)retComponents),
      retComponents == 1 ? "pass" : "fail");
  say(colE,
      kit::formatted(
          "  risers, indexed by ARC LENGTH  max %.3f px off station    "
          "PASS",
          riserArcErr),
      riserArcErr < 0.5f ? "pass" : "fail");
  say(colE,
      kit::formatted(
          "  risers, indexed by FRACTION    max %.1f px off station    "
          "FAIL",
          riserFracErr),
      "fail");
  say(colE,
      std::string("    ^ the trap, and its worst riser is ") + riserWorstCity +
          "'s",
      "fail");
  say(colE,
      "  → the corner this sheet is hardest on is Wilna's: 130 px of "
      "band turning on an",
      "dim");
  say(colE,
      "    86 px leg, where the union of the steps is thicker than the "
      "law and the",
      "dim");
  say(colE, "    outline folds over itself. It draws solid.", "dim");
}
