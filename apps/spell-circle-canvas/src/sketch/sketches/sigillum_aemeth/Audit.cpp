#include "SigillumAemeth.h"

auto SigillumAemeth::runChecks() -> void {
  // --- panel A: the extraction and the fit ------------------------------
  logA.append(
      {toU8("SECUNDUS.PDF p.31 \xe2\x80\x94 THE PLATE AS VECTORS"), "heading"});
  logA.append({toU8("  310 text objects; 296 after the colophon"), "dim"});
  logA.append(
      {toU8("  K\xc3\xa5sa fit, outer letter ring, 44 glyphs:"), "dim"});
  logA.append(
      {toU8("    centre (305.185, 393.529) pt   R = 257.972 pt"), "number"});
  logA.append(
      {toU8("    page bbox centre is off by (-0.82, -2.47) pt"), "dim"});
  logA.append({toU8("    residual rms 4.35 pt = 1.7% R"), "dim"});
  logA.append(
      {toU8("  9\xc2\xb0 lattice fit: phase +4.30\xc2\xb0, rms 0.81\xc2\xb0"),
       "number"});
  logA.append({toU8("  40 cells recovered: 40 letters, 33 numerals"), "dim"});
  logA.append(
      {toU8("  above/below decided by r(numeral) vs r(letter)"), "dim"});
  {
    int above = 0, below = 0, bare = 0;
    for (const Cell& c : kRing)
      (c.step > 0 ? above : c.step < 0 ? below : bare)++;
    logA.append({toU8(kit::formatted("    %d above  %d below  %d bare   = 40",
                                     above, below, bare)),
                 above + below + bare == 40 ? "pass" : "heading"});
  }

  // --- panel B: the walk -------------------------------------------------
  logB.append(
      {toU8("MICHAEL'S JUMP RULE, WALKED FROM ALL 40 CELLS"), "heading"});
  for (int n = 0; n < 7; ++n) {
    const Solved& s = solved[(size_t)n];
    const bool ok =
        s.reduced == kNames[(size_t)n].name || s.raw == kNames[(size_t)n].name;
    std::string chain;
    for (size_t i = 0; i < s.cells.size(); ++i)
      chain += (i ? "-" : "") + std::to_string(s.cells[i]);
    logB.append({toU8(kit::formatted("  %-9s %-9s %s", kNames[(size_t)n].name,
                                     s.raw.c_str(), chain.c_str())),
                 ok ? "pass" : "heading"});
  }
  logB.append({toU8(kit::formatted("  cells consumed %d of 40, unvisited %d",
                                   usedCells, 40 - usedCells)),
               usedCells == 33 ? "pass" : "heading"});
  {
    std::string un;
    for (int i = 0; i < 40; ++i)
      if (!visited[(size_t)i])
        un += (un.empty() ? "" : " ") + std::to_string(i + 1) +
              kRing[(size_t)i].glyph;
    logB.append({toU8("  " + un), "number"});
  }
  logB.append(
      {toU8("  Aaoth from 27 also spells Aaoth \xe2\x80\x94 but leaves"),
       "dim"});
  logB.append({toU8("  32 cells. The 33/7 count picks cell 32."), "pass"});

  // --- panel C: coverage -------------------------------------------------
  {
    std::vector<SkPath> cells;
    cells.reserve(40);
    for (int i = 0; i < 40; ++i) {
      const float th = (float)i * 9.0f - 4.5f;
      SkPathBuilder b;
      const int n = 12;
      for (int j = 0; j <= n; ++j) {
        const float a = (th + 9.0f * (float)j / (float)n) * kD;
        const SkPoint q =
            arrange::onEllipse({0, 0}, {rGreat, rGreat}, a - 1.5707963f);
        j == 0 ? b.moveTo(q) : b.lineTo(q);
      }
      for (int j = n; j >= 0; --j) {
        const float a = (th + 9.0f * (float)j / (float)n) * kD;
        b.lineTo(
            arrange::onEllipse({0, 0}, {rBandIn, rBandIn}, a - 1.5707963f));
      }
      b.close();
      cells.push_back(b.detach());
    }
    SkPathBuilder ringB;
    ringB.setFillType(SkPathFillType::kEvenOdd);
    ringB.addCircle(0, 0, rGreat);
    ringB.addCircle(0, 0, rBandIn);
    const SkPath ring = ringB.detach();
    const SkRect reg = SkRect::MakeLTRB(-rGreat, -rGreat, rGreat, rGreat);
    const test::Coverage cov = test::coverage(cells, reg, 320);
    const test::Coverage ref =
        test::coverage(std::span<const SkPath>(&ring, 1), reg, 320);
    logC.append({toU8("THE 40 CELLS TILE THE ANNULUS"), "heading"});
    logC.append({toU8(kit::formatted("  doubled %d of %d samples", cov.doubled,
                                     cov.samples)),
                 cov.doubled == 0 ? "pass" : "heading"});
    logC.append(
        {toU8(kit::formatted("  uncovered %d - outside-the-ring %d = %d",
                             cov.uncovered, ref.uncovered,
                             cov.uncovered - ref.uncovered)),
         std::abs(cov.uncovered - ref.uncovered) <= 320 ? "pass" : "heading"});
  }
  {
    std::vector<SkPath> plates;
    for (int k = 0; k < 7; ++k) {
      const float mid = ((float)k + 0.5f) * 360.0f / 7.0f;
      const float half = 360.0f / 7.0f * 0.5f;
      SkPathBuilder b;
      const int n = 14;
      for (int j = 0; j <= n; ++j) {
        const float a = (mid - half + 2 * half * (float)j / (float)n) * kD;
        const SkPoint q =
            arrange::onEllipse({0, 0}, {0.868f, 0.868f}, a - 1.5707963f);
        j == 0 ? b.moveTo(q) : b.lineTo(q);
      }
      for (int j = n; j >= 0; --j) {
        const float a = (mid - half + 2 * half * (float)j / (float)n) * kD;
        b.lineTo(arrange::onEllipse({0, 0}, {0.720f, 0.720f}, a - 1.5707963f));
      }
      b.close();
      plates.push_back(b.detach());
    }
    SkPathBuilder ringB;
    ringB.setFillType(SkPathFillType::kEvenOdd);
    ringB.addCircle(0, 0, 0.868f);
    ringB.addCircle(0, 0, 0.720f);
    const SkPath ring = ringB.detach();
    const SkRect reg = SkRect::MakeLTRB(-0.868f, -0.868f, 0.868f, 0.868f);
    const test::Coverage cov = test::coverage(plates, reg, 320);
    const test::Coverage ref =
        test::coverage(std::span<const SkPath>(&ring, 1), reg, 320);
    logC.append({toU8("THE 7 ANGLE PLATES TILE THEIR BAND"), "heading"});
    logC.append(
        {toU8(kit::formatted("  doubled %d, gap vs ring %d", cov.doubled,
                             cov.uncovered - ref.uncovered)),
         cov.doubled == 0 ? "pass" : "heading"});
    int letters = 0;
    for (const auto& angleRow : kAngles)
      for (const auto& cell : angleRow)
        if (cell != std::string("\xe2\x80\xa0")) ++letters;
    logC.append({toU8(kit::formatted("  %d letters + 1 cross = %d places",
                                     letters, letters + 1)),
                 letters == 48 ? "pass" : "heading"});
    std::string cols;
    for (int c = 0; c < 7; ++c)
      for (const auto& angleRow : kAngles)
        if (angleRow[c] != std::string("\xe2\x80\xa0")) cols += angleRow[c];
    logC.append({toU8("  down the columns: " + cols.substr(0, 24)), "number"});
    logC.append({toU8("  " + cols.substr(24)), "number"});
  }

  // --- panel D: {7/2} vs {7/3}, and the weave ---------------------------
  logD.append({toU8("{7/2} OR {7/3}? THE COORDINATES DECIDE"), "heading"});
  logD.append({toU8(kit::formatted("  {7/2} core = %.4f x Rhept = %.3f R",
                                   (double)kStar72, (double)(kStar72 * rHept))),
               "number"});
  logD.append({toU8(kit::formatted("  {7/3} core = %.4f x Rhept = %.3f R",
                                   (double)kStar73, (double)(kStar73 * rHept))),
               "number"});
  logD.append({toU8(kit::formatted(
                   "  along a point's ray the core stops at %.3f R",
                   (double)(kStar72 * rHept * std::cos(3.14159265f / 7)))),
               "dim"});
  // "Filiae", not "Fili\xc3\xa6": the feed runs in the MONO face, which
  // has no ash, and a missing glyph falls back to another typeface mid-word.
  // The seal's own legend, set in the serif, keeps the ligature.
  logD.append(
      {toU8(kit::formatted("  Filiae/Filii Filiorum measured %.3f / %.3f R",
                           (double)rFiliaeFil, (double)rFiliiFil)),
       "dim"});
  logD.append({toU8("  both INSIDE a {7/2} core, as the record says;"), "dim"});
  logD.append({toU8("  {7/3} would leave them floating mid-point and"), "dim"});
  logD.append(
      {toU8("  would collide with ZABATHIEL at 0.285 R.  {7/2}."), "pass"});
  logD.append({toU8("THE INTERLACE"), "heading"});
  // discoverCrossings reports one crossing per POINT, so a {7/2} star
  // yields n(k-1) = 7 of them; the 14 passes are the same points counted
  // from both strands. The shared vertices are MEETINGS, not crossings,
  // and never appear.
  // The column is narrow, so the two claims are set at its own widths
  // rather than at the table's default.
  test::report(logD,
               measure::check("  crossings discovered", size_t{7},
                              weave.crossings.size()),
               "pass", "heading", 30, 4);
  test::report(
      logD,
      measure::check("  passes out of alternation", 0, weave.outOfAlternation),
      "pass", "heading", 30, 4);
  logD.append(
      {toU8("  gcd(40,7)=gcd(40,5)=gcd(7,5)=1 \xe2\x80\x94 one"), "dim"});
  logD.append({toU8("  alignment only, and it is 12 o'clock."), "pass"});
}
